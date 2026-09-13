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

    // Token navigation and compilation units (Parser.cpp).
    const Token& current() const;
    const Token& peek(int offset) const;
    bool check(TokenKind kind) const;
    bool match(TokenKind kind);
    const Token& advance();
    const Token& expect(TokenKind kind, const char* context);
    [[noreturn]] void fail(const std::string& message);
    void skipToSemicolon();
    void parseContextClause(CompilationUnit& unit);
    DeclPtr parseLibraryUnit();

    // Declarations (ParserDecl.cpp).
    DeclList parseDeclarativePart(bool stopAtPrivate = false);
    DeclPtr parseDeclarativeItem();
    DeclPtr parseObjectOrNumberDecl();
    DeclPtr parseUseClause();
    DeclPtr parsePragma();
    DeclPtr parseRepresentationClause();

    // Types and ranges (ParserTypes.cpp).
    DeclPtr parseTypeDecl();
    DeclPtr parseSubtypeDecl();
    TypeDefinitionPtr parseTypeDefinition();
    void parseDiscriminantPart(std::vector<RecordField>& discriminants);
    void parseRecordComponents(std::vector<RecordField>& fields);
    VariantPartPtr parseVariantPart();
    SubtypeIndicationPtr parseSubtypeIndication();
    void parseDiscreteRange(std::string& typeName, std::string& typeLower, ExprPtr& low, ExprPtr& high);

    // Subprograms (ParserSubprograms.cpp).
    SubprogramSpec parseSubprogramSpec();
    void parseParameterList(SubprogramSpec& spec);
    DeclPtr parseSubprogramDeclOrBody();

    // Packages (ParserPackages.cpp).
    DeclPtr parsePackage();

    // Generics (ParserGenerics.cpp).
    DeclPtr parseGenericDeclaration();
    DeclPtr parseGenericInstantiation(const SourceLocation& location, const std::string& name,
                                      const std::string& lower, bool isPackage);

    // Statements (ParserStatements.cpp).
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

    // Expressions (ParserExpr.cpp).
    ExprPtr parseExpression();
    ExprPtr parseRelation();
    ExprPtr parseSimpleExpression();
    ExprPtr parseTerm();
    ExprPtr parseFactor();
    ExprPtr parsePrimary();
    ExprPtr parseParenthesizedOrAggregate();
    ExprPtr makeBinary(BinaryOp op, ExprPtr left, ExprPtr right, const SourceLocation& location);

    // Names (ParserNames.cpp).
    std::vector<std::string> parseIdentifierList(std::vector<std::string>& lowered);
    std::string parseCompoundName(std::string& lowered);
    std::string parseSubtypeMark(std::string& lowered);
    void parseClosingName(const std::string& lower, bool allowSimpleName = false);
    ExprPtr parseNameSuffixes(ExprPtr prefix);

    std::vector<Token> m_tokens;
    Diagnostics& m_diagnostics;
    std::size_t m_position = 0;
};
