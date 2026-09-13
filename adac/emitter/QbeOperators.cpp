#include "QbeEmitter.h"
#include "QbeSupport.h"

using QbeSupport::realLiteral;
using QbeSupport::isFloatClass;
using QbeSupport::comparisonInstruction;

Value QbeEmitter::emitBinary(BinaryExpr* expr)
{
    if (expr->operatorCall) {
        return emitExpr(expr->operatorCall.get());
    }
    switch (expr->op) {
    case BinaryOp::AndThen:
    case BinaryOp::OrElse:
        return emitShortCircuit(expr);
    case BinaryOp::Concatenate:
        return emitConcatenation(expr);
    default:
        break;
    }

    Value left = emitExpr(expr->left.get());
    Value right = emitExpr(expr->right.get());
    char type = left.type == 'l' || right.type == 'l' ? 'l' : left.type;
    if (isFloatClass(left.type)) {
        type = left.type;
    } else if (isFloatClass(right.type)) {
        type = right.type;
    }

    if (!isFloatClass(type)) {
        int operation = -1;
        switch (expr->op) {
        case BinaryOp::Add: operation = 0; break;
        case BinaryOp::Subtract: operation = 1; break;
        case BinaryOp::Multiply: operation = 2; break;
        case BinaryOp::Divide: operation = 3; break;
        case BinaryOp::Remainder: operation = 4; break;
        case BinaryOp::Modulo: operation = 5; break;
        case BinaryOp::Power: operation = 6; break;
        default: break;
        }
        if (operation >= 0) {
            return emitIntegerOperation(operation, left, right, left.type);
        }
    }

    switch (expr->op) {
    case BinaryOp::Equal:
    case BinaryOp::NotEqual:
    case BinaryOp::Less:
    case BinaryOp::LessEqual:
    case BinaryOp::Greater:
    case BinaryOp::GreaterEqual: {
        if (expr->left->type != nullptr && expr->left->type->kind == TypeKind::Array) {
            return compareArrays(expr->op, left, expr->left->type, right, expr->right->type);
        }
        if (baseType(expr->left->type) != nullptr && baseType(expr->left->type)->kind == TypeKind::Record) {
            // The operands are addresses, so comparing them directly would be
            // asking whether they are the same object rather than equal ones.
            Value equal = compareRecords(left, right, expr->left->type);
            if (expr->op == BinaryOp::NotEqual) {
                std::string negated = newTemp();
                line(negated + " =w ceqw " + equal.name + ", 0");
                return Value { negated, 'w' };
            }
            return equal;
        }
        std::string temp = newTemp();
        line(temp + " =w " + comparisonInstruction(expr->op, type) + " " + left.name + ", " + right.name);
        return Value { temp, 'w' };
    }
    case BinaryOp::Modulo:
        return emitModulo(left, right, type);
    case BinaryOp::Power:
        return emitPower(left, right, type);
    default:
        break;
    }

    const char* instruction = "add";
    switch (expr->op) {
    case BinaryOp::Add:
        instruction = "add";
        break;
    case BinaryOp::Subtract:
        instruction = "sub";
        break;
    case BinaryOp::Multiply:
        instruction = "mul";
        break;
    case BinaryOp::Divide:
        instruction = "div";
        break;
    case BinaryOp::Remainder:
        instruction = "rem";
        break;
    case BinaryOp::And:
        instruction = "and";
        break;
    case BinaryOp::Or:
        instruction = "or";
        break;
    case BinaryOp::Xor:
        instruction = "xor";
        break;
    default:
        break;
    }

    std::string temp = newTemp();
    line(temp + " =" + std::string(1, type) + " " + instruction + " " + left.name + ", " + right.name);
    return Value { temp, type };
}

Value QbeEmitter::emitUnary(UnaryExpr* expr)
{
    Value operand = emitExpr(expr->operand.get());
    std::string temp = newTemp();

    switch (expr->op) {
    case UnaryOp::Plus:
        return operand;
    case UnaryOp::Negate: {
        if (!isFloatClass(operand.type)) {
            return emitIntegerOperation(1, Value { "0", operand.type }, operand, operand.type);
        }
        std::string zero = isFloatClass(operand.type) ? realLiteral(0.0, operand.type) : std::string("0");
        line(temp + " =" + std::string(1, operand.type) + " sub " + zero + ", " + operand.name);
        return Value { temp, operand.type };
    }
    case UnaryOp::Not:
        line(temp + " =w ceqw " + operand.name + ", 0");
        return Value { temp, 'w' };
    case UnaryOp::Abs: {
        bool isFloat = isFloatClass(operand.type);
        long long slotSize = operand.type == 'l' || operand.type == 'd' ? 8 : 4;
        std::string slot = allocScratch(slotSize);
        const char* storeInstruction = isFloat ? (operand.type == 'd' ? "stored" : "stores")
                                               : (operand.type == 'l' ? "storel" : "storew");
        const char* loadInstruction = isFloat ? (operand.type == 'd' ? "loadd" : "loads")
                                              : (operand.type == 'l' ? "loadl" : "loadsw");
        std::string zero = isFloat ? realLiteral(0.0, operand.type) : std::string("0");
        line(std::string(storeInstruction) + " " + operand.name + ", " + slot);
        std::string negative = newTemp();
        line(negative + " =w " + comparisonInstruction(BinaryOp::Less, operand.type) + " " + operand.name + ", "
             + zero);
        std::string negate = newLabel("absneg");
        std::string done = newLabel("absdone");
        branch(Value { negative, 'w' }, negate, done);
        label(negate);
        std::string negated = newTemp();
        if (isFloat) {
            line(negated + " =" + std::string(1, operand.type) + " sub " + zero + ", " + operand.name);
        } else {
            negated = emitIntegerOperation(1, Value { "0", operand.type }, operand, operand.type).name;
        }
        line(std::string(storeInstruction) + " " + negated + ", " + slot);
        jump(done);
        label(done);
        std::string result = newTemp();
        line(result + " =" + std::string(1, operand.type) + " " + loadInstruction + " " + slot);
        return Value { result, operand.type };
    }
    }

    return operand;
}

Value QbeEmitter::emitIntegerOperation(int operation, const Value& left, const Value& right, char type)
{
    auto widen = [&](const Value& value) {
        if (value.type == 'l') {
            return value.name;
        }
        std::string wide = newTemp();
        line(wide + " =l extsw " + value.name);
        return wide;
    };
    std::string leftWide = widen(left);
    std::string rightWide = widen(right);
    std::string result = newTemp();
    line(result + " =l call $__ada_integer_operation(w " + std::to_string(operation) + ", w "
         + (type == 'l' ? "64" : "32") + ", l " + leftWide + ", l " + rightWide + ")");
    emitExceptionCheck();
    if (type == 'w') {
        std::string narrowed = newTemp();
        line(narrowed + " =w copy " + result);
        result = narrowed;
    }
    return Value { result, type };
}

Value QbeEmitter::emitShortCircuit(BinaryExpr* expr)
{
    std::string slot = allocScratch(4);
    Value left = emitExpr(expr->left.get());
    line("storew " + left.name + ", " + slot);

    std::string evaluate = newLabel("shortcircuit");
    std::string done = newLabel("shortdone");
    if (expr->op == BinaryOp::AndThen) {
        branch(left, evaluate, done);
    } else {
        branch(left, done, evaluate);
    }
    label(evaluate);
    Value right = emitExpr(expr->right.get());
    line("storew " + right.name + ", " + slot);
    jump(done);
    label(done);

    std::string result = newTemp();
    line(result + " =w loadsw " + slot);
    return Value { result, 'w' };
}

Value QbeEmitter::emitModulo(const Value& left, const Value& right, char type)
{
    std::string slot = allocScratch(type == 'l' ? 8 : 4);
    std::string remainder = newTemp();
    line(remainder + " =" + std::string(1, type) + " rem " + left.name + ", " + right.name);
    line(std::string(type == 'l' ? "storel " : "storew ") + remainder + ", " + slot);

    std::string sign = newTemp();
    line(sign + " =" + std::string(1, type) + " xor " + remainder + ", " + right.name);
    std::string negative = newTemp();
    line(negative + " =w " + std::string(type == 'l' ? "csltl" : "csltw") + " " + sign + ", 0");
    std::string nonZero = newTemp();
    line(nonZero + " =w " + std::string(type == 'l' ? "cnel" : "cnew") + " " + remainder + ", 0");
    std::string adjust = newTemp();
    line(adjust + " =w and " + negative + ", " + nonZero);

    std::string fix = newLabel("modfix");
    std::string done = newLabel("moddone");
    branch(Value { adjust, 'w' }, fix, done);
    label(fix);
    std::string adjusted = newTemp();
    line(adjusted + " =" + std::string(1, type) + " add " + remainder + ", " + right.name);
    line(std::string(type == 'l' ? "storel " : "storew ") + adjusted + ", " + slot);
    jump(done);
    label(done);

    std::string result = newTemp();
    line(result + " =" + std::string(1, type) + " " + (type == 'l' ? "loadl " : "loadsw ") + slot);
    return Value { result, type };
}

Value QbeEmitter::emitPower(const Value& left, const Value& right, char type)
{
    bool isFloat = isFloatClass(type);
    std::string resultSlot = allocScratch(type == 'l' || type == 'd' ? 8 : 4);
    std::string counterSlot = allocScratch(4);
    const char* storeInstruction = isFloat ? (type == 'd' ? "stored" : "stores")
                                          : (type == 'l' ? "storel" : "storew");
    const char* loadInstruction = isFloat ? (type == 'd' ? "loadd" : "loads")
                                          : (type == 'l' ? "loadl" : "loadsw");

    line(std::string(storeInstruction) + " " + (isFloat ? realLiteral(1.0, type) : std::string("1")) + ", "
         + resultSlot);
    line("storew " + right.name + ", " + counterSlot);

    std::string head = newLabel("power");
    std::string body = newLabel("powerbody");
    std::string done = newLabel("powerdone");

    label(head);
    std::string counter = newTemp();
    line(counter + " =w loadsw " + counterSlot);
    std::string test = newTemp();
    line(test + " =w csgtw " + counter + ", 0");
    branch(Value { test, 'w' }, body, done);

    label(body);
    std::string accumulator = newTemp();
    line(accumulator + " =" + std::string(1, type) + " " + loadInstruction + " " + resultSlot);
    std::string product = newTemp();
    line(product + " =" + std::string(1, type) + " mul " + accumulator + ", " + left.name);
    line(std::string(storeInstruction) + " " + product + ", " + resultSlot);
    std::string decremented = newTemp();
    line(decremented + " =w sub " + counter + ", 1");
    line("storew " + decremented + ", " + counterSlot);
    jump(head);

    label(done);
    std::string result = newTemp();
    line(result + " =" + std::string(1, type) + " " + loadInstruction + " " + resultSlot);
    return Value { result, type };
}
