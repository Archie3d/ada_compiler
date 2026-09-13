#include "Lexer.h"

#include <cctype>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include <utility>

namespace
{

const std::unordered_map<std::string, TokenKind>& keywordTable()
{
    static const std::unordered_map<std::string, TokenKind> table = {
        { "abort", TokenKind::KwAbort },
        { "abs", TokenKind::KwAbs },
        { "accept", TokenKind::KwAccept },
        { "access", TokenKind::KwAccess },
        { "all", TokenKind::KwAll },
        { "and", TokenKind::KwAnd },
        { "array", TokenKind::KwArray },
        { "at", TokenKind::KwAt },
        { "begin", TokenKind::KwBegin },
        { "body", TokenKind::KwBody },
        { "case", TokenKind::KwCase },
        { "constant", TokenKind::KwConstant },
        { "declare", TokenKind::KwDeclare },
        { "delay", TokenKind::KwDelay },
        { "delta", TokenKind::KwDelta },
        { "digits", TokenKind::KwDigits },
        { "do", TokenKind::KwDo },
        { "else", TokenKind::KwElse },
        { "elsif", TokenKind::KwElsif },
        { "end", TokenKind::KwEnd },
        { "entry", TokenKind::KwEntry },
        { "exception", TokenKind::KwException },
        { "exit", TokenKind::KwExit },
        { "for", TokenKind::KwFor },
        { "function", TokenKind::KwFunction },
        { "generic", TokenKind::KwGeneric },
        { "goto", TokenKind::KwGoto },
        { "if", TokenKind::KwIf },
        { "in", TokenKind::KwIn },
        { "is", TokenKind::KwIs },
        { "limited", TokenKind::KwLimited },
        { "loop", TokenKind::KwLoop },
        { "mod", TokenKind::KwMod },
        { "new", TokenKind::KwNew },
        { "not", TokenKind::KwNot },
        { "null", TokenKind::KwNull },
        { "of", TokenKind::KwOf },
        { "or", TokenKind::KwOr },
        { "others", TokenKind::KwOthers },
        { "out", TokenKind::KwOut },
        { "package", TokenKind::KwPackage },
        { "pragma", TokenKind::KwPragma },
        { "private", TokenKind::KwPrivate },
        { "procedure", TokenKind::KwProcedure },
        { "raise", TokenKind::KwRaise },
        { "range", TokenKind::KwRange },
        { "record", TokenKind::KwRecord },
        { "rem", TokenKind::KwRem },
        { "renames", TokenKind::KwRenames },
        { "return", TokenKind::KwReturn },
        { "reverse", TokenKind::KwReverse },
        { "select", TokenKind::KwSelect },
        { "separate", TokenKind::KwSeparate },
        { "subtype", TokenKind::KwSubtype },
        { "task", TokenKind::KwTask },
        { "terminate", TokenKind::KwTerminate },
        { "then", TokenKind::KwThen },
        { "type", TokenKind::KwType },
        { "use", TokenKind::KwUse },
        { "when", TokenKind::KwWhen },
        { "while", TokenKind::KwWhile },
        { "with", TokenKind::KwWith },
        { "xor", TokenKind::KwXor }
    };
    return table;
}

bool isLetter(char c)
{
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
}

bool isDigit(char c)
{
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

bool isExtendedDigit(char c)
{
    return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int digitValue(char c)
{
    if (isDigit(c)) {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return c - 'A' + 10;
}

}

std::string toLower(const std::string& text)
{
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

Lexer::Lexer(std::string source, int file, Diagnostics& diagnostics)
    : m_source(std::move(source))
    , m_diagnostics(diagnostics)
    , m_file(file)
{
}

char Lexer::peek(int offset) const
{
    std::size_t index = m_position + static_cast<std::size_t>(offset);
    if (index >= m_source.size()) {
        return '\0';
    }
    return m_source[index];
}

char Lexer::advance()
{
    char c = m_source[m_position++];
    if (c == '\n') {
        ++m_line;
        m_column = 1;
    } else {
        ++m_column;
    }
    return c;
}

void Lexer::skipSpacingAndComments()
{
    while (!atEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v') {
            advance();
        } else if (c == '-' && peek(1) == '-') {
            while (!atEnd() && peek() != '\n') {
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::makeToken(TokenKind kind, const SourceLocation& location, std::string text) const
{
    Token token;
    token.kind = kind;
    token.location = location;
    token.lower = toLower(text);
    token.text = std::move(text);
    return token;
}

Token Lexer::lexIdentifierOrKeyword()
{
    SourceLocation location = here();
    std::string text;
    while (!atEnd() && (isLetter(peek()) || isDigit(peek()) || peek() == '_')) {
        text.push_back(advance());
    }

    if (text.back() == '_' || text.find("__") != std::string::npos) {
        m_diagnostics.error(location, "an identifier cannot end with an underscore or contain consecutive underscores");
    }

    std::string lower = toLower(text);
    auto it = keywordTable().find(lower);
    TokenKind kind = it != keywordTable().end() ? it->second : TokenKind::Identifier;
    return makeToken(kind, location, std::move(text));
}

Token Lexer::lexNumericLiteral()
{
    SourceLocation location = here();
    std::string text;

    auto readDigits = [&](bool extended) {
        while (!atEnd()) {
            char c = peek();
            if (c == '_') {
                advance();
                continue;
            }
            if (extended ? isExtendedDigit(c) : isDigit(c)) {
                text.push_back(advance());
                continue;
            }
            break;
        }
    };

    std::string integerPart;
    readDigits(false);
    integerPart = text;

    if (peek() == '#') {
        return lexBasedLiteral(location, integerPart);
    }

    bool isReal = false;
    if (peek() == '.' && isDigit(peek(1))) {
        isReal = true;
        text.push_back(advance());
        readDigits(false);
    }
    if (peek() == 'e' || peek() == 'E') {
        char sign = peek(1);
        if (isDigit(sign) || ((sign == '+' || sign == '-') && isDigit(peek(2)))) {
            text.push_back(advance());
            if (peek() == '+' || peek() == '-') {
                text.push_back(advance());
            }
            std::string digits;
            while (!atEnd() && (isDigit(peek()) || peek() == '_')) {
                char c = advance();
                if (c != '_') {
                    digits.push_back(c);
                }
            }
            text += digits;
            if (!isReal && sign == '-') {
                m_diagnostics.error(location, "integer literal cannot have a negative exponent");
            }
            if (!isReal) {
                long long power = std::strtoll(digits.c_str(), nullptr, 10);
                long long value = std::strtoll(integerPart.c_str(), nullptr, 10);
                for (long long i = 0; i < power; ++i) {
                    value *= 10;
                }
                Token token = makeToken(TokenKind::IntegerLiteral, location, text);
                token.intValue = value;
                return token;
            }
        }
    }

    if (isReal) {
        Token token = makeToken(TokenKind::RealLiteral, location, text);
        token.realValue = std::strtod(text.c_str(), nullptr);
        return token;
    }

    Token token = makeToken(TokenKind::IntegerLiteral, location, text);
    token.intValue = std::strtoll(text.c_str(), nullptr, 10);
    return token;
}

// A based literal spells its value in a base from 2 to 16.  It may carry a
// fraction, in which case it denotes a real, and an exponent that scales it by
// a power of that base rather than of ten.
Token Lexer::lexBasedLiteral(const SourceLocation& location, const std::string& baseText)
{
    long long base = std::strtoll(baseText.c_str(), nullptr, 10);
    bool baseIsValid = base >= 2 && base <= 16;

    if (!baseIsValid) {
        m_diagnostics.error(location, "based literal base must be between 2 and 16");
        base = 16;
    }

    std::string spelling = baseText;
    spelling.push_back(advance());   // The opening '#'.

    auto readDigits = [&](std::string& digits) {
        while (!atEnd()) {
            char c = peek();
            if (c == '_') {
                advance();
                continue;
            }
            if (!isExtendedDigit(c)) {
                break;
            }
            digits.push_back(advance());
        }
        spelling += digits;
    };

    bool reportedDigit = false;
    auto valueOf = [&](char c) {
        int digit = digitValue(c);
        if (digit >= base && baseIsValid && !reportedDigit) {
            m_diagnostics.error(location, "digit out of range for the literal base");
            reportedDigit = true;
        }
        return digit < base ? digit : 0;
    };

    std::string wholeDigits;
    readDigits(wholeDigits);
    if (wholeDigits.empty()) {
        m_diagnostics.error(location, "based literal has no digits");
    }

    // The whole part is accumulated both ways: exactly for an integer literal,
    // where overflow has to be reported, and approximately for a real one.
    long long wholeValue = 0;
    double realValue = 0.0;
    bool overflowed = false;
    for (char c : wholeDigits) {
        int digit = valueOf(c);
        if (wholeValue > (LLONG_MAX - digit) / base) {
            overflowed = true;
        } else {
            wholeValue = wholeValue * base + digit;
        }
        realValue = realValue * static_cast<double>(base) + digit;
    }

    bool isReal = false;
    if (peek() == '.') {
        isReal = true;
        spelling.push_back(advance());

        std::string fractionDigits;
        readDigits(fractionDigits);
        if (fractionDigits.empty()) {
            m_diagnostics.error(location, "based literal has no digits after the point");
        }
        double scale = 1.0;
        for (char c : fractionDigits) {
            scale /= static_cast<double>(base);
            realValue += valueOf(c) * scale;
        }
    }

    if (peek() == '#') {
        spelling.push_back(advance());
    } else {
        m_diagnostics.error(location, "missing closing '#' in based literal");
    }

    long long power = 0;
    bool negativePower = false;
    if (peek() == 'e' || peek() == 'E') {
        char sign = peek(1);
        if (isDigit(sign) || ((sign == '+' || sign == '-') && isDigit(peek(2)))) {
            spelling.push_back(advance());
            if (peek() == '+' || peek() == '-') {
                negativePower = peek() == '-';
                spelling.push_back(advance());
            }
            std::string digits;
            while (!atEnd() && (isDigit(peek()) || peek() == '_')) {
                char c = advance();
                if (c != '_') {
                    digits.push_back(c);
                    spelling.push_back(c);
                }
            }
            power = std::strtoll(digits.c_str(), nullptr, 10);
            if (negativePower && !isReal) {
                m_diagnostics.error(location, "integer literal cannot have a negative exponent");
                negativePower = false;
            }
        }
    }

    if (isReal) {
        Token token = makeToken(TokenKind::RealLiteral, location, spelling);
        token.realValue = realValue * std::pow(static_cast<double>(base),
                                               static_cast<double>(negativePower ? -power : power));
        return token;
    }

    for (long long i = 0; i < power && !overflowed; ++i) {
        if (wholeValue > LLONG_MAX / base) {
            overflowed = true;
        } else {
            wholeValue *= base;
        }
    }
    if (overflowed) {
        m_diagnostics.error(location, "based literal is too large");
        wholeValue = 0;
    }

    Token token = makeToken(TokenKind::IntegerLiteral, location, spelling);
    token.intValue = wholeValue;
    return token;
}

Token Lexer::lexStringLiteral()
{
    SourceLocation location = here();
    advance();  // Opening quote.

    std::string value;
    while (true) {
        if (atEnd() || peek() == '\n') {
            m_diagnostics.error(location, "unterminated string literal");
            break;
        }
        char c = advance();
        if (c == '"') {
            if (peek() == '"') {
                advance();
                value.push_back('"');
                continue;
            }
            break;
        }
        value.push_back(c);
    }

    Token token;
    token.kind = TokenKind::StringLiteral;
    token.location = location;
    token.text = value;
    token.lower = value;
    return token;
}

Token Lexer::lexCharacterLiteral()
{
    SourceLocation location = here();
    advance();  // Opening tick.
    char value = advance();
    if (peek() == '\'') {
        advance();
    } else {
        m_diagnostics.error(location, "unterminated character literal");
    }

    Token token;
    token.kind = TokenKind::CharacterLiteral;
    token.location = location;
    token.text = std::string(1, value);
    token.lower = token.text;
    token.intValue = static_cast<unsigned char>(value);
    return token;
}

bool Lexer::tickStartsCharacterLiteral(const Token* previous) const
{
    if (peek(2) != '\'') {
        return false;
    }
    if (previous == nullptr) {
        return true;
    }
    // After a name or a closing parenthesis a tick introduces an attribute.
    switch (previous->kind) {
    case TokenKind::Identifier:
    case TokenKind::RightParen:
    case TokenKind::KwAll:
    case TokenKind::CharacterLiteral:
    case TokenKind::StringLiteral:
        return false;
    default:
        return true;
    }
}

std::vector<Token> Lexer::tokenize()
{
    std::vector<Token> tokens;

    while (true) {
        skipSpacingAndComments();
        if (atEnd()) {
            SourceLocation location = here();
            tokens.push_back(makeToken(TokenKind::EndOfFile, location, ""));
            break;
        }

        SourceLocation location = here();
        char c = peek();

        if (isLetter(c)) {
            tokens.push_back(lexIdentifierOrKeyword());
            continue;
        }
        if (isDigit(c)) {
            tokens.push_back(lexNumericLiteral());
            continue;
        }
        if (c == '"') {
            tokens.push_back(lexStringLiteral());
            continue;
        }
        if (c == '\'') {
            const Token* previous = tokens.empty() ? nullptr : &tokens.back();
            if (tickStartsCharacterLiteral(previous)) {
                tokens.push_back(lexCharacterLiteral());
            } else {
                advance();
                tokens.push_back(makeToken(TokenKind::Tick, location, "'"));
            }
            continue;
        }

        advance();
        char next = peek();
        switch (c) {
        case '&': tokens.push_back(makeToken(TokenKind::Ampersand, location, "&")); break;
        case '(': tokens.push_back(makeToken(TokenKind::LeftParen, location, "(")); break;
        case ')': tokens.push_back(makeToken(TokenKind::RightParen, location, ")")); break;
        case ',': tokens.push_back(makeToken(TokenKind::Comma, location, ",")); break;
        case '+': tokens.push_back(makeToken(TokenKind::Plus, location, "+")); break;
        case '-': tokens.push_back(makeToken(TokenKind::Minus, location, "-")); break;
        case ';': tokens.push_back(makeToken(TokenKind::Semicolon, location, ";")); break;
        case '|': tokens.push_back(makeToken(TokenKind::Bar, location, "|")); break;
        case '*':
            if (next == '*') {
                advance();
                tokens.push_back(makeToken(TokenKind::DoubleStar, location, "**"));
            } else {
                tokens.push_back(makeToken(TokenKind::Star, location, "*"));
            }
            break;
        case '.':
            if (next == '.') {
                advance();
                tokens.push_back(makeToken(TokenKind::DoubleDot, location, ".."));
            } else {
                tokens.push_back(makeToken(TokenKind::Dot, location, "."));
            }
            break;
        case '/':
            if (next == '=') {
                advance();
                tokens.push_back(makeToken(TokenKind::NotEqual, location, "/="));
            } else {
                tokens.push_back(makeToken(TokenKind::Slash, location, "/"));
            }
            break;
        case ':':
            if (next == '=') {
                advance();
                tokens.push_back(makeToken(TokenKind::Assign, location, ":="));
            } else {
                tokens.push_back(makeToken(TokenKind::Colon, location, ":"));
            }
            break;
        case '<':
            if (next == '=') {
                advance();
                tokens.push_back(makeToken(TokenKind::LessEqual, location, "<="));
            } else if (next == '<') {
                advance();
                tokens.push_back(makeToken(TokenKind::LeftLabel, location, "<<"));
            } else if (next == '>') {
                advance();
                tokens.push_back(makeToken(TokenKind::Box, location, "<>"));
            } else {
                tokens.push_back(makeToken(TokenKind::Less, location, "<"));
            }
            break;
        case '=':
            if (next == '>') {
                advance();
                tokens.push_back(makeToken(TokenKind::Arrow, location, "=>"));
            } else {
                tokens.push_back(makeToken(TokenKind::Equal, location, "="));
            }
            break;
        case '>':
            if (next == '=') {
                advance();
                tokens.push_back(makeToken(TokenKind::GreaterEqual, location, ">="));
            } else if (next == '>') {
                advance();
                tokens.push_back(makeToken(TokenKind::RightLabel, location, ">>"));
            } else {
                tokens.push_back(makeToken(TokenKind::Greater, location, ">"));
            }
            break;
        default:
            m_diagnostics.error(location, std::string("illegal character '") + c + "'");
            break;
        }
    }

    return tokens;
}
