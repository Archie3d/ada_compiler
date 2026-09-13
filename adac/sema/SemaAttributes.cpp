#include "Sema.h"
#include "SemaSupport.h"

#include <algorithm>
#include <cfloat>

using SemaSupport::adaptUniversal;

namespace
{

// Without an explicit range a floating point type spans what its machine
// representation can hold.
double floatLowBound(const Type* type)
{
    if (type->hasRealRange) {
        return type->lowReal;
    }
    return typeSize(type) == 8 ? -DBL_MAX : -static_cast<double>(FLT_MAX);
}

double floatHighBound(const Type* type)
{
    if (type->hasRealRange) {
        return type->highReal;
    }
    return typeSize(type) == 8 ? DBL_MAX : static_cast<double>(FLT_MAX);
}

// Ada's T'Width: the length of the longest image the type can produce.  For an
// enumeration that is its longest literal, and for a number its widest run of
// digits plus the column the sign or its blank occupies.
long long widthOf(const Type* type)
{
    const Type* base = type;
    while (base != nullptr && base->base != nullptr && base->literals.empty()
           && base->kind == TypeKind::Enumeration) {
        base = base->base;
    }

    if (base != nullptr && base->kind == TypeKind::Enumeration) {
        // Character names its values by their spelling, so its widest image is
        // one character between two quotes.
        if (base->literals.empty()) {
            return 3;
        }
        std::size_t longest = 0;
        for (long long i = type->low; i <= type->high && i < static_cast<long long>(base->literals.size()); ++i) {
            longest = std::max(longest, base->literals[static_cast<std::size_t>(i)].size());
        }
        return static_cast<long long>(longest);
    }

    auto imageWidth = [](long long value) {
        return static_cast<long long>(std::to_string(value).size()) + (value >= 0 ? 1 : 0);
    };
    return std::max(imageWidth(type->low), imageWidth(type->high));
}

// A subtype is declared without repeating the digits of the type it comes from.
int digitsOf(const Type* type)
{
    for (const Type* current = type; current != nullptr; current = current->base) {
        if (current->digits > 0) {
            return current->digits;
        }
    }
    return 6;
}

}

Type* Sema::analyzeAttribute(AttributeExpr* expr, Scope* scope)
{
    bool prefixIsType = false;
    Type* prefixType = nullptr;

    if (expr->prefix->kind == ExprKind::Identifier) {
        auto* identifier = static_cast<IdentifierExpr*>(expr->prefix.get());
        std::vector<Symbol*> candidates = scope->lookup(identifier->lower);
        for (Symbol* candidate : candidates) {
            if (candidate->kind == SymbolKind::TypeName) {
                prefixIsType = true;
                prefixType = candidate->type;
                identifier->symbol = candidate;
                identifier->type = candidate->type;
                break;
            }
        }
    }
    if (!prefixIsType) {
        prefixType = analyzeExpr(expr->prefix.get(), scope, nullptr);
    }
    if (expr->prefix->kind == ExprKind::Selected) {
        auto* selected = static_cast<SelectedExpr*>(expr->prefix.get());
        prefixIsType = selected->symbol != nullptr && selected->symbol->kind == SymbolKind::TypeName;
    } else if (expr->prefix->kind == ExprKind::Attribute) {
        prefixIsType = static_cast<AttributeExpr*>(expr->prefix.get())->lower == "base";
    }
    expr->prefixType = prefixType;
    if (expr->lower == "base") {
        if (!prefixIsType || !expr->arguments.empty()) {
            m_diagnostics.error(expr->location, "'Base requires a scalar subtype mark");
            return nullptr;
        }
        expr->type = m_types.scalarBaseType(prefixType);
        if (expr->type == nullptr) {
            m_diagnostics.error(expr->location, "'Base requires a scalar subtype mark");
        }
        return expr->type;
    }

    for (const ExprPtr& argument : expr->arguments) {
        // An aggregate waits for the stream attributes below, which know the
        // type it is meant to have.
        if (argument->kind != ExprKind::Aggregate) {
            bool contextual = expr->lower == "image" || expr->lower == "pos"
                || expr->lower == "succ" || expr->lower == "pred";
            analyzeExpr(argument.get(), scope, contextual ? prefixType : nullptr);
        }
    }

    // Subtypes carry their own constraint, so the prefix type is used as written.
    Type* base = prefixType;
    const std::string& name = expr->lower;
    if (base != nullptr && base->kind == TypeKind::Array
        && (name == "first" || name == "last" || name == "length")) {
        if (prefixIsType && !base->constrained) {
            m_diagnostics.error(expr->location, "array bound attributes require an object or a constrained array subtype");
            return nullptr;
        }
        long long dimension = 1;
        Type* dimensionType = expr->arguments.empty() ? nullptr : rootType(expr->arguments.front()->type);
        if (expr->arguments.size() > 1
            || (!expr->arguments.empty() && (!foldStatic(expr->arguments.front().get(), dimension)
                || dimensionType == nullptr
                || (dimensionType->kind != TypeKind::Integer && dimensionType->kind != TypeKind::UniversalInteger)))
            || dimension < 1 || dimension > base->arrayRank) {
            m_diagnostics.error(expr->location, "array dimension must be a static value within the array rank");
            return nullptr;
        }
        for (long long i = 1; i < dimension; ++i) {
            base = base->element;
        }
        expr->prefixType = base;
    }

    if (name == "read" || name == "write" || name == "input" || name == "output") {
        if (base != nullptr && base->kind == TypeKind::Array && base->arrayRank > 1 && !base->constrained) {
            m_diagnostics.error(expr->location, "stream attributes for unconstrained multidimensional arrays are not yet supported");
            return nullptr;
        }
        bool reads = name == "read" || name == "input";
        bool yieldsValue = name == "input";
        std::size_t wanted = yieldsValue ? 1u : 2u;

        if (!prefixIsType) {
            m_diagnostics.error(expr->location, "'" + expr->name + "' applies to a type");
            return nullptr;
        }
        if (expr->arguments.size() != wanted) {
            m_diagnostics.error(expr->location,
                                "'" + expr->name + "' expects "
                                    + (yieldsValue ? "a stream" : "a stream and an item"));
            return nullptr;
        }
        if (expr->arguments[0]->type == nullptr
            || baseType(expr->arguments[0]->type)->kind != TypeKind::Access) {
            m_diagnostics.error(expr->arguments[0]->location, "'" + expr->name + "' expects a stream access");
            return nullptr;
        }
        if (yieldsValue) {
            expr->type = base;
            return expr->type;
        }

        Expr* item = expr->arguments[1].get();
        if (item->type == nullptr) {
            analyzeExpr(item, scope, base);
        }
        // A literal takes the type the attribute names, so that it occupies
        // the width that type does rather than a universal one.
        adaptUniversal(item, base);
        if (reads) {
            checkAssignable(item, scope);
        }
        if (!typesCompatible(base, expr->arguments[1]->type)) {
            m_diagnostics.error(expr->arguments[1]->location,
                                "the item does not have type '" + (base != nullptr ? base->name : "") + "'");
        }
        expr->type = m_types.voidType();
        return expr->type;
    }

    if (name == "first" || name == "last") {
        if (base != nullptr && base->kind == TypeKind::Array) {
            expr->type = base->index != nullptr ? base->index : m_types.integerType();
            if (base->constrained) {
                expr->isStatic = true;
                expr->staticValue = name == "first" ? base->indexLow : base->indexHigh;
            }
            return expr->type;
        }
        if (isDiscrete(base)) {
            expr->type = prefixType;
            expr->isStatic = true;
            expr->staticValue = name == "first" ? prefixType->low : prefixType->high;
            return expr->type;
        }
        if (base != nullptr && base->kind == TypeKind::Float) {
            expr->type = prefixType;
            expr->isStatic = true;
            expr->staticReal = name == "first" ? floatLowBound(base) : floatHighBound(base);
            return expr->type;
        }
        m_diagnostics.error(expr->location, "'" + expr->name + " requires an array or scalar prefix");
        return nullptr;
    }

    if (name == "digits") {
        if (base == nullptr || base->kind != TypeKind::Float) {
            m_diagnostics.error(expr->location, "'Digits requires a floating point prefix");
            return nullptr;
        }
        expr->type = m_types.integerType();
        expr->isStatic = true;
        expr->staticValue = base->digits;
        return expr->type;
    }

    if (name == "length") {
        expr->type = m_types.integerType();
        if (base != nullptr && base->kind == TypeKind::Array && base->constrained) {
            expr->isStatic = true;
            expr->staticValue = arrayLength(base);
        }
        return expr->type;
    }

    if (name == "pos") {
        if (expr->arguments.size() != 1) {
            m_diagnostics.error(expr->location, "'Pos takes exactly one argument");
            return nullptr;
        }
        adaptUniversal(expr->arguments.front().get(), prefixType);
        expr->type = m_types.integerType();
        expr->isStatic = expr->arguments.front()->isStatic;
        expr->staticValue = expr->arguments.front()->staticValue;
        return expr->type;
    }

    if (name == "val" || name == "succ" || name == "pred") {
        if (expr->arguments.size() != 1) {
            m_diagnostics.error(expr->location, "'" + expr->name + " takes exactly one argument");
            return nullptr;
        }
        Type* argumentType = name == "val" ? m_types.universalInteger() : prefixType;
        adaptUniversal(expr->arguments.front().get(), argumentType);
        expr->type = prefixType;
        if (expr->arguments.front()->isStatic) {
            long long value = expr->arguments.front()->staticValue;
            bool overflow = name == "succ" ? __builtin_add_overflow(value, 1LL, &value)
                : (name == "pred" && __builtin_sub_overflow(value, 1LL, &value));
            // Keep exceptional cases in the emitted path so they raise an Ada
            // exception instead of overflowing the compiler's own arithmetic.
            if (!overflow && value >= prefixType->low && value <= prefixType->high) {
                expr->isStatic = true;
                expr->staticValue = value;
            }
        }
        return expr->type;
    }

    if (name == "image") {
        if (expr->arguments.size() != 1) {
            m_diagnostics.error(expr->location, "'Image takes exactly one argument");
            return nullptr;
        }
        adaptUniversal(expr->arguments.front().get(), prefixType);
        expr->type = m_types.stringType();
        return expr->type;
    }

    // Where an object is and how much room it takes.  Together they let a
    // generic body hand a value to the run time without either side having to
    // know what shape it has.
    if (name == "address") {
        if (prefixIsType) {
            m_diagnostics.error(expr->location, "'Address requires an object, not a type");
            return nullptr;
        }
        expr->type = m_addressType;
        return expr->type;
    }

    if (name == "size") {
        if (prefixType == nullptr) {
            m_diagnostics.error(expr->location, "'Size requires a type or an object");
            return nullptr;
        }
        expr->type = m_types.integerType();
        expr->isStatic = true;
        expr->staticValue = typeSize(prefixType) * 8;
        return expr->type;
    }

    // How wide the longest image of the type is, which is what a generic body
    // uses to lay a value out in a column without knowing the type.
    if (name == "width") {
        if (!isDiscrete(base)) {
            m_diagnostics.error(expr->location, "'Width requires a discrete prefix");
            return nullptr;
        }
        expr->type = m_types.integerType();
        expr->isStatic = true;
        expr->staticValue = widthOf(prefixType);
        return expr->type;
    }

    // The other way round from 'Image: the value a string spells out.
    if (name == "value") {
        if (expr->arguments.size() != 1) {
            m_diagnostics.error(expr->location, "'Value takes exactly one argument");
            return nullptr;
        }
        if (!isDiscrete(base)) {
            m_diagnostics.error(expr->location, "'Value requires a discrete prefix");
            return nullptr;
        }
        Type* argumentType = expr->arguments.front()->type;
        if (argumentType != nullptr && !typesCompatible(m_types.stringType(), argumentType)) {
            m_diagnostics.error(expr->location, "'Value reads its value from a string");
        }
        expr->type = prefixType;
        return expr->type;
    }

    m_diagnostics.error(expr->location, "unsupported attribute '" + expr->name + "'");
    return nullptr;
}
