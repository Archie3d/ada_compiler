#include "Sema.h"
#include "SemaSupport.h"

#include <utility>

using SemaSupport::isUniversal;
using SemaSupport::adaptUniversal;

void Sema::analyzeStatements(StmtList& statements, Scope* scope)
{
    for (const StmtPtr& statement : statements) {
        analyzeStatement(statement.get(), scope);
    }
}

void Sema::analyzeHandlers(std::vector<ExceptionHandler>& handlers, Scope* scope)
{
    for (ExceptionHandler& handler : handlers) {
        for (std::size_t i = 0; i < handler.namesLower.size(); ++i) {
            Symbol* symbol = lookupName(handler.namesLower[i], scope);
            if (symbol == nullptr || symbol->kind != SymbolKind::Exception) {
                m_diagnostics.error(handler.location, "unknown exception '" + handler.names[i] + "'");
                continue;
            }
            handler.identifiers.push_back(symbol->exceptionId);
        }
        ++m_handlerDepth;
        analyzeStatements(handler.body, scope);
        --m_handlerDepth;
    }
}

void Sema::checkAssignable(Expr* target, Scope* scope)
{
    (void)scope;

    // A limited private type is not copied outside the package that declared
    // it; whatever it takes to make one is that package's to offer.
    Type* type = baseType(target->type);
    if (type != nullptr && type->isLimited && type->privateTo != nullptr && !withinPackage(type->privateTo)) {
        m_diagnostics.error(target->location, "'" + type->name + "' is limited private, so a value of it cannot "
                                                  + "be assigned outside '" + type->privateTo->displayName + "'");
        return;
    }

    // A discriminant is fixed when the object is declared and stays that way,
    // since the components the value has were settled by it.
    if (target->kind == ExprKind::Selected) {
        auto* selected = static_cast<SelectedExpr*>(target);
        Type* record = baseType(selected->prefix->type);
        if (record != nullptr && record->kind == TypeKind::Access) {
            record = baseType(record->target);
        }
        if (selected->fieldIndex >= 0 && record != nullptr
            && record->fields[static_cast<std::size_t>(selected->fieldIndex)].isDiscriminant) {
            m_diagnostics.error(target->location, "'" + selected->selector
                                                      + "' is a discriminant, fixed when the object was declared");
            return;
        }
    }

    switch (target->kind) {
    case ExprKind::Identifier: {
        Symbol* symbol = static_cast<IdentifierExpr*>(target)->symbol;
        if (symbol == nullptr) {
            return;
        }
        if (symbol->kind == SymbolKind::Number || symbol->kind == SymbolKind::LoopParameter
            || (symbol->kind == SymbolKind::Object && symbol->isConstant)
            || (symbol->kind == SymbolKind::Parameter && symbol->mode == ParameterMode::In)) {
            m_diagnostics.error(target->location, "'" + symbol->displayName + "' cannot be assigned to");
        }
        return;
    }
    case ExprKind::Selected:
    case ExprKind::Call:
        return;
    default:
        m_diagnostics.error(target->location, "the target of an assignment must be a variable");
        return;
    }
}

void Sema::analyzeStatement(Stmt* statement, Scope* scope)
{
    switch (statement->kind) {
    case StmtKind::Null:
        break;

    case StmtKind::Assign: {
        auto* assign = static_cast<AssignStmt*>(statement);
        Type* targetType = analyzeExpr(assign->target.get(), scope, nullptr);
        checkAssignable(assign->target.get(), scope);
        Type* valueType = analyzeExpr(assign->value.get(), scope, targetType);
        if (!typesCompatible(targetType, valueType)) {
            m_diagnostics.error(assign->location, "the assigned value has an incompatible type");
        }
        adaptUniversal(assign->value.get(), targetType);
        break;
    }

    case StmtKind::ProcedureCall: {
        auto* call = static_cast<ProcedureCallStmt*>(statement);
        int errorsBefore = m_diagnostics.errorCount();
        Expr* expression = call->call.get();
        Type* result = analyzeExpr(expression, scope, m_types.voidType());
        if (m_diagnostics.errorCount() != errorsBefore) {
            break;
        }
        Symbol* target = nullptr;
        if (expression->kind == ExprKind::Identifier) {
            target = static_cast<IdentifierExpr*>(expression)->symbol;
        } else if (expression->kind == ExprKind::Selected) {
            target = static_cast<SelectedExpr*>(expression)->symbol;
        } else if (expression->kind == ExprKind::Call) {
            target = static_cast<CallExpr*>(expression)->subprogram;
        }
        bool streamProcedure = expression->kind == ExprKind::Attribute && result == m_types.voidType();
        if (!streamProcedure && (target == nullptr || target->kind != SymbolKind::Subprogram
                                 || target->returnType != nullptr)) {
            m_diagnostics.error(expression->location, "a procedure call statement requires a procedure");
        }
        break;
    }

    case StmtKind::If: {
        auto* ifStatement = static_cast<IfStmt*>(statement);
        for (IfBranch& branch : ifStatement->branches) {
            Type* conditionType = analyzeExpr(branch.condition.get(), scope, m_types.booleanType());
            if (!m_types.isBoolean(conditionType)) {
                m_diagnostics.error(branch.condition->location, "condition must be of type Boolean");
            }
            analyzeStatements(branch.body, scope);
        }
        analyzeStatements(ifStatement->elseBody, scope);
        break;
    }

    case StmtKind::Loop: {
        auto* loop = static_cast<LoopStmt*>(statement);
        Scope* inner = m_symbolTable.createScope(scope);

        if (loop->loopKind == LoopKind::While) {
            Type* conditionType = analyzeExpr(loop->condition.get(), scope, m_types.booleanType());
            if (!m_types.isBoolean(conditionType)) {
                m_diagnostics.error(loop->condition->location, "condition must be of type Boolean");
            }
        } else if (loop->loopKind == LoopKind::For) {
            Type* variableType = nullptr;
            if (!loop->rangeTypeLower.empty()) {
                variableType = resolveTypeName(loop->rangeTypeLower, scope, loop->location);
            }
            if (loop->rangeLow && loop->rangeHigh) {
                Type* lowType = analyzeExpr(loop->rangeLow.get(), scope, variableType);
                Type* highType = analyzeExpr(loop->rangeHigh.get(), scope, variableType);
                if (variableType == nullptr) {
                    variableType = isUniversal(lowType) ? highType : lowType;
                    if (isUniversal(variableType)) {
                        variableType = m_types.integerType();
                    }
                }
                adaptUniversal(loop->rangeLow.get(), variableType);
                adaptUniversal(loop->rangeHigh.get(), variableType);
            } else if (variableType != nullptr) {
                loop->rangeLow = scalarBoundExpr(variableType, true, loop->location);
                loop->rangeHigh = scalarBoundExpr(variableType, false, loop->location);
            }
            if (variableType == nullptr) {
                variableType = m_types.integerType();
            }

            Symbol* variable = m_symbolTable.createSymbol(SymbolKind::LoopParameter, loop->variableLower,
                                                          loop->variableName);
            variable->type = variableType;
            variable->isConstant = true;
            variable->location = loop->location;
            variable->owner = m_currentSubprogram;
            variable->level = m_currentSubprogram != nullptr ? m_currentSubprogram->level : 0;
            variable->isGlobal = m_currentSubprogram == nullptr;
            inner->add(variable);
            loop->variableSymbol = variable;
        }

        m_loops.push_back(loop);
        analyzeStatements(loop->body, inner);
        m_loops.pop_back();
        break;
    }

    case StmtKind::Exit: {
        auto* exitStatement = static_cast<ExitStmt*>(statement);
        if (exitStatement->condition) {
            Type* conditionType = analyzeExpr(exitStatement->condition.get(), scope, m_types.booleanType());
            if (!m_types.isBoolean(conditionType)) {
                m_diagnostics.error(exitStatement->condition->location, "condition must be of type Boolean");
            }
        }
        if (m_loops.empty()) {
            m_diagnostics.error(exitStatement->location, "exit statement outside of a loop");
            break;
        }
        if (exitStatement->labelLower.empty()) {
            exitStatement->target = m_loops.back();
        } else {
            for (auto it = m_loops.rbegin(); it != m_loops.rend(); ++it) {
                if ((*it)->labelLower == exitStatement->labelLower) {
                    exitStatement->target = *it;
                    break;
                }
            }
            if (exitStatement->target == nullptr) {
                m_diagnostics.error(exitStatement->location, "unknown loop label '" + exitStatement->label + "'");
                exitStatement->target = m_loops.back();
            }
        }
        break;
    }

    case StmtKind::Return: {
        auto* returnStatement = static_cast<ReturnStmt*>(statement);
        Type* expected = m_currentSubprogram != nullptr ? m_currentSubprogram->returnType : nullptr;
        if (m_currentSubprogram == nullptr) {
            m_diagnostics.error(returnStatement->location, "a return statement must appear within a subprogram");
            break;
        }
        if (returnStatement->value) {
            Type* valueType = analyzeExpr(returnStatement->value.get(), scope, expected);
            if (expected == nullptr) {
                m_diagnostics.error(returnStatement->location, "a procedure cannot return a value");
            } else if (!typesCompatible(expected, valueType)) {
                m_diagnostics.error(returnStatement->location, "the returned value has an incompatible type");
            }
            adaptUniversal(returnStatement->value.get(), expected);
        } else if (expected != nullptr) {
            m_diagnostics.error(returnStatement->location, "a function must return a value");
        }
        break;
    }

    case StmtKind::Case:
        analyzeCaseStatement(static_cast<CaseStmt*>(statement), scope);
        break;

    case StmtKind::Block: {
        auto* block = static_cast<BlockStmt*>(statement);
        Scope* inner = m_symbolTable.createScope(scope);
        analyzeDeclarativePart(block->declarations, inner);
        analyzeStatements(block->body, inner);
        analyzeHandlers(block->handlers, inner);
        break;
    }

    case StmtKind::Raise: {
        auto* raise = static_cast<RaiseStmt*>(statement);
        if (raise->lower.empty()) {
            if (m_handlerDepth == 0) {
                m_diagnostics.error(raise->location, "a bare raise must appear within an exception handler, not an enclosed body");
            }
            break;
        }
        Symbol* symbol = lookupName(raise->lower, scope);
        if (symbol == nullptr || symbol->kind != SymbolKind::Exception) {
            m_diagnostics.error(raise->location, "unknown exception '" + raise->name + "'");
            break;
        }
        raise->exceptionSymbol = symbol;
        break;
    }
    }
}

// Ada asks a case to account for every value its selector can take, each of
// them exactly once.  That is what lets a case compile into a plain choice
// with no run time check behind it and nothing to fall through to.
void Sema::analyzeCaseStatement(CaseStmt* statement, Scope* scope)
{
    Type* selectorType = analyzeExpr(statement->selector.get(), scope, nullptr);
    if (isUniversal(selectorType)) {
        selectorType = m_types.integerType();
        adaptUniversal(statement->selector.get(), selectorType);
    }

    if (selectorType != nullptr && selectorType->m_scalarBoundsSymbol != nullptr) {
        selectorType = m_types.scalarBaseType(selectorType);
    }
    bool discrete = selectorType != nullptr && isDiscrete(selectorType);
    if (selectorType != nullptr && !discrete) {
        m_diagnostics.error(statement->selector->location,
                            "a case selector has to be discrete, and '" + selectorType->name + "' is not");
    }

    // What the choices have accounted for so far, which is both how a value
    // covered twice is noticed and how the gaps are found at the end.
    std::vector<CaseChoice> covered;
    bool sawOthers = false;

    for (CaseAlternative& alternative : statement->alternatives) {
        if (sawOthers) {
            m_diagnostics.error(alternative.location, "'others' has to be the last alternative of a case");
        }
        if (alternative.isOthers) {
            sawOthers = true;
            analyzeStatements(alternative.body, scope);
            continue;
        }

        for (std::size_t i = 0; i < alternative.choiceLows.size(); ++i) {
            CaseChoice choice;
            if (!resolveChoice(alternative.choiceLows[i].get(), alternative.choiceHighs[i].get(), selectorType,
                               covered, scope, choice)) {
                continue;
            }
            covered.push_back(choice);
            alternative.choices.push_back(choice);
        }

        analyzeStatements(alternative.body, scope);
    }

    if (!sawOthers && discrete) {
        reportUncovered(covered, selectorType, statement->location, "case");
    }
}
