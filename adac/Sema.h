#pragma once

#include "Ast.h"
#include "Diagnostics.h"
#include "Scope.h"
#include "Type.h"

#include <string>
#include <unordered_map>
#include <vector>

class Sema
{
public:
    explicit Sema(Diagnostics& diagnostics);

    void analyze(CompilationUnit& unit);

    TypeTable& typeTable() { return m_types; }
    Symbol* mainSubprogram() const { return m_main; }

private:
    void setupStandardScope();
    Symbol* addException(Scope* scope, const std::string& displayName);
    Symbol* addTypeTo(Scope* scope, Type* type);

    void analyzeDeclarativePart(DeclList& declarations, Scope* scope, bool reportIncomplete = true);
    void layoutRecord(TypeDecl* decl, TypeDefinition* definition, Type* type, Scope* scope);
    bool resolveChoice(Expr* lowExpr, Expr* highExpr, Type* selectorType, const std::vector<CaseChoice>& covered,
                       Scope* scope, CaseChoice& choice);
    void reportUncovered(std::vector<CaseChoice> covered, Type* selectorType, const SourceLocation& location,
                         const char* what);
    void reportIncompleteTypes(DeclList& declarations);
    void analyzeDecl(Decl* decl, Scope* scope);
    void analyzeObjectDecl(ObjectDecl* decl, Scope* scope);
    void analyzeNumberDecl(NumberDecl* decl, Scope* scope);
    void analyzeTypeDecl(TypeDecl* decl, Scope* scope);
    void analyzeSubtypeDecl(SubtypeDecl* decl, Scope* scope);
    Symbol* declareSubprogram(SubprogramSpec& spec, Scope* scope, bool isBody);
    void analyzeSubprogramBody(SubprogramBody* body, Scope* scope);
    Symbol* declarePackagePath(const std::string& lower, const std::string& displayName, Scope* scope,
                               const SourceLocation& location, std::size_t& pushed);
    void analyzePackageSpec(PackageSpecDecl* decl, Scope* scope);
    void analyzePackageBody(PackageBodyDecl* decl, Scope* scope);
    void adoptLibraryUnit(PackageSpecDecl* decl, Symbol* package);
    void analyzeUseClause(UseDecl& decl, Scope* scope);
    void analyzeExceptionDecl(ExceptionDecl* decl, Scope* scope);
    void analyzePragma(PragmaDecl* decl, Scope* scope);
    void analyzeRepresentation(RepresentationDecl* decl, Scope* scope);
    void analyzeGenericDecl(GenericDecl* decl, Scope* scope);
    void analyzeGenericInstantiation(GenericInstantiationDecl* decl, Scope* scope);
    bool bindGenericFormals(GenericInstantiationDecl* decl, Symbol* generic, Scope* bindings, Scope* scope);
    bool acceptsFormalType(const GenericFormal& formal, Type* actual, const std::string& genericName,
                           const SourceLocation& location);

    void analyzeStatements(StmtList& statements, Scope* scope);
    void analyzeStatement(Stmt* statement, Scope* scope);
    void analyzeCaseStatement(CaseStmt* statement, Scope* scope);
    Type* choiceSubtypeMark(Expr* expr, Scope* scope);
    std::string describeValue(Type* type, long long value) const;
    void analyzeHandlers(std::vector<ExceptionHandler>& handlers, Scope* scope);
    void checkAssignable(Expr* target, Scope* scope);

    Type* analyzeExpr(Expr* expr, Scope* scope, Type* expected = nullptr);
    Type* analyzeIdentifier(IdentifierExpr* expr, Scope* scope, Type* expected);
    bool matchesResult(Symbol* subprogram, Type* expected) const;
    Symbol* resolveBareName(const std::vector<Symbol*>& candidates, Type* expected,
                            const SourceLocation& location, const std::string& name);
    Type* analyzeSelected(SelectedExpr* expr, Scope* scope, Type* expected);
    Type* constrainDiscriminants(SubtypeIndication* indication, Type* base, Scope* scope);
    int knownVariant(Type* type) const;
    bool hasKnownDiscriminants(Type* type) const;
    bool discriminantValue(Type* type, int index, long long& value) const;

    bool withinPackage(Symbol* package) const;
    bool representationVisible(Type* type) const;
    bool checkNotPrivate(Type* type, const SourceLocation& location, const char* what);

    Type* analyzeCall(CallExpr* expr, Scope* scope, Type* expected);
    Type* analyzeAllocator(AllocatorExpr* expr, Scope* scope, Type* expected);
    Type* analyzeAttribute(AttributeExpr* expr, Scope* scope);
    Type* analyzeAggregate(AggregateExpr* expr, Scope* scope, Type* expected);
    Type* analyzeBinary(BinaryExpr* expr, Scope* scope, Type* expected);
    Type* analyzeBinaryOperation(BinaryExpr* expr, Scope* scope, Type* expected);
    void checkPrivateOperands(BinaryExpr* expr);
    bool checkAggregateDiscriminants(AggregateExpr* expr, Type* target, Scope* scope);
    bool aggregateDiscriminant(AggregateExpr* expr, Type* record, Scope* scope, int index, long long& value,
                               Expr** source = nullptr);
    Type* analyzeUnary(UnaryExpr* expr, Scope* scope, Type* expected);
    Type* analyzeMembership(MembershipExpr* expr, Scope* scope);

    Type* resolveSubtypeIndication(SubtypeIndication* indication, Scope* scope, bool allowDynamic = false);
    Type* resolveTypeName(const std::string& lower, Scope* scope, const SourceLocation& location);
    Symbol* lookupName(const std::string& lower, Scope* scope);
    std::vector<Symbol*> lookupAll(const std::string& lower, Scope* scope);

    bool typesCompatible(Type* target, Type* source) const;
    bool foldStatic(Expr* expr, long long& value) const;
    bool foldStaticReal(Expr* expr, double& value) const;
    void noteStaticValue(Expr* expr);
    void noteReference(Symbol* symbol);
    std::string mangle(const std::string& name) const;
    std::string anonymousTypeName();

    Diagnostics& m_diagnostics;
    TypeTable m_types;
    SymbolTable m_symbolTable;
    Scope* m_standardScope = nullptr;
    Scope* m_globalScope = nullptr;
    std::vector<Symbol*> m_ioExceptions;

    // The packages being analysed, innermost last.  A private type is only
    // transparent while one of them declared it.
    std::vector<Symbol*> m_packages;

    // Set while the visible part of a package specification is being analysed,
    // which is the only place a constant may be named without a value.
    bool m_inVisiblePart = false;
    Type* m_textFileType = nullptr;
    Type* m_fieldType = nullptr;
    Type* m_numberBaseType = nullptr;
    Type* m_typeSetType = nullptr;
    Type* m_addressType = nullptr;
    Symbol* m_main = nullptr;
    Symbol* m_currentSubprogram = nullptr;
    int m_handlerDepth = 0;
    std::vector<LoopStmt*> m_loops;
    std::vector<std::string> m_namePrefix;
    std::unordered_map<std::string, std::size_t> m_subprogramNames;
    int m_anonymousCounter = 0;
    int m_exceptionCounter = 1;
    int m_instantiationDepth = 0;
};
