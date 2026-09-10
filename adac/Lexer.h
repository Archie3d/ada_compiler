#pragma once

#include "Diagnostics.h"
#include "Token.h"

#include <string>
#include <vector>

class Lexer
{
public:
    Lexer(std::string source, int file, Diagnostics& diagnostics);

    std::vector<Token> tokenize();

private:
    char peek(int offset = 0) const;
    bool atEnd() const { return m_position >= m_source.size(); }
    char advance();
    void skipSpacingAndComments();

    Token makeToken(TokenKind kind, const SourceLocation& location, std::string text) const;
    Token lexIdentifierOrKeyword();
    Token lexNumericLiteral();
    Token lexBasedLiteral(const SourceLocation& location, const std::string& baseText);
    Token lexStringLiteral();
    Token lexCharacterLiteral();
    bool tickStartsCharacterLiteral(const Token* previous) const;

    SourceLocation here() const { return SourceLocation { m_line, m_column, m_file }; }

    std::string m_source;
    Diagnostics& m_diagnostics;
    int m_file = 0;
    std::size_t m_position = 0;
    int m_line = 1;
    int m_column = 1;
};

std::string toLower(const std::string& text);
