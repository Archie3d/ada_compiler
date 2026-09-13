#include "Parser.h"

#include <utility>

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
