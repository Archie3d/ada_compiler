#include "QbeEmitter.h"
#include "QbeSupport.h"

using QbeSupport::comparisonInstruction;

// Operands here are addresses, including when the element is a scalar.
Value QbeEmitter::compareObjects(const Value& left, const Value& right, Type* type)
{
    if (type->kind == TypeKind::Array) {
        return compareArrays(BinaryOp::Equal, left, type, right, type);
    }
    if (type->kind == TypeKind::Record) {
        return compareRecords(left, right, type);
    }
    Value leftValue = loadFrom(left, type);
    Value rightValue = loadFrom(right, type);
    std::string equal = newTemp();
    line(equal + " =w " + comparisonInstruction(BinaryOp::Equal, leftValue.type) + " "
         + leftValue.name + ", " + rightValue.name);
    return Value { equal, 'w' };
}

Value QbeEmitter::compareArrays(BinaryOp op, const Value& left, Type* leftType, const Value& right,
                                Type* rightType)
{
    // Normalize null ranges to length zero, even for runtime bounds such as
    // 10 .. 5. Widen before subtracting so the span does not overflow a word.
    auto length = [&](const Value& value, Type* type) {
        if (type->constrained) {
            return std::to_string(arrayLength(type));
        }
        std::string first = newTemp();
        std::string last = newTemp();
        std::string span = newTemp();
        std::string count = newTemp();
        std::string positive = newTemp();
        std::string widePositive = newTemp();
        std::string result = newTemp();
        line(first + " =l extsw " + value.first);
        line(last + " =l extsw " + value.last);
        line(span + " =l sub " + last + ", " + first);
        line(count + " =l add " + span + ", 1");
        line(positive + " =w csgtl " + count + ", 0");
        line(widePositive + " =l extuw " + positive);
        line(result + " =l mul " + count + ", " + widePositive);
        return result;
    };
    std::string leftLength = length(left, leftType);
    std::string rightLength = length(right, rightType);
    bool equality = op == BinaryOp::Equal || op == BinaryOp::NotEqual;
    std::string resultSlot = allocScratch(4);
    std::string indexSlot = allocScratch(8);
    std::string head = newLabel("comparearray");
    std::string body = newLabel("compareelement");
    std::string advance = newLabel("comparenext");
    std::string different = newLabel("comparedifferent");
    std::string exhausted = newLabel("compareend");
    std::string done = newLabel("comparedone");
    line("storel 0, " + indexSlot);

    if (equality && leftType->arrayRank > 1) {
        Value leftAxis = left;
        Value rightAxis = right;
        Type* leftAxisType = leftType;
        Type* rightAxisType = rightType;
        for (int dimension = 1; dimension < leftType->arrayRank; ++dimension) {
            leftAxis = arrayRow(leftAxis, leftAxisType);
            rightAxis = arrayRow(rightAxis, rightAxisType);
            leftAxisType = leftAxisType->element;
            rightAxisType = rightAxisType->element;
            std::string leftCount = length(leftAxis, leftAxisType);
            std::string rightCount = length(rightAxis, rightAxisType);
            std::string same = newTemp();
            std::string next = newLabel("compareshape");
            line(same + " =w ceql " + leftCount + ", " + rightCount);
            branch(Value { same, 'w' }, next, different);
            label(next);
        }
    }
    if (equality) {
        std::string sameLength = newTemp();
        line(sameLength + " =w ceql " + leftLength + ", " + rightLength);
        branch(Value { sameLength, 'w' }, head, different);
    }
    label(head);
    std::string index = newTemp();
    std::string withinLeft = newTemp();
    std::string withinRight = newTemp();
    std::string withinBoth = newTemp();
    line(index + " =l loadl " + indexSlot);
    line(withinLeft + " =w csltl " + index + ", " + leftLength);
    line(withinRight + " =w csltl " + index + ", " + rightLength);
    line(withinBoth + " =w and " + withinLeft + ", " + withinRight);
    branch(Value { withinBoth, 'w' }, body, exhausted);

    label(body);
    auto elementAddress = [&](const Value& array, Type* type) {
        std::string offset = newTemp();
        std::string address = newTemp();
        std::string elementSize = arrayElementSize(array, type);
        line(offset + " =l mul " + index + ", " + elementSize);
        line(address + " =l add " + array.name + ", " + offset);
        Value cell = arrayRow(array, type);
        cell.name = address;
        return cell;
    };
    Value leftElement = elementAddress(left, leftType);
    Value rightElement = elementAddress(right, rightType);
    Value equal = leftType->arrayRank > 1
        ? compareArrays(BinaryOp::Equal, leftElement, leftType->element, rightElement, rightType->element)
        : compareObjects(leftElement, rightElement, leftType->element);
    branch(equal, advance, different);

    label(advance);
    std::string nextIndex = newTemp();
    line(nextIndex + " =l add " + index + ", 1");
    line("storel " + nextIndex + ", " + indexSlot);
    jump(head);

    label(different);
    if (equality) {
        line("storew " + std::string(op == BinaryOp::Equal ? "0" : "1") + ", " + resultSlot);
    } else {
        // Only discrete-element arrays have predefined ordering. The first
        // unequal element decides the result using its numeric/ordinal value.
        Value leftValue = loadFrom(leftElement, leftType->element);
        Value rightValue = loadFrom(rightElement, rightType->element);
        std::string ordered = newTemp();
        line(ordered + " =w " + comparisonInstruction(op, leftValue.type) + " "
             + leftValue.name + ", " + rightValue.name);
        line("storew " + ordered + ", " + resultSlot);
    }
    jump(done);

    label(exhausted);
    std::string orderedLengths = newTemp();
    line(orderedLengths + " =w " + comparisonInstruction(op, 'l') + " " + leftLength + ", " + rightLength);
    line("storew " + orderedLengths + ", " + resultSlot);
    jump(done);

    label(done);
    std::string result = newTemp();
    line(result + " =w loadsw " + resultSlot);
    return Value { result, 'w' };
}

// Compare common fields first, including every discriminant, then dispatch to
// the active variant. Neither padding nor inactive storage contributes to equality.
Value QbeEmitter::compareRecords(const Value& left, const Value& right, Type* type)
{
    Type* record = baseType(type);
    std::string resultSlot = allocScratch(4);
    std::string different = newLabel("recorddifferent");
    std::string done = newLabel("recordcompared");
    line("storew 1, " + resultSlot);

    auto fieldAddress = [&](const Value& address, const FieldInfo& field) {
        if (field.offset == 0) {
            return address;
        }
        std::string moved = newTemp();
        line(moved + " =l add " + address.name + ", " + std::to_string(field.offset));
        return Value { moved, 'l' };
    };
    auto compareField = [&](const FieldInfo& field) {
        Value leftField = fieldAddress(left, field);
        Value rightField = fieldAddress(right, field);
        Value equal = compareObjects(leftField, rightField, field.type);
        std::string next = newLabel("recordnextfield");
        branch(equal, next, different);
        label(next);
    };
    for (const FieldInfo& field : record->fields) {
        if (field.variant < 0) {
            compareField(field);
        }
    }

    if (record->variantOn >= 0) {
        const FieldInfo& discriminant = record->fields[static_cast<std::size_t>(record->variantOn)];
        Value value = loadFrom(fieldAddress(left, discriminant), discriminant.type);
        std::vector<std::string> alternatives;
        std::string fallback = done;
        for (const VariantInfo& variant : record->variants) {
            alternatives.push_back(newLabel("comparevariant"));
            if (variant.isOthers) {
                fallback = alternatives.back();
            }
        }
        for (std::size_t i = 0; i < record->variants.size(); ++i) {
            const VariantInfo& variant = record->variants[i];
            if (variant.isOthers) {
                continue;
            }
            for (const VariantChoice& choice : variant.choices) {
                std::string matches = newTemp();
                std::string next = newLabel("comparenextvariant");
                line(matches + " =w " + rangeTest(value, choice.low, choice.high));
                branch(Value { matches, 'w' }, alternatives[i], next);
                label(next);
            }
        }
        jump(fallback);
        for (std::size_t i = 0; i < record->variants.size(); ++i) {
            label(alternatives[i]);
            for (const FieldInfo& field : record->fields) {
                if (field.variant == static_cast<int>(i)) {
                    compareField(field);
                }
            }
            jump(done);
        }
    } else {
        jump(done);
    }

    label(different);
    line("storew 0, " + resultSlot);
    jump(done);
    label(done);
    std::string result = newTemp();
    line(result + " =w loadsw " + resultSlot);
    return Value { result, 'w' };
}
