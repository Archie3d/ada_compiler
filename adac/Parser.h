#pragma once

#include "Ast.h"
#include "Diagnostics.h"
#include "Token.h"

#include <vector>

class Parser
{
public:
    Parser(std::vector<Token> tokens, Diagnostics& diagnostics);

    CompilationUnitPtr parseCompilation();

    // Parses a run of declarations, which is how a generic unit is expanded
    // from the tokens it was written with.
    DeclList parseDeclarations();

private:
    struct ParseError
    {
    };

    const Token& current() const;
    const Token& peek(int offset) const;
    bool check(TokenKind kind) const;
    bool match(TokenKind kind);
    const Token& advance();
    const Token& expect(TokenKind kind, const char* context);
    [[noreturn]] void fail(const std::string& message);
    void skipToSemicolon();

    // Declarations.
    void parseContextClause(CompilationUnit& unit);
    DeclPtr parseLibraryUnit();
    DeclList parseDeclarativePart(bool stopAtPrivate = false);
    DeclPtr parseDeclarativeItem();
    DeclPtr parseObjectOrNumberDecl();
    DeclPtr parseTypeDecl();
    DeclPtr parseSubtypeDecl();
    DeclPtr parseSubprogramDeclOrBody();
    DeclPtr parsePackage();
    DeclPtr parseUseClause();
    DeclPtr parseGenericDeclaration();
    DeclPtr parseGenericInstantiation(const SourceLocation& location, const std::string& name,
                                      const std::string& lower, bool isPackage);
    DeclPtr parsePragma();
    DeclPtr parseRepresentationClause();

    SubprogramSpec parseSubprogramSpec();
    void parseParameterList(SubprogramSpec& spec);
    TypeDefinitionPtr parseTypeDefinition();
    void parseDiscriminantPart(std::vector<RecordField>& discriminants);
    void parseRecordComponents(std::vector<RecordField>& fields);
    VariantPartPtr parseVariantPart();
    SubtypeIndicationPtr parseSubtypeIndication();
    std::vector<std::string> parseIdentifierList(std::vector<std::string>& lowered);
    std::string parseCompoundName(std::string& lowered);

    // Statements.
    StmtList parseSequenceOfStatements();
    std::vector<ExceptionHandler> parseExceptionHandlers();
    StmtPtr parseStatement();
    StmtPtr parseIfStatement();
    StmtPtr parseCaseStatement();
    StmtPtr parseLoopStatement(const std::string& label);
    StmtPtr parseBlockStatement(const std::string& label);
    StmtPtr parseExitStatement();
    StmtPtr parseReturnStatement();
    StmtPtr parseRaiseStatement();
    void parseDiscreteRange(std::string& typeName, std::string& typeLower, ExprPtr& low, ExprPtr& high);

    // Expressions.
    ExprPtr parseExpression();
    ExprPtr parseRelation();
    ExprPtr parseSimpleExpression();
    ExprPtr parseTerm();
    ExprPtr parseFactor();
    ExprPtr parsePrimary();
    ExprPtr parseNameSuffixes(ExprPtr prefix);
    ExprPtr parseParenthesizedOrAggregate();
    ExprPtr makeBinary(BinaryOp op, ExprPtr left, ExprPtr right, const SourceLocation& location);

    std::vector<Token> m_tokens;
    Diagnostics& m_diagnostics;
    std::size_t m_position = 0;
};
