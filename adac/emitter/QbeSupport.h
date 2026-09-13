#pragma once

#include <string>

struct Value;
class Type;
enum class BinaryOp;

// Helpers shared by the QBE emitter implementation.
namespace QbeSupport
{

Value constantValue(long long value, char type);
std::string realLiteral(double value, char type);
bool isFloatClass(char type);
bool isUnconstrainedArray(const Type* type);
bool isLiteralOperand(const std::string& operand);
const char* comparisonInstruction(BinaryOp op, char type);

}
