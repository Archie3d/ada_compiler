#include "QbeEmitter.h"
#include "QbeSupport.h"

using QbeSupport::isUnconstrainedArray;
using QbeSupport::isLiteralOperand;

std::string QbeEmitter::allocScratch(long long size)
{
    std::string temp = newTemp();
    const char* instruction = size <= 4 ? "alloc4" : "alloc8";
    m_context->prologue << "    " << temp << " =l " << instruction << " " << size << "\n";
    return temp;
}

std::string QbeEmitter::storageArena(bool temporary, bool allocate)
{
    if (allocate) {
        (temporary ? m_context->temporaryArenaUsed : m_context->arrayArenaUsed) = true;
    }
    std::string& arena = temporary ? m_context->temporaryArena : m_context->arrayArena;
    if (arena.empty()) {
        arena = allocScratch(8);
        m_context->prologue << "    storel 0, " << arena << "\n";
    }
    return arena;
}

std::pair<std::string, std::string> QbeEmitter::storageCheckpoint()
{
    std::string local = newTemp();
    std::string temporary = newTemp();
    line(local + " =l loadl " + storageArena(false));
    line(temporary + " =l loadl " + storageArena(true));
    return { local, temporary };
}

void QbeEmitter::rewindStorage(const std::pair<std::string, std::string>& checkpoint)
{
    line("call $__ada_array_rewind(l " + storageArena(true) + ", l " + checkpoint.second + ")");
    line("call $__ada_array_rewind(l " + storageArena(false) + ", l " + checkpoint.first + ")");
}

Value QbeEmitter::staticLinkFor(int targetLevel)
{
    Symbol* current = m_context->symbol;
    int currentLevel = current != nullptr ? current->level : 0;

    if (targetLevel == currentLevel) {
        if (m_context->hasFrame) {
            return Value { m_context->frameTemp, 'l' };
        }
        return Value { "0", 'l' };
    }

    if (currentLevel == 0) {
        return Value { "0", 'l' };
    }

    std::string pointer = "%.link";
    for (int level = currentLevel - 1; level > targetLevel; --level) {
        std::string next = newTemp();
        line(next + " =l loadl " + pointer);
        pointer = next;
    }
    return Value { pointer, 'l' };
}

Value QbeEmitter::addressOf(Symbol* symbol)
{
    if (symbol->isGlobal) {
        return withBounds(Value { symbol->qbeName, 'l' }, symbol->type, symbol);
    }
    Value address;
    if (symbol->frameOffset >= 0) {
        Value frame = symbol->owner == m_context->symbol
                          ? Value { m_context->frameTemp, 'l' }
                          : staticLinkFor(symbol->owner->level);
        address = Value { newTemp(), 'l' };
        line(address.name + " =l add " + frame.name + ", " + std::to_string(symbol->frameOffset));
        if ((symbol->kind == SymbolKind::Parameter && symbol->byReference)
            || (symbol->kind == SymbolKind::Object && isUnconstrainedArray(symbol->type))) {
            std::string pointer = newTemp();
            line(pointer + " =l loadl " + address.name);
            address.name = pointer;
        }
    } else {
        auto it = m_context->locals.find(symbol);
        if (it == m_context->locals.end()) {
            m_diagnostics.error(symbol->location, "internal error: '" + symbol->displayName + "' has no storage");
            return Value { "0", 'l' };
        }
        address = Value { it->second, 'l' };
    }
    return withBounds(address, symbol->type, symbol);
}

Value QbeEmitter::loadFrom(const Value& address, Type* type)
{
    char resultType = qbeClass(type);
    std::string temp = newTemp();
    line(temp + " =" + std::string(1, resultType) + " " + qbeLoadInstruction(type) + " " + address.name);
    return Value { temp, resultType };
}

void QbeEmitter::storeInto(const Value& address, const Value& value, Type* type)
{
    line(std::string(qbeStoreInstruction(type)) + " " + value.name + ", " + address.name);
}

void QbeEmitter::copyInto(const Value& destination, const Value& source, Type* type)
{
    long long size = typeSize(type);
    if (size <= 0) {
        return;
    }
    // Source and destination may refer to overlapping slices of the same array.
    line("call $memmove(l " + destination.name + ", l " + source.name + ", l " + std::to_string(size) + ")");
}

void QbeEmitter::assignInto(const Value& address, Type* type, Expr* value)
{
    if (value == nullptr) {
        return;
    }
    if (value->kind == ExprKind::Aggregate) {
        emitAggregateInto(static_cast<AggregateExpr*>(value), address, type);
        return;
    }
    if (type != nullptr && type->kind == TypeKind::Array) {
        if (type->arrayRank > 1) {
            Value target = withBounds(address, type, nullptr);
            Value source = emitExpr(value);
            checkArrayShape(target, type, source, value->type);
            std::string elementSize = arrayElementSize(target, type);
            std::string size = newTemp();
            line(size + " =l call $__ada_array_size(w " + target.first + ", w " + target.last + ", l " + elementSize + ")");
            emitExceptionCheck();
            line("call $memmove(l " + target.name + ", l " + source.name + ", l " + size + ")");
            return;
        }
        if (!type->constrained) {
            Value source = emitExpr(value);
            Value targetLength = lengthOf(address, type);
            Value sourceLength = lengthOf(source, value->type);
            std::string same = newTemp();
            line(same + " =w ceqw " + targetLength.name + ", " + sourceLength.name);
            std::string ok = newLabel("lengthok");
            std::string bad = newLabel("lengthbad");
            branch(Value { same, 'w' }, ok, bad);
            label(bad);
            raiseConstraintError();
            label(ok);
            std::string wide = newTemp();
            std::string size = newTemp();
            line(wide + " =l extuw " + targetLength.name);
            line(size + " =l mul " + wide + ", " + std::to_string(typeSize(type->element)));
            line("call $memmove(l " + address.name + ", l " + source.name + ", l " + size + ")");
            return;
        }
        Value source = emitExpr(value);
        long long target = arrayLength(type);
        Value sourceLength = lengthOf(source, value->type);
        if (isLiteralOperand(sourceLength.name)) {
            if (std::stoll(sourceLength.name) != target) {
                m_diagnostics.error(value->location, "the assigned value has a different length");
                return;
            }
        } else {
            // Ada requires the lengths to match, so check them at run time.
            std::string same = newTemp();
            line(same + " =w ceqw " + sourceLength.name + ", " + std::to_string(target));
            std::string ok = newLabel("lengthok");
            std::string bad = newLabel("lengthbad");
            branch(Value { same, 'w' }, ok, bad);
            label(bad);
            raiseConstraintError();
            label(ok);
        }
        copyInto(address, source, type);
        return;
    }
    if (isComposite(type)) {
        Value source = emitExpr(value);
        copyInto(address, source, type);
        return;
    }
    Value result = emitExpr(value);
    emitRangeCheck(result, type, value->location);
    storeInto(address, result, type);
}
