#include "Sema.h"
#include "SemaSupport.h"

#include <utility>

using SemaSupport::isUniversal;
using SemaSupport::adaptUniversal;

Type* Sema::analyzeExpr(Expr* expr, Scope* scope, Type* expected)
{
    if (expr == nullptr) {
        return nullptr;
    }

    switch (expr->kind) {
    case ExprKind::IntegerLiteral: {
        auto* literal = static_cast<IntegerLiteralExpr*>(expr);
        Type* type = m_types.universalInteger();
        if (expected != nullptr && expected->kind == TypeKind::Integer) {
            type = expected;
        }
        expr->type = type;
        expr->isStatic = true;
        expr->staticValue = literal->value;
        return type;
    }
    case ExprKind::RealLiteral: {
        auto* literal = static_cast<RealLiteralExpr*>(expr);
        expr->type = expected != nullptr && expected->kind == TypeKind::Float ? expected : m_types.universalReal();
        expr->isStatic = true;
        expr->staticReal = literal->value;
        return expr->type;
    }
    case ExprKind::CharacterLiteral: {
        auto* literal = static_cast<CharacterLiteralExpr*>(expr);
        expr->type = expected != nullptr && m_types.isCharacter(expected) ? expected : m_types.characterType();
        expr->isStatic = true;
        expr->staticValue = static_cast<unsigned char>(literal->value);
        return expr->type;
    }
    case ExprKind::StringLiteral: {
        auto* literal = static_cast<StringLiteralExpr*>(expr);
        Type* type = m_types.makeSubtype(anonymousTypeName(), m_types.stringType(), 0, 0);
        type->constrained = true;
        type->indexLow = 1;
        type->indexHigh = static_cast<long long>(literal->value.size());
        expr->type = type;
        return type;
    }
    case ExprKind::Null:
        expr->type = expected != nullptr && expected->kind == TypeKind::Access ? expected : nullptr;
        return expr->type;
    case ExprKind::Allocator:
        return analyzeAllocator(static_cast<AllocatorExpr*>(expr), scope, expected);
    case ExprKind::Identifier:
        return analyzeIdentifier(static_cast<IdentifierExpr*>(expr), scope, expected);
    case ExprKind::Selected:
        return analyzeSelected(static_cast<SelectedExpr*>(expr), scope, expected);
    case ExprKind::Call:
        return analyzeCall(static_cast<CallExpr*>(expr), scope, expected);
    case ExprKind::Attribute:
        return analyzeAttribute(static_cast<AttributeExpr*>(expr), scope);
    case ExprKind::Aggregate:
        return analyzeAggregate(static_cast<AggregateExpr*>(expr), scope, expected);
    case ExprKind::Binary:
        return analyzeBinary(static_cast<BinaryExpr*>(expr), scope, expected);
    case ExprKind::Unary:
        return analyzeUnary(static_cast<UnaryExpr*>(expr), scope, expected);
    case ExprKind::Membership:
        return analyzeMembership(static_cast<MembershipExpr*>(expr), scope);
    case ExprKind::Qualified: {
        auto* qualified = static_cast<QualifiedExpr*>(expr);
        Type* type = resolveTypeName(qualified->typeLower, scope, qualified->location);
        analyzeExpr(qualified->operand.get(), scope, type);
        adaptUniversal(qualified->operand.get(), type);
        expr->type = type;
        expr->isStatic = qualified->operand->isStatic;
        expr->staticValue = qualified->operand->staticValue;
        expr->staticReal = qualified->operand->staticReal;
        return type;
    }
    }

    return nullptr;
}

Type* Sema::analyzeAllocator(AllocatorExpr* expr, Scope* scope, Type* expected)
{
    // Like null, an allocator has no type of its own: the access type it is
    // being used as says what it makes.
    Type* access = expected != nullptr ? baseType(expected) : nullptr;
    if (access == nullptr || access->kind != TypeKind::Access) {
        m_diagnostics.error(expr->location, "the access type of an allocator has to be known from its context");
        return nullptr;
    }

    Type* designated = resolveSubtypeIndication(expr->subtype.get(), scope);
    if (designated == nullptr) {
        return nullptr;
    }
    if (rootType(designated) != rootType(access->target)) {
        m_diagnostics.error(expr->location, "an allocator for '" + access->name + "' has to make a '"
                                                + (access->target != nullptr ? access->target->name : "?") + "'");
        return nullptr;
    }
    if (designated->kind == TypeKind::Array && !designated->constrained) {
        m_diagnostics.error(expr->location, "an allocator for an array needs its bounds, as in 'new " + expr->subtype->name
                                                + " (1 .. 10)'");
        return nullptr;
    }

    expr->designated = designated;
    if (expr->value != nullptr) {
        analyzeExpr(expr->value.get(), scope, designated);
        adaptUniversal(expr->value.get(), designated);
    }

    expr->type = expected;
    return expr->type;
}

// Outside the package that declared it, a private type carries assignment and
// equality and nothing else; a limited one carries not even those.
void Sema::checkPrivateOperands(BinaryExpr* expr)
{
    bool comparison = expr->op == BinaryOp::Equal || expr->op == BinaryOp::NotEqual;

    for (Expr* operand : { expr->left.get(), expr->right.get() }) {
        Type* type = operand != nullptr ? baseType(operand->type) : nullptr;
        if (type == nullptr || type->privateTo == nullptr || withinPackage(type->privateTo)) {
            continue;
        }
        if (comparison && !type->isLimited) {
            continue;
        }
        m_diagnostics.error(expr->location, "'" + type->name + "' is "
                                                + (type->isLimited ? "limited private" : "private")
                                                + ", so '" + type->privateTo->displayName
                                                + "' has to be the one to say what this operator means");
        return;
    }
}

Type* Sema::analyzeBinary(BinaryExpr* expr, Scope* scope, Type* expected)
{
    Type* result = analyzeBinaryOperation(expr, scope, expected);
    checkPrivateOperands(expr);
    return result;
}

Type* Sema::analyzeBinaryOperation(BinaryExpr* expr, Scope* scope, Type* expected)
{
    switch (expr->op) {
    case BinaryOp::And:
    case BinaryOp::Or:
    case BinaryOp::Xor:
    case BinaryOp::AndThen:
    case BinaryOp::OrElse: {
        Type* left = analyzeExpr(expr->left.get(), scope, m_types.booleanType());
        Type* right = analyzeExpr(expr->right.get(), scope, m_types.booleanType());
        if (!m_types.isBoolean(left) || !m_types.isBoolean(right)) {
            m_diagnostics.error(expr->location, "logical operators require Boolean operands");
        }
        expr->type = m_types.booleanType();
        return expr->type;
    }

    case BinaryOp::Equal:
    case BinaryOp::NotEqual:
    case BinaryOp::Less:
    case BinaryOp::LessEqual:
    case BinaryOp::Greater:
    case BinaryOp::GreaterEqual: {
        Type* left = analyzeExpr(expr->left.get(), scope, nullptr);
        Type* right = analyzeExpr(expr->right.get(), scope, isUniversal(left) ? nullptr : left);
        // Compatibility is judged before adaptation, which would otherwise give
        // a literal the very type it is being compared against.
        if (!typesCompatible(left, right)) {
            m_diagnostics.error(expr->location, "the operands of a comparison must have the same type");
        }
        if (isUniversal(left) && !isUniversal(right)) {
            adaptUniversal(expr->left.get(), right);
        } else if (!isUniversal(left) && isUniversal(right)) {
            adaptUniversal(expr->right.get(), left);
        }
        // Records are equal or not; nothing says which of two comes first.  A
        // private one is left to the check below, which has more to say.
        if (expr->op != BinaryOp::Equal && expr->op != BinaryOp::NotEqual && baseType(left) != nullptr
            && baseType(left)->kind == TypeKind::Record && representationVisible(left)) {
            m_diagnostics.error(expr->location, "records can be compared for equality, but not put in order");
        }
        if (expr->op != BinaryOp::Equal && expr->op != BinaryOp::NotEqual
            && left != nullptr && left->kind == TypeKind::Array
            && !isDiscrete(left->element) && representationVisible(left)) {
            m_diagnostics.error(expr->location, "array ordering requires discrete components");
        }
        expr->type = m_types.booleanType();
        return expr->type;
    }

    case BinaryOp::Concatenate: {
        Type* left = analyzeExpr(expr->left.get(), scope, nullptr);
        Type* right = analyzeExpr(expr->right.get(), scope, nullptr);
        long long length = 0;
        bool constrained = true;
        auto measure = [&](Type* type) {
            if (type == nullptr) {
                constrained = false;
                return;
            }
            if (type->kind == TypeKind::Array) {
                if (!type->constrained) {
                    constrained = false;
                } else {
                    length += arrayLength(type);
                }
                return;
            }
            length += 1;  // A single character operand.
        };
        measure(left);
        measure(right);
        Type* result = m_types.makeSubtype(anonymousTypeName(), m_types.stringType(), 0, 0);
        result->constrained = constrained;
        result->indexLow = 1;
        result->indexHigh = constrained ? length : 0;
        expr->type = result;
        return expr->type;
    }

    case BinaryOp::Power: {
        Type* left = analyzeExpr(expr->left.get(), scope, expected);
        std::vector<Symbol*> operators = scope->lookup("**");
        Type* right = analyzeExpr(expr->right.get(), scope,
                                  operators.empty() ? m_types.integerType() : nullptr);
        bool visibleOperator = false;
        for (Symbol* candidate : operators) {
            if (candidate->kind == SymbolKind::Subprogram && candidate->parameters.size() == 2
                && matchesResult(candidate, expected)
                && typesCompatible(candidate->parameters[0]->type, left)
                && typesCompatible(candidate->parameters[1]->type, right)) {
                visibleOperator = true;
            }
        }
        if (visibleOperator) {
            auto call = std::make_unique<CallExpr>();
            call->location = expr->location;
            auto callee = std::make_unique<IdentifierExpr>();
            callee->location = expr->location;
            callee->name = "**";
            callee->lower = "**";
            call->callee = std::move(callee);
            Association first;
            first.value = std::move(expr->left);
            Association second;
            second.value = std::move(expr->right);
            call->arguments.push_back(std::move(first));
            call->arguments.push_back(std::move(second));
            expr->type = analyzeCall(call.get(), scope, expected);
            expr->operatorCall = std::move(call);
            return expr->type;
        }
        // Predefined exponentiation still requires an integer exponent.
        adaptUniversal(expr->right.get(), m_types.integerType());
        if (!isNumeric(baseType(left))) {
            m_diagnostics.error(expr->location, "arithmetic operators require numeric operands");
        }
        if (!isDiscrete(baseType(right))) {
            m_diagnostics.error(expr->right->location, "the exponent of '**' must be an integer");
        }
        expr->type = left;
        noteStaticValue(expr);
        return expr->type;
    }

    default: {
        Type* left = analyzeExpr(expr->left.get(), scope, expected);
        Type* right = analyzeExpr(expr->right.get(), scope, isUniversal(left) ? expected : left);
        Type* result = left;
        if (!typesCompatible(left, right)) {
            m_diagnostics.error(expr->location, "the operands of an arithmetic operator must have the same type");
        }
        if (isUniversal(left) && !isUniversal(right)) {
            adaptUniversal(expr->left.get(), right);
            result = right;
        } else if (!isUniversal(left) && isUniversal(right)) {
            adaptUniversal(expr->right.get(), left);
        }
        if (!isNumeric(baseType(result))) {
            m_diagnostics.error(expr->location, "arithmetic operators require numeric operands");
        }
        if ((expr->op == BinaryOp::Modulo || expr->op == BinaryOp::Remainder) && isReal(baseType(result))) {
            m_diagnostics.error(expr->location, "'mod' and 'rem' require integer operands");
        }
        expr->type = result;
        noteStaticValue(expr);
        return expr->type;
    }
    }
}

Type* Sema::analyzeUnary(UnaryExpr* expr, Scope* scope, Type* expected)
{
    Type* operand = analyzeExpr(expr->operand.get(), scope, expected);
    if (expr->op == UnaryOp::Not) {
        if (!m_types.isBoolean(operand)) {
            m_diagnostics.error(expr->location, "'not' requires a Boolean operand");
        }
        expr->type = m_types.booleanType();
        return expr->type;
    }
    if (!isNumeric(baseType(operand))) {
        m_diagnostics.error(expr->location, "unary operators require a numeric operand");
    }
    expr->type = operand;
    noteStaticValue(expr);
    return expr->type;
}

Type* Sema::analyzeMembership(MembershipExpr* expr, Scope* scope)
{
    Type* operand = analyzeExpr(expr->operand.get(), scope, nullptr);
    if (isUniversal(operand)) {
        operand = m_types.integerType();
        adaptUniversal(expr->operand.get(), operand);
    }

    if (!expr->typeLower.empty()) {
        Type* type = resolveTypeName(expr->typeLower, scope, expr->location);
        if (type != nullptr) {
            auto makeBound = [&](long long value) {
                auto literal = std::make_unique<IntegerLiteralExpr>();
                literal->location = expr->location;
                literal->value = value;
                literal->type = operand;
                literal->isStatic = true;
                literal->staticValue = value;
                return ExprPtr(std::move(literal));
            };
            expr->low = makeBound(type->low);
            expr->high = makeBound(type->high);
        }
    } else {
        analyzeExpr(expr->low.get(), scope, operand);
        analyzeExpr(expr->high.get(), scope, operand);
        adaptUniversal(expr->low.get(), operand);
        adaptUniversal(expr->high.get(), operand);
    }

    expr->type = m_types.booleanType();
    return expr->type;
}
