#include "QbeEmitter.h"
#include "QbeSupport.h"

using QbeSupport::isUnconstrainedArray;
using QbeSupport::comparisonInstruction;

void QbeEmitter::emitStatements(StmtList& statements)
{
    for (const StmtPtr& statement : statements) {
        auto checkpoint = storageCheckpoint();
        emitStatement(statement.get());
        if (!m_context->terminated) {
            rewindStorage(checkpoint);
        }
    }
}

void QbeEmitter::emitStatement(Stmt* statement)
{
    switch (statement->kind) {
    case StmtKind::Null:
        break;

    case StmtKind::Assign: {
        auto* assign = static_cast<AssignStmt*>(statement);
        Value address = emitAddress(assign->target.get());
        assignInto(address, assign->target->type, assign->value.get());
        break;
    }

    case StmtKind::ProcedureCall: {
        auto* call = static_cast<ProcedureCallStmt*>(statement);
        emitExpr(call->call.get());
        break;
    }

    case StmtKind::If: {
        auto* ifStatement = static_cast<IfStmt*>(statement);
        std::string end = newLabel("endif");
        for (IfBranch& branchStatement : ifStatement->branches) {
            Value condition = emitExpr(branchStatement.condition.get());
            std::string then = newLabel("then");
            std::string next = newLabel("elsif");
            branch(condition, then, next);
            label(then);
            emitStatements(branchStatement.body);
            jump(end);
            label(next);
        }
        if (ifStatement->hasElse) {
            emitStatements(ifStatement->elseBody);
        }
        jump(end);
        label(end);
        break;
    }

    case StmtKind::Loop: {
        auto* loop = static_cast<LoopStmt*>(statement);
        std::string head = newLabel("loop");
        std::string bodyLabel = newLabel("loopbody");
        std::string exit = newLabel("endloop");
        m_context->loopExits[loop] = exit;

        if (loop->loopKind == LoopKind::For) {
            Symbol* variable = loop->variableSymbol;
            long long size = typeSize(variable->type);
            if (size < 1) {
                size = 1;
            }
            if (variable->isUplevel) {
                variable->frameOffset = m_context->frameSize;
                m_context->frameSize += size;
            } else {
                std::string slot = "%v." + variable->name + "." + std::to_string(m_tempCounter++);
                m_context->prologue << "    " << slot << " =l " << (size <= 4 ? "alloc4" : "alloc8") << " " << size
                                    << "\n";
                m_context->locals[variable] = slot;
            }

            Value low = emitExpr(loop->rangeLow.get());
            Value high = emitExpr(loop->rangeHigh.get());
            char type = qbeClass(variable->type);
            if (variable->type->m_scalarBoundsSymbol != nullptr) {
                std::string nonNull = newTemp();
                std::string check = newLabel("looprangecheck");
                std::string ready = newLabel("looprangeready");
                line(nonNull + " =w csge" + type + " " + high.name + ", " + low.name);
                branch(Value { nonNull, 'w' }, check, ready);
                label(check);
                emitRangeCheck(low, variable->type, loop->location);
                emitRangeCheck(high, variable->type, loop->location);
                jump(ready);
                label(ready);
            }
            std::string boundSlot = allocScratch(type == 'l' ? 8 : 4);
            Value address = addressOf(variable);
            storeInto(address, loop->isReverse ? high : low, variable->type);
            line(std::string(type == 'l' ? "storel " : "storew ")
                 + (loop->isReverse ? low.name : high.name) + ", " + boundSlot);

            label(head);
            Value current = loadFrom(addressOf(variable), variable->type);
            std::string bound = newTemp();
            line(bound + " =" + std::string(1, type) + (type == 'l' ? " loadl " : " loadsw ") + boundSlot);
            std::string test = newTemp();
            line(test + " =w " + comparisonInstruction(loop->isReverse ? BinaryOp::GreaterEqual : BinaryOp::LessEqual, type)
                 + " " + current.name + ", " + bound);
            branch(Value { test, 'w' }, bodyLabel, exit);
            label(bodyLabel);
            emitStatements(loop->body);
            Value step = loadFrom(addressOf(variable), variable->type);
            std::string lastIteration = newTemp();
            line(lastIteration + " =w " + comparisonInstruction(BinaryOp::Equal, type) + " " + step.name + ", " + bound);
            std::string advance = newLabel("loopstep");
            branch(Value { lastIteration, 'w' }, exit, advance);
            label(advance);
            std::string updated = newTemp();
            line(updated + " =" + std::string(1, type) + " " + (loop->isReverse ? "sub " : "add ") + step.name
                 + ", 1");
            storeInto(addressOf(variable), Value { updated, type }, variable->type);
            jump(head);
            label(exit);
            break;
        }

        label(head);
        if (loop->loopKind == LoopKind::While) {
            auto conditionStorage = storageCheckpoint();
            Value condition = emitExpr(loop->condition.get());
            rewindStorage(conditionStorage);
            branch(condition, bodyLabel, exit);
            label(bodyLabel);
        }
        emitStatements(loop->body);
        jump(head);
        label(exit);
        break;
    }

    case StmtKind::Exit: {
        auto* exitStatement = static_cast<ExitStmt*>(statement);
        auto it = m_context->loopExits.find(exitStatement->target);
        if (it == m_context->loopExits.end()) {
            break;
        }
        if (exitStatement->condition) {
            Value condition = emitExpr(exitStatement->condition.get());
            std::string next = newLabel("noexit");
            branch(condition, it->second, next);
            label(next);
        } else {
            jump(it->second);
        }
        break;
    }

    case StmtKind::Return: {
        auto* returnStatement = static_cast<ReturnStmt*>(statement);
        Type* resultType = m_context->symbol == nullptr ? nullptr : m_context->symbol->returnType;
        if (returnStatement->value && isComposite(resultType)) {
            if (isUnconstrainedArray(resultType)) {
                Value value = emitExpr(returnStatement->value.get());
                value = withBounds(value, returnStatement->value->type, nullptr);
                if (resultType->m_boundsSymbol != nullptr) {
                    Value target = withBounds(Value {}, resultType, nullptr);
                    checkArrayShape(target, resultType, value, returnStatement->value->type);
                    value.first = target.first;
                    value.last = target.last;
                    value.innerBounds = target.innerBounds;
                }
                std::string elementSize = arrayElementSize(value, resultType);
                line("call $__ada_array_result(l %.result, l " + value.name + ", w " + value.first
                     + ", w " + value.last + ", l " + elementSize + ")");
                emitExceptionCheck();
                for (std::size_t dimension = 0; dimension < value.innerBounds.size(); ++dimension) {
                    std::string first = newTemp();
                    std::string last = newTemp();
                    line(first + " =l add %.result, " + std::to_string(24 + dimension * 8));
                    line(last + " =l add " + first + ", 4");
                    line("storew " + value.innerBounds[dimension].first + ", " + first);
                    line("storew " + value.innerBounds[dimension].second + ", " + last);
                }
            } else {
                assignInto(Value { "%.result", 'l' }, resultType, returnStatement->value.get());
            }
            line("ret");
        } else if (returnStatement->value) {
            Value value = emitExpr(returnStatement->value.get());
            if (m_context->symbol != nullptr) {
                emitRangeCheck(value, m_context->symbol->returnType, returnStatement->location);
            }
            line("ret " + value.name);
        } else {
            line("ret");
        }
        m_context->terminated = true;
        break;
    }

    case StmtKind::Case: {
        auto* caseStatement = static_cast<CaseStmt*>(statement);
        Value selector = emitExpr(caseStatement->selector.get());
        char type = selector.type;
        std::string end = newLabel("endcase");

        std::vector<std::string> bodyLabels;
        for (std::size_t i = 0; i < caseStatement->alternatives.size(); ++i) {
            bodyLabels.push_back(newLabel("casebody"));
        }

        std::string othersLabel;
        for (std::size_t i = 0; i < caseStatement->alternatives.size(); ++i) {
            CaseAlternative& alternative = caseStatement->alternatives[i];
            if (alternative.isOthers) {
                othersLabel = bodyLabels[i];
                continue;
            }
            std::string match;
            for (const CaseChoice& choice : alternative.choices) {
                std::string test = newTemp();
                if (choice.low == choice.high) {
                    line(test + " =w " + comparisonInstruction(BinaryOp::Equal, type) + " " + selector.name + ", "
                         + std::to_string(choice.low));
                } else {
                    std::string lowTest = newTemp();
                    std::string highTest = newTemp();
                    line(lowTest + " =w " + comparisonInstruction(BinaryOp::GreaterEqual, type) + " "
                         + selector.name + ", " + std::to_string(choice.low));
                    line(highTest + " =w " + comparisonInstruction(BinaryOp::LessEqual, type) + " " + selector.name
                         + ", " + std::to_string(choice.high));
                    line(test + " =w and " + lowTest + ", " + highTest);
                }
                if (match.empty()) {
                    match = test;
                } else {
                    std::string combined = newTemp();
                    line(combined + " =w or " + match + ", " + test);
                    match = combined;
                }
            }
            std::string next = newLabel("nextcase");
            branch(Value { match, 'w' }, bodyLabels[i], next);
            label(next);
        }

        jump(othersLabel.empty() ? end : othersLabel);

        for (std::size_t i = 0; i < caseStatement->alternatives.size(); ++i) {
            label(bodyLabels[i]);
            emitStatements(caseStatement->alternatives[i].body);
            jump(end);
        }
        label(end);
        break;
    }

    case StmtKind::Block: {
        auto* block = static_cast<BlockStmt*>(statement);
        emitLocalDeclarations(block->declarations);
        if (block->handlers.empty()) {
            emitStatements(block->body);
            break;
        }
        std::string dispatch = newLabel("handler");
        std::string after = newLabel("handled");
        m_context->handlerStorage[dispatch] = storageCheckpoint();
        m_context->handlerLabels.push_back(dispatch);
        emitStatements(block->body);
        m_context->handlerLabels.pop_back();
        jump(after);
        emitHandlers(block->handlers, after, dispatch);
        label(after);
        break;
    }

    case StmtKind::Raise: {
        auto* raise = static_cast<RaiseStmt*>(statement);
        emitRaise(raise->exceptionSymbol, raise->location);
        break;
    }
    }
}
