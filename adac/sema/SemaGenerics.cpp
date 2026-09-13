#include "Sema.h"

#include "Parser.h"

namespace
{

// Renames the unit a generic was written as to the name of the instance, so
// that its symbols and the names the emitter gives them are its own.
void renameUnit(Decl* decl, const std::string& name, const std::string& lower, const std::string& fromLower)
{
    switch (decl->kind) {
    case DeclKind::PackageSpecification: {
        auto* spec = static_cast<PackageSpecDecl*>(decl);
        if (spec->lower == fromLower) {
            spec->name = name;
            spec->lower = lower;
        }
        break;
    }
    case DeclKind::PackageBody: {
        auto* body = static_cast<PackageBodyDecl*>(decl);
        if (body->lower == fromLower) {
            body->name = name;
            body->lower = lower;
        }
        break;
    }
    case DeclKind::SubprogramDeclaration: {
        auto* declaration = static_cast<SubprogramDecl*>(decl);
        if (declaration->spec.lower == fromLower) {
            declaration->spec.name = name;
            declaration->spec.lower = lower;
        }
        break;
    }
    case DeclKind::SubprogramBody: {
        auto* body = static_cast<SubprogramBody*>(decl);
        if (body->spec.lower == fromLower) {
            body->spec.name = name;
            body->spec.lower = lower;
        }
        break;
    }
    default:
        break;
    }
}

// An unconstrained array has no width until an object of it exists, so it
// cannot be the element type of a file.
bool isDefinite(Type* type)
{
    Type* base = baseType(type);
    return base == nullptr || base->kind != TypeKind::Array || base->constrained;
}

// The symbol a unit declared, which is what makes an instance visible where it
// was written.
Symbol* declaredSymbol(Decl* decl)
{
    switch (decl->kind) {
    case DeclKind::PackageSpecification:
        return static_cast<PackageSpecDecl*>(decl)->symbol;
    case DeclKind::PackageBody:
        return static_cast<PackageBodyDecl*>(decl)->symbol;
    case DeclKind::SubprogramDeclaration:
        return static_cast<SubprogramDecl*>(decl)->symbol;
    case DeclKind::SubprogramBody:
        return static_cast<SubprogramBody*>(decl)->symbol;
    default:
        return nullptr;
    }
}

}

void Sema::analyzeGenericDecl(GenericDecl* decl, Scope* scope)
{
    // A generic unit is not analysed where it stands; only its instances are.
    // One with a dotted name is a child like any other, so it is declared
    // inside its parent rather than under a name with a dot in it.
    Scope* target = scope;
    std::string lower = decl->lower;
    std::string name = decl->name;

    std::size_t dot = decl->lower.find_last_of('.');
    if (dot != std::string::npos) {
        std::size_t pushed = 0;
        Symbol* parent = declarePackagePath(decl->lower.substr(0, dot), decl->name.substr(0, dot), scope,
                                            decl->location, pushed);
        for (std::size_t i = 0; i < pushed; ++i) {
            m_namePrefix.pop_back();
        }
        target = parent->scope;
        lower = decl->lower.substr(dot + 1);
        name = decl->name.substr(dot + 1);
    }

    Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::Generic, lower, name);
    symbol->location = decl->location;
    symbol->generic = decl;
    decl->symbol = symbol;
    target->add(symbol);
}

// A formal type says what kind of type the instantiation may supply, and a
// generic body is only sound for the kinds it was written for.
bool Sema::acceptsFormalType(const GenericFormal& formal, Type* actual, const std::string& genericName,
                             const SourceLocation& location)
{
    Type* base = baseType(actual);
    const char* wanted = nullptr;

    switch (formal.typeClass) {
    case FormalTypeClass::IntegerType:
        if (base->kind != TypeKind::Integer) {
            wanted = "an integer type";
        }
        break;
    case FormalTypeClass::FloatType:
        if (base->kind != TypeKind::Float) {
            wanted = "a floating point type";
        }
        break;
    case FormalTypeClass::Discrete:
        // Character is an enumeration whose literal names this compiler does
        // not record, and Text_IO writes one out directly anyway.
        if (base->kind == TypeKind::Enumeration && base->literals.empty()) {
            m_diagnostics.error(location, "'" + actual->name
                                              + "' names its literals by their spelling, so use Text_IO.Put on one "
                                                "instead of instantiating '"
                                              + genericName + "'");
            return false;
        }
        if (!isDiscrete(base)) {
            wanted = "a discrete type";
        }
        break;
    case FormalTypeClass::Any:
        if (!isDefinite(actual)) {
            wanted = "a type of a fixed size";
        }
        break;
    }

    if (wanted != nullptr) {
        m_diagnostics.error(location, "'" + genericName + "' expects " + wanted + ", and '" + actual->name
                                          + "' is not one");
        return false;
    }
    return true;
}

// Matches the actuals to the formals and declares each one in a scope of its
// own, which the copy of the generic is then analysed inside.
bool Sema::bindGenericFormals(GenericInstantiationDecl* decl, Symbol* generic, Scope* bindings, Scope* scope)
{
    const std::vector<GenericFormal>& formals = generic->generic->formals;
    std::vector<Expr*> actuals(formals.size(), nullptr);

    if (decl->arguments.size() > formals.size()) {
        m_diagnostics.error(decl->location, "too many actual parameters for generic '" + decl->genericName + "'");
        return false;
    }
    for (std::size_t i = 0; i < decl->arguments.size(); ++i) {
        std::size_t index = i;
        if (!decl->arguments[i].nameLower.empty()) {
            index = formals.size();
            for (std::size_t f = 0; f < formals.size(); ++f) {
                if (formals[f].lower == decl->arguments[i].nameLower) {
                    index = f;
                    break;
                }
            }
            if (index == formals.size()) {
                std::string names;
                for (const GenericFormal& candidate : formals) {
                    names += names.empty() ? "'" : "', '";
                    names += candidate.name;
                }
                m_diagnostics.error(decl->location,
                                    "'" + decl->arguments[i].name + "' is not a formal of '" + decl->genericName
                                        + "', which takes " + names + "'");
                return false;
            }
        }
        actuals[index] = decl->arguments[i].value.get();
    }

    for (std::size_t i = 0; i < formals.size(); ++i) {
        const GenericFormal& formal = formals[i];
        Expr* actual = actuals[i];
        if (actual == nullptr && formal.defaultValue) {
            actual = formal.defaultValue.get();
        }
        if (actual == nullptr) {
            m_diagnostics.error(decl->location, "no actual supplied for generic formal '" + formal.name + "'");
            return false;
        }

        if (formal.kind == GenericFormalKind::TypeFormal) {
            Type* type = nullptr;
            if (actual->kind == ExprKind::Identifier) {
                type = resolveTypeName(static_cast<IdentifierExpr*>(actual)->lower, scope, actual->location);
            } else if (actual->kind == ExprKind::Attribute
                       && static_cast<AttributeExpr*>(actual)->lower == "base") {
                type = analyzeAttribute(static_cast<AttributeExpr*>(actual), scope);
            } else if (actual->kind == ExprKind::Selected) {
                auto* selected = static_cast<SelectedExpr*>(actual);
                type = analyzeSelected(selected, scope, nullptr);
                if (selected->symbol == nullptr || selected->symbol->kind != SymbolKind::TypeName) {
                    m_diagnostics.error(actual->location, "a generic formal type expects a type name");
                    return false;
                }
            } else {
                m_diagnostics.error(actual->location, "a generic formal type expects a type name");
                return false;
            }
            if (type == nullptr) {
                return false;
            }
            if (!acceptsFormalType(formal, type, decl->genericName, actual->location)) {
                return false;
            }
            Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::TypeName, formal.lower, formal.name);
            symbol->type = type;
            symbol->location = formal.location;
            bindings->add(symbol);
            continue;
        }

        Type* type = resolveSubtypeIndication(formal.subtype.get(), scope);
        analyzeExpr(actual, scope, type);
        Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::Number, formal.lower, formal.name);
        symbol->type = type;
        symbol->location = formal.location;
        long long value = 0;
        double real = 0.0;
        if (isReal(type) ? foldStaticReal(actual, real) : foldStatic(actual, value)) {
            symbol->hasStaticValue = true;
            symbol->staticValue = value;
            symbol->staticReal = real;
        } else {
            m_diagnostics.error(actual->location, "a generic formal object expects a static value");
        }
        bindings->add(symbol);
    }

    return true;
}

void Sema::analyzeGenericInstantiation(GenericInstantiationDecl* decl, Scope* scope)
{
    Symbol* generic = lookupName(decl->genericLower, scope);
    if (generic == nullptr || generic->kind != SymbolKind::Generic) {
        m_diagnostics.error(decl->location, "'" + decl->genericName + "' is not a generic unit");
        return;
    }
    if (m_instantiationDepth > 16) {
        m_diagnostics.error(decl->location, "generic instantiation nests too deeply");
        return;
    }
    // An instance with a dotted name belongs inside its parent, but it is
    // analysed against the formals it was bound with.  So it is given the
    // simple name here, and only put in its parent once it is built.
    Scope* home = scope;
    std::size_t pushed = 0;
    std::string instanceName = decl->lower;
    std::string instanceDisplay = decl->name;

    std::size_t dot = decl->lower.find_last_of('.');
    if (dot != std::string::npos) {
        Symbol* parent = declarePackagePath(decl->lower.substr(0, dot), decl->name.substr(0, dot), scope,
                                            decl->location, pushed);
        home = parent->scope;
        instanceName = decl->lower.substr(dot + 1);
        instanceDisplay = decl->name.substr(dot + 1);
    }

    Scope* bindings = m_symbolTable.createScope(scope);
    if (!bindGenericFormals(decl, generic, bindings, scope)) {
        for (std::size_t i = 0; i < pushed; ++i) {
            m_namePrefix.pop_back();
        }
        return;
    }

    Parser parser(generic->generic->tokens, m_diagnostics);
    decl->expansion = parser.parseDeclarations();
    for (const DeclPtr& unit : decl->expansion) {
        renameUnit(unit.get(), instanceDisplay, instanceName, generic->generic->lower);
    }

    ++m_instantiationDepth;
    analyzeDeclarativePart(decl->expansion, bindings);
    --m_instantiationDepth;

    for (std::size_t i = 0; i < pushed; ++i) {
        m_namePrefix.pop_back();
    }

    // The instance lives in the bindings scope while it is analysed; here it
    // becomes visible where it was written.
    for (const DeclPtr& unit : decl->expansion) {
        Symbol* symbol = declaredSymbol(unit.get());
        if (symbol != nullptr && symbol->name == instanceName) {
            decl->symbol = symbol;
            home->add(symbol);
            break;
        }
    }
    if (decl->symbol == nullptr) {
        m_diagnostics.error(decl->location, "generic '" + decl->genericName + "' produced no instance");
    }
}
