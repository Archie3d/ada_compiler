#include "Sema.h"
#include "SemaSupport.h"

#include <algorithm>
#include <utility>

using SemaSupport::adaptUniversal;

Type* Sema::analyzeAggregate(AggregateExpr* expr, Scope* scope, Type* expected)
{
    Type* target = expected;
    if (target == nullptr || (target->kind != TypeKind::Array && target->kind != TypeKind::Record)) {
        m_diagnostics.error(expr->location, "an aggregate needs a known array or record type");
        for (AggregateComponent& component : expr->components) {
            analyzeExpr(component.value.get(), scope, nullptr);
        }
        return nullptr;
    }
    if (!checkNotPrivate(target, expr->location, "an aggregate spells out the components")) {
        return nullptr;
    }

    if (target->kind == TypeKind::Record) {
        // A record with a variant part carries the components of one
        // alternative and no other, so which one it is has to be settled before
        // the aggregate can be read.
        int variant = knownVariant(target);
        // Nothing fixed the discriminant, so the aggregate itself has to say
        // what it is: 'new Shape'(Rectangle, ...)' makes a rectangle.
        long long chosen = 0;
        if (baseType(target)->variantOn >= 0 && variant < 0
            && aggregateDiscriminant(expr, baseType(target), scope, baseType(target)->variantOn, chosen)) {
            variant = variantFor(baseType(target), chosen);
        }
        if (baseType(target)->variantOn >= 0 && variant < 0) {
            m_diagnostics.error(expr->location, "an aggregate for '" + baseType(target)->name
                                                    + "' needs its discriminant fixed, as in '" + baseType(target)->name
                                                    + " (...)', so that its components are known");
            for (AggregateComponent& component : expr->components) {
                analyzeExpr(component.value.get(), scope, nullptr);
            }
            return nullptr;
        }

        // The aggregate says what the discriminant is too, and it has to be the
        // one the subtype fixed: otherwise the components it goes on to name
        // are those of a different value than the one being made.
        if (!checkAggregateDiscriminants(expr, target, scope)) {
            for (AggregateComponent& component : expr->components) {
                analyzeExpr(component.value.get(), scope, nullptr);
            }
            return nullptr;
        }

        auto present = [&](const FieldInfo& field) { return field.variant < 0 || field.variant == variant; };
        bool complained = false;

        expr->resolvedFields.assign(target->fields.size(), nullptr);
        std::size_t positional = 0;
        for (AggregateComponent& component : expr->components) {
            if (component.isOthers) {
                for (std::size_t i = 0; i < target->fields.size(); ++i) {
                    if (expr->resolvedFields[i] == nullptr && present(target->fields[i])) {
                        analyzeExpr(component.value.get(), scope, target->fields[i].type);
                        expr->resolvedFields[i] = component.value.get();
                    }
                }
                continue;
            }
            if (component.names.empty() || component.names.front().empty()) {
                while (positional < target->fields.size() && !present(target->fields[positional])) {
                    ++positional;
                }
                if (positional >= target->fields.size()) {
                    m_diagnostics.error(component.value->location, "too many components in record aggregate");
                    break;
                }
                Type* fieldType = target->fields[positional].type;
                analyzeExpr(component.value.get(), scope, fieldType);
                adaptUniversal(component.value.get(), fieldType);
                expr->resolvedFields[positional] = component.value.get();
                ++positional;
                continue;
            }
            for (const std::string& fieldName : component.names) {
                bool found = false;
                for (const FieldInfo& field : target->fields) {
                    if (field.name != fieldName) {
                        continue;
                    }
                    found = true;
                    if (!present(field)) {
                        // One message for the aggregate rather than one for
                        // every component of the alternative it named.
                        if (!complained) {
                            m_diagnostics.error(component.value->location,
                                                "'" + field.displayName + "' belongs to another variant of '"
                                                    + baseType(target)->name + "' than this one");
                            complained = true;
                        }
                        break;
                    }
                    analyzeExpr(component.value.get(), scope, field.type);
                    adaptUniversal(component.value.get(), field.type);
                    expr->resolvedFields[field.index] = component.value.get();
                    break;
                }
                if (!found) {
                    m_diagnostics.error(component.value->location,
                                        "'" + fieldName + "' is not a component of '" + target->name + "'");
                }
            }
        }

        for (std::size_t i = 0; i < expr->resolvedFields.size() && !complained; ++i) {
            if (expr->resolvedFields[i] == nullptr && present(target->fields[i])) {
                m_diagnostics.error(expr->location, "component '" + target->fields[i].displayName
                                                        + "' has no value in the aggregate");
            }
        }
        expr->type = expected;
        return expr->type;
    }

    bool hasNamed = false;
    bool hasPositional = false;
    bool hasOthers = false;
    std::vector<std::pair<long long, long long>> staticChoices;
    for (AggregateComponent& component : expr->components) {
        for (std::size_t i = 0; i < component.choiceLows.size(); ++i) {
            analyzeExpr(component.choiceLows[i].get(), scope, target->index);
            adaptUniversal(component.choiceLows[i].get(),
                           target->index != nullptr ? target->index : m_types.integerType());
            if (component.choiceHighs[i]) {
                analyzeExpr(component.choiceHighs[i].get(), scope, target->index);
                adaptUniversal(component.choiceHighs[i].get(),
                               target->index != nullptr ? target->index : m_types.integerType());
            }
        }
        if (target->arrayRank > 1 && component.value->kind != ExprKind::Aggregate
            && !(target->arrayRank == 2 && component.value->kind == ExprKind::StringLiteral)) {
            m_diagnostics.error(component.value->location, "multidimensional aggregate requires nested subaggregates");
        }
        Type* valueType = analyzeExpr(component.value.get(), scope, target->element);
        if (!typesCompatible(target->element, valueType)) {
            m_diagnostics.error(component.value->location, "aggregate component has an incompatible type");
        }
        adaptUniversal(component.value.get(), target->element);
    }
    for (std::size_t c = 0; c < expr->components.size(); ++c) {
        AggregateComponent& component = expr->components[c];
        if (component.isOthers) {
            if (hasOthers || c + 1 != expr->components.size()) {
                m_diagnostics.error(component.value->location, "others must appear once, as the final aggregate association");
            }
            hasOthers = true;
            continue;
        }
        if (component.choiceLows.empty()) {
            hasPositional = true;
            continue;
        }
        hasNamed = true;
        for (std::size_t i = 0; i < component.choiceLows.size(); ++i) {
            long long low = 0;
            long long high = 0;
            bool known = foldStatic(component.choiceLows[i].get(), low);
            if (component.choiceHighs[i]) {
                known = foldStatic(component.choiceHighs[i].get(), high) && known;
            } else {
                high = low;
            }
            if (!isDiscrete(component.choiceLows[i]->type)
                || !typesCompatible(target->index, component.choiceLows[i]->type)
                || (component.choiceHighs[i] && (!isDiscrete(component.choiceHighs[i]->type)
                    || !typesCompatible(target->index, component.choiceHighs[i]->type)))) {
                m_diagnostics.error(component.choiceLows[i]->location, "aggregate choice is incompatible with the index type");
            }
            if ((!known || high < low)
                && (expr->components.size() != 1 || component.choiceLows.size() != 1)) {
                m_diagnostics.error(component.choiceLows[i]->location,
                    "a dynamic or null aggregate choice must be the only choice");
            }
            if (known) {
                component.choiceLows[i]->staticValue = low;
                if (component.choiceHighs[i]) {
                    component.choiceHighs[i]->staticValue = high;
                }
                if (high >= low) {
                    staticChoices.emplace_back(low, high);
                }
            }
        }
    }
    if (hasNamed && hasPositional) {
        m_diagnostics.error(expr->location, "positional and named array associations cannot be mixed");
    }
    std::sort(staticChoices.begin(), staticChoices.end());
    for (std::size_t i = 1; i < staticChoices.size(); ++i) {
        if (staticChoices[i].first <= staticChoices[i - 1].second) {
            m_diagnostics.error(expr->location, "array aggregate choices overlap");
            break;
        }
        if (!hasOthers && staticChoices[i].first - 1 != staticChoices[i - 1].second) {
            m_diagnostics.error(expr->location, "array aggregate choices must cover a contiguous range");
            break;
        }
    }
    expr->type = expected;
    return expr->type;
}

// What a record aggregate writes for one of the discriminants, if it writes
// anything static there.  Discriminants come first among the components, so a
// positional aggregate names them by being long enough.
bool Sema::aggregateDiscriminant(AggregateExpr* expr, Type* record, Scope* scope, int index,
                                 long long& value, Expr** source)
{
    std::size_t positional = 0;

    for (AggregateComponent& component : expr->components) {
        if (component.isOthers) {
            break;
        }

        int which = -1;
        if (component.names.empty() || component.names.front().empty()) {
            which = static_cast<int>(positional++);
        } else {
            for (const FieldInfo& field : record->fields) {
                if (field.name == component.names.front()) {
                    which = field.index;
                    break;
                }
            }
        }
        if (which != index) {
            continue;
        }
        analyzeExpr(component.value.get(), scope, record->fields[static_cast<std::size_t>(index)].type);
        if (source != nullptr) {
            *source = component.value.get();
        }
        return foldStatic(component.value.get(), value);
    }
    return false;
}

// A record aggregate names the discriminants along with everything else, and
// what it names them has to be what the subtype fixed them to.  Answers whether
// the aggregate is worth reading any further.
bool Sema::checkAggregateDiscriminants(AggregateExpr* expr, Type* target, Scope* scope)
{
    Type* record = baseType(target);

    for (int which = 0; which < record->discriminantCount; ++which) {
        long long fixed = 0;
        long long given = 0;
        Expr* source = nullptr;
        if (!discriminantValue(target, which, fixed)
            || !aggregateDiscriminant(expr, record, scope, which, given, &source) || given == fixed) {
            continue;
        }
        const FieldInfo& discriminant = record->fields[static_cast<std::size_t>(which)];
        m_diagnostics.error(source->location, "'" + discriminant.displayName + "' was fixed at "
                                                  + describeValue(discriminant.type, fixed)
                                                  + " when the object was declared");
        return false;
    }
    return true;
}
