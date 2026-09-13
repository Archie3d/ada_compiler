#pragma once

#include "Ast.h"
#include "Diagnostics.h"
#include "Sema.h"

#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// A value is a QBE operand.  Array values additionally carry their bounds, so
// that unconstrained arrays keep working at run time.
struct Value
{
    std::string name;
    char type = 'w';
    std::string first;
    std::string last;

    // Bounds for dimensions 2 through rank; ordinary arrays of arrays keep
    // their component bounds in the component type instead.
    std::vector<std::pair<std::string, std::string>> innerBounds;

    bool hasBounds() const { return !first.empty() && !last.empty(); }
};

class QbeEmitter
{
public:
    QbeEmitter(Sema& sema, Diagnostics& diagnostics);

    void emit(const std::vector<CompilationUnit*>& units, std::ostream& out);

private:
    struct ArrayAggregatePlan
    {
        std::unordered_map<Expr*, Value> choices;
        std::unordered_map<Expr*, Value> shapes;
    };

    struct FunctionContext
    {
        Symbol* symbol = nullptr;
        std::ostringstream prologue;
        std::ostringstream body;
        std::string frameAllocation;
        std::string arrayArena;
        std::string temporaryArena;
        bool arrayArenaUsed = false;
        bool temporaryArenaUsed = false;
        std::string frameTemp;
        bool hasFrame = false;
        bool terminated = false;
        long long frameSize = 8;
        std::unordered_map<Symbol*, std::string> locals;
        std::unordered_map<Symbol*, Value> bounds;
        std::vector<std::string> handlerLabels;
        std::unordered_map<std::string, std::pair<std::string, std::string>> handlerStorage;
        std::vector<std::pair<std::string, std::string>> activeExceptions;
        std::unordered_map<const LoopStmt*, std::string> loopExits;
        std::string propagateLabel;
        bool usesPropagate = false;
        std::vector<SubprogramBody*> nested;
    };

    // Output and data (QbeEmitter.cpp).
    std::string newTemp();
    std::string newLabel(const char* prefix);
    void line(const std::string& text);
    void label(const std::string& name);
    void jump(const std::string& target);
    void branch(const Value& condition, const std::string& ifTrue, const std::string& ifFalse);
    std::string stringData(const std::string& text);
    std::string enumTableFor(const Type* type);

    // Declarations (QbeDecl.cpp).
    void collectGlobals(DeclList& declarations);
    void emitElaborationDeclarations(DeclList& declarations);
    void emitLocalDeclarations(DeclList& declarations);

    // Functions (QbeFunctions.cpp).
    void emitElaboration(const std::vector<CompilationUnit*>& units);
    void emitSubprogramsIn(DeclList& declarations);
    void emitSubprogram(SubprogramBody* body);
    void emitMain();
    void finishFunction(const std::string& signature);

    // Statements (QbeStatements.cpp).
    void emitStatements(StmtList& statements);
    void emitStatement(Stmt* statement);

    // Exceptions and checks (QbeExceptions.cpp).
    void emitRaise(Symbol* exception, const SourceLocation& location);
    void emitExceptionCheck();
    void emitHandlers(std::vector<ExceptionHandler>& handlers, const std::string& afterLabel,
                      const std::string& dispatchLabel);
    void emitRangeCheck(const Value& value, Type* type, const SourceLocation& location);
    void checkVariant(const Value& address, Type* record, int variant);
    std::string rangeTest(const Value& value, long long low, long long high);
    void checkNotNull(const Value& pointer);
    void raiseConstraintError();

    // Storage and assignment (QbeStorage.cpp).
    std::string allocScratch(long long size);
    std::string storageArena(bool temporary, bool allocate = false);
    std::pair<std::string, std::string> storageCheckpoint();
    void rewindStorage(const std::pair<std::string, std::string>& checkpoint);
    Value staticLinkFor(int targetLevel);
    Value addressOf(Symbol* symbol);
    Value loadFrom(const Value& address, Type* type);
    void storeInto(const Value& address, const Value& value, Type* type);
    void copyInto(const Value& destination, const Value& source, Type* type);
    void assignInto(const Value& address, Type* type, Expr* value);

    // Arrays (QbeArrays.cpp).
    void emitDynamicArray(ObjectDecl* object, Symbol* symbol);
    void emitArrayFill(const Value& address, Type* type, Expr* value);
    Value boundsFor(Symbol* symbol);
    Value withBounds(const Value& address, Type* type, Symbol* symbol);
    Value arrayRow(const Value& array, Type* type);
    std::string arrayElementSize(const Value& array, Type* type);
    void checkArrayShape(const Value& target, Type* targetType, const Value& source, Type* sourceType);
    Value lengthOf(const Value& array, Type* type);
    Value emitSlice(CallExpr* expr);
    Value emitConcatenation(BinaryExpr* expr);

    // Expressions and initialization (QbeExpr.cpp).
    Value emitExpr(Expr* expr);
    Value emitAddress(Expr* expr);
    Value emitAllocator(AllocatorExpr* expr);
    bool hasComponentDefaults(Type* type);
    void emitDefaultInit(const Value& address, Type* type);

    // Calls (QbeCalls.cpp).
    Value emitCall(CallExpr* expr);
    Value emitRuntimeCall(CallExpr* expr, Symbol* subprogram);

    // Operators (QbeOperators.cpp).
    Value emitBinary(BinaryExpr* expr);
    Value emitUnary(UnaryExpr* expr);
    Value emitIntegerOperation(int operation, const Value& left, const Value& right, char type);
    Value emitShortCircuit(BinaryExpr* expr);
    Value emitModulo(const Value& left, const Value& right, char type);
    Value emitPower(const Value& left, const Value& right, char type);

    // Comparisons (QbeComparisons.cpp).
    Value compareObjects(const Value& left, const Value& right, Type* type);
    Value compareArrays(BinaryOp op, const Value& left, Type* leftType, const Value& right, Type* rightType);
    Value compareRecords(const Value& left, const Value& right, Type* type);

    // Attributes (QbeAttributes.cpp).
    Value emitAttribute(AttributeExpr* expr);
    Value emitStreamAttribute(AttributeExpr* expr);
    std::string widenToDouble(const Value& value);
    static int defaultAft(const Type* type);

    // Aggregates (QbeAggregates.cpp).
    Value prepareArrayAggregate(Expr* expr, const Value& context, Type* type, ArrayAggregatePlan& plan);
    Value emitDynamicAggregateInto(AggregateExpr* expr, const Value& address, Type* type,
                                   ArrayAggregatePlan* plan = nullptr);
    void emitAggregateInto(AggregateExpr* expr, const Value& address, Type* type);
    Value emitAggregate(AggregateExpr* expr);

    Sema& m_sema;
    Diagnostics& m_diagnostics;
    std::ostringstream m_data;
    std::vector<std::string> m_functions;
    std::vector<SubprogramBody*> m_pendingSubprograms;
    std::unordered_map<std::string, std::string> m_stringPool;
    std::unordered_map<const Type*, std::string> m_enumTables;
    FunctionContext* m_context = nullptr;
    int m_tempCounter = 0;
    int m_labelCounter = 0;
    int m_dataCounter = 0;
};
