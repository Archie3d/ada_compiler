#pragma once

#include "Diagnostics.h"
#include "Token.h"

#include <memory>
#include <string>
#include <vector>

class Type;
struct Symbol;

struct Node
{
    SourceLocation location;
    virtual ~Node() = default;
};

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

enum class ExprKind
{
    Identifier,
    Selected,
    IntegerLiteral,
    RealLiteral,
    StringLiteral,
    CharacterLiteral,
    Null,
    Binary,
    Unary,
    Call,
    Attribute,
    Aggregate,
    Qualified,
    Membership,
    Allocator
};

enum class BinaryOp
{
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,
    Remainder,
    Power,
    Concatenate,
    And,
    Or,
    Xor,
    AndThen,
    OrElse,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual
};

enum class UnaryOp
{
    Plus,
    Negate,
    Not,
    Abs
};

struct Expr : Node
{
    explicit Expr(ExprKind exprKind)
        : kind(exprKind)
    {
    }

    ExprKind kind;
    Type* type = nullptr;
    bool isStatic = false;
    long long staticValue = 0;
    double staticReal = 0.0;   // Holds the folded value of a real expression.
};

using ExprPtr = std::unique_ptr<Expr>;

struct IdentifierExpr : Expr
{
    IdentifierExpr()
        : Expr(ExprKind::Identifier)
    {
    }

    std::string name;
    std::string lower;
    Symbol* symbol = nullptr;
};

struct SelectedExpr : Expr
{
    SelectedExpr()
        : Expr(ExprKind::Selected)
    {
    }

    ExprPtr prefix;
    std::string selector;
    std::string selectorLower;
    Symbol* symbol = nullptr;      // Set when the selection names an entity.
    int fieldIndex = -1;           // Set when the selection is a record component.
    bool isDereference = false;    // Set for 'P.all', the object an access value designates.

    // Set when the component belongs to a variant part and nothing fixed the
    // discriminant, so the emitter has to ask at run time which variant the
    // value has.  Holds the alternative the component belongs to.
    int checkedVariant = -1;
};

struct IntegerLiteralExpr : Expr
{
    IntegerLiteralExpr()
        : Expr(ExprKind::IntegerLiteral)
    {
    }

    long long value = 0;
};

struct RealLiteralExpr : Expr
{
    RealLiteralExpr()
        : Expr(ExprKind::RealLiteral)
    {
    }

    double value = 0.0;
};

struct StringLiteralExpr : Expr
{
    StringLiteralExpr()
        : Expr(ExprKind::StringLiteral)
    {
    }

    std::string value;
};

struct CharacterLiteralExpr : Expr
{
    CharacterLiteralExpr()
        : Expr(ExprKind::CharacterLiteral)
    {
    }

    char value = '\0';
};

struct NullExpr : Expr
{
    NullExpr()
        : Expr(ExprKind::Null)
    {
    }
};

struct BinaryExpr : Expr
{
    BinaryExpr()
        : Expr(ExprKind::Binary)
    {
    }

    BinaryOp op = BinaryOp::Add;
    ExprPtr left;
    ExprPtr right;
};

struct UnaryExpr : Expr
{
    UnaryExpr()
        : Expr(ExprKind::Unary)
    {
    }

    UnaryOp op = UnaryOp::Plus;
    ExprPtr operand;
};

struct Association
{
    std::string name;   // Named notation, empty when positional.
    std::string nameLower;
    ExprPtr value;
    ExprPtr high;       // Upper bound, set when the argument is a slice range.
};

// A parenthesised name application: a subprogram call, an array indexing or a
// type conversion.  Semantic analysis decides which one it is.
enum class CallForm
{
    Unresolved,
    Subprogram,
    Indexing,
    Conversion,
    Slice
};

struct CallExpr : Expr
{
    CallExpr()
        : Expr(ExprKind::Call)
    {
    }

    ExprPtr callee;
    std::vector<Association> arguments;
    CallForm form = CallForm::Unresolved;
    Symbol* subprogram = nullptr;
    std::vector<Expr*> resolvedArguments;  // Positional order after resolution.
};

struct AttributeExpr : Expr
{
    AttributeExpr()
        : Expr(ExprKind::Attribute)
    {
    }

    ExprPtr prefix;
    std::string name;
    std::string lower;
    std::vector<ExprPtr> arguments;
    Type* prefixType = nullptr;
};

struct AggregateComponent
{
    // Empty choices means positional notation.
    std::vector<std::string> names;      // Record component names.
    std::vector<ExprPtr> choiceLows;     // Array index choices.
    std::vector<ExprPtr> choiceHighs;    // Matching upper bounds, may be null.
    bool isOthers = false;
    ExprPtr value;
};

struct AggregateExpr : Expr
{
    AggregateExpr()
        : Expr(ExprKind::Aggregate)
    {
    }

    std::vector<AggregateComponent> components;
    std::vector<Expr*> resolvedFields;  // Record aggregates, in field order.
};

struct QualifiedExpr : Expr
{
    QualifiedExpr()
        : Expr(ExprKind::Qualified)
    {
    }

    std::string typeName;
    std::string typeLower;
    ExprPtr operand;
};

struct MembershipExpr : Expr
{
    MembershipExpr()
        : Expr(ExprKind::Membership)
    {
    }

    ExprPtr operand;
    bool negated = false;
    std::string typeName;   // "X in Positive"
    std::string typeLower;
    ExprPtr low;            // "X in 1 .. 10"
    ExprPtr high;
};

// ---------------------------------------------------------------------------
// Type and subtype denotations
// ---------------------------------------------------------------------------

struct SubtypeIndication : Node
{
    std::string name;
    std::string lower;
    ExprPtr digits;   // Accuracy constraint on a floating point subtype.
    ExprPtr rangeLow;
    ExprPtr rangeHigh;
    std::vector<ExprPtr> indexLows;   // Index constraint for array subtypes.
    std::vector<ExprPtr> indexHighs;
    Type* resolved = nullptr;
};

using SubtypeIndicationPtr = std::unique_ptr<SubtypeIndication>;

// 'new Node' or 'new Node'(Value)': takes storage for one object of the
// designated subtype and yields an access value pointing at it.
struct AllocatorExpr : Expr
{
    AllocatorExpr()
        : Expr(ExprKind::Allocator)
    {
    }

    SubtypeIndicationPtr subtype;
    ExprPtr value;            // The qualified expression, when one is given.
    Type* designated = nullptr;
};

// One choice of a case alternative, once Sema has worked out what it stands
// for.  A single value is a range of one, and a subtype mark is the range the
// subtype covers, so the emitter has only the one shape to deal with.
struct CaseChoice
{
    long long low = 0;
    long long high = 0;
};

enum class TypeDefKind
{
    Enumeration,
    IntegerRange,
    FloatDigits,
    Array,
    Record,
    Derived,
    Access,
    Private
};

struct RecordField
{
    std::string name;
    std::string lower;
    SubtypeIndicationPtr subtype;
    ExprPtr defaultValue;
    SourceLocation location;
};

// One alternative of a variant part: the discriminant values it answers to and
// the components that only exist when the discriminant has one of them.
struct RecordVariant
{
    std::vector<ExprPtr> choiceLows;
    std::vector<ExprPtr> choiceHighs;   // Null when the choice is a single value.
    std::vector<CaseChoice> choices;
    bool isOthers = false;
    std::vector<RecordField> fields;
    SourceLocation location;
};

// 'case Kind is when ... end case;' at the end of a record definition.  Which
// components a value of the record has depends on its discriminant.
struct VariantPart
{
    std::string discriminant;
    std::string discriminantLower;
    int discriminantIndex = -1;
    std::vector<RecordVariant> variants;
    SourceLocation location;
};

using VariantPartPtr = std::unique_ptr<VariantPart>;

struct TypeDefinition : Node
{
    explicit TypeDefinition(TypeDefKind defKind)
        : kind(defKind)
    {
    }

    TypeDefKind kind;

    // Enumeration
    std::vector<std::string> literals;
    std::vector<std::string> literalsLower;

    // Integer range, and the optional range of a floating point type
    ExprPtr rangeLow;
    ExprPtr rangeHigh;

    // Floating point
    ExprPtr digits;

    // Array
    std::vector<SubtypeIndicationPtr> indexTypes;
    bool unconstrainedIndexes = false;
    SubtypeIndicationPtr elementType;

    // Record
    std::vector<RecordField> fields;
    VariantPartPtr variant;

    // Derived / access
    SubtypeIndicationPtr parent;

    // Private
    bool isLimited = false;
};

using TypeDefinitionPtr = std::unique_ptr<TypeDefinition>;

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

enum class StmtKind
{
    Null,
    Assign,
    ProcedureCall,
    If,
    Loop,
    Exit,
    Return,
    Case,
    Block,
    Raise
};

struct Stmt : Node
{
    explicit Stmt(StmtKind stmtKind)
        : kind(stmtKind)
    {
    }

    StmtKind kind;
};

using StmtPtr = std::unique_ptr<Stmt>;
using StmtList = std::vector<StmtPtr>;

struct Decl;
using DeclPtr = std::unique_ptr<Decl>;
using DeclList = std::vector<DeclPtr>;

struct ExceptionHandler
{
    std::vector<std::string> names;
    std::vector<std::string> namesLower;
    std::vector<int> identifiers;   // Exception identifiers filled in by analysis.
    bool isOthers = false;
    StmtList body;
    SourceLocation location;
};

struct NullStmt : Stmt
{
    NullStmt()
        : Stmt(StmtKind::Null)
    {
    }
};

struct AssignStmt : Stmt
{
    AssignStmt()
        : Stmt(StmtKind::Assign)
    {
    }

    ExprPtr target;
    ExprPtr value;
};

struct ProcedureCallStmt : Stmt
{
    ProcedureCallStmt()
        : Stmt(StmtKind::ProcedureCall)
    {
    }

    ExprPtr call;
};

struct IfBranch
{
    ExprPtr condition;
    StmtList body;
};

struct IfStmt : Stmt
{
    IfStmt()
        : Stmt(StmtKind::If)
    {
    }

    std::vector<IfBranch> branches;
    StmtList elseBody;
    bool hasElse = false;
};

enum class LoopKind
{
    Plain,
    While,
    For
};

struct LoopStmt : Stmt
{
    LoopStmt()
        : Stmt(StmtKind::Loop)
    {
    }

    LoopKind loopKind = LoopKind::Plain;
    std::string label;
    std::string labelLower;
    ExprPtr condition;

    std::string variableName;
    std::string variableLower;
    bool isReverse = false;
    std::string rangeTypeName;
    std::string rangeTypeLower;
    ExprPtr rangeLow;
    ExprPtr rangeHigh;
    Symbol* variableSymbol = nullptr;

    StmtList body;
};

struct ExitStmt : Stmt
{
    ExitStmt()
        : Stmt(StmtKind::Exit)
    {
    }

    std::string label;
    std::string labelLower;
    ExprPtr condition;
    LoopStmt* target = nullptr;
};

struct ReturnStmt : Stmt
{
    ReturnStmt()
        : Stmt(StmtKind::Return)
    {
    }

    ExprPtr value;
};

struct CaseAlternative
{
    std::vector<ExprPtr> choiceLows;
    std::vector<ExprPtr> choiceHighs;   // Null when the choice is a single value.
    std::vector<CaseChoice> choices;
    bool isOthers = false;
    StmtList body;
    SourceLocation location;
};

struct CaseStmt : Stmt
{
    CaseStmt()
        : Stmt(StmtKind::Case)
    {
    }

    ExprPtr selector;
    std::vector<CaseAlternative> alternatives;
};

// The declarative part makes this node depend on declarations, which are
// defined further down; its special members live in Ast.cpp.
struct BlockStmt : Stmt
{
    BlockStmt();
    ~BlockStmt() override;

    std::string label;
    DeclList declarations;
    StmtList body;
    std::vector<ExceptionHandler> handlers;
};

struct RaiseStmt : Stmt
{
    RaiseStmt()
        : Stmt(StmtKind::Raise)
    {
    }

    std::string name;
    std::string lower;
    Symbol* exceptionSymbol = nullptr;
};

// ---------------------------------------------------------------------------
// Declarations
// ---------------------------------------------------------------------------

enum class DeclKind
{
    Object,
    Number,
    Type,
    Subtype,
    SubprogramDeclaration,
    SubprogramBody,
    PackageSpecification,
    PackageBody,
    Use,
    Exception,
    GenericDeclaration,
    GenericInstantiation,
    Pragma,
    Representation
};

struct Decl : Node
{
    explicit Decl(DeclKind declKind)
        : kind(declKind)
    {
    }

    DeclKind kind;
};

struct ObjectDecl : Decl
{
    ObjectDecl()
        : Decl(DeclKind::Object)
    {
    }

    std::vector<std::string> names;
    std::vector<std::string> namesLower;
    bool isConstant = false;
    SubtypeIndicationPtr subtype;
    ExprPtr initializer;
    std::vector<Symbol*> symbols;

    // A deferred constant: named in the visible part of a package, with its
    // value given in the private part.  The declaration that gives the value
    // is the one that takes storage for it.
    bool awaitsValue = false;
};

struct NumberDecl : Decl
{
    NumberDecl()
        : Decl(DeclKind::Number)
    {
    }

    std::vector<std::string> names;
    std::vector<std::string> namesLower;
    ExprPtr value;
    std::vector<Symbol*> symbols;
};

struct TypeDecl : Decl
{
    TypeDecl()
        : Decl(DeclKind::Type)
    {
    }

    std::string name;
    std::string lower;
    std::vector<RecordField> discriminants;
    TypeDefinitionPtr definition;
    Type* declaredType = nullptr;
};

struct SubtypeDecl : Decl
{
    SubtypeDecl()
        : Decl(DeclKind::Subtype)
    {
    }

    std::string name;
    std::string lower;
    SubtypeIndicationPtr subtype;
    Type* declaredType = nullptr;
};

enum class ParameterMode
{
    In,
    Out,
    InOut
};

struct ParameterDecl
{
    std::string name;
    std::string lower;
    ParameterMode mode = ParameterMode::In;
    SubtypeIndicationPtr subtype;
    ExprPtr defaultValue;
    SourceLocation location;
    Symbol* symbol = nullptr;
};

struct SubprogramSpec
{
    bool isFunction = false;
    std::string name;
    std::string lower;
    std::vector<ParameterDecl> parameters;
    SubtypeIndicationPtr returnType;
    SourceLocation location;
};

struct SubprogramDecl : Decl
{
    SubprogramDecl()
        : Decl(DeclKind::SubprogramDeclaration)
    {
    }

    SubprogramSpec spec;
    Symbol* symbol = nullptr;
};

struct SubprogramBody : Decl
{
    SubprogramBody()
        : Decl(DeclKind::SubprogramBody)
    {
    }

    SubprogramSpec spec;
    DeclList declarations;
    StmtList body;
    std::vector<ExceptionHandler> handlers;
    Symbol* symbol = nullptr;

    // As for a package body: a generic subprogram declared in one file has its
    // body in another, and every instance parses that body again.
    std::vector<Token> tokens;
};

struct PackageSpecDecl : Decl
{
    PackageSpecDecl()
        : Decl(DeclKind::PackageSpecification)
    {
    }

    std::string name;
    std::string lower;
    DeclList publicPart;
    DeclList privatePart;
    Symbol* symbol = nullptr;
};

struct PackageBodyDecl : Decl
{
    PackageBodyDecl()
        : Decl(DeclKind::PackageBody)
    {
    }

    std::string name;
    std::string lower;
    DeclList declarations;
    StmtList body;
    std::vector<ExceptionHandler> handlers;
    Symbol* symbol = nullptr;

    // The tokens the body was written with.  They matter when the body belongs
    // to a generic declared in another file, since an instance has to parse the
    // whole unit again.
    std::vector<Token> tokens;
};

struct UseDecl : Decl
{
    UseDecl()
        : Decl(DeclKind::Use)
    {
    }

    std::vector<std::string> names;
    std::vector<std::string> namesLower;
};

struct ExceptionDecl : Decl
{
    ExceptionDecl()
        : Decl(DeclKind::Exception)
    {
    }

    std::vector<std::string> names;
    std::vector<std::string> namesLower;

    // Set when the declaration is a renaming, and so gives another name to an
    // exception that already exists rather than making one.
    std::string renames;
    std::string renamesLower;

    std::vector<Symbol*> symbols;
};

// ---------------------------------------------------------------------------
// Generics
// ---------------------------------------------------------------------------

enum class GenericFormalKind
{
    // 'type T is private', 'type T is (<>)', 'type T is range <>' and the
    // like.  The instantiation supplies a type mark.
    TypeFormal,

    // 'Size : Integer := 10'.  The instantiation supplies a static value.
    ObjectFormal
};

// What a formal type will accept.  Ada writes this after 'is', and the
// instantiation has to honour it: Integer_IO is declared with 'range <>' and so
// cannot be made with an enumeration type.
enum class FormalTypeClass
{
    Any,
    Discrete,
    IntegerType,
    FloatType
};

struct GenericFormal
{
    GenericFormalKind kind = GenericFormalKind::TypeFormal;
    FormalTypeClass typeClass = FormalTypeClass::Any;
    std::string name;
    std::string lower;
    SubtypeIndicationPtr subtype;   // The type of an object formal.
    ExprPtr defaultValue;
    SourceLocation location;
};

// A generic unit is kept as the tokens it was written with.  Each instance
// parses them again, which costs one pass over a short token run and saves
// teaching every node in this file how to copy itself.
struct GenericDecl : Decl
{
    GenericDecl()
        : Decl(DeclKind::GenericDeclaration)
    {
    }

    std::string name;
    std::string lower;
    bool isPackage = true;
    std::vector<GenericFormal> formals;
    std::vector<Token> tokens;
    Symbol* symbol = nullptr;
};

struct GenericInstantiationDecl : Decl
{
    GenericInstantiationDecl()
        : Decl(DeclKind::GenericInstantiation)
    {
    }

    std::string name;
    std::string lower;
    std::string genericName;
    std::string genericLower;
    bool isPackage = true;
    std::vector<Association> arguments;
    Symbol* symbol = nullptr;

    // Filled in by Sema for a generic written in Ada: what came back from
    // parsing the tokens again, analysed as though it were written here.
    DeclList expansion;
};

// A pragma the compiler acts on.  Only Import is understood, and it is what
// lets the predefined environment be written in Ada while the work is still
// done by the C run time:
//
//     pragma Import (C, Put_Line, "__ada_put_line");
//
struct PragmaDecl : Decl
{
    PragmaDecl()
        : Decl(DeclKind::Pragma)
    {
    }

    std::string name;
    std::string lower;
    std::string entity;
    std::string entityLower;
    std::string linkName;
};

// How wide a type is to be laid out, which is how Ada says that a stream
// element takes one byte and not the four an integer type would otherwise:
//
//     for Stream_Element'Size use 8;
//
struct RepresentationDecl : Decl
{
    RepresentationDecl()
        : Decl(DeclKind::Representation)
    {
    }

    std::string name;
    std::string lower;
    std::string attribute;
    ExprPtr value;
};

// ---------------------------------------------------------------------------
// Compilation unit
// ---------------------------------------------------------------------------

struct WithClause
{
    std::vector<std::string> names;
    std::vector<std::string> namesLower;
    SourceLocation location;
};

struct CompilationUnit
{
    std::vector<WithClause> withClauses;
    std::vector<UseDecl> useClauses;
    DeclList units;
    std::string fileName;
};

using CompilationUnitPtr = std::unique_ptr<CompilationUnit>;
