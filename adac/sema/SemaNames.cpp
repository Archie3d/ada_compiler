#include "Sema.h"
#include "SemaSupport.h"

#include <algorithm>

using SemaSupport::splitDottedName;

Symbol* Sema::lookupName(const std::string& lower, Scope* scope)
{
    std::vector<std::string> names = splitDottedName(lower);

    std::vector<Symbol*> candidates = scope->lookup(names.front());
    if (candidates.empty()) {
        return nullptr;
    }
    Symbol* symbol = candidates.front();
    for (Symbol* candidate : candidates) {
        if (names.size() > 1 && candidate->kind == SymbolKind::Package) {
            symbol = candidate;
            break;
        }
    }

    for (std::size_t i = 1; i < names.size(); ++i) {
        if (symbol->scope == nullptr) {
            return nullptr;
        }
        std::vector<Symbol*> nested = symbol->scope->lookupLocal(names[i]);
        if (nested.empty()) {
            return nullptr;
        }
        symbol = nested.front();
    }
    return symbol;
}

std::vector<Symbol*> Sema::lookupAll(const std::string& lower, Scope* scope)
{
    return scope->lookup(lower);
}

Type* Sema::resolveTypeName(const std::string& lower, Scope* scope, const SourceLocation& location)
{
    if (lower.ends_with("'base")) {
        Type* prefix = resolveTypeName(lower.substr(0, lower.size() - 5), scope, location);
        Type* result = m_types.scalarBaseType(prefix);
        if (prefix != nullptr && result == nullptr) {
            m_diagnostics.error(location, "'Base requires a scalar subtype mark");
        }
        return result;
    }
    Symbol* symbol = lookupName(lower, scope);
    if (symbol == nullptr) {
        m_diagnostics.error(location, "'" + lower + "' is not declared");
        return nullptr;
    }
    if (symbol->kind != SymbolKind::TypeName) {
        m_diagnostics.error(location, "'" + symbol->displayName + "' is not a type");
        return nullptr;
    }
    return symbol->type;
}

void Sema::noteReference(Symbol* symbol)
{
    if (symbol == nullptr || symbol->owner == nullptr || symbol->isGlobal) {
        return;
    }
    if (symbol->owner != m_currentSubprogram) {
        symbol->isUplevel = true;
        symbol->owner->needsFrame = true;
    }
}

bool Sema::matchesResult(Symbol* subprogram, Type* expected) const
{
    if (expected == nullptr) {
        return true;
    }
    if (expected->kind == TypeKind::Void) {
        return subprogram->returnType == nullptr;
    }
    return subprogram->returnType != nullptr && typesCompatible(expected, subprogram->returnType);
}

Symbol* Sema::resolveBareName(const std::vector<Symbol*>& candidates, Type* expected,
                              const SourceLocation& location, const std::string& name)
{
    // Ordinary objects and type/package names continue through their existing
    // semantic paths. Overloadable names must have exactly one interpretation.
    for (Symbol* candidate : candidates) {
        if (candidate->kind != SymbolKind::Subprogram && candidate->kind != SymbolKind::EnumerationLiteral) {
            return candidate;
        }
    }
    Symbol* chosen = nullptr;
    for (Symbol* candidate : candidates) {
        if (candidate->kind == SymbolKind::Subprogram) {
            if (!matchesResult(candidate, expected)
                || std::any_of(candidate->parameters.begin(), candidate->parameters.end(), [](Symbol* parameter) {
                    return !parameter->hasDefault;
                })) {
                continue;
            }
        } else if (expected != nullptr && !typesCompatible(expected, candidate->type)) {
            continue;
        }
        if (chosen != nullptr) {
            m_diagnostics.error(location, "ambiguous name '" + name + "'");
            return nullptr;
        }
        chosen = candidate;
    }
    if (chosen == nullptr) {
        m_diagnostics.error(location, "no visible interpretation of '" + name + "' matches this context");
    }
    return chosen;
}

Type* Sema::analyzeIdentifier(IdentifierExpr* expr, Scope* scope, Type* expected)
{
    std::vector<Symbol*> candidates = scope->lookup(expr->lower);
    if (candidates.empty()) {
        m_diagnostics.error(expr->location, "'" + expr->name + "' is not declared");
        return nullptr;
    }

    Symbol* chosen = resolveBareName(candidates, expected, expr->location, expr->name);
    if (chosen == nullptr) {
        return nullptr;
    }
    expr->symbol = chosen;

    switch (chosen->kind) {
    case SymbolKind::Object:
    case SymbolKind::Parameter:
    case SymbolKind::LoopParameter:
        noteReference(chosen);
        expr->type = chosen->type;
        if (chosen->hasStaticValue) {
            expr->isStatic = true;
            expr->staticValue = chosen->staticValue;
            expr->staticReal = chosen->staticReal;
        }
        return expr->type;
    case SymbolKind::Number:
        expr->type = chosen->type;
        expr->isStatic = chosen->hasStaticValue;
        expr->staticValue = chosen->staticValue;
        expr->staticReal = chosen->staticReal;
        return expr->type;
    case SymbolKind::EnumerationLiteral:
        expr->type = chosen->type;
        expr->isStatic = true;
        expr->staticValue = chosen->enumerationValue;
        return expr->type;
    case SymbolKind::Subprogram:
        expr->type = chosen->returnType;
        return expr->type;
    case SymbolKind::TypeName:
        expr->type = chosen->type;
        return expr->type;
    default:
        expr->type = nullptr;
        return nullptr;
    }
}

Type* Sema::analyzeSelected(SelectedExpr* expr, Scope* scope, Type* expected)
{
    // A selected name is either a qualified entity (Package.Entity) or a record
    // component selection.
    Symbol* prefixSymbol = nullptr;
    if (expr->prefix->kind == ExprKind::Identifier) {
        auto* identifier = static_cast<IdentifierExpr*>(expr->prefix.get());
        std::vector<Symbol*> candidates = scope->lookup(identifier->lower);
        for (Symbol* candidate : candidates) {
            if (candidate->kind == SymbolKind::Package) {
                prefixSymbol = candidate;
                identifier->symbol = candidate;
                break;
            }
        }
    } else if (expr->prefix->kind == ExprKind::Selected) {
        auto* selected = static_cast<SelectedExpr*>(expr->prefix.get());
        analyzeSelected(selected, scope, nullptr);
        if (selected->symbol != nullptr && selected->symbol->kind == SymbolKind::Package) {
            prefixSymbol = selected->symbol;
        }
    }

    if (prefixSymbol != nullptr && prefixSymbol->scope != nullptr) {
        std::vector<Symbol*> candidates = prefixSymbol->scope->lookupLocal(expr->selectorLower);
        if (candidates.empty()) {
            m_diagnostics.error(expr->location, "'" + expr->selector + "' is not declared in '"
                                                    + prefixSymbol->displayName + "'");
            return nullptr;
        }
        Symbol* chosen = resolveBareName(candidates, expected, expr->location, expr->selector);
        if (chosen == nullptr) {
            return nullptr;
        }
        expr->symbol = chosen;
        noteReference(chosen);
        if (chosen->kind == SymbolKind::Subprogram) {
            expr->type = chosen->returnType;
        } else {
            expr->type = chosen->type;
            if (chosen->kind == SymbolKind::EnumerationLiteral) {
                expr->isStatic = true;
                expr->staticValue = chosen->enumerationValue;
            } else if (chosen->hasStaticValue) {
                expr->isStatic = true;
                expr->staticValue = chosen->staticValue;
                expr->staticReal = chosen->staticReal;
            }
        }
        return expr->type;
    }

    Type* prefixType = analyzeExpr(expr->prefix.get(), scope, nullptr);

    if (expr->isDereference) {
        Type* access = baseType(prefixType);
        if (access == nullptr || access->kind != TypeKind::Access) {
            m_diagnostics.error(expr->location, "only an access value designates an object with '.all'");
            return nullptr;
        }
        expr->type = access->target;
        return expr->type;
    }

    Type* record = baseType(prefixType);
    if (record != nullptr && record->kind == TypeKind::Access && record->target != nullptr) {
        record = baseType(record->target);
    }
    if (record == nullptr || record->kind != TypeKind::Record) {
        m_diagnostics.error(expr->location, "'" + expr->selector + "' is not a component of a record");
        return nullptr;
    }
    if (!checkNotPrivate(record, expr->location, "'.' reaches inside a value")) {
        return nullptr;
    }
    for (const FieldInfo& field : record->fields) {
        if (field.name != expr->selectorLower) {
            continue;
        }
        expr->fieldIndex = field.index;
        expr->type = field.type;

        // A component of a variant part is only there when the discriminant
        // says so.  Where the subtype fixed it, the answer is known now; where
        // it did not, the emitter asks at run time.
        if (field.variant >= 0) {
            Type* designated = baseType(prefixType) != nullptr && baseType(prefixType)->kind == TypeKind::Access
                                   ? baseType(prefixType)->target
                                   : prefixType;
            int variant = knownVariant(designated);
            if (variant >= 0 && variant != field.variant) {
                m_diagnostics.error(expr->location, "'" + expr->selector + "' belongs to another variant of '"
                                                        + record->name + "' than the one this value has");
                return nullptr;
            }
            expr->checkedVariant = variant < 0 ? field.variant : -1;
        }
        return expr->type;
    }
    m_diagnostics.error(expr->location, "'" + expr->selector + "' is not a component of type '" + record->name + "'");
    return nullptr;
}
