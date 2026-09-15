#include "QbeEmitter.h"
#include "QbeSupport.h"

using QbeSupport::constantValue;
using QbeSupport::realLiteral;
using QbeSupport::isFloatClass;
using QbeSupport::comparisonInstruction;

Value QbeEmitter::emitExpr(Expr* expr)
{
    if (expr == nullptr) {
        return Value { "0", 'w' };
    }

    if (expr->isStatic && isDiscrete(baseType(expr->type))) {
        // Check before narrowing the literal to a QBE word. Subtype bounds
        // are checked at value boundaries, not on intermediate expressions.
        if (qbeClass(expr->type) == 'w'
            && (expr->staticValue < -2147483648LL || expr->staticValue > 2147483647LL)) {
            raiseConstraintError();
        }
        return constantValue(expr->staticValue, qbeClass(expr->type));
    }
    if (expr->isStatic && isReal(expr->type)) {
        char type = qbeClass(expr->type);
        return Value { realLiteral(expr->staticReal, type), type };
    }

    switch (expr->kind) {
    case ExprKind::IntegerLiteral:
        return constantValue(static_cast<IntegerLiteralExpr*>(expr)->value, qbeClass(expr->type));
    case ExprKind::CharacterLiteral:
        return constantValue(static_cast<unsigned char>(static_cast<CharacterLiteralExpr*>(expr)->value), 'w');
    case ExprKind::RealLiteral: {
        char type = qbeClass(expr->type);
        return Value { realLiteral(static_cast<RealLiteralExpr*>(expr)->value, type), type };
    }
    case ExprKind::StringLiteral: {
        auto* literal = static_cast<StringLiteralExpr*>(expr);
        return Value { stringData(literal->value), 'l', "1", std::to_string(literal->value.size()) };
    }
    case ExprKind::Null:
        return Value { "0", 'l' };

    case ExprKind::Identifier: {
        auto* identifier = static_cast<IdentifierExpr*>(expr);
        Symbol* symbol = identifier->symbol;
        if (symbol == nullptr) {
            return Value { "0", 'w' };
        }
        if (symbol->kind == SymbolKind::Subprogram) {
            CallExpr call;
            call.location = expr->location;
            call.form = CallForm::Subprogram;
            call.subprogram = symbol;
            call.type = symbol->returnType;
            return emitCall(&call);
        }
        if (symbol->kind == SymbolKind::EnumerationLiteral) {
            return constantValue(symbol->enumerationValue, qbeClass(symbol->type));
        }
        if (symbol->kind == SymbolKind::Number) {
            if (isReal(symbol->type)) {
                char type = qbeClass(symbol->type);
                return Value { realLiteral(symbol->staticReal, type), type };
            }
            return constantValue(symbol->staticValue, qbeClass(symbol->type));
        }
        if (symbol->kind == SymbolKind::TypeName && symbol->type->m_boundsSymbol != nullptr) {
            return boundsFor(symbol->type->m_boundsSymbol);
        }
        Value address = addressOf(symbol);
        if (isComposite(symbol->type)) {
            return withBounds(address, symbol->type, symbol);
        }
        return loadFrom(address, symbol->type);
    }

    case ExprKind::Selected: {
        auto* selected = static_cast<SelectedExpr*>(expr);
        Symbol* symbol = selected->symbol;
        if (symbol != nullptr && symbol->kind == SymbolKind::Subprogram) {
            CallExpr call;
            call.location = expr->location;
            call.form = CallForm::Subprogram;
            call.subprogram = symbol;
            call.type = symbol->returnType;
            return emitCall(&call);
        }
        if (symbol != nullptr && symbol->kind == SymbolKind::EnumerationLiteral) {
            return constantValue(symbol->enumerationValue, qbeClass(symbol->type));
        }
        if (symbol != nullptr && symbol->kind == SymbolKind::TypeName && symbol->type->m_boundsSymbol != nullptr) {
            return boundsFor(symbol->type->m_boundsSymbol);
        }
        Value address = emitAddress(expr);
        if (isComposite(expr->type)) {
            return withBounds(address, expr->type, selected->symbol);
        }
        return loadFrom(address, expr->type);
    }

    case ExprKind::Allocator:
        return emitAllocator(static_cast<AllocatorExpr*>(expr));

    case ExprKind::Call: {
        auto* call = static_cast<CallExpr*>(expr);
        if (call->form == CallForm::Subprogram) {
            return emitCall(call);
        }
        if (call->form == CallForm::Slice) {
            return emitSlice(call);
        }
        if (call->form == CallForm::Indexing) {
            Value address = emitAddress(expr);
            if (isComposite(expr->type)) {
                return withBounds(address, expr->type, nullptr);
            }
            return loadFrom(address, expr->type);
        }
        if (call->form == CallForm::Conversion) {
            Expr* operand = call->resolvedArguments.front();
            Value value = emitExpr(operand);
            char from = value.type;
            char to = qbeClass(expr->type);
            if (from == to) {
                emitRangeCheck(value, expr->type, expr->location);
                return value;
            }
            std::string temp = newTemp();
            if (isFloatClass(from) && !isFloatClass(to)) {
                // Ada rounds when a real value becomes an integer, while the
                // conversion instructions of the backend truncate.
                std::string wide = value.name;
                if (from == 's') {
                    wide = newTemp();
                    line(wide + " =d exts " + value.name);
                }
                std::string rounded = newTemp();
                line(rounded + " =l call $__ada_round_to_integer(d " + wide + ")");
                emitExceptionCheck();
                emitRangeCheck(Value { rounded, 'l' }, expr->type, expr->location);
                line(temp + " =" + std::string(1, to) + " copy " + rounded);
            } else {
                std::string instruction;
                if (!isFloatClass(from) && isFloatClass(to)) {
                    instruction = from == 'w' ? "swtof" : "sltof";
                } else if (from == 's' && to == 'd') {
                    instruction = "exts";
                } else if (from == 'd' && to == 's') {
                    instruction = "truncd";
                } else if (from == 'w' && to == 'l') {
                    instruction = "extsw";
                } else {
                    emitRangeCheck(value, expr->type, expr->location);
                    instruction = "copy";
                }
                line(temp + " =" + std::string(1, to) + " " + instruction + " " + value.name);
            }
            Value result { temp, to };
            emitRangeCheck(result, expr->type, expr->location);
            return result;
        }
        m_diagnostics.error(expr->location, "unsupported call form");
        return Value { "0", 'w' };
    }

    case ExprKind::Binary:
        return emitBinary(static_cast<BinaryExpr*>(expr));
    case ExprKind::Unary:
        return emitUnary(static_cast<UnaryExpr*>(expr));
    case ExprKind::Attribute:
        return emitAttribute(static_cast<AttributeExpr*>(expr));
    case ExprKind::Aggregate:
        return emitAggregate(static_cast<AggregateExpr*>(expr));
    case ExprKind::Qualified: {
        Value value = emitExpr(static_cast<QualifiedExpr*>(expr)->operand.get());
        if (expr->type->m_scalarBoundsSymbol != nullptr) {
            emitRangeCheck(value, expr->type, expr->location);
        }
        return value;
    }

    case ExprKind::Membership: {
        auto* membership = static_cast<MembershipExpr*>(expr);
        Value operand = emitExpr(membership->operand.get());
        Value low = emitExpr(membership->low.get());
        Value high = emitExpr(membership->high.get());
        std::string lowTest = newTemp();
        std::string highTest = newTemp();
        std::string combined = newTemp();
        line(lowTest + " =w " + comparisonInstruction(BinaryOp::GreaterEqual, operand.type) + " " + operand.name
             + ", " + low.name);
        line(highTest + " =w " + comparisonInstruction(BinaryOp::LessEqual, operand.type) + " " + operand.name
             + ", " + high.name);
        line(combined + " =w and " + lowTest + ", " + highTest);
        if (!membership->negated) {
            return Value { combined, 'w' };
        }
        std::string negated = newTemp();
        line(negated + " =w ceqw " + combined + ", 0");
        return Value { negated, 'w' };
    }
    }

    return Value { "0", 'w' };
}

Value QbeEmitter::emitAddress(Expr* expr)
{
    switch (expr->kind) {
    case ExprKind::Identifier: {
        auto* identifier = static_cast<IdentifierExpr*>(expr);
        if (identifier->symbol != nullptr
            && (identifier->symbol->kind == SymbolKind::Object
                || identifier->symbol->kind == SymbolKind::Parameter
                || identifier->symbol->kind == SymbolKind::LoopParameter)) {
            return addressOf(identifier->symbol);
        }
        break;
    }
    case ExprKind::Selected: {
        auto* selected = static_cast<SelectedExpr*>(expr);
        if (selected->symbol != nullptr
            && (selected->symbol->kind == SymbolKind::Object || selected->symbol->kind == SymbolKind::Parameter)) {
            return addressOf(selected->symbol);
        }
        if (selected->isDereference) {
            // The designated object lives where the access value points.
            Value pointer = emitExpr(selected->prefix.get());
            checkNotNull(pointer);
            return pointer;
        }
        if (selected->fieldIndex >= 0) {
            Value base = emitAddress(selected->prefix.get());
            Type* record = baseType(selected->prefix->type);
            if (record != nullptr && record->kind == TypeKind::Access) {
                base = loadFrom(base, record);
                checkNotNull(base);
                record = record->target;
            }
            record = baseType(record);
            if (selected->checkedVariant >= 0) {
                checkVariant(base, record, selected->checkedVariant);
            }
            long long offset = record->fields[selected->fieldIndex].offset;
            if (offset == 0) {
                return base;
            }
            std::string address = newTemp();
            line(address + " =l add " + base.name + ", " + std::to_string(offset));
            return Value { address, 'l' };
        }
        break;
    }
    case ExprKind::Call: {
        auto* call = static_cast<CallExpr*>(expr);
        if (call->form == CallForm::Slice) {
            return emitSlice(call);
        }
        if (call->form == CallForm::Indexing) {
            Value base = emitExpr(call->callee.get());
            Type* array = baseType(call->callee->type);
            if (array != nullptr && array->kind == TypeKind::Access) {
                // The access value is already the address of the array.
                checkNotNull(base);
                array = baseType(array->target);
            }
            if (array->arrayRank > 1) {
                Value address = base;
                Type* dimension = array;
                Value axis = base;
                for (Expr* argument : call->resolvedArguments) {
                    Value index = emitExpr(argument);
                    std::string wide = index.name;
                    if (index.type != 'l') {
                        wide = newTemp();
                        line(wide + " =l extsw " + index.name);
                    }
                    std::string lowBound = newTemp();
                    std::string highBound = newTemp();
                    line(lowBound + " =l extsw " + axis.first);
                    line(highBound + " =l extsw " + axis.last);
                    std::string low = newTemp();
                    std::string high = newTemp();
                    std::string valid = newTemp();
                    line(low + " =w csgel " + wide + ", " + lowBound);
                    line(high + " =w cslel " + wide + ", " + highBound);
                    line(valid + " =w and " + low + ", " + high);
                    std::string ok = newLabel("matrixindexok");
                    std::string bad = newLabel("matrixindexbad");
                    branch(Value { valid, 'w' }, ok, bad);
                    label(bad);
                    raiseConstraintError();
                    label(ok);
                    std::string offset = newTemp();
                    std::string scaled = newTemp();
                    std::string next = newTemp();
                    std::string elementSize = arrayElementSize(axis, dimension);
                    line(offset + " =l sub " + wide + ", " + lowBound);
                    line(scaled + " =l mul " + offset + ", " + elementSize);
                    line(next + " =l add " + address.name + ", " + scaled);
                    address = Value { next, 'l' };
                    axis = arrayRow(axis, dimension);
                    dimension = dimension->element;
                }
                return address;
            }
            Value index = emitExpr(call->resolvedArguments.front());
            long long elementSize = typeSize(array->element);
            std::string lowBound = array->constrained
                                       ? std::to_string(array->indexLow)
                                       : (base.hasBounds() ? base.first : std::string("1"));

            if (!array->constrained && base.hasBounds()) {
                std::string wideIndex = index.name;
                if (index.type != 'l') {
                    wideIndex = newTemp();
                    line(wideIndex + " =l extsw " + index.name);
                }
                std::string first = newTemp();
                std::string last = newTemp();
                line(first + " =l extsw " + base.first);
                line(last + " =l extsw " + base.last);
                std::string low = newTemp();
                std::string high = newTemp();
                std::string valid = newTemp();
                line(low + " =w csgel " + wideIndex + ", " + first);
                line(high + " =w cslel " + wideIndex + ", " + last);
                line(valid + " =w and " + low + ", " + high);
                std::string ok = newLabel("indexok");
                std::string bad = newLabel("indexbad");
                branch(Value { valid, 'w' }, ok, bad);
                label(bad);
                raiseConstraintError();
                label(ok);
            }
            std::string offset = newTemp();
            line(offset + " =w sub " + index.name + ", " + lowBound);
            std::string wide = newTemp();
            line(wide + " =l extsw " + offset);
            std::string scaled = newTemp();
            line(scaled + " =l mul " + wide + ", " + std::to_string(elementSize));
            std::string address = newTemp();
            line(address + " =l add " + base.name + ", " + scaled);
            return Value { address, 'l' };
        }
        break;
    }
    default:
        break;
    }

    if (isComposite(baseType(expr->type))) {
        return emitExpr(expr);
    }

    m_diagnostics.error(expr->location, "this expression does not designate an object");
    return Value { "0", 'l' };
}

Value QbeEmitter::emitAllocator(AllocatorExpr* expr)
{
    Type* designated = expr->designated;
    long long size = typeSize(designated);

    // The run time hands back cleared storage, so an access component of the
    // new object starts out null even when no value is given.
    std::string pointer = newTemp();
    line(pointer + " =l call $__ada_allocate(l " + std::to_string(size) + ")");
    emitExceptionCheck();

    Value address { pointer, 'l' };
    if (expr->value != nullptr) {
        assignInto(address, designated, expr->value.get());
    } else {
        emitDefaultInit(address, designated);
    }
    return address;
}

// Whether anything inside the type carries a value of its own to start from.
bool QbeEmitter::hasComponentDefaults(Type* type)
{
    Type* base = baseType(type);
    if (base == nullptr) {
        return false;
    }
    if (base->kind == TypeKind::Array) {
        return hasComponentDefaults(type->element);
    }
    if (base->kind != TypeKind::Record) {
        return false;
    }
    // A discriminant the subtype fixed is written into the object too, so that
    // reading it back gives what the declaration said.
    long long fixed = 0;
    for (int i = 0; i < base->discriminantCount; ++i) {
        if (discriminantValueOf(type, i, fixed)) {
            return true;
        }
    }
    for (const FieldInfo& field : base->fields) {
        if (field.defaultValue != nullptr || hasComponentDefaults(field.type)) {
            return true;
        }
    }
    return false;
}

// Gives an object the values its component declarations named.  Anything a
// component says nothing about is left as it was found, which for storage from
// an allocator means cleared.
void QbeEmitter::emitDefaultInit(const Value& address, Type* type)
{
    Type* base = baseType(type);
    if (base == nullptr || !hasComponentDefaults(type)) {
        return;
    }

    if (type->kind == TypeKind::Array && !type->constrained) {
        emitArrayFill(address, type, nullptr);
        return;
    }
    if (base->kind == TypeKind::Array) {
        base = type;
        long long count = arrayLength(base);
        long long elementSize = typeSize(base->element);
        for (long long i = 0; i < count; ++i) {
            std::string slot = newTemp();
            line(slot + " =l add " + address.name + ", " + std::to_string(i * elementSize));
            emitDefaultInit(Value { slot, 'l' }, base->element);
        }
        return;
    }

    long long discriminant = 0;
    bool fixedVariant = base->variantOn >= 0 && discriminantValueOf(type, base->variantOn, discriminant);
    int activeVariant = fixedVariant ? variantFor(base, discriminant) : -1;
    for (const FieldInfo& field : base->fields) {
        // Inactive alternatives share storage with the active one. Their
        // defaults must neither write that storage nor evaluate side effects.
        if (fixedVariant && field.variant >= 0 && field.variant != activeVariant) {
            continue;
        }
        Value slot = address;
        if (field.offset != 0) {
            std::string moved = newTemp();
            line(moved + " =l add " + address.name + ", " + std::to_string(field.offset));
            slot = Value { moved, 'l' };
        }
        long long fixed = 0;
        if (field.isDiscriminant && discriminantValueOf(type, field.index, fixed)) {
            storeInto(slot, constantValue(fixed, qbeClass(field.type)), field.type);
        } else if (field.defaultValue != nullptr) {
            assignInto(slot, field.type, field.defaultValue);
        } else {
            emitDefaultInit(slot, field.type);
        }
    }
}
