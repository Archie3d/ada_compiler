#include "Parser.h"

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
