#include "Parser.h"

#include "Lexer.h"

#include <utility>

namespace
{

bool isAttributeName(TokenKind kind)
{
    switch (kind) {
    case TokenKind::Identifier:
    case TokenKind::KwRange:
    case TokenKind::KwAccess:
    case TokenKind::KwDelta:
    case TokenKind::KwDigits:
        return true;
    default:
        return false;
    }
}

}

Parser::Parser(std::vector<Token> tokens, Diagnostics& diagnostics)
    : m_tokens(std::move(tokens))
    , m_diagnostics(diagnostics)
{
}

const Token& Parser::current() const
{
    return m_tokens[m_position];
}

const Token& Parser::peek(int offset) const
{
    std::size_t index = m_position + static_cast<std::size_t>(offset);
    if (index >= m_tokens.size()) {
        return m_tokens.back();
    }
    return m_tokens[index];
}

bool Parser::check(TokenKind kind) const
{
    return current().kind == kind;
}

bool Parser::match(TokenKind kind)
{
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::advance()
{
    const Token& token = m_tokens[m_position];
    if (m_position + 1 < m_tokens.size()) {
        ++m_position;
    }
    return token;
}

const Token& Parser::expect(TokenKind kind, const char* context)
{
    if (check(kind)) {
        return advance();
    }
    fail(std::string("expected '") + tokenKindName(kind) + "' " + context + ", found '"
         + (current().text.empty() ? tokenKindName(current().kind) : current().text) + "'");
}

void Parser::fail(const std::string& message)
{
    m_diagnostics.error(current().location, message);
    throw ParseError {};
}

void Parser::skipToSemicolon()
{
    while (!check(TokenKind::EndOfFile)) {
        if (match(TokenKind::Semicolon)) {
            return;
        }
        advance();
    }
}

// ---------------------------------------------------------------------------
// Compilation unit
// ---------------------------------------------------------------------------

CompilationUnitPtr Parser::parseCompilation()
{
    auto unit = std::make_unique<CompilationUnit>();

    while (!check(TokenKind::EndOfFile)) {
        try {
            parseContextClause(*unit);
            if (check(TokenKind::EndOfFile)) {
                break;
            }
            DeclPtr decl = parseLibraryUnit();
            if (decl) {
                unit->units.push_back(std::move(decl));
            }
        } catch (const ParseError&) {
            skipToSemicolon();
        }
    }

    return unit;
}

DeclList Parser::parseDeclarations()
{
    DeclList declarations;

    while (!check(TokenKind::EndOfFile)) {
        try {
            DeclPtr decl = parseDeclarativeItem();
            if (decl) {
                declarations.push_back(std::move(decl));
            }
        } catch (const ParseError&) {
            skipToSemicolon();
        }
    }

    return declarations;
}

void Parser::parseContextClause(CompilationUnit& unit)
{
    while (true) {
        if (check(TokenKind::KwWith)) {
            WithClause clause;
            clause.location = current().location;
            advance();
            while (true) {
                std::string lowered;
                std::string name = parseCompoundName(lowered);
                clause.names.push_back(name);
                clause.namesLower.push_back(lowered);
                if (!match(TokenKind::Comma)) {
                    break;
                }
            }
            expect(TokenKind::Semicolon, "after with clause");
            unit.withClauses.push_back(std::move(clause));
            continue;
        }
        if (check(TokenKind::KwUse)) {
            DeclPtr use = parseUseClause();
            unit.useClauses.push_back(std::move(*static_cast<UseDecl*>(use.get())));
            continue;
        }
        if (check(TokenKind::KwPragma)) {
            parsePragma();
            continue;
        }
        break;
    }
}

DeclPtr Parser::parseLibraryUnit()
{
    if (check(TokenKind::KwGeneric)) {
        return parseGenericDeclaration();
    }
    if (check(TokenKind::KwProcedure) || check(TokenKind::KwFunction)) {
        return parseSubprogramDeclOrBody();
    }
    if (check(TokenKind::KwPackage)) {
        return parsePackage();
    }
    fail("expected a procedure, function or package declaration");
}

// ---------------------------------------------------------------------------
// Declarations
// ---------------------------------------------------------------------------

DeclList Parser::parseDeclarativePart(bool stopAtPrivate)
{
    DeclList declarations;

    while (true) {
        if (check(TokenKind::EndOfFile) || check(TokenKind::KwBegin) || check(TokenKind::KwEnd)
            || check(TokenKind::KwException)) {
            break;
        }
        if (stopAtPrivate && check(TokenKind::KwPrivate)) {
            break;
        }

        try {
            DeclPtr decl = parseDeclarativeItem();
            if (decl) {
                declarations.push_back(std::move(decl));
            }
        } catch (const ParseError&) {
            skipToSemicolon();
        }
    }

    return declarations;
}

DeclPtr Parser::parseDeclarativeItem()
{
    switch (current().kind) {
    case TokenKind::KwGeneric:
        return parseGenericDeclaration();
    case TokenKind::KwType:
        return parseTypeDecl();
    case TokenKind::KwSubtype:
        return parseSubtypeDecl();
    case TokenKind::KwProcedure:
    case TokenKind::KwFunction:
        return parseSubprogramDeclOrBody();
    case TokenKind::KwPackage:
        return parsePackage();
    case TokenKind::KwUse:
        return parseUseClause();
    case TokenKind::KwPragma:
        return parsePragma();
    case TokenKind::KwFor:
        return parseRepresentationClause();
    case TokenKind::Identifier:
        return parseObjectOrNumberDecl();
    default:
        fail("expected a declaration");
    }
}

std::vector<std::string> Parser::parseIdentifierList(std::vector<std::string>& lowered)
{
    std::vector<std::string> names;
    while (true) {
        const Token& token = expect(TokenKind::Identifier, "in declaration");
        names.push_back(token.text);
        lowered.push_back(token.lower);
        if (!match(TokenKind::Comma)) {
            break;
        }
    }
    return names;
}

std::string Parser::parseCompoundName(std::string& lowered)
{
    const Token& first = expect(TokenKind::Identifier, "in name");
    std::string name = first.text;
    lowered = first.lower;
    while (check(TokenKind::Dot) && peek(1).kind == TokenKind::Identifier) {
        advance();
        const Token& part = advance();
        name += "." + part.text;
        lowered += "." + part.lower;
    }
    return name;
}

DeclPtr Parser::parseObjectOrNumberDecl()
{
    SourceLocation location = current().location;
    std::vector<std::string> lowered;
    std::vector<std::string> names = parseIdentifierList(lowered);
    expect(TokenKind::Colon, "in object declaration");

    if (match(TokenKind::KwException)) {
        auto decl = std::make_unique<ExceptionDecl>();
        decl->location = location;
        decl->names = std::move(names);
        decl->namesLower = std::move(lowered);
        // 'Status_Error : exception renames IO_Exceptions.Status_Error;' is a
        // second name for one exception, not a second exception.
        if (match(TokenKind::KwRenames)) {
            decl->renames = parseCompoundName(decl->renamesLower);
        }
        expect(TokenKind::Semicolon, "after exception declaration");
        return decl;
    }

    bool isConstant = match(TokenKind::KwConstant);
    if (isConstant && check(TokenKind::Assign)) {
        auto decl = std::make_unique<NumberDecl>();
        decl->location = location;
        decl->names = std::move(names);
        decl->namesLower = std::move(lowered);
        advance();
        decl->value = parseExpression();
        expect(TokenKind::Semicolon, "after number declaration");
        return decl;
    }

    auto decl = std::make_unique<ObjectDecl>();
    decl->location = location;
    decl->names = std::move(names);
    decl->namesLower = std::move(lowered);
    decl->isConstant = isConstant;
    decl->subtype = parseSubtypeIndication();
    if (match(TokenKind::Assign)) {
        decl->initializer = parseExpression();
    }
    expect(TokenKind::Semicolon, "after object declaration");
    return decl;
}

DeclPtr Parser::parseTypeDecl()
{
    auto decl = std::make_unique<TypeDecl>();
    decl->location = current().location;
    expect(TokenKind::KwType, "in type declaration");
    const Token& name = expect(TokenKind::Identifier, "in type declaration");
    decl->name = name.text;
    decl->lower = name.lower;

    // 'type Shape (Kind : Figure)' names the discriminants an object of the
    // type is fixed with when it is declared.
    if (check(TokenKind::LeftParen)) {
        parseDiscriminantPart(decl->discriminants);
    }

    // 'type Node;' names a type and says no more about it, which is how a
    // record and an access type pointing at it are declared in either order.
    if (match(TokenKind::Semicolon)) {
        return decl;
    }

    expect(TokenKind::KwIs, "in type declaration");
    decl->definition = parseTypeDefinition();
    expect(TokenKind::Semicolon, "after type declaration");
    return decl;
}

DeclPtr Parser::parseSubtypeDecl()
{
    auto decl = std::make_unique<SubtypeDecl>();
    decl->location = current().location;
    expect(TokenKind::KwSubtype, "in subtype declaration");
    const Token& name = expect(TokenKind::Identifier, "in subtype declaration");
    decl->name = name.text;
    decl->lower = name.lower;
    expect(TokenKind::KwIs, "in subtype declaration");
    decl->subtype = parseSubtypeIndication();
    expect(TokenKind::Semicolon, "after subtype declaration");
    return decl;
}

// '(Kind : Figure; Size : Positive)' after a type name.  The components it
// names come first in every value of the type and are fixed once an object of
// it is declared.
void Parser::parseDiscriminantPart(std::vector<RecordField>& discriminants)
{
    expect(TokenKind::LeftParen, "in discriminant part");
    while (true) {
        SourceLocation location = current().location;
        std::vector<std::string> lowered;
        std::vector<std::string> names = parseIdentifierList(lowered);
        expect(TokenKind::Colon, "in discriminant");
        SubtypeIndicationPtr subtype = parseSubtypeIndication();
        ExprPtr defaultValue;
        if (match(TokenKind::Assign)) {
            defaultValue = parseExpression();
        }

        for (std::size_t i = 0; i < names.size(); ++i) {
            RecordField field;
            field.name = names[i];
            field.lower = lowered[i];
            field.location = location;
            auto copy = std::make_unique<SubtypeIndication>();
            copy->location = subtype->location;
            copy->name = subtype->name;
            copy->lower = subtype->lower;
            field.subtype = std::move(copy);
            if (i + 1 == names.size()) {
                field.subtype = std::move(subtype);
                field.defaultValue = std::move(defaultValue);
            }
            discriminants.push_back(std::move(field));
        }

        if (!match(TokenKind::Semicolon)) {
            break;
        }
    }
    expect(TokenKind::RightParen, "after discriminant part");
}

// The components a record has only when its discriminant holds one of the
// values named, written as a case at the end of the record definition.
VariantPartPtr Parser::parseVariantPart()
{
    auto part = std::make_unique<VariantPart>();
    part->location = current().location;
    expect(TokenKind::KwCase, "in variant part");
    const Token& name = expect(TokenKind::Identifier, "in variant part");
    part->discriminant = name.text;
    part->discriminantLower = name.lower;
    expect(TokenKind::KwIs, "in variant part");

    while (match(TokenKind::KwWhen)) {
        RecordVariant variant;
        variant.location = current().location;
        if (match(TokenKind::KwOthers)) {
            variant.isOthers = true;
        } else {
            while (true) {
                variant.choiceLows.push_back(parseSimpleExpression());
                if (match(TokenKind::DoubleDot)) {
                    variant.choiceHighs.push_back(parseSimpleExpression());
                } else {
                    variant.choiceHighs.push_back(nullptr);
                }
                if (!match(TokenKind::Bar)) {
                    break;
                }
            }
        }
        expect(TokenKind::Arrow, "in variant alternative");
        parseRecordComponents(variant.fields);
        part->variants.push_back(std::move(variant));
    }

    expect(TokenKind::KwEnd, "at end of variant part");
    expect(TokenKind::KwCase, "at end of variant part");
    expect(TokenKind::Semicolon, "after variant part");
    return part;
}

// The component declarations of a record definition or of one alternative of a
// variant part, up to whatever ends them.
void Parser::parseRecordComponents(std::vector<RecordField>& fields)
{
    while (!check(TokenKind::KwEnd) && !check(TokenKind::KwWhen) && !check(TokenKind::KwCase)
           && !check(TokenKind::EndOfFile)) {
        if (match(TokenKind::KwNull)) {
            expect(TokenKind::Semicolon, "after null component");
            continue;
        }
        SourceLocation fieldLocation = current().location;
        std::vector<std::string> lowered;
        std::vector<std::string> names = parseIdentifierList(lowered);
        expect(TokenKind::Colon, "in record component");
        SubtypeIndicationPtr subtype = parseSubtypeIndication();
        ExprPtr defaultValue;
        if (match(TokenKind::Assign)) {
            defaultValue = parseExpression();
        }
        expect(TokenKind::Semicolon, "after record component");
        for (std::size_t i = 0; i < names.size(); ++i) {
            RecordField field;
            field.name = names[i];
            field.lower = lowered[i];
            field.location = fieldLocation;
            auto copy = std::make_unique<SubtypeIndication>();
            copy->location = subtype->location;
            copy->name = subtype->name;
            copy->lower = subtype->lower;
            field.subtype = std::move(copy);
            if (i + 1 == names.size()) {
                field.subtype = std::move(subtype);
                field.defaultValue = std::move(defaultValue);
            }
            fields.push_back(std::move(field));
        }
    }
}

TypeDefinitionPtr Parser::parseTypeDefinition()
{
    SourceLocation location = current().location;

    if (check(TokenKind::LeftParen)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::Enumeration);
        definition->location = location;
        advance();
        while (true) {
            if (check(TokenKind::Identifier) || check(TokenKind::CharacterLiteral)) {
                const Token& literal = advance();
                definition->literals.push_back(literal.text);
                definition->literalsLower.push_back(literal.lower);
            } else {
                fail("expected an enumeration literal");
            }
            if (!match(TokenKind::Comma)) {
                break;
            }
        }
        expect(TokenKind::RightParen, "after enumeration literals");
        return definition;
    }

    // 'is private' and 'is limited private' name a type without saying what it
    // is made of; the private part of the package says that.
    if (check(TokenKind::KwPrivate) || (check(TokenKind::KwLimited) && peek(1).kind == TokenKind::KwPrivate)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::Private);
        definition->location = location;
        definition->isLimited = match(TokenKind::KwLimited);
        expect(TokenKind::KwPrivate, "in private type declaration");
        return definition;
    }

    if (check(TokenKind::KwRange)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::IntegerRange);
        definition->location = location;
        advance();
        definition->rangeLow = parseSimpleExpression();
        expect(TokenKind::DoubleDot, "in range");
        definition->rangeHigh = parseSimpleExpression();
        return definition;
    }

    if (check(TokenKind::KwDigits)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::FloatDigits);
        definition->location = location;
        advance();
        definition->digits = parseSimpleExpression();
        if (match(TokenKind::KwRange)) {
            definition->rangeLow = parseSimpleExpression();
            expect(TokenKind::DoubleDot, "in range");
            definition->rangeHigh = parseSimpleExpression();
        }
        return definition;
    }

    if (check(TokenKind::KwArray)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::Array);
        definition->location = location;
        advance();
        expect(TokenKind::LeftParen, "in array type definition");
        while (true) {
            auto index = std::make_unique<SubtypeIndication>();
            index->location = current().location;
            std::size_t saved = m_position;
            if (check(TokenKind::Identifier)) {
                index->name = parseCompoundName(index->lower);
                if (match(TokenKind::KwRange)) {
                    if (match(TokenKind::Box)) {
                        definition->unconstrainedIndexes = true;
                    } else {
                        index->rangeLow = parseSimpleExpression();
                        expect(TokenKind::DoubleDot, "in index range");
                        index->rangeHigh = parseSimpleExpression();
                    }
                } else if (check(TokenKind::DoubleDot)) {
                    m_position = saved;
                    index->name.clear();
                    index->lower.clear();
                    index->rangeLow = parseSimpleExpression();
                    expect(TokenKind::DoubleDot, "in index range");
                    index->rangeHigh = parseSimpleExpression();
                }
            } else {
                index->rangeLow = parseSimpleExpression();
                expect(TokenKind::DoubleDot, "in index range");
                index->rangeHigh = parseSimpleExpression();
            }
            definition->indexTypes.push_back(std::move(index));
            if (!match(TokenKind::Comma)) {
                break;
            }
        }
        expect(TokenKind::RightParen, "after array index list");
        expect(TokenKind::KwOf, "in array type definition");
        definition->elementType = parseSubtypeIndication();
        return definition;
    }

    if (check(TokenKind::KwRecord) || (check(TokenKind::KwNull) && peek(1).kind == TokenKind::KwRecord)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::Record);
        definition->location = location;
        if (match(TokenKind::KwNull)) {
            expect(TokenKind::KwRecord, "in null record definition");
            return definition;
        }
        advance();
        parseRecordComponents(definition->fields);
        // A variant part comes last, after the components every value has.
        if (check(TokenKind::KwCase)) {
            definition->variant = parseVariantPart();
        }
        expect(TokenKind::KwEnd, "at end of record definition");
        expect(TokenKind::KwRecord, "at end of record definition");
        return definition;
    }

    if (check(TokenKind::KwNew)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::Derived);
        definition->location = location;
        advance();
        definition->parent = parseSubtypeIndication();
        return definition;
    }

    if (check(TokenKind::KwAccess)) {
        auto definition = std::make_unique<TypeDefinition>(TypeDefKind::Access);
        definition->location = location;
        advance();
        definition->parent = parseSubtypeIndication();
        return definition;
    }

    fail("unsupported type definition");
}

SubtypeIndicationPtr Parser::parseSubtypeIndication()
{
    auto indication = std::make_unique<SubtypeIndication>();
    indication->location = current().location;
    indication->name = parseCompoundName(indication->lower);

    if (match(TokenKind::KwDigits)) {
        indication->digits = parseSimpleExpression();
    }

    if (match(TokenKind::KwRange)) {
        indication->rangeLow = parseSimpleExpression();
        expect(TokenKind::DoubleDot, "in range constraint");
        indication->rangeHigh = parseSimpleExpression();
        return indication;
    }

    if (check(TokenKind::LeftParen)) {
        advance();
        while (true) {
            ExprPtr low = parseSimpleExpression();
            ExprPtr high;
            if (match(TokenKind::DoubleDot)) {
                high = parseSimpleExpression();
            }
            indication->indexLows.push_back(std::move(low));
            indication->indexHighs.push_back(std::move(high));
            if (!match(TokenKind::Comma)) {
                break;
            }
        }
        expect(TokenKind::RightParen, "after index constraint");
    }

    return indication;
}

SubprogramSpec Parser::parseSubprogramSpec()
{
    SubprogramSpec spec;
    spec.location = current().location;
    if (match(TokenKind::KwFunction)) {
        spec.isFunction = true;
    } else {
        expect(TokenKind::KwProcedure, "in subprogram specification");
    }

    // A library unit may be a child, as Ada.Unchecked_Deallocation is.
    spec.name = parseCompoundName(spec.lower);

    if (check(TokenKind::LeftParen)) {
        parseParameterList(spec);
    }
    if (spec.isFunction) {
        expect(TokenKind::KwReturn, "in function specification");
        spec.returnType = parseSubtypeIndication();
    }
    return spec;
}

void Parser::parseParameterList(SubprogramSpec& spec)
{
    expect(TokenKind::LeftParen, "in parameter list");
    while (true) {
        SourceLocation location = current().location;
        std::vector<std::string> lowered;
        std::vector<std::string> names = parseIdentifierList(lowered);
        expect(TokenKind::Colon, "in parameter specification");

        ParameterMode mode = ParameterMode::In;
        if (match(TokenKind::KwIn)) {
            mode = match(TokenKind::KwOut) ? ParameterMode::InOut : ParameterMode::In;
        } else if (match(TokenKind::KwOut)) {
            mode = ParameterMode::Out;
        }

        // Parse a separate owned subtree for each name in a grouped profile.
        // Each omitted actual must evaluate its own default, including side effects.
        std::size_t subtypeStart = m_position;
        for (std::size_t i = 0; i < names.size(); ++i) {
            m_position = subtypeStart;
            ParameterDecl parameter;
            parameter.name = names[i];
            parameter.lower = lowered[i];
            parameter.mode = mode;
            parameter.location = location;
            parameter.subtype = parseSubtypeIndication();
            if (match(TokenKind::Assign)) {
                parameter.defaultValue = parseExpression();
            }
            spec.parameters.push_back(std::move(parameter));
        }

        if (!match(TokenKind::Semicolon)) {
            break;
        }
    }
    expect(TokenKind::RightParen, "after parameter list");
}

// A generic unit is remembered as the tokens it was written with.  It is parsed
// once here so that its name is known and its syntax is checked, and then again
// for each instantiation with the formals bound to actuals.
DeclPtr Parser::parseGenericDeclaration()
{
    SourceLocation location = current().location;
    expect(TokenKind::KwGeneric, "in generic declaration");

    auto decl = std::make_unique<GenericDecl>();
    decl->location = location;

    while (!check(TokenKind::KwPackage) && !check(TokenKind::KwProcedure) && !check(TokenKind::KwFunction)) {
        if (check(TokenKind::EndOfFile)) {
            fail("expected a package, procedure or function after the generic formal part");
        }

        GenericFormal formal;
        formal.location = current().location;
        if (match(TokenKind::KwType)) {
            const Token& name = expect(TokenKind::Identifier, "in generic formal type");
            formal.kind = GenericFormalKind::TypeFormal;
            formal.name = name.text;
            formal.lower = name.lower;
            expect(TokenKind::KwIs, "in generic formal type");
            // What follows says which types the instantiation may supply.  Only
            // the first word of it is telling: 'range' and 'digits' each name a
            // family of their own, and '(<>)' asks for a discrete type.
            if (check(TokenKind::KwRange)) {
                formal.typeClass = FormalTypeClass::IntegerType;
            } else if (check(TokenKind::KwDigits)) {
                formal.typeClass = FormalTypeClass::FloatType;
            } else if (check(TokenKind::LeftParen)) {
                formal.typeClass = FormalTypeClass::Discrete;
            }
            while (!check(TokenKind::Semicolon) && !check(TokenKind::EndOfFile)) {
                advance();
            }
        } else if (check(TokenKind::Identifier)) {
            const Token& name = advance();
            formal.kind = GenericFormalKind::ObjectFormal;
            formal.name = name.text;
            formal.lower = name.lower;
            expect(TokenKind::Colon, "in generic formal object");
            match(TokenKind::KwIn);
            match(TokenKind::KwOut);
            formal.subtype = parseSubtypeIndication();
            if (match(TokenKind::Assign)) {
                formal.defaultValue = parseExpression();
            }
        } else {
            fail("expected a generic formal parameter");
        }
        expect(TokenKind::Semicolon, "after generic formal parameter");
        decl->formals.push_back(std::move(formal));
    }

    decl->isPackage = check(TokenKind::KwPackage);

    std::size_t start = m_position;
    DeclPtr unit = decl->isPackage ? parsePackage() : parseSubprogramDeclOrBody();
    if (unit == nullptr) {
        fail("a generic declaration must name a unit");
    }
    switch (unit->kind) {
    case DeclKind::PackageSpecification:
        decl->name = static_cast<PackageSpecDecl*>(unit.get())->name;
        decl->lower = static_cast<PackageSpecDecl*>(unit.get())->lower;
        break;
    case DeclKind::SubprogramDeclaration:
        decl->name = static_cast<SubprogramDecl*>(unit.get())->spec.name;
        decl->lower = static_cast<SubprogramDecl*>(unit.get())->spec.lower;
        break;
    case DeclKind::SubprogramBody:
        decl->name = static_cast<SubprogramBody*>(unit.get())->spec.name;
        decl->lower = static_cast<SubprogramBody*>(unit.get())->spec.lower;
        break;
    default:
        fail("a generic declaration must name a package or a subprogram");
    }

    // The body carries no 'generic' of its own, so it is swallowed here and
    // becomes part of what an instantiation parses.
    while (true) {
        bool packageBody = check(TokenKind::KwPackage) && peek(1).kind == TokenKind::KwBody
            && peek(2).lower == decl->lower;
        bool subprogramBody = (check(TokenKind::KwProcedure) || check(TokenKind::KwFunction))
            && peek(1).lower == decl->lower;
        if (!packageBody && !subprogramBody) {
            break;
        }
        if (packageBody) {
            parsePackage();
        } else {
            parseSubprogramDeclOrBody();
        }
    }

    decl->tokens.assign(m_tokens.begin() + static_cast<std::ptrdiff_t>(start),
                        m_tokens.begin() + static_cast<std::ptrdiff_t>(m_position));
    decl->tokens.push_back(m_tokens.back());   // The end of file the parser stops on.
    return decl;
}

DeclPtr Parser::parseGenericInstantiation(const SourceLocation& location, const std::string& name,
                                          const std::string& lower, bool isPackage)
{
    auto decl = std::make_unique<GenericInstantiationDecl>();
    decl->location = location;
    decl->name = name;
    decl->lower = lower;
    decl->isPackage = isPackage;
    decl->genericName = parseCompoundName(decl->genericLower);

    if (match(TokenKind::LeftParen)) {
        while (true) {
            Association association;
            if (check(TokenKind::Identifier) && peek(1).kind == TokenKind::Arrow) {
                association.name = current().text;
                association.nameLower = current().lower;
                advance();
                advance();
            }
            association.value = parseExpression();
            decl->arguments.push_back(std::move(association));
            if (!match(TokenKind::Comma)) {
                break;
            }
        }
        expect(TokenKind::RightParen, "after generic actual parameters");
    }
    expect(TokenKind::Semicolon, "after generic instantiation");
    return decl;
}

DeclPtr Parser::parseSubprogramDeclOrBody()
{
    SourceLocation location = current().location;
    std::size_t start = m_position;
    SubprogramSpec spec = parseSubprogramSpec();

    if (match(TokenKind::Semicolon)) {
        auto decl = std::make_unique<SubprogramDecl>();
        decl->location = location;
        decl->spec = std::move(spec);
        return decl;
    }

    expect(TokenKind::KwIs, "in subprogram body");
    if (match(TokenKind::KwNew)) {
        return parseGenericInstantiation(location, spec.name, spec.lower, false);
    }
    if (match(TokenKind::KwSeparate)) {
        expect(TokenKind::Semicolon, "after separate");
        auto decl = std::make_unique<SubprogramDecl>();
        decl->location = location;
        decl->spec = std::move(spec);
        return decl;
    }

    auto body = std::make_unique<SubprogramBody>();
    body->location = location;
    body->spec = std::move(spec);
    body->declarations = parseDeclarativePart();
    expect(TokenKind::KwBegin, "in subprogram body");
    body->body = parseSequenceOfStatements();
    if (check(TokenKind::KwException)) {
        body->handlers = parseExceptionHandlers();
    }
    expect(TokenKind::KwEnd, "at end of subprogram body");
    parseClosingName(body->spec.lower, true);
    expect(TokenKind::Semicolon, "after subprogram body");
    body->tokens.assign(m_tokens.begin() + static_cast<std::ptrdiff_t>(start),
                        m_tokens.begin() + static_cast<std::ptrdiff_t>(m_position));
    return body;
}

void Parser::parseClosingName(const std::string& lower, bool allowSimpleName)
{
    if (!check(TokenKind::Identifier)) {
        return;
    }
    SourceLocation location = current().location;
    std::string repeated;
    std::string name = parseCompoundName(repeated);
    std::size_t dot = lower.rfind('.');
    bool simpleName = allowSimpleName && dot != std::string::npos && repeated == lower.substr(dot + 1);
    if (lower.empty() || (repeated != lower && !simpleName)) {
        m_diagnostics.error(location, "closing name '" + name + "' does not match '" + lower + "'");
    }
}

DeclPtr Parser::parsePackage()
{
    SourceLocation location = current().location;
    expect(TokenKind::KwPackage, "in package declaration");

    if (match(TokenKind::KwBody)) {
        auto body = std::make_unique<PackageBodyDecl>();
        body->location = location;
        // The whole body is kept as tokens too, since a generic declared in one
        // file has its body in another and every instance parses it again.
        std::size_t start = m_position - 2;
        body->name = parseCompoundName(body->lower);
        expect(TokenKind::KwIs, "in package body");
        body->declarations = parseDeclarativePart();
        if (match(TokenKind::KwBegin)) {
            body->body = parseSequenceOfStatements();
            if (check(TokenKind::KwException)) {
                body->handlers = parseExceptionHandlers();
            }
        }
        expect(TokenKind::KwEnd, "at end of package body");
        parseClosingName(body->lower, true);
        expect(TokenKind::Semicolon, "after package body");
        body->tokens.assign(m_tokens.begin() + static_cast<std::ptrdiff_t>(start),
                            m_tokens.begin() + static_cast<std::ptrdiff_t>(m_position));
        return body;
    }

    auto spec = std::make_unique<PackageSpecDecl>();
    spec->location = location;
    spec->name = parseCompoundName(spec->lower);
    expect(TokenKind::KwIs, "in package specification");
    if (match(TokenKind::KwNew)) {
        return parseGenericInstantiation(location, spec->name, spec->lower, true);
    }
    spec->publicPart = parseDeclarativePart(true);
    if (match(TokenKind::KwPrivate)) {
        spec->privatePart = parseDeclarativePart();
    }
    expect(TokenKind::KwEnd, "at end of package specification");
    parseClosingName(spec->lower, true);
    expect(TokenKind::Semicolon, "after package specification");
    return spec;
}

DeclPtr Parser::parseUseClause()
{
    auto decl = std::make_unique<UseDecl>();
    decl->location = current().location;
    expect(TokenKind::KwUse, "in use clause");
    match(TokenKind::KwType);
    while (true) {
        std::string lowered;
        std::string name = parseCompoundName(lowered);
        decl->names.push_back(name);
        decl->namesLower.push_back(lowered);
        if (!match(TokenKind::Comma)) {
            break;
        }
    }
    expect(TokenKind::Semicolon, "after use clause");
    return decl;
}

// A pragma the compiler has nothing to say about is read through and dropped.
// Import is kept, because it is what ties a declaration in the predefined
// environment to the C entry point that carries it out.
DeclPtr Parser::parsePragma()
{
    SourceLocation location = current().location;
    expect(TokenKind::KwPragma, "in pragma");

    std::unique_ptr<PragmaDecl> pragma;
    if (check(TokenKind::Identifier)) {
        pragma = std::make_unique<PragmaDecl>();
        pragma->location = location;
        pragma->name = current().text;
        pragma->lower = current().lower;
        advance();

        if (pragma->lower == "import" && match(TokenKind::LeftParen)) {
            // The convention is read and let go: C is the only one there is.
            if (check(TokenKind::Identifier)) {
                advance();
            }
            if (match(TokenKind::Comma) && check(TokenKind::Identifier)) {
                pragma->entity = current().text;
                pragma->entityLower = current().lower;
                advance();
            }
            if (match(TokenKind::Comma) && check(TokenKind::StringLiteral)) {
                pragma->linkName = current().text;
                advance();
            }
        }
    }

    while (!check(TokenKind::Semicolon) && !check(TokenKind::EndOfFile)) {
        advance();
    }
    expect(TokenKind::Semicolon, "after pragma");

    if (pragma != nullptr && pragma->lower == "import" && !pragma->entityLower.empty()
        && !pragma->linkName.empty()) {
        return pragma;
    }
    return nullptr;
}

// A representation clause says how a declaration is to be laid out rather than
// what it means.  Only 'Size is honoured; anything else is read through and
// left to the machine's own judgement.
DeclPtr Parser::parseRepresentationClause()
{
    SourceLocation location = current().location;
    expect(TokenKind::KwFor, "in representation clause");

    auto decl = std::make_unique<RepresentationDecl>();
    decl->location = location;
    decl->name = parseCompoundName(decl->lower);
    expect(TokenKind::Tick, "in representation clause");

    const Token& attribute = expect(TokenKind::Identifier, "in representation clause");
    decl->attribute = attribute.lower;

    expect(TokenKind::KwUse, "in representation clause");
    decl->value = parseExpression();
    expect(TokenKind::Semicolon, "after representation clause");
    return decl;
}

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

StmtList Parser::parseSequenceOfStatements()
{
    StmtList statements;

    while (!check(TokenKind::KwEnd) && !check(TokenKind::KwElse) && !check(TokenKind::KwElsif)
           && !check(TokenKind::KwWhen) && !check(TokenKind::KwException) && !check(TokenKind::EndOfFile)) {
        try {
            StmtPtr statement = parseStatement();
            if (statement) {
                statements.push_back(std::move(statement));
            }
        } catch (const ParseError&) {
            skipToSemicolon();
        }
    }

    return statements;
}

std::vector<ExceptionHandler> Parser::parseExceptionHandlers()
{
    std::vector<ExceptionHandler> handlers;
    expect(TokenKind::KwException, "in exception part");

    while (check(TokenKind::KwWhen)) {
        ExceptionHandler handler;
        handler.location = current().location;
        advance();
        if (check(TokenKind::Identifier) && peek(1).kind == TokenKind::Colon) {
            advance();
            advance();
        }
        if (match(TokenKind::KwOthers)) {
            handler.isOthers = true;
        } else {
            while (true) {
                std::string lowered;
                std::string name = parseCompoundName(lowered);
                handler.names.push_back(name);
                handler.namesLower.push_back(lowered);
                if (!match(TokenKind::Bar)) {
                    break;
                }
            }
        }
        expect(TokenKind::Arrow, "in exception handler");
        handler.body = parseSequenceOfStatements();
        handlers.push_back(std::move(handler));
    }

    return handlers;
}

StmtPtr Parser::parseStatement()
{
    SourceLocation location = current().location;

    switch (current().kind) {
    case TokenKind::KwNull: {
        advance();
        expect(TokenKind::Semicolon, "after null statement");
        auto statement = std::make_unique<NullStmt>();
        statement->location = location;
        return statement;
    }
    case TokenKind::KwIf:
        return parseIfStatement();
    case TokenKind::KwCase:
        return parseCaseStatement();
    case TokenKind::KwWhile:
    case TokenKind::KwFor:
    case TokenKind::KwLoop:
        return parseLoopStatement(std::string());
    case TokenKind::KwDeclare:
    case TokenKind::KwBegin:
        return parseBlockStatement(std::string());
    case TokenKind::KwExit:
        return parseExitStatement();
    case TokenKind::KwReturn:
        return parseReturnStatement();
    case TokenKind::KwRaise:
        return parseRaiseStatement();
    case TokenKind::KwPragma:
        parsePragma();
        return nullptr;
    default:
        break;
    }

    if (check(TokenKind::Identifier) && peek(1).kind == TokenKind::Colon) {
        TokenKind after = peek(2).kind;
        if (after == TokenKind::KwLoop || after == TokenKind::KwWhile || after == TokenKind::KwFor) {
            std::string label = current().lower;
            advance();
            advance();
            return parseLoopStatement(label);
        }
        if (after == TokenKind::KwDeclare || after == TokenKind::KwBegin) {
            std::string label = current().lower;
            advance();
            advance();
            return parseBlockStatement(label);
        }
    }

    ExprPtr name = parsePrimary();
    if (match(TokenKind::Assign)) {
        auto statement = std::make_unique<AssignStmt>();
        statement->location = location;
        statement->target = std::move(name);
        statement->value = parseExpression();
        expect(TokenKind::Semicolon, "after assignment");
        return statement;
    }

    auto statement = std::make_unique<ProcedureCallStmt>();
    statement->location = location;
    statement->call = std::move(name);
    expect(TokenKind::Semicolon, "after procedure call");
    return statement;
}

StmtPtr Parser::parseIfStatement()
{
    auto statement = std::make_unique<IfStmt>();
    statement->location = current().location;
    expect(TokenKind::KwIf, "in if statement");

    while (true) {
        IfBranch branch;
        branch.condition = parseExpression();
        expect(TokenKind::KwThen, "in if statement");
        branch.body = parseSequenceOfStatements();
        statement->branches.push_back(std::move(branch));
        if (!match(TokenKind::KwElsif)) {
            break;
        }
    }

    if (match(TokenKind::KwElse)) {
        statement->hasElse = true;
        statement->elseBody = parseSequenceOfStatements();
    }

    expect(TokenKind::KwEnd, "at end of if statement");
    expect(TokenKind::KwIf, "at end of if statement");
    expect(TokenKind::Semicolon, "after if statement");
    return statement;
}

StmtPtr Parser::parseCaseStatement()
{
    auto statement = std::make_unique<CaseStmt>();
    statement->location = current().location;
    expect(TokenKind::KwCase, "in case statement");
    statement->selector = parseExpression();
    expect(TokenKind::KwIs, "in case statement");

    while (check(TokenKind::KwWhen)) {
        CaseAlternative alternative;
        alternative.location = current().location;
        advance();
        if (match(TokenKind::KwOthers)) {
            alternative.isOthers = true;
        } else {
            while (true) {
                ExprPtr low = parseSimpleExpression();
                ExprPtr high;
                if (match(TokenKind::DoubleDot)) {
                    high = parseSimpleExpression();
                }
                alternative.choiceLows.push_back(std::move(low));
                alternative.choiceHighs.push_back(std::move(high));
                if (!match(TokenKind::Bar)) {
                    break;
                }
            }
        }
        expect(TokenKind::Arrow, "in case alternative");
        alternative.body = parseSequenceOfStatements();
        statement->alternatives.push_back(std::move(alternative));
    }

    expect(TokenKind::KwEnd, "at end of case statement");
    expect(TokenKind::KwCase, "at end of case statement");
    expect(TokenKind::Semicolon, "after case statement");
    return statement;
}

void Parser::parseDiscreteRange(std::string& typeName, std::string& typeLower, ExprPtr& low, ExprPtr& high)
{
    std::size_t saved = m_position;

    if (check(TokenKind::Identifier)) {
        std::string lowered;
        std::string name = parseCompoundName(lowered);
        if (match(TokenKind::KwRange)) {
            typeName = name;
            typeLower = lowered;
            low = parseSimpleExpression();
            expect(TokenKind::DoubleDot, "in discrete range");
            high = parseSimpleExpression();
            return;
        }
        // A name on its own is a type mark, but only where the range ends: a
        // name that something follows, as in 'Length + 1 .. Width', starts an
        // expression like any other.
        if (check(TokenKind::KwLoop) || check(TokenKind::Semicolon) || check(TokenKind::RightParen)
            || check(TokenKind::Comma) || check(TokenKind::Arrow) || check(TokenKind::Bar)) {
            typeName = name;
            typeLower = lowered;
            return;
        }
        m_position = saved;
    }

    std::size_t rangeStart = m_position;
    ExprPtr first = parseSimpleExpression();
    if (first->kind == ExprKind::Attribute) {
        auto* attribute = static_cast<AttributeExpr*>(first.get());
        if (attribute->lower == "range") {
            auto makeAttribute = [&](const char* name) {
                m_position = rangeStart;
                ExprPtr copy = parseSimpleExpression();
                auto* expr = static_cast<AttributeExpr*>(copy.get());
                expr->name = name;
                expr->lower = name;
                return copy;
            };
            low = makeAttribute("first");
            high = makeAttribute("last");
            return;
        }
    }

    low = std::move(first);
    expect(TokenKind::DoubleDot, "in discrete range");
    high = parseSimpleExpression();
}

StmtPtr Parser::parseLoopStatement(const std::string& label)
{
    auto statement = std::make_unique<LoopStmt>();
    statement->location = current().location;
    statement->label = label;
    statement->labelLower = label;

    if (match(TokenKind::KwWhile)) {
        statement->loopKind = LoopKind::While;
        statement->condition = parseExpression();
    } else if (match(TokenKind::KwFor)) {
        statement->loopKind = LoopKind::For;
        const Token& variable = expect(TokenKind::Identifier, "in for loop");
        statement->variableName = variable.text;
        statement->variableLower = variable.lower;
        expect(TokenKind::KwIn, "in for loop");
        statement->isReverse = match(TokenKind::KwReverse);
        parseDiscreteRange(statement->rangeTypeName, statement->rangeTypeLower, statement->rangeLow,
                           statement->rangeHigh);
    }

    expect(TokenKind::KwLoop, "in loop statement");
    statement->body = parseSequenceOfStatements();
    expect(TokenKind::KwEnd, "at end of loop");
    expect(TokenKind::KwLoop, "at end of loop");
    parseClosingName(toLower(label));
    expect(TokenKind::Semicolon, "after loop statement");
    return statement;
}

StmtPtr Parser::parseBlockStatement(const std::string& label)
{
    auto statement = std::make_unique<BlockStmt>();
    statement->location = current().location;
    statement->label = label;

    if (match(TokenKind::KwDeclare)) {
        statement->declarations = parseDeclarativePart();
    }
    expect(TokenKind::KwBegin, "in block statement");
    statement->body = parseSequenceOfStatements();
    if (check(TokenKind::KwException)) {
        statement->handlers = parseExceptionHandlers();
    }
    expect(TokenKind::KwEnd, "at end of block statement");
    parseClosingName(toLower(label));
    expect(TokenKind::Semicolon, "after block statement");
    return statement;
}

StmtPtr Parser::parseExitStatement()
{
    auto statement = std::make_unique<ExitStmt>();
    statement->location = current().location;
    expect(TokenKind::KwExit, "in exit statement");
    if (check(TokenKind::Identifier)) {
        statement->label = current().text;
        statement->labelLower = current().lower;
        advance();
    }
    if (match(TokenKind::KwWhen)) {
        statement->condition = parseExpression();
    }
    expect(TokenKind::Semicolon, "after exit statement");
    return statement;
}

StmtPtr Parser::parseReturnStatement()
{
    auto statement = std::make_unique<ReturnStmt>();
    statement->location = current().location;
    expect(TokenKind::KwReturn, "in return statement");
    if (!check(TokenKind::Semicolon)) {
        statement->value = parseExpression();
    }
    expect(TokenKind::Semicolon, "after return statement");
    return statement;
}

StmtPtr Parser::parseRaiseStatement()
{
    auto statement = std::make_unique<RaiseStmt>();
    statement->location = current().location;
    expect(TokenKind::KwRaise, "in raise statement");
    if (check(TokenKind::Identifier)) {
        statement->name = parseCompoundName(statement->lower);
    }
    expect(TokenKind::Semicolon, "after raise statement");
    return statement;
}

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

ExprPtr Parser::makeBinary(BinaryOp op, ExprPtr left, ExprPtr right, const SourceLocation& location)
{
    auto expr = std::make_unique<BinaryExpr>();
    expr->location = location;
    expr->op = op;
    expr->left = std::move(left);
    expr->right = std::move(right);
    return expr;
}

ExprPtr Parser::parseExpression()
{
    ExprPtr left = parseRelation();

    while (true) {
        SourceLocation location = current().location;
        BinaryOp op;
        if (check(TokenKind::KwAnd)) {
            advance();
            op = match(TokenKind::KwThen) ? BinaryOp::AndThen : BinaryOp::And;
        } else if (check(TokenKind::KwOr)) {
            advance();
            op = match(TokenKind::KwElse) ? BinaryOp::OrElse : BinaryOp::Or;
        } else if (check(TokenKind::KwXor)) {
            advance();
            op = BinaryOp::Xor;
        } else {
            break;
        }
        left = makeBinary(op, std::move(left), parseRelation(), location);
    }

    return left;
}

ExprPtr Parser::parseRelation()
{
    ExprPtr left = parseSimpleExpression();
    SourceLocation location = current().location;

    switch (current().kind) {
    case TokenKind::Equal:
        advance();
        return makeBinary(BinaryOp::Equal, std::move(left), parseSimpleExpression(), location);
    case TokenKind::NotEqual:
        advance();
        return makeBinary(BinaryOp::NotEqual, std::move(left), parseSimpleExpression(), location);
    case TokenKind::Less:
        advance();
        return makeBinary(BinaryOp::Less, std::move(left), parseSimpleExpression(), location);
    case TokenKind::LessEqual:
        advance();
        return makeBinary(BinaryOp::LessEqual, std::move(left), parseSimpleExpression(), location);
    case TokenKind::Greater:
        advance();
        return makeBinary(BinaryOp::Greater, std::move(left), parseSimpleExpression(), location);
    case TokenKind::GreaterEqual:
        advance();
        return makeBinary(BinaryOp::GreaterEqual, std::move(left), parseSimpleExpression(), location);
    default:
        break;
    }

    bool negated = false;
    if (check(TokenKind::KwNot) && peek(1).kind == TokenKind::KwIn) {
        negated = true;
        advance();
    }
    if (check(TokenKind::KwIn)) {
        advance();
        auto membership = std::make_unique<MembershipExpr>();
        membership->location = location;
        membership->negated = negated;
        membership->operand = std::move(left);
        parseDiscreteRange(membership->typeName, membership->typeLower, membership->low, membership->high);
        return membership;
    }

    return left;
}

ExprPtr Parser::parseSimpleExpression()
{
    SourceLocation location = current().location;
    ExprPtr left;

    if (check(TokenKind::Plus) || check(TokenKind::Minus)) {
        bool negate = check(TokenKind::Minus);
        advance();
        auto unary = std::make_unique<UnaryExpr>();
        unary->location = location;
        unary->op = negate ? UnaryOp::Negate : UnaryOp::Plus;
        unary->operand = parseTerm();
        left = std::move(unary);
    } else {
        left = parseTerm();
    }

    while (true) {
        SourceLocation operatorLocation = current().location;
        BinaryOp op;
        if (check(TokenKind::Plus)) {
            op = BinaryOp::Add;
        } else if (check(TokenKind::Minus)) {
            op = BinaryOp::Subtract;
        } else if (check(TokenKind::Ampersand)) {
            op = BinaryOp::Concatenate;
        } else {
            break;
        }
        advance();
        left = makeBinary(op, std::move(left), parseTerm(), operatorLocation);
    }

    return left;
}

ExprPtr Parser::parseTerm()
{
    ExprPtr left = parseFactor();

    while (true) {
        SourceLocation location = current().location;
        BinaryOp op;
        if (check(TokenKind::Star)) {
            op = BinaryOp::Multiply;
        } else if (check(TokenKind::Slash)) {
            op = BinaryOp::Divide;
        } else if (check(TokenKind::KwMod)) {
            op = BinaryOp::Modulo;
        } else if (check(TokenKind::KwRem)) {
            op = BinaryOp::Remainder;
        } else {
            break;
        }
        advance();
        left = makeBinary(op, std::move(left), parseFactor(), location);
    }

    return left;
}

ExprPtr Parser::parseFactor()
{
    SourceLocation location = current().location;

    if (check(TokenKind::KwNot)) {
        advance();
        auto expr = std::make_unique<UnaryExpr>();
        expr->location = location;
        expr->op = UnaryOp::Not;
        expr->operand = parseFactor();
        return expr;
    }
    if (check(TokenKind::KwAbs)) {
        advance();
        auto expr = std::make_unique<UnaryExpr>();
        expr->location = location;
        expr->op = UnaryOp::Abs;
        expr->operand = parseFactor();
        return expr;
    }

    ExprPtr left = parsePrimary();
    if (check(TokenKind::DoubleStar)) {
        SourceLocation operatorLocation = current().location;
        advance();
        return makeBinary(BinaryOp::Power, std::move(left), parsePrimary(), operatorLocation);
    }
    return left;
}

ExprPtr Parser::parsePrimary()
{
    SourceLocation location = current().location;

    switch (current().kind) {
    case TokenKind::IntegerLiteral: {
        auto expr = std::make_unique<IntegerLiteralExpr>();
        expr->location = location;
        expr->value = advance().intValue;
        return expr;
    }
    case TokenKind::RealLiteral: {
        auto expr = std::make_unique<RealLiteralExpr>();
        expr->location = location;
        expr->value = advance().realValue;
        return expr;
    }
    case TokenKind::StringLiteral: {
        auto expr = std::make_unique<StringLiteralExpr>();
        expr->location = location;
        expr->value = advance().text;
        return parseNameSuffixes(std::move(expr));
    }
    case TokenKind::CharacterLiteral: {
        auto expr = std::make_unique<CharacterLiteralExpr>();
        expr->location = location;
        expr->value = advance().text[0];
        return expr;
    }
    case TokenKind::KwNull: {
        advance();
        auto expr = std::make_unique<NullExpr>();
        expr->location = location;
        return expr;
    }
    case TokenKind::KwNew: {
        advance();
        auto expr = std::make_unique<AllocatorExpr>();
        expr->location = location;
        expr->subtype = parseSubtypeIndication();
        // 'new Node'(...)' gives the new object its value straight away.
        if (check(TokenKind::Tick) && peek(1).kind == TokenKind::LeftParen) {
            advance();
            expr->value = parseParenthesizedOrAggregate();
        }
        return expr;
    }
    case TokenKind::LeftParen:
        return parseParenthesizedOrAggregate();
    case TokenKind::Identifier: {
        auto expr = std::make_unique<IdentifierExpr>();
        expr->location = location;
        expr->name = current().text;
        expr->lower = current().lower;
        advance();
        return parseNameSuffixes(std::move(expr));
    }
    default:
        fail("expected an expression");
    }
}

ExprPtr Parser::parseNameSuffixes(ExprPtr prefix)
{
    while (true) {
        if (check(TokenKind::Dot) && (peek(1).kind == TokenKind::Identifier || peek(1).kind == TokenKind::KwAll)) {
            SourceLocation location = current().location;
            advance();
            const Token& selector = advance();
            auto expr = std::make_unique<SelectedExpr>();
            expr->location = location;
            expr->prefix = std::move(prefix);
            expr->selector = selector.text.empty() ? "all" : selector.text;
            expr->selectorLower = selector.lower.empty() ? "all" : selector.lower;
            expr->isDereference = selector.kind == TokenKind::KwAll;
            prefix = std::move(expr);
            continue;
        }

        if (check(TokenKind::LeftParen)) {
            SourceLocation location = current().location;
            advance();
            auto expr = std::make_unique<CallExpr>();
            expr->location = location;
            expr->callee = std::move(prefix);
            if (!check(TokenKind::RightParen)) {
                while (true) {
                    Association association;
                    if (check(TokenKind::Identifier) && peek(1).kind == TokenKind::Arrow) {
                        association.name = current().text;
                        association.nameLower = current().lower;
                        advance();
                        advance();
                    }
                    association.value = parseExpression();
                    if (match(TokenKind::DoubleDot)) {
                        association.high = parseExpression();
                    }
                    expr->arguments.push_back(std::move(association));
                    if (!match(TokenKind::Comma)) {
                        break;
                    }
                }
            }
            expect(TokenKind::RightParen, "after argument list");
            prefix = std::move(expr);
            continue;
        }

        if (check(TokenKind::Tick)) {
            SourceLocation location = current().location;
            if (peek(1).kind == TokenKind::LeftParen) {
                advance();
                advance();
                auto expr = std::make_unique<QualifiedExpr>();
                expr->location = location;
                if (prefix->kind == ExprKind::Identifier) {
                    auto* identifier = static_cast<IdentifierExpr*>(prefix.get());
                    expr->typeName = identifier->name;
                    expr->typeLower = identifier->lower;
                } else if (prefix->kind == ExprKind::Selected) {
                    auto* selected = static_cast<SelectedExpr*>(prefix.get());
                    expr->typeName = selected->selector;
                    expr->typeLower = selected->selectorLower;
                }
                expr->operand = parseExpression();
                expect(TokenKind::RightParen, "after qualified expression");
                prefix = std::move(expr);
                continue;
            }
            if (!isAttributeName(peek(1).kind)) {
                fail("expected an attribute name after '''");
            }
            advance();
            const Token& name = advance();
            auto expr = std::make_unique<AttributeExpr>();
            expr->location = location;
            expr->prefix = std::move(prefix);
            expr->name = name.text.empty() ? tokenKindName(name.kind) : name.text;
            expr->lower = toLower(expr->name);
            if (check(TokenKind::LeftParen)) {
                advance();
                while (true) {
                    expr->arguments.push_back(parseExpression());
                    if (!match(TokenKind::Comma)) {
                        break;
                    }
                }
                expect(TokenKind::RightParen, "after attribute arguments");
            }
            prefix = std::move(expr);
            continue;
        }

        break;
    }

    return prefix;
}

ExprPtr Parser::parseParenthesizedOrAggregate()
{
    SourceLocation location = current().location;
    std::size_t saved = m_position;
    expect(TokenKind::LeftParen, "in expression");

    if (!check(TokenKind::KwOthers)) {
        std::size_t afterParen = m_position;
        bool parenthesized = false;
        ExprPtr inner;
        try {
            inner = parseExpression();
            parenthesized = check(TokenKind::RightParen);
        } catch (const ParseError&) {
            parenthesized = false;
        }
        if (parenthesized) {
            advance();
            return parseNameSuffixes(std::move(inner));
        }
        m_position = afterParen;
    }

    m_position = saved;
    expect(TokenKind::LeftParen, "in aggregate");

    auto aggregate = std::make_unique<AggregateExpr>();
    aggregate->location = location;

    while (true) {
        AggregateComponent component;
        if (match(TokenKind::KwOthers)) {
            component.isOthers = true;
            expect(TokenKind::Arrow, "in aggregate component");
            component.value = parseExpression();
        } else {
            std::size_t start = m_position;
            ExprPtr first = parseExpression();
            bool named = false;
            if (check(TokenKind::DoubleDot) || check(TokenKind::Bar) || check(TokenKind::Arrow)) {
                named = true;
            }
            if (named) {
                m_position = start;
                while (true) {
                    ExprPtr low = parseSimpleExpression();
                    ExprPtr high;
                    if (match(TokenKind::DoubleDot)) {
                        high = parseSimpleExpression();
                    }
                    if (low->kind == ExprKind::Identifier && !high) {
                        component.names.push_back(static_cast<IdentifierExpr*>(low.get())->lower);
                    } else {
                        component.names.push_back(std::string());
                    }
                    component.choiceLows.push_back(std::move(low));
                    component.choiceHighs.push_back(std::move(high));
                    if (!match(TokenKind::Bar)) {
                        break;
                    }
                }
                expect(TokenKind::Arrow, "in aggregate component");
                component.value = parseExpression();
            } else {
                component.value = std::move(first);
            }
        }
        aggregate->components.push_back(std::move(component));
        if (!match(TokenKind::Comma)) {
            break;
        }
    }

    expect(TokenKind::RightParen, "after aggregate");
    return aggregate;
}
