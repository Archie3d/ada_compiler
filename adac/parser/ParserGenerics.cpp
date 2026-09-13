#include "Parser.h"

#include <utility>

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
