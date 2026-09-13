#include "QbeSupport.h"

#include "QbeEmitter.h"

#include <cctype>
#include <cstdio>

namespace QbeSupport
{

Value constantValue(long long value, char type)
{
    return Value { std::to_string(value), type };
}

// QBE reads these with scanf, so the full precision of the value is written out
// rather than the six decimals std::to_string would give.
std::string realLiteral(double value, char type)
{
    char buffer[40];
    std::snprintf(buffer, sizeof buffer, "%.17g", value);
    return (type == 's' ? "s_" : "d_") + std::string(buffer);
}

bool isFloatClass(char type)
{
    return type == 's' || type == 'd';
}

bool isUnconstrainedArray(const Type* type)
{
    return type != nullptr && type->kind == TypeKind::Array && !type->constrained;
}

bool isLiteralOperand(const std::string& operand)
{
    if (operand.empty()) {
        return false;
    }
    std::size_t start = operand[0] == '-' ? 1 : 0;
    if (start >= operand.size()) {
        return false;
    }
    for (std::size_t i = start; i < operand.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(operand[i])) == 0) {
            return false;
        }
    }
    return true;
}

const char* comparisonInstruction(BinaryOp op, char type)
{
    bool isFloat = type == 's' || type == 'd';
    switch (op) {
    case BinaryOp::Equal:
        return isFloat ? (type == 's' ? "ceqs" : "ceqd") : (type == 'l' ? "ceql" : "ceqw");
    case BinaryOp::NotEqual:
        return isFloat ? (type == 's' ? "cnes" : "cned") : (type == 'l' ? "cnel" : "cnew");
    case BinaryOp::Less:
        return isFloat ? (type == 's' ? "clts" : "cltd") : (type == 'l' ? "csltl" : "csltw");
    case BinaryOp::LessEqual:
        return isFloat ? (type == 's' ? "cles" : "cled") : (type == 'l' ? "cslel" : "cslew");
    case BinaryOp::Greater:
        return isFloat ? (type == 's' ? "cgts" : "cgtd") : (type == 'l' ? "csgtl" : "csgtw");
    case BinaryOp::GreaterEqual:
        return isFloat ? (type == 's' ? "cges" : "cged") : (type == 'l' ? "csgel" : "csgew");
    default:
        return "ceqw";
    }
}

}
