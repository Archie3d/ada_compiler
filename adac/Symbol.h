#pragma once

#include "Ast.h"
#include "Type.h"

#include <string>
#include <vector>

class Scope;

enum class SymbolKind
{
    Object,
    Number,
    TypeName,
    Subprogram,
    Parameter,
    Package,
    EnumerationLiteral,
    Exception,
    LoopParameter,
    Generic
};

enum class BuiltinKind
{
    None,

    // A plain call into the run time library.  The symbol names the C entry
    // point and the parameter list drives how the arguments are marshalled, so
    // that a whole package can be declared without new code in the emitter.
    Runtime
};

struct Symbol
{
    SymbolKind kind = SymbolKind::Object;
    std::string name;         // Lower-cased, used for lookup.
    std::string displayName;  // Original spelling.
    Type* type = nullptr;
    SourceLocation location;

    // Objects, constants and parameters.
    bool isConstant = false;
    bool awaitsValue = false;   // A deferred constant, until its value is given.
    bool isGlobal = false;
    bool byReference = false;
    ParameterMode mode = ParameterMode::In;
    bool hasStaticValue = false;
    long long staticValue = 0;
    double staticReal = 0.0;
    std::string qbeName;   // Global symbol name, including the '$' sigil.

    // A parameter the caller may leave out.
    bool hasDefault = false;
    long long defaultValue = 0;

    // The expression a parameter declared in Ada falls back on.  It belongs to
    // the declaration, so a call that leaves the parameter out simply points at
    // it rather than making a copy.
    Expr* defaultExpr = nullptr;

    // Subprograms.
    std::vector<Symbol*> parameters;
    Type* returnType = nullptr;
    BuiltinKind builtin = BuiltinKind::None;
    bool hasBody = false;

    // Run time builtins.  The symbol names the C entry point, and a call into it
    // may leave an exception pending.
    std::string runtimeSymbol;
    bool canRaise = false;

    // Packages.  A child names the parent it was declared in, which is what
    // lets it see what its parent keeps private.
    Scope* scope = nullptr;
    Symbol* parentPackage = nullptr;

    // Generic units, which keep the declaration they came from so that an
    // instantiation can read the tokens again.
    struct GenericDecl* generic = nullptr;

    // Enumeration literals.
    long long enumerationValue = 0;

    // Exceptions.
    int exceptionId = 0;

    // Nesting: subprograms know their static level, objects know the subprogram
    // that owns them.  Objects referenced from a nested subprogram live in the
    // owner's frame block instead of a plain stack slot.
    int level = 0;
    Symbol* owner = nullptr;
    bool isUplevel = false;
    long long frameOffset = -1;
    long long frameSize = 8;
    bool needsFrame = false;
};
