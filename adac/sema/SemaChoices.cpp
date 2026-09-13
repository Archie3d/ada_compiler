#include "Sema.h"

#include <algorithm>
#include <cctype>

// A discrete choice, as written in a case alternative or a variant part: a
// value, a range, or a subtype mark standing for the range it covers.  Reports
// what is wrong with it and answers whether anything usable came out.
bool Sema::resolveChoice(Expr* lowExpr, Expr* highExpr, Type* selectorType, const std::vector<CaseChoice>& covered,
                         Scope* scope, CaseChoice& choice)
{
    if (Type* mark = highExpr == nullptr ? choiceSubtypeMark(lowExpr, scope) : nullptr) {
        if (!isDiscrete(mark)) {
            m_diagnostics.error(lowExpr->location,
                                "'" + mark->name + "' is not a discrete type, so it names no choices");
            return false;
        }
        lowExpr->type = mark;
        choice.low = mark->low;
        choice.high = mark->high;
    } else {
        analyzeExpr(lowExpr, scope, selectorType);
        if (highExpr != nullptr) {
            analyzeExpr(highExpr, scope, selectorType);
        }

        // A selector that is not discrete has already been complained about, and
        // its choices cannot be right whatever they say.
        if (selectorType == nullptr || !isDiscrete(selectorType)) {
            return false;
        }
        if (!foldStatic(lowExpr, choice.low)) {
            m_diagnostics.error(lowExpr->location, "choices have to be static");
            return false;
        }
        choice.high = choice.low;
        if (highExpr != nullptr && !foldStatic(highExpr, choice.high)) {
            m_diagnostics.error(highExpr->location, "choices have to be static");
            return false;
        }
    }

    if (selectorType == nullptr || !isDiscrete(selectorType)) {
        return false;
    }
    if (choice.low > choice.high) {
        m_diagnostics.error(lowExpr->location, "the range " + describeValue(selectorType, choice.low) + " .. "
                                                   + describeValue(selectorType, choice.high) + " is empty");
        return false;
    }
    if (choice.low < selectorType->low || choice.high > selectorType->high) {
        m_diagnostics.error(lowExpr->location,
                            describeValue(selectorType, choice.low) + " is not a value the selector can take");
        return false;
    }
    for (const CaseChoice& earlier : covered) {
        if (choice.low <= earlier.high && earlier.low <= choice.high) {
            std::string what = describeValue(selectorType, choice.low);
            if (choice.high != choice.low) {
                what += " .. " + describeValue(selectorType, choice.high);
            }
            m_diagnostics.error(lowExpr->location, what + " is covered by an earlier choice");
            return false;
        }
    }
    return true;
}

// Without an 'others', whatever the choices left out is an error rather than a
// value quietly unaccounted for.
void Sema::reportUncovered(std::vector<CaseChoice> covered, Type* selectorType, const SourceLocation& location,
                           const char* what)
{
    if (selectorType == nullptr || !isDiscrete(selectorType)) {
        return;
    }
    std::sort(covered.begin(), covered.end(),
              [](const CaseChoice& left, const CaseChoice& right) { return left.low < right.low; });

    auto reportGap = [&](long long low, long long high) {
        std::string gap = describeValue(selectorType, low);
        if (high != low) {
            gap += " .. " + describeValue(selectorType, high);
        }
        m_diagnostics.error(location, std::string("the ") + what + " leaves " + gap + " uncovered");
    };

    long long next = selectorType->low;
    for (const CaseChoice& choice : covered) {
        if (choice.low > next) {
            reportGap(next, choice.low - 1);
        }
        next = choice.high + 1;
    }
    if (next <= selectorType->high) {
        reportGap(next, selectorType->high);
    }
}

// A name standing for a type is a choice covering everything that type holds,
// which is how 'when Weekday' names five days at once.  Anything else is an
// expression and is read as one.
Type* Sema::choiceSubtypeMark(Expr* expr, Scope* scope)
{
    if (expr->kind != ExprKind::Identifier) {
        return nullptr;
    }
    Symbol* symbol = lookupName(static_cast<IdentifierExpr*>(expr)->lower, scope);
    if (symbol == nullptr || symbol->kind != SymbolKind::TypeName) {
        return nullptr;
    }
    return symbol->type;
}

// A value of the selector's type as a reader would write it, so that a
// complaint about a case on days talks about SAT rather than about 5.
std::string Sema::describeValue(Type* type, long long value) const
{
    Type* base = baseType(type);
    if (base == nullptr || base->kind != TypeKind::Enumeration) {
        return std::to_string(value);
    }
    if (base->literals.empty()) {
        // Character, which names its values by their spelling.
        return std::string("'") + static_cast<char>(value) + "'";
    }
    if (value < 0 || value >= static_cast<long long>(base->literals.size())) {
        return std::to_string(value);
    }

    std::string name = base->literals[static_cast<std::size_t>(value)];
    for (char& c : name) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return name;
}
