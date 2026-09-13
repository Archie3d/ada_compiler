#include "QbeEmitter.h"
#include "QbeSupport.h"

#include <cctype>

using QbeSupport::realLiteral;
using QbeSupport::comparisonInstruction;

namespace
{

std::string upperCase(const std::string& text)
{
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return result;
}

}

void QbeEmitter::emitRaise(Symbol* exception, const SourceLocation& location)
{
    if (exception == nullptr) {
        if (m_context->activeExceptions.empty()) {
            m_diagnostics.error(location, "internal error: bare raise without an active handler");
            return;
        }
        const auto& occurrence = m_context->activeExceptions.back();
        line("storew " + occurrence.first + ", $__ada_exception");
        line("storel " + occurrence.second + ", $__ada_exception_name");
    } else {
        line("storew " + std::to_string(exception->exceptionId) + ", $__ada_exception");
        line("storel " + stringData(upperCase(exception->displayName)) + ", $__ada_exception_name");
    }
    if (!m_context->handlerLabels.empty()) {
        jump(m_context->handlerLabels.back());
    } else {
        m_context->usesPropagate = true;
        jump(m_context->propagateLabel);
    }
}

void QbeEmitter::emitExceptionCheck()
{
    std::string status = newTemp();
    std::string next = newLabel("nothrow");
    line(status + " =w loadsw $__ada_exception");
    if (!m_context->handlerLabels.empty()) {
        branch(Value { status, 'w' }, m_context->handlerLabels.back(), next);
    } else {
        m_context->usesPropagate = true;
        branch(Value { status, 'w' }, m_context->propagateLabel, next);
    }
    label(next);
}

void QbeEmitter::emitHandlers(std::vector<ExceptionHandler>& handlers, const std::string& afterLabel,
                              const std::string& dispatchLabel)
{
    label(dispatchLabel);
    rewindStorage(m_context->handlerStorage.at(dispatchLabel));
    std::string status = newTemp();
    line(status + " =w loadsw $__ada_exception");

    // Save the occurrence before clearing the pending status. Nested handlers
    // and calls may replace both globals while this handler remains active.
    std::string name = newTemp();
    line(name + " =l loadl $__ada_exception_name");
    std::vector<std::string> bodyLabels;
    for (std::size_t i = 0; i < handlers.size(); ++i) {
        bodyLabels.push_back(newLabel("handle"));
    }

    for (std::size_t i = 0; i < handlers.size(); ++i) {
        ExceptionHandler& handler = handlers[i];
        if (handler.isOthers) {
            jump(bodyLabels[i]);
            break;
        }
        std::string match;
        for (std::size_t k = 0; k < handler.identifiers.size(); ++k) {
            std::string test = newTemp();
            line(test + " =w ceqw " + status + ", " + std::to_string(handler.identifiers[k]));
            if (match.empty()) {
                match = test;
            } else {
                std::string combined = newTemp();
                line(combined + " =w or " + match + ", " + test);
                match = combined;
            }
        }
        std::string next = newLabel("nexthandler");
        if (match.empty()) {
            jump(next);
        } else {
            branch(Value { match, 'w' }, bodyLabels[i], next);
        }
        label(next);
    }

    if (!m_context->terminated) {
        if (m_context->handlerLabels.empty()) {
            m_context->usesPropagate = true;
            jump(m_context->propagateLabel);
        } else {
            jump(m_context->handlerLabels.back());
        }
    }

    for (std::size_t i = 0; i < handlers.size(); ++i) {
        label(bodyLabels[i]);
        line("storew 0, $__ada_exception");
        m_context->activeExceptions.emplace_back(status, name);
        emitStatements(handlers[i].body);
        m_context->activeExceptions.pop_back();
        jump(afterLabel);
    }
}

void QbeEmitter::emitRangeCheck(const Value& value, Type* type, const SourceLocation& location)
{
    if (type != nullptr && type->kind == TypeKind::Float) {
        if (!type->hasRealRange) {
            return;
        }
        char type_ = value.type;
        std::string lowTest = newTemp();
        std::string highTest = newTemp();
        std::string combined = newTemp();
        line(lowTest + " =w " + comparisonInstruction(BinaryOp::GreaterEqual, type_) + " " + value.name + ", "
             + realLiteral(type->lowReal, type_));
        line(highTest + " =w " + comparisonInstruction(BinaryOp::LessEqual, type_) + " " + value.name + ", "
             + realLiteral(type->highReal, type_));
        line(combined + " =w and " + lowTest + ", " + highTest);

        std::string ok = newLabel("inrange");
        std::string bad = newLabel("outofrange");
        branch(Value { combined, 'w' }, ok, bad);
        label(bad);
        raiseConstraintError();
        label(ok);
        return;
    }
    if (type == nullptr || !isDiscrete(type)) {
        return;
    }
    char type_ = value.type;
    std::string lowTest = newTemp();
    std::string highTest = newTemp();
    std::string combined = newTemp();
    line(lowTest + " =w " + comparisonInstruction(BinaryOp::GreaterEqual, type_) + " " + value.name + ", "
         + std::to_string(type->low));
    line(highTest + " =w " + comparisonInstruction(BinaryOp::LessEqual, type_) + " " + value.name + ", "
         + std::to_string(type->high));
    line(combined + " =w and " + lowTest + ", " + highTest);

    std::string ok = newLabel("inrange");
    std::string bad = newLabel("outofrange");
    branch(Value { combined, 'w' }, ok, bad);
    label(bad);
    (void)location;
    raiseConstraintError();
    label(ok);
}

// A component of a variant part is only there when the discriminant selects
// that alternative.  Where nothing fixed the discriminant, the value carries
// the answer and reaching for the wrong component raises Constraint_Error.
void QbeEmitter::checkVariant(const Value& address, Type* record, int variant)
{
    const FieldInfo& discriminant = record->fields[static_cast<std::size_t>(record->variantOn)];
    Value slot = address;
    if (discriminant.offset != 0) {
        std::string moved = newTemp();
        line(moved + " =l add " + address.name + ", " + std::to_string(discriminant.offset));
        slot = Value { moved, 'l' };
    }
    Value value = loadFrom(slot, discriminant.type);

    const VariantInfo& alternative = record->variants[static_cast<std::size_t>(variant)];
    std::string ok = newLabel("rightvariant");
    std::string bad = newLabel("wrongvariant");

    if (alternative.isOthers) {
        // 'when others' holds whatever no other alternative named, so the test
        // is that none of them does.
        std::string matched = newTemp();
        line(matched + " =w copy 0");
        for (const VariantInfo& other : record->variants) {
            if (other.isOthers) {
                continue;
            }
            for (const VariantChoice& choice : other.choices) {
                std::string within = newTemp();
                line(within + " =w " + rangeTest(value, choice.low, choice.high));
                std::string combined = newTemp();
                line(combined + " =w or " + matched + ", " + within);
                matched = combined;
            }
        }
        branch(Value { matched, 'w' }, bad, ok);
    } else {
        std::string matched = newTemp();
        line(matched + " =w copy 0");
        for (const VariantChoice& choice : alternative.choices) {
            std::string within = newTemp();
            line(within + " =w " + rangeTest(value, choice.low, choice.high));
            std::string combined = newTemp();
            line(combined + " =w or " + matched + ", " + within);
            matched = combined;
        }
        branch(Value { matched, 'w' }, ok, bad);
    }

    label(bad);
    raiseConstraintError();
    label(ok);
}

// The instruction that answers whether a discriminant value falls in a range.
std::string QbeEmitter::rangeTest(const Value& value, long long low, long long high)
{
    if (low == high) {
        return std::string(comparisonInstruction(BinaryOp::Equal, value.type)) + " " + value.name + ", "
            + std::to_string(low);
    }
    std::string atLeast = newTemp();
    std::string atMost = newTemp();
    line(atLeast + " =w " + comparisonInstruction(BinaryOp::GreaterEqual, value.type) + " " + value.name + ", "
         + std::to_string(low));
    line(atMost + " =w " + comparisonInstruction(BinaryOp::LessEqual, value.type) + " " + value.name + ", "
         + std::to_string(high));
    return "and " + atLeast + ", " + atMost;
}

void QbeEmitter::checkNotNull(const Value& pointer)
{
    // null designates no object at all, so reaching through it is an error.
    std::string isNull = newTemp();
    line(isNull + " =w ceql " + pointer.name + ", 0");

    std::string bad = newLabel("nullaccess");
    std::string ok = newLabel("notnull");
    branch(Value { isNull, 'w' }, bad, ok);
    label(bad);
    raiseConstraintError();
    label(ok);
}

void QbeEmitter::raiseConstraintError()
{
    line("storew 1, $__ada_exception");
    line("storel " + stringData("CONSTRAINT_ERROR") + ", $__ada_exception_name");
    if (!m_context->handlerLabels.empty()) {
        jump(m_context->handlerLabels.back());
    } else {
        m_context->usesPropagate = true;
        jump(m_context->propagateLabel);
    }
}
