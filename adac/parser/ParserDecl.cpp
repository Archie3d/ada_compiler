#include "Parser.h"

#include "Lexer.h"

#include <utility>

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
            if (match(TokenKind::Comma) && (check(TokenKind::Identifier)
                || (check(TokenKind::StringLiteral) && current().text == "**"))) {
                pragma->entity = current().text;
                pragma->entityLower = toLower(current().text);
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
