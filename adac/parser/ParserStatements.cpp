#include "Parser.h"

#include "Lexer.h"

#include <utility>

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
