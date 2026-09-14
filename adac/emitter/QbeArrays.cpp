#include "QbeEmitter.h"
#include "QbeSupport.h"

#include <algorithm>

using QbeSupport::constantValue;
using QbeSupport::isLiteralOperand;

void QbeEmitter::emitTypeBounds(Type* type, const SourceLocation& location)
{
    if (type == nullptr || type->m_boundExpressions.empty()) {
        return;
    }
    Symbol* symbol = type->m_boundsSymbol;
    m_context->frameSize = (m_context->frameSize + 7) & ~7LL;
    symbol->frameOffset = m_context->frameSize;
    m_context->frameSize += 8 + 8 * type->arrayRank;
    Type* axis = type;
    for (int dimension = 0; dimension < type->arrayRank; ++dimension) {
        auto [low, high] = type->m_boundExpressions[dimension];
        Value first = low ? emitExpr(low) : constantValue(axis->indexLow, 'w');
        Value last = high ? emitExpr(high) : constantValue(axis->indexHigh, 'w');
        emitRangeCheck(first, m_sema.typeTable().integerType(), location);
        emitRangeCheck(last, m_sema.typeTable().integerType(), location);
        std::string nonNull = newTemp();
        line(nonNull + " =w csgew " + last.name + ", " + first.name);
        std::string check = newLabel("typeboundscheck");
        std::string ready = newLabel("typeboundsready");
        branch(Value { nonNull, 'w' }, check, ready);
        label(check);
        emitRangeCheck(first, axis->index, location);
        emitRangeCheck(last, axis->index, location);
        jump(ready);
        label(ready);
        // Bound descriptors and array loops use signed 32-bit lengths. Validate
        // each dimension without allocating storage for the declared type.
        line(newTemp() + " =l call $__ada_array_size(w " + first.name + ", w " + last.name + ", l 1)");
        emitExceptionCheck();
        std::string firstSlot = newTemp();
        std::string lastSlot = newTemp();
        line(firstSlot + " =l add " + m_context->frameTemp + ", "
             + std::to_string(symbol->frameOffset + 8 + dimension * 8));
        line(lastSlot + " =l add " + firstSlot + ", 4");
        line("storew " + first.name + ", " + firstSlot);
        line("storew " + last.name + ", " + lastSlot);
        axis = axis->element;
    }
}

void QbeEmitter::emitDynamicArray(ObjectDecl* object, Symbol* symbol)
{
    Type* type = symbol->type;
    Value bounds;
    bool explicitBounds = !object->subtype->indexLows.empty() || type->m_boundsSymbol != nullptr;
    if (type->m_boundsSymbol != nullptr) {
        bounds = boundsFor(type->m_boundsSymbol);
    } else if (explicitBounds) {
        Type* axis = type;
        for (int dimension = 0; dimension < type->arrayRank; ++dimension) {
            Value first = emitExpr(object->subtype->indexLows[dimension].get());
            Value last = emitExpr(object->subtype->indexHighs[dimension].get());
            emitRangeCheck(first, m_sema.typeTable().integerType(), object->location);
            emitRangeCheck(last, m_sema.typeTable().integerType(), object->location);
            if (dimension == 0) {
                bounds.first = first.name;
                bounds.last = last.name;
            } else {
                bounds.innerBounds.push_back({ first.name, last.name });
            }
            std::string nonNull = newTemp();
            line(nonNull + " =w csgew " + last.name + ", " + first.name);
            std::string check = newLabel("checkbounds");
            std::string ready = newLabel("boundsready");
            branch(Value { nonNull, 'w' }, check, ready);
            label(check);
            emitRangeCheck(first, axis->index, object->location);
            emitRangeCheck(last, axis->index, object->location);
            jump(ready);
            label(ready);
            axis = axis->element;
        }
    }
    Value source;
    bool aggregate = explicitBounds && object->initializer
        && object->initializer->kind == ExprKind::Aggregate;
    if (object->initializer && !aggregate) {
        source = emitExpr(object->initializer.get());
        if (!explicitBounds) {
            bounds.first = source.first;
            bounds.last = source.last;
            bounds.innerBounds = source.innerBounds;
        }
    }
    if (!bounds.hasBounds()) {
        m_diagnostics.error(object->location, "array object bounds cannot be inferred from this initializer");
        return;
    }
    m_context->arrayArenaUsed = true;
    if (m_context->arrayArena.empty()) {
        m_context->arrayArena = allocScratch(8);
        m_context->prologue << "    storel 0, " << m_context->arrayArena << "\n";
    }
    std::string elementSize = arrayElementSize(bounds, type);
    std::string pointer = newTemp();
    line(pointer + " =l call $__ada_array_local(l " + m_context->arrayArena + ", w " + bounds.first
         + ", w " + bounds.last + ", l " + elementSize + ")");
    emitExceptionCheck();
    Value address = bounds;
    address.name = pointer;
    address.type = 'l';
    if (symbol->isUplevel) {
        m_context->frameSize = (m_context->frameSize + 7) & ~7LL;
        symbol->frameOffset = m_context->frameSize;
        m_context->frameSize += 8 + 8 * type->arrayRank;
        std::string slot = newTemp();
        line(slot + " =l add " + m_context->frameTemp + ", " + std::to_string(symbol->frameOffset));
        line("storel " + pointer + ", " + slot);
        std::string firstSlot = newTemp();
        std::string lastSlot = newTemp();
        line(firstSlot + " =l add " + slot + ", 8");
        line(lastSlot + " =l add " + slot + ", 12");
        line("storew " + bounds.first + ", " + firstSlot);
        line("storew " + bounds.last + ", " + lastSlot);
        for (std::size_t dimension = 0; dimension < bounds.innerBounds.size(); ++dimension) {
            std::string firstAddress = newTemp();
            std::string lastAddress = newTemp();
            line(firstAddress + " =l add " + slot + ", " + std::to_string(16 + dimension * 8));
            line(lastAddress + " =l add " + firstAddress + ", 4");
            line("storew " + bounds.innerBounds[dimension].first + ", " + firstAddress);
            line("storew " + bounds.innerBounds[dimension].second + ", " + lastAddress);
        }
    } else {
        m_context->locals[symbol] = pointer;
        m_context->bounds[symbol] = bounds;
    }
    if (aggregate) {
        assignInto(address, type, object->initializer.get());
    } else if (object->initializer) {
        checkArrayShape(address, type, source, object->initializer->type);
        std::string size = newTemp();
        line(size + " =l call $__ada_array_size(w " + address.first + ", w " + address.last + ", l " + elementSize + ")");
        emitExceptionCheck();
        line("call $memmove(l " + pointer + ", l " + source.name + ", l " + size + ")");
    } else if (hasComponentDefaults(type->element)) {
        emitArrayFill(address, type, nullptr);
    }
}

void QbeEmitter::emitArrayFill(const Value& address, Type* type, Expr* value)
{
    std::string elementSize = arrayElementSize(address, type);
    Value length = lengthOf(address, type);
    std::string wideLength = newTemp();
    line(wideLength + " =l extuw " + length.name);
    std::string slot = allocScratch(8);
    line("storel 0, " + slot);
    std::string head = newLabel("arrayfill");
    std::string body = newLabel("arrayfillbody");
    std::string done = newLabel("arrayfilldone");
    label(head);
    std::string index = newTemp();
    std::string test = newTemp();
    line(index + " =l loadl " + slot);
    line(test + " =w csltl " + index + ", " + wideLength);
    branch(Value { test, 'w' }, body, done);
    label(body);
    std::string offset = newTemp();
    std::string element = newTemp();
    line(offset + " =l mul " + index + ", " + elementSize);
    line(element + " =l add " + address.name + ", " + offset);
    Value cell = arrayRow(address, type);
    cell.name = element;
    auto elementStorage = storageCheckpoint();
    if (value != nullptr) {
        assignInto(cell, type->element, value);
    } else {
        emitDefaultInit(cell, type->element);
    }
    rewindStorage(elementStorage);
    std::string next = newTemp();
    line(next + " =l add " + index + ", 1");
    line("storel " + next + ", " + slot);
    jump(head);
    label(done);
}

Value QbeEmitter::boundsFor(Symbol* symbol)
{
    if (symbol->owner == m_context->symbol && symbol->frameOffset < 0) {
        auto it = m_context->bounds.find(symbol);
        if (it != m_context->bounds.end()) {
            return it->second;
        }
    }
    Value result;
    if (symbol->frameOffset < 0) {
        return result;
    }
    Value frame = symbol->owner == m_context->symbol ? Value { m_context->frameTemp, 'l' }
                                                     : staticLinkFor(symbol->owner->level);
    for (int dimension = 0; dimension < symbol->type->arrayRank; ++dimension) {
        std::string firstAddress = newTemp();
        std::string lastAddress = newTemp();
        std::string first = newTemp();
        std::string last = newTemp();
        line(firstAddress + " =l add " + frame.name + ", " + std::to_string(symbol->frameOffset + 8 + dimension * 8));
        line(lastAddress + " =l add " + firstAddress + ", 4");
        line(first + " =w loadsw " + firstAddress);
        line(last + " =w loadsw " + lastAddress);
        if (dimension == 0) {
            result.first = first;
            result.last = last;
        } else {
            result.innerBounds.push_back({ first, last });
        }
    }
    return result;
}

Value QbeEmitter::withBounds(const Value& address, Type* type, Symbol* symbol)
{
    Value result = address;
    if (type == nullptr || type->kind != TypeKind::Array) {
        return result;
    }
    if (type->constrained) {
        result.first = std::to_string(type->indexLow);
        result.last = std::to_string(type->indexHigh);
        result.innerBounds.clear();
        Type* row = type;
        for (int dimension = 1; dimension < type->arrayRank; ++dimension) {
            row = row->element;
            result.innerBounds.push_back({ std::to_string(row->indexLow), std::to_string(row->indexHigh) });
        }
    } else if (symbol != nullptr || (!result.hasBounds() && type->m_boundsSymbol != nullptr)) {
        Value bounds = boundsFor(symbol != nullptr && symbol->kind != SymbolKind::TypeName
                                    ? symbol : type->m_boundsSymbol);
        result.first = bounds.first;
        result.last = bounds.last;
        result.innerBounds = bounds.innerBounds;
    }
    return result;
}

Value QbeEmitter::arrayRow(const Value& array, Type* type)
{
    Value row { array.name, 'l' };
    if (type->arrayRank > 1 && !array.innerBounds.empty()) {
        row.first = array.innerBounds.front().first;
        row.last = array.innerBounds.front().second;
        row.innerBounds.assign(array.innerBounds.begin() + 1, array.innerBounds.end());
    }
    return withBounds(row, type->element, nullptr);
}

std::string QbeEmitter::arrayElementSize(const Value& array, Type* type)
{
    if (type->arrayRank == 1 || type->constrained) {
        return std::to_string(typeSize(type->element));
    }
    Value row = arrayRow(array, type);
    std::string element = arrayElementSize(row, type->element);
    std::string size = newTemp();
    line(size + " =l call $__ada_array_size(w " + row.first + ", w " + row.last + ", l " + element + ")");
    emitExceptionCheck();
    return size;
}

void QbeEmitter::checkArrayShape(const Value& target, Type* targetType, const Value& source, Type* sourceType)
{
    Value left = withBounds(target, targetType, nullptr);
    Value right = withBounds(source, sourceType, nullptr);
    int rank = targetType->arrayRank;
    for (int dimension = 0; dimension < rank; ++dimension) {
        Value leftLength = lengthOf(left, targetType);
        Value rightLength = lengthOf(right, sourceType);
        std::string same = newTemp();
        std::string ok = newLabel("shapeok");
        std::string bad = newLabel("shapebad");
        line(same + " =w ceqw " + leftLength.name + ", " + rightLength.name);
        branch(Value { same, 'w' }, ok, bad);
        label(bad);
        raiseConstraintError();
        label(ok);
        if (dimension + 1 < rank) {
            left = arrayRow(left, targetType);
            right = arrayRow(right, sourceType);
            targetType = targetType->element;
            sourceType = sourceType->element;
        }
    }
}

Value QbeEmitter::lengthOf(const Value& array, Type* type)
{
    if (type != nullptr && type->kind == TypeKind::Array && type->constrained) {
        return constantValue(arrayLength(type), 'w');
    }
    if (!array.hasBounds()) {
        return constantValue(0, 'w');
    }
    if (isLiteralOperand(array.first) && isLiteralOperand(array.last)) {
        return constantValue(std::max(0LL, std::stoll(array.last) - std::stoll(array.first) + 1), 'w');
    }
    std::string span = newTemp();
    line(span + " =w sub " + array.last + ", " + array.first);
    std::string length = newTemp();
    line(length + " =w add " + span + ", 1");
    std::string nonNull = newTemp();
    std::string normalized = newTemp();
    line(nonNull + " =w csgew " + array.last + ", " + array.first);
    line(normalized + " =w mul " + length + ", " + nonNull);
    return Value { normalized, 'w' };
}

Value QbeEmitter::emitSlice(CallExpr* expr)
{
    Value base = emitExpr(expr->callee.get());
    Type* array = expr->callee->type;
    Value low = emitExpr(expr->resolvedArguments[0]);
    Value high = emitExpr(expr->resolvedArguments[1]);

    std::string lowBound = array != nullptr && array->constrained
                               ? std::to_string(array->indexLow)
                               : (base.hasBounds() ? base.first : std::string("1"));
    long long elementSize = array != nullptr ? typeSize(array->element) : 1;

    std::string offset = newTemp();
    line(offset + " =w sub " + low.name + ", " + lowBound);
    std::string wide = newTemp();
    line(wide + " =l extsw " + offset);
    std::string scaled = newTemp();
    line(scaled + " =l mul " + wide + ", " + std::to_string(elementSize));
    std::string address = newTemp();
    line(address + " =l add " + base.name + ", " + scaled);

    return Value { address, 'l', low.name, high.name };
}

Value QbeEmitter::emitConcatenation(BinaryExpr* expr)
{
    std::vector<Expr*> parts;
    std::vector<Expr*> pending = { expr };
    while (!pending.empty()) {
        Expr* current = pending.back();
        pending.pop_back();
        if (current->kind == ExprKind::Binary
            && static_cast<BinaryExpr*>(current)->op == BinaryOp::Concatenate) {
            auto* binary = static_cast<BinaryExpr*>(current);
            pending.push_back(binary->right.get());
            pending.push_back(binary->left.get());
            continue;
        }
        parts.push_back(current);
    }

    struct Operand
    {
        Value value;
        Value length;
        bool isCharacter = false;
    };

    std::vector<Operand> operands;
    bool allStatic = true;
    long long staticTotal = 0;

    for (Expr* part : parts) {
        Operand operand;
        operand.value = emitExpr(part);
        operand.isCharacter = part->type == nullptr || part->type->kind != TypeKind::Array;
        if (operand.isCharacter) {
            operand.length = constantValue(1, 'w');
        } else {
            if (typeSize(part->type->element) != 1) {
                m_diagnostics.error(part->location, "'&' is only supported on arrays of characters");
                return Value { "0", 'l' };
            }
            operand.length = lengthOf(operand.value, part->type);
        }
        if (isLiteralOperand(operand.length.name)) {
            staticTotal += std::stoll(operand.length.name);
        } else {
            allStatic = false;
        }
        operands.push_back(operand);
    }

    std::string total = std::to_string(staticTotal);
    for (const Operand& operand : operands) {
        if (isLiteralOperand(operand.length.name)) {
            continue;
        }
        total = emitIntegerOperation(0, Value { total, 'w' }, operand.length, 'w').name;
    }

    std::string buffer;
    if (allStatic) {
        buffer = allocScratch(staticTotal > 0 ? staticTotal : 1);
    } else {
        buffer = newTemp();
        line(buffer + " =l call $__ada_array_local(l " + storageArena(true, true) + ", w 1, w " + total + ", l 1)");
        emitExceptionCheck();
    }

    std::string running = buffer;
    long long staticOffset = 0;
    bool dynamic = false;

    for (const Operand& operand : operands) {
        std::string target = running;
        if (!dynamic) {
            target = buffer;
            if (staticOffset > 0) {
                target = newTemp();
                line(target + " =l add " + buffer + ", " + std::to_string(staticOffset));
            }
        }

        if (operand.isCharacter) {
            line("storeb " + operand.value.name + ", " + target);
        } else if (isLiteralOperand(operand.length.name)) {
            line("blit " + operand.value.name + ", " + target + ", " + operand.length.name);
        } else {
            std::string bytes = newTemp();
            line(bytes + " =l extsw " + operand.length.name);
            line(newTemp() + " =l call $memcpy(l " + target + ", l " + operand.value.name + ", l " + bytes + ")");
        }

        if (!dynamic && isLiteralOperand(operand.length.name)) {
            staticOffset += std::stoll(operand.length.name);
            continue;
        }
        dynamic = true;
        std::string advance = newTemp();
        line(advance + " =l extsw " + operand.length.name);
        std::string next = newTemp();
        line(next + " =l add " + target + ", " + advance);
        running = next;
    }

    return Value { buffer, 'l', "1", total };
}
