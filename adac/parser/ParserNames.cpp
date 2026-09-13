#include "Parser.h"

#include "Lexer.h"

#include <utility>

namespace
{

// Flatten names that can denote a subtype, including the Base attribute.
std::string typeMarkName(Expr* expr)
{
    if (expr->kind == ExprKind::Identifier) {
        return static_cast<IdentifierExpr*>(expr)->lower;
    }
    if (expr->kind == ExprKind::Selected) {
        auto* selected = static_cast<SelectedExpr*>(expr);
        std::string prefix = typeMarkName(selected->prefix.get());
        return prefix.empty() ? "" : prefix + "." + selected->selectorLower;
    }
    if (expr->kind == ExprKind::Attribute) {
        auto* attribute = static_cast<AttributeExpr*>(expr);
        std::string prefix = typeMarkName(attribute->prefix.get());
        if (attribute->lower == "base" && attribute->arguments.empty() && !prefix.empty()) {
            return prefix + "'base";
        }
    }
    return "";
}

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

std::string Parser::parseSubtypeMark(std::string& lowered)
{
    std::string name = parseCompoundName(lowered);
    while (check(TokenKind::Tick) && peek(1).lower == "base") {
        advance();
        advance();
        name += "'Base";
        lowered += "'base";
    }
    return name;
}

void Parser::parseClosingName(const std::string& lower, bool allowSimpleName)
{
    if (check(TokenKind::StringLiteral) && lower == "**") {
        if (advance().text != lower) {
            fail("closing operator designator does not match");
        }
        return;
    }
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

ExprPtr Parser::parseNameSuffixes(ExprPtr prefix)
{
    while (true) {
        if (check(TokenKind::Dot) && (peek(1).kind == TokenKind::Identifier || peek(1).kind == TokenKind::KwAll
            || (peek(1).kind == TokenKind::StringLiteral && peek(1).text == "**"))) {
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
                expr->typeLower = typeMarkName(prefix.get());
                expr->typeName = expr->typeLower;
                if (expr->typeLower.empty()) {
                    fail("qualified expression requires a subtype mark");
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
            if (expr->lower == "base" && check(TokenKind::LeftParen)) {
                // Keep conversion argument ownership and emission identical to
                // ordinary type conversions; resolveTypeName handles the mark.
                auto mark = std::make_unique<IdentifierExpr>();
                mark->location = location;
                mark->name = typeMarkName(expr.get());
                if (mark->name.empty()) {
                    fail("'Base conversion requires a subtype mark");
                }
                mark->lower = mark->name;
                prefix = std::move(mark);
                continue;
            }
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
