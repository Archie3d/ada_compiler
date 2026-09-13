#include "Parser.h"

#include <utility>

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
    if (spec.isFunction && check(TokenKind::StringLiteral)) {
        const Token& designator = advance();
        if (designator.text != "**") {
            fail("only the '**' operator designator is supported");
        }
        spec.name = designator.text;
        spec.lower = designator.text;
    } else {
        spec.name = parseCompoundName(spec.lower);
    }

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
