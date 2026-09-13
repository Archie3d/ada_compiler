#include "Parser.h"

#include <utility>

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
        if (current().text == "**" && peek(1).kind == TokenKind::LeftParen) {
            auto expr = std::make_unique<IdentifierExpr>();
            expr->location = location;
            expr->name = advance().text;
            expr->lower = expr->name;
            return parseNameSuffixes(std::move(expr));
        }
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

ExprPtr Parser::makeBinary(BinaryOp op, ExprPtr left, ExprPtr right, const SourceLocation& location)
{
    auto expr = std::make_unique<BinaryExpr>();
    expr->location = location;
    expr->op = op;
    expr->left = std::move(left);
    expr->right = std::move(right);
    return expr;
}
