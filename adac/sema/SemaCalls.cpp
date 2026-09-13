#include "Sema.h"
#include "SemaSupport.h"

#include <algorithm>

using SemaSupport::adaptUniversal;

Type* Sema::analyzeCall(CallExpr* expr, Scope* scope, Type* expected)
{
    // Collect the entities the callee may denote.
    std::vector<Symbol*> candidates;
    if (expr->callee->kind == ExprKind::Identifier) {
        auto* identifier = static_cast<IdentifierExpr*>(expr->callee.get());
        candidates = scope->lookup(identifier->lower);
        if (identifier->lower.ends_with("'base")) {
            Type* type = resolveTypeName(identifier->lower, scope, identifier->location);
            if (type == nullptr) {
                return nullptr;
            }
            Symbol* mark = m_symbolTable.createSymbol(SymbolKind::TypeName, identifier->lower, identifier->name);
            mark->type = type;
            candidates = { mark };
        }
        if (candidates.empty()) {
            m_diagnostics.error(expr->callee->location, "'" + identifier->name + "' is not declared");
            return nullptr;
        }
    } else if (expr->callee->kind == ExprKind::Selected) {
        auto* selected = static_cast<SelectedExpr*>(expr->callee.get());
        Symbol* prefixSymbol = nullptr;
        if (selected->prefix->kind == ExprKind::Identifier) {
            auto* identifier = static_cast<IdentifierExpr*>(selected->prefix.get());
            for (Symbol* candidate : scope->lookup(identifier->lower)) {
                if (candidate->kind == SymbolKind::Package) {
                    prefixSymbol = candidate;
                    identifier->symbol = candidate;
                    break;
                }
            }
        } else if (selected->prefix->kind == ExprKind::Selected) {
            auto* nested = static_cast<SelectedExpr*>(selected->prefix.get());
            analyzeSelected(nested, scope, nullptr);
            if (nested->symbol != nullptr && nested->symbol->kind == SymbolKind::Package) {
                prefixSymbol = nested->symbol;
            }
        }
        if (prefixSymbol != nullptr && prefixSymbol->scope != nullptr) {
            candidates = prefixSymbol->scope->lookupLocal(selected->selectorLower);
            if (candidates.empty()) {
                m_diagnostics.error(expr->callee->location, "'" + selected->selector + "' is not declared in '"
                                                                + prefixSymbol->displayName + "'");
                return nullptr;
            }
        }
    }

    // A range as the only argument selects a slice of an array.
    if (expr->arguments.size() == 1 && expr->arguments.front().high) {
        Type* prefixType = analyzeExpr(expr->callee.get(), scope, nullptr);
        if (prefixType == nullptr || prefixType->kind != TypeKind::Array) {
            m_diagnostics.error(expr->location, "only an array can be sliced");
            return nullptr;
        }
        if (prefixType->arrayRank > 1) {
            m_diagnostics.error(expr->location, "only one-dimensional arrays can be sliced");
            return nullptr;
        }
        Expr* low = expr->arguments.front().value.get();
        Expr* high = expr->arguments.front().high.get();
        analyzeExpr(low, scope, prefixType->index);
        analyzeExpr(high, scope, prefixType->index);
        adaptUniversal(low, prefixType->index != nullptr ? prefixType->index : m_types.integerType());
        adaptUniversal(high, prefixType->index != nullptr ? prefixType->index : m_types.integerType());

        Type* result = m_types.makeSubtype(anonymousTypeName(), rootType(prefixType), 0, 0);
        result->element = prefixType->element;
        result->index = prefixType->index;
        long long lowBound = 0;
        long long highBound = 0;
        if (foldStatic(low, lowBound) && foldStatic(high, highBound)) {
            result->constrained = true;
            result->indexLow = lowBound;
            result->indexHigh = highBound;
        } else {
            result->constrained = false;
        }

        expr->form = CallForm::Slice;
        expr->resolvedArguments = { low, high };
        expr->type = result;
        return expr->type;
    }

    // Reject malformed associations before selecting a profile. In particular,
    // a repeated name must not overwrite an earlier actual argument.
    bool sawNamed = false;
    std::vector<std::string> names;
    for (const Association& association : expr->arguments) {
        if (association.nameLower.empty()) {
            if (sawNamed) {
                m_diagnostics.error(association.value->location, "a positional argument cannot follow a named argument");
                return nullptr;
            }
        } else {
            sawNamed = true;
            if (std::find(names.begin(), names.end(), association.nameLower) != names.end()) {
                m_diagnostics.error(association.value->location, "a parameter cannot be supplied more than once");
                return nullptr;
            }
            names.push_back(association.nameLower);
        }
    }

    // Result context can eliminate profiles before their arguments are resolved.
    bool hasSubprograms = std::any_of(candidates.begin(), candidates.end(), [](Symbol* candidate) {
        return candidate->kind == SymbolKind::Subprogram;
    });
    if (expected != nullptr && hasSubprograms) {
        std::erase_if(candidates, [&](Symbol* candidate) {
            return candidate->kind == SymbolKind::Subprogram
                && !matchesResult(candidate, expected);
        });
        if (candidates.empty() && (expr->callee->kind == ExprKind::Identifier
                                  || expr->callee->kind == ExprKind::Selected)) {
            m_diagnostics.error(expr->location, "no visible subprogram matches this call");
            return nullptr;
        }
    }

    for (std::size_t argumentIndex = 0; argumentIndex < expr->arguments.size(); ++argumentIndex) {
        const Association& association = expr->arguments[argumentIndex];
        // An aggregate only means something once the parameter it fills is
        // known, so it waits until the profile has been chosen.
        if (association.value->kind == ExprKind::Aggregate) {
            continue;
        }
        if (association.value->type == nullptr) {
            // A shared formal type provides context to nested calls without
            // prematurely choosing between otherwise distinct overloads.
            Type* context = nullptr;
            bool differs = false;
            for (Symbol* candidate : candidates) {
                if (candidate->kind != SymbolKind::Subprogram) {
                    continue;
                }
                Symbol* parameter = nullptr;
                if (association.nameLower.empty()) {
                    if (argumentIndex < candidate->parameters.size()) {
                        parameter = candidate->parameters[argumentIndex];
                    }
                } else {
                    for (Symbol* formal : candidate->parameters) {
                        if (formal->name == association.nameLower) {
                            parameter = formal;
                            break;
                        }
                    }
                }
                if (parameter != nullptr) {
                    if (context != nullptr && rootType(context) != rootType(parameter->type)) {
                        differs = true;
                    }
                    context = parameter->type;
                }
            }
            ExprKind kind = association.value->kind;
            bool needsContext = kind == ExprKind::Call || kind == ExprKind::Identifier
                || kind == ExprKind::Selected || kind == ExprKind::Allocator || kind == ExprKind::Null;
            analyzeExpr(association.value.get(), scope, differs || !needsContext ? nullptr : context);
        }
    }

    // A type mark applied to one argument is a type conversion.
    if (candidates.size() == 1 && candidates.front()->kind == SymbolKind::TypeName) {
        if (expr->arguments.size() != 1) {
            m_diagnostics.error(expr->location, "a type conversion takes exactly one operand");
            return nullptr;
        }
        expr->form = CallForm::Conversion;
        expr->type = candidates.front()->type;
        Expr* operand = expr->arguments.front().value.get();
        expr->resolvedArguments.push_back(operand);
        // A literal only takes the type it is converted to when both belong to
        // the same family; crossing families is what the conversion is for.
        if (isReal(expr->type) == isReal(operand->type)) {
            adaptUniversal(operand, expr->type);
        }

        bool numeric = isNumeric(baseType(expr->type)) && isNumeric(baseType(operand->type));
        if (!numeric && !typesCompatible(expr->type, operand->type)) {
            m_diagnostics.error(expr->location, "this type conversion is not allowed");
        }
        if (operand->isStatic && isReal(expr->type) == isReal(operand->type)) {
            expr->isStatic = true;
            expr->staticValue = operand->staticValue;
            expr->staticReal = operand->staticReal;
        }
        return expr->type;
    }

    std::vector<Symbol*> subprograms;
    for (Symbol* candidate : candidates) {
        if (candidate->kind == SymbolKind::Subprogram) {
            subprograms.push_back(candidate);
        }
    }

    if (!subprograms.empty()) {
        Symbol* chosen = nullptr;
        for (Symbol* candidate : subprograms) {
            if (candidate->parameters.size() < expr->arguments.size()) {
                continue;
            }
            std::vector<bool> filled(candidate->parameters.size(), false);
            bool matches = true;
            for (std::size_t i = 0; i < expr->arguments.size(); ++i) {
                std::size_t index = i;
                if (!expr->arguments[i].nameLower.empty()) {
                    matches = false;
                    for (std::size_t p = 0; p < candidate->parameters.size(); ++p) {
                        if (candidate->parameters[p]->name == expr->arguments[i].nameLower) {
                            index = p;
                            matches = true;
                            break;
                        }
                    }
                    if (!matches) {
                        break;
                    }
                }
                // An argument still without a type is an aggregate, which suits
                // whichever composite parameter it lands on.
                if (expr->arguments[i].value->type != nullptr
                    && !typesCompatible(candidate->parameters[index]->type, expr->arguments[i].value->type)) {
                    matches = false;
                    break;
                }
                if (filled[index]) {
                    matches = false;
                    break;
                }
                filled[index] = true;
            }
            // Whatever the caller left out has to have a default of its own.
            for (std::size_t p = 0; matches && p < candidate->parameters.size(); ++p) {
                if (!filled[p] && !candidate->parameters[p]->hasDefault) {
                    matches = false;
                }
            }
            if (matches) {
                if (chosen != nullptr) {
                    m_diagnostics.error(expr->location, "ambiguous subprogram call");
                    return nullptr;
                }
                chosen = candidate;
            }
        }

        if (chosen == nullptr) {
            m_diagnostics.error(expr->location, "no visible subprogram matches this call");
            return nullptr;
        }

        expr->form = CallForm::Subprogram;
        expr->subprogram = chosen;
        expr->resolvedArguments.assign(chosen->parameters.size(), nullptr);
        for (std::size_t i = 0; i < expr->arguments.size(); ++i) {
            std::size_t index = i;
            if (!expr->arguments[i].nameLower.empty()) {
                for (std::size_t p = 0; p < chosen->parameters.size(); ++p) {
                    if (chosen->parameters[p]->name == expr->arguments[i].nameLower) {
                        index = p;
                        break;
                    }
                }
            }
            Expr* argument = expr->arguments[i].value.get();
            adaptUniversal(argument, chosen->parameters[index]->type);
            if (argument->kind == ExprKind::Aggregate || argument->kind == ExprKind::StringLiteral
                || argument->kind == ExprKind::Null || argument->kind == ExprKind::Allocator) {
                analyzeExpr(argument, scope, chosen->parameters[index]->type);
            }
            if (chosen->parameters[index]->mode != ParameterMode::In) {
                checkAssignable(argument, scope);
            }
            expr->resolvedArguments[index] = argument;
        }
        // Whatever the caller left out stands in as the expression its
        // declaration gives.
        for (std::size_t p = 0; p < chosen->parameters.size(); ++p) {
            if (expr->resolvedArguments[p] == nullptr) {
                expr->resolvedArguments[p] = chosen->parameters[p]->defaultExpr;
            }
        }
        expr->type = chosen->returnType;
        return expr->type;
    }

    // Otherwise this is an indexed component.
    Type* prefixType = analyzeExpr(expr->callee.get(), scope, nullptr);
    Type* array = prefixType;
    // Indexing an access value reads the array it designates, without '.all'.
    if (array != nullptr && baseType(array)->kind == TypeKind::Access) {
        array = baseType(array)->target;
    }
    if (array == nullptr || array->kind != TypeKind::Array) {
        m_diagnostics.error(expr->location, "only arrays and subprograms can be applied to arguments");
        return nullptr;
    }
    if (expr->arguments.size() != static_cast<std::size_t>(array->arrayRank)) {
        m_diagnostics.error(expr->location, "array indexing requires " + std::to_string(array->arrayRank) + " index value(s)");
        return nullptr;
    }
    expr->form = CallForm::Indexing;
    int rank = array->arrayRank;
    for (int dimension = 0; dimension < rank; ++dimension) {
        Expr* index = expr->arguments[dimension].value.get();
        Type* indexType = analyzeExpr(index, scope, array->index);
        if (!typesCompatible(array->index, indexType)) {
            m_diagnostics.error(index->location, "index value has an incompatible type");
        }
        adaptUniversal(index, array->index != nullptr ? array->index : m_types.integerType());
        expr->resolvedArguments.push_back(index);
        array = array->element;
    }
    expr->type = array;
    (void)expected;
    return expr->type;
}
