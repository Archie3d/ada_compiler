#include "QbeEmitter.h"
#include "QbeSupport.h"

using QbeSupport::constantValue;
using QbeSupport::isFloatClass;
using QbeSupport::isUnconstrainedArray;

Value QbeEmitter::emitAttribute(AttributeExpr* expr)
{
    const std::string& name = expr->lower;
    Type* prefixType = expr->prefixType;

    if (name == "read" || name == "write" || name == "input" || name == "output") {
        return emitStreamAttribute(expr);
    }
    if (name == "pos") {
        return emitExpr(expr->arguments.front().get());
    }
    if (name == "val") {
        Value value = emitExpr(expr->arguments.front().get());
        emitRangeCheck(value, prefixType, expr->location);
        char type = qbeClass(prefixType);
        if (value.type != type) {
            std::string converted = newTemp();
            line(converted + " =" + std::string(1, type)
                 + (type == 'l' ? " extsw " : " copy ") + value.name);
            return Value { converted, type };
        }
        return value;
    }
    if (name == "succ" || name == "pred") {
        Value value = emitExpr(expr->arguments.front().get());
        Value result = emitIntegerOperation(name == "succ" ? 0 : 1, value, Value { "1", value.type }, value.type);
        if (prefixType->kind == TypeKind::Enumeration) {
            emitRangeCheck(result, baseType(prefixType), expr->location);
        }
        return result;
    }
    if (name == "image") {
        Value value = emitExpr(expr->arguments.front().get());
        std::string temp = newTemp();
        Type* base = baseType(prefixType);
        if (isFloatClass(value.type)) {
            std::string wide = widenToDouble(value);
            line(temp + " =l call $__ada_image_float(d " + wide + ", w " + std::to_string(defaultAft(prefixType))
                 + ", w 3)");
        } else if (base != nullptr && base->kind == TypeKind::Enumeration && !base->literals.empty()) {
            // Ada spells the image of an enumeration value in upper case.
            line(temp + " =l call $__ada_image_enum(w " + value.name + ", l " + enumTableFor(base) + ", w "
                 + std::to_string(base->literals.size()) + ")");
        } else if (base != nullptr && base->kind == TypeKind::Enumeration) {
            // Character, whose image is the literal in its quotes.
            line(temp + " =l call $__ada_image_character(w " + value.name + ")");
        } else {
            line(temp + " =l call $" + (value.type == 'l' ? "__ada_image_long_integer(l " : "__ada_image_integer(w ")
                 + value.name + ")");
        }
        std::string size = newTemp();
        line(size + " =l call $strlen(l " + temp + ")");
        std::string length = newTemp();
        line(length + " =w copy " + size);
        return Value { temp, 'l', "1", length };
    }
    if (name == "address") {
        Value address = isComposite(prefixType) ? emitExpr(expr->prefix.get()) : emitAddress(expr->prefix.get());
        return Value { address.name, 'l' };
    }
    if (name == "size") {
        return constantValue(typeSize(prefixType) * 8, 'w');
    }
    if (name == "value") {
        Value text = emitExpr(expr->arguments.front().get());
        Value length = lengthOf(text, expr->arguments.front()->type);
        Value bounds = scalarBounds(prefixType);
        Type* base = baseType(prefixType);
        std::string temp = newTemp();
        if (base != nullptr && base->kind == TypeKind::Enumeration && !base->literals.empty()) {
            line(temp + " =w call $__ada_value_enum(l " + text.name + ", w " + length.name + ", l "
                 + enumTableFor(base) + ", w " + std::to_string(base->literals.size()) + ")");
        } else if (base != nullptr && base->kind == TypeKind::Enumeration) {
            // Character, whose values are named by their spelling.
            line(temp + " =w call $__ada_value_character(l " + text.name + ", w " + length.name + ")");
        } else if (qbeClass(prefixType) == 'l') {
            line(temp + " =l call $__ada_value_long_integer(l " + text.name + ", w " + length.name + ", l "
                 + bounds.first + ", l " + bounds.last + ")");
        } else {
            line(temp + " =w call $__ada_value_integer(l " + text.name + ", w " + length.name + ", w "
                 + bounds.first + ", w " + bounds.last + ")");
        }
        emitExceptionCheck();
        if (prefixType->m_scalarBoundsSymbol != nullptr) {
            emitRangeCheck(Value { temp, qbeClass(prefixType) }, prefixType, expr->location);
        }
        return Value { temp, qbeClass(prefixType) };
    }
    if (name == "first" || name == "last" || name == "length") {
        Type* base = prefixType;
        if (base != nullptr && isDiscrete(base) && base->m_scalarBoundsSymbol != nullptr) {
            Value bounds = scalarBounds(base);
            return Value { name == "first" ? bounds.first : bounds.last, bounds.type };
        }
        if (isUnconstrainedArray(base)) {
            Value array = emitExpr(expr->prefix.get());
            Type* axis = expr->prefix->type;
            long long dimension = expr->arguments.empty() ? 1 : expr->arguments.front()->staticValue;
            for (long long i = 1; i < dimension; ++i) {
                array = arrayRow(array, axis);
                axis = axis->element;
            }
            if (name == "length") {
                return lengthOf(array, base);
            }
            return Value { name == "first" ? array.first : array.last, 'w' };
        }
        long long value = 0;
        if (base != nullptr && base->kind == TypeKind::Array) {
            value = name == "first" ? base->indexLow : (name == "last" ? base->indexHigh : arrayLength(base));
        } else if (base != nullptr) {
            value = name == "first" ? base->low : base->high;
        }
        return constantValue(value, 'w');
    }

    m_diagnostics.error(expr->location, "unsupported attribute '" + expr->name + "'");
    return Value { "0", 'w' };
}

// 'Read, 'Write, 'Output and 'Input move a value between an object and a
// stream as the bytes it occupies.  'Output and 'Input additionally carry the
// bounds of an array whose type does not fix them.
Value QbeEmitter::emitStreamAttribute(AttributeExpr* expr)
{
    const std::string& name = expr->lower;
    Type* prefixType = expr->prefixType;
    Value stream = emitExpr(expr->arguments.front().get());

    if (name == "input") {
        if (isUnconstrainedArray(prefixType)) {
            std::string firstSlot = allocScratch(4);
            std::string lastSlot = allocScratch(4);
            std::string buffer = newTemp();
            line(buffer + " =l call $__ada_stream_read_array(l " + stream.name + ", w "
                 + std::to_string(typeSize(prefixType->element)) + ", l " + firstSlot + ", l " + lastSlot + ")");
            emitExceptionCheck();

            std::string first = newTemp();
            line(first + " =w loadsw " + firstSlot);
            std::string last = newTemp();
            line(last + " =w loadsw " + lastSlot);
            return Value { buffer, 'l', first, last };
        }

        long long size = typeSize(prefixType);
        std::string slot = allocScratch(size < 1 ? 1 : size);
        line("call $__ada_stream_read(l " + stream.name + ", l " + slot + ", w " + std::to_string(size) + ")");
        emitExceptionCheck();

        Value address { slot, 'l' };
        if (isComposite(prefixType)) {
            return withBounds(address, prefixType, nullptr);
        }
        return loadFrom(address, prefixType);
    }

    Expr* item = expr->arguments[1].get();
    Type* type = item->type;
    bool reading = name == "read";

    Value address;
    if (isComposite(type)) {
        address = emitExpr(item);
    } else if (reading) {
        address = emitAddress(item);
    } else {
        // A value being written need not live anywhere of its own.
        long long size = typeSize(type);
        Value value = emitExpr(item);
        address = Value { allocScratch(size < 1 ? 1 : size), 'l' };
        storeInto(address, value, type);
    }

    // An array occupies as many bytes as it currently holds, which the type
    // alone does not say.
    std::string size = std::to_string(typeSize(type));
    Type* array = baseType(type);
    if (array != nullptr && array->kind == TypeKind::Array) {
        if (!address.hasBounds()) {
            address = withBounds(address, type, nullptr);
        }
        Value length = lengthOf(address, type);
        std::string bytes = newTemp();
        line(bytes + " =w mul " + length.name + ", " + std::to_string(typeSize(array->element)));
        size = bytes;

        if (name == "output" && isUnconstrainedArray(prefixType)) {
            line("call $__ada_stream_write_bounds(l " + stream.name + ", w " + address.first + ", w "
                 + address.last + ")");
            emitExceptionCheck();
        }
    }

    line(std::string("call ") + (reading ? "$__ada_stream_read" : "$__ada_stream_write") + "(l " + stream.name
         + ", l " + address.name + ", w " + size + ")");
    emitExceptionCheck();
    return Value { "0", 'w' };
}

// The run time formats every real value as a double.
std::string QbeEmitter::widenToDouble(const Value& value)
{
    if (value.type == 'd') {
        return value.name;
    }
    std::string wide = newTemp();
    line(wide + " =d exts " + value.name);
    return wide;
}

// Ada writes one digit before the point and the rest after it.
int QbeEmitter::defaultAft(const Type* type)
{
    int digits = type != nullptr && type->digits > 0 ? type->digits : 6;
    return digits - 1;
}
