#include "Sema.h"

#include <cstdint>
#include <limits>

bool Sema::foldStatic(Expr* expr, long long& value) const
{
    if (expr == nullptr || isReal(expr->type)) {
        return false;
    }
    if (expr->isStatic) {
        value = expr->staticValue;
        return true;
    }

    switch (expr->kind) {
    case ExprKind::IntegerLiteral:
        value = static_cast<IntegerLiteralExpr*>(expr)->value;
        return true;
    case ExprKind::CharacterLiteral:
        value = static_cast<unsigned char>(static_cast<CharacterLiteralExpr*>(expr)->value);
        return true;
    case ExprKind::Unary: {
        auto* unary = static_cast<UnaryExpr*>(expr);
        long long operand = 0;
        if (!foldStatic(unary->operand.get(), operand)) {
            return false;
        }
        switch (unary->op) {
        case UnaryOp::Plus:
            value = operand;
            return true;
        case UnaryOp::Negate:
            return !__builtin_sub_overflow(0LL, operand, &value);
        case UnaryOp::Abs:
            if (operand < 0) {
                return !__builtin_sub_overflow(0LL, operand, &value);
            }
            value = operand;
            return true;
        case UnaryOp::Not:
            value = operand != 0 ? 0 : 1;
            return true;
        }
        return false;
    }
    case ExprKind::Binary: {
        auto* binary = static_cast<BinaryExpr*>(expr);
        if (binary->operatorCall) {
            return false;
        }
        long long left = 0;
        long long right = 0;
        if (!foldStatic(binary->left.get(), left) || !foldStatic(binary->right.get(), right)) {
            return false;
        }
        switch (binary->op) {
        case BinaryOp::Add:
            return !__builtin_add_overflow(left, right, &value);
        case BinaryOp::Subtract:
            return !__builtin_sub_overflow(left, right, &value);
        case BinaryOp::Multiply:
            return !__builtin_mul_overflow(left, right, &value);
        case BinaryOp::Divide:
            if (right == 0 || (left == std::numeric_limits<long long>::min() && right == -1)) {
                return false;
            }
            value = left / right;
            return true;
        case BinaryOp::Modulo:
        case BinaryOp::Remainder:
            if (right == 0) {
                return false;
            }
            value = left == std::numeric_limits<long long>::min() && right == -1 ? 0 : left % right;
            if (binary->op == BinaryOp::Modulo && value != 0 && (value < 0) != (right < 0)) {
                value += right;
            }
            return true;
        case BinaryOp::Power: {
            if (right < 0) {
                return false;
            }
            long long result = 1;
            while (right != 0) {
                if ((right & 1) && __builtin_mul_overflow(result, left, &result)) {
                    return false;
                }
                right >>= 1;
                if (right != 0 && __builtin_mul_overflow(left, left, &left)) {
                    return false;
                }
            }
            value = result;
            return true;
        }
        default:
            return false;
        }
    }
    default:
        return false;
    }
}

bool Sema::foldStaticReal(Expr* expr, double& value) const
{
    if (expr == nullptr) {
        return false;
    }
    if (expr->isStatic && isReal(expr->type)) {
        value = expr->staticReal;
        return true;
    }

    switch (expr->kind) {
    case ExprKind::RealLiteral:
        value = static_cast<RealLiteralExpr*>(expr)->value;
        return true;
    case ExprKind::IntegerLiteral:
        value = static_cast<double>(static_cast<IntegerLiteralExpr*>(expr)->value);
        return true;
    case ExprKind::Unary: {
        auto* unary = static_cast<UnaryExpr*>(expr);
        double operand = 0.0;
        if (!foldStaticReal(unary->operand.get(), operand)) {
            return false;
        }
        switch (unary->op) {
        case UnaryOp::Plus:
            value = operand;
            return true;
        case UnaryOp::Negate:
            value = -operand;
            return true;
        case UnaryOp::Abs:
            value = operand < 0.0 ? -operand : operand;
            return true;
        default:
            return false;
        }
    }
    case ExprKind::Binary: {
        auto* binary = static_cast<BinaryExpr*>(expr);
        if (binary->operatorCall) {
            return false;
        }
        double left = 0.0;
        double right = 0.0;
        if (!foldStaticReal(binary->left.get(), left) || !foldStaticReal(binary->right.get(), right)) {
            return false;
        }
        switch (binary->op) {
        case BinaryOp::Add:
            value = left + right;
            return true;
        case BinaryOp::Subtract:
            value = left - right;
            return true;
        case BinaryOp::Multiply:
            value = left * right;
            return true;
        case BinaryOp::Divide:
            if (right == 0.0) {
                return false;
            }
            value = left / right;
            return true;
        default:
            return false;
        }
    }
    default:
        return false;
    }
}

// Records whatever an expression folds to, choosing the representation that
// matches its type.
void Sema::noteStaticValue(Expr* expr)
{
    if (expr == nullptr || expr->isStatic) {
        return;
    }
    if (isReal(expr->type)) {
        double value = 0.0;
        if (foldStaticReal(expr, value)) {
            expr->isStatic = true;
            expr->staticReal = value;
        }
        return;
    }
    long long value = 0;
    if (foldStatic(expr, value)) {
        expr->isStatic = true;
        expr->staticValue = value;
    }
}
