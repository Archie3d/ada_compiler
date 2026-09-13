#include "Sema.h"
#include "SemaSupport.h"
#include <cstdint>
#include <limits>

using SemaSupport::isUniversal;
using SemaSupport::adaptUniversal;

void Sema::analyzeTypeDecl(TypeDecl* decl, Scope* scope)
{
    // A type already named here without being described is completed by this
    // declaration rather than shadowed by it, so that the access type which
    // pointed at it goes on pointing at the same thing.
    Type* completing = nullptr;
    for (Symbol* candidate : scope->lookupLocal(decl->lower)) {
        if (candidate->kind == SymbolKind::TypeName && candidate->type != nullptr
            && candidate->type->isIncomplete) {
            completing = candidate->type;
        }
    }

    if (decl->definition == nullptr) {
        if (completing != nullptr) {
            m_diagnostics.error(decl->location, "'" + decl->name + "' has already been named here");
            return;
        }
        Type* placeholder = m_types.create(TypeKind::Record, decl->name);
        placeholder->isIncomplete = true;
        decl->declaredType = placeholder;

        Symbol* named = m_symbolTable.createSymbol(SymbolKind::TypeName, decl->lower, decl->name);
        named->type = placeholder;
        named->location = decl->location;
        scope->add(named);
        return;
    }

    // Every branch below builds the type through this, so that a completion
    // fills in the object already handed out instead of making another.  What
    // the visible declaration settled - that the type is private, and to whom -
    // outlives the completion, since that is the whole point of it.
    auto makeType = [&](TypeKind kind) {
        if (completing == nullptr) {
            return m_types.create(kind, decl->name);
        }
        std::string name = completing->name;
        Symbol* privateTo = completing->privateTo;
        bool isLimited = completing->isLimited;
        *completing = Type(kind, name);
        completing->privateTo = privateTo;
        completing->isLimited = isLimited;
        return completing;
    };

    TypeDefinition* definition = decl->definition.get();
    Type* type = nullptr;

    switch (definition->kind) {
    case TypeDefKind::Enumeration: {
        type = makeType(TypeKind::Enumeration);
        type->low = 0;
        type->high = static_cast<long long>(definition->literals.size()) - 1;
        for (std::size_t i = 0; i < definition->literals.size(); ++i) {
            type->literals.push_back(definition->literalsLower[i]);
            Symbol* literal = m_symbolTable.createSymbol(SymbolKind::EnumerationLiteral,
                                                         definition->literalsLower[i], definition->literals[i]);
            literal->type = type;
            literal->enumerationValue = static_cast<long long>(i);
            scope->add(literal);
        }
        break;
    }
    case TypeDefKind::IntegerRange: {
        type = makeType(TypeKind::Integer);
        analyzeExpr(definition->rangeLow.get(), scope, m_types.integerType());
        analyzeExpr(definition->rangeHigh.get(), scope, m_types.integerType());
        long long low = 0;
        long long high = 0;
        if (!foldStatic(definition->rangeLow.get(), low) || !foldStatic(definition->rangeHigh.get(), high)) {
            m_diagnostics.error(definition->location, "the bounds of an integer type must be static");
        }
        type->low = low;
        type->high = high;
        break;
    }
    case TypeDefKind::FloatDigits: {
        type = makeType(TypeKind::Float);
        analyzeExpr(definition->digits.get(), scope, m_types.integerType());
        long long digits = 0;
        if (!foldStatic(definition->digits.get(), digits)) {
            m_diagnostics.error(definition->location, "the accuracy of a floating point type must be static");
            digits = 6;
        }
        if (digits < 1 || digits > 15) {
            m_diagnostics.error(definition->location, "a floating point type supports 1 to 15 digits");
            digits = digits < 1 ? 1 : 15;
        }
        type->digits = static_cast<int>(digits);

        if (definition->rangeLow && definition->rangeHigh) {
            Type* lowType = analyzeExpr(definition->rangeLow.get(), scope, type);
            Type* highType = analyzeExpr(definition->rangeHigh.get(), scope, type);
            double low = 0.0;
            double high = 0.0;
            if (!isReal(lowType) || !isReal(highType)) {
                m_diagnostics.error(definition->location, "the bounds of a floating point type must be real");
            } else if (!foldStaticReal(definition->rangeLow.get(), low)
                       || !foldStaticReal(definition->rangeHigh.get(), high)) {
                m_diagnostics.error(definition->location, "the bounds of a floating point type must be static");
            } else {
                type->hasRealRange = true;
                type->lowReal = low;
                type->highReal = high;
            }
        }
        break;
    }
    case TypeDefKind::Array: {
        type = makeType(TypeKind::Array);
        int rank = static_cast<int>(definition->indexTypes.size());
        Type* cell = resolveSubtypeIndication(definition->elementType.get(), scope);
        if (cell != nullptr && cell->kind == TypeKind::Array && !cell->constrained) {
            m_diagnostics.error(definition->elementType->location, "array components require a constrained array subtype");
        }
        Type* row = type;
        for (int dimension = 0; dimension < rank; ++dimension) {
            row->arrayRank = rank - dimension;
            row->constrained = !definition->unconstrainedIndexes;
            SubtypeIndication* index = definition->indexTypes[dimension].get();
            Type* indexType = index->name.empty() ? m_types.integerType()
                : resolveTypeName(index->lower, scope, index->location);
            row->index = indexType;
            if (!isDiscrete(indexType)) {
                m_diagnostics.error(index->location, "array indices require a discrete type");
            }
            if (index->rangeLow && index->rangeHigh) {
                analyzeExpr(index->rangeLow.get(), scope, indexType);
                analyzeExpr(index->rangeHigh.get(), scope, indexType);
                if (!foldStatic(index->rangeLow.get(), row->indexLow)
                    || !foldStatic(index->rangeHigh.get(), row->indexHigh)) {
                    m_diagnostics.error(index->location, "array index bounds must be static");
                }
            } else if (indexType != nullptr && row->constrained) {
                row->indexLow = indexType->low;
                row->indexHigh = indexType->high;
            }
            if (dimension + 1 == rank) {
                row->element = cell;
            } else {
                row->element = m_types.create(TypeKind::Array, anonymousTypeName());
                row = row->element;
                row->isArrayRow = true;
            }
        }
        if (rank > 1 && type->constrained) {
            long long size = typeSize(cell);
            std::vector<Type*> dimensions;
            for (Type* dimension = type; dimension != cell; dimension = dimension->element) {
                if (dimension->indexLow < std::numeric_limits<int>::min()
                    || dimension->indexHigh > std::numeric_limits<int>::max()
                    || dimension->indexLow > std::numeric_limits<int>::max()
                    || dimension->indexHigh < std::numeric_limits<int>::min()) {
                    m_diagnostics.error(definition->location, "multidimensional bounds must fit a 32-bit index");
                    return;
                }
                dimensions.push_back(dimension);
            }
            for (auto dimension = dimensions.rbegin(); dimension != dimensions.rend(); ++dimension) {
                if (__builtin_mul_overflow(size, arrayLength(*dimension), &size)) {
                    m_diagnostics.error(definition->location, "multidimensional array storage size is too large");
                    return;
                }
            }
        }
        break;
    }
    case TypeDefKind::Record: {
        type = makeType(TypeKind::Record);
        layoutRecord(decl, definition, type, scope);
        break;
    }
    case TypeDefKind::Derived: {
        Type* parent = resolveSubtypeIndication(definition->parent.get(), scope);
        if (parent == nullptr) {
            return;
        }
        Type* built = m_types.makeSubtype(decl->name, parent, parent->low, parent->high);
        built->isSubtype = false;
        built->base = nullptr;
        built->kind = parent->kind;
        built->literals = parent->literals;

        // Settled before the literals are made, so that each of them names the
        // type this declaration ends up being.
        type = makeType(parent->kind);
        std::string name = type->name;
        *type = *built;
        type->name = name;

        for (std::size_t i = 0; i < parent->literals.size(); ++i) {
            Symbol* literal = m_symbolTable.createSymbol(SymbolKind::EnumerationLiteral, parent->literals[i],
                                                         parent->literals[i]);
            literal->type = type;
            literal->enumerationValue = static_cast<long long>(i);
            scope->add(literal);
        }
        break;
    }
    case TypeDefKind::Access: {
        type = makeType(TypeKind::Access);
        type->target = resolveSubtypeIndication(definition->parent.get(), scope);
        break;
    }
    case TypeDefKind::Private: {
        if (m_packages.empty()) {
            m_diagnostics.error(decl->location, "a private type belongs in a package specification");
            return;
        }
        type = makeType(TypeKind::Record);
        type->isIncomplete = true;
        type->privateTo = m_packages.back();
        type->isLimited = definition->isLimited;
        break;
    }
    }

    if (type == nullptr) {
        return;
    }
    decl->declaredType = type;
    if (completing != nullptr) {
        // The name is already in the scope, standing for this very type.
        return;
    }

    Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::TypeName, decl->lower, decl->name);
    symbol->type = type;
    symbol->location = decl->location;
    scope->add(symbol);
}

void Sema::analyzeSubtypeDecl(SubtypeDecl* decl, Scope* scope)
{
    Type* base = resolveSubtypeIndication(decl->subtype.get(), scope);
    if (base == nullptr) {
        return;
    }
    Type* type = m_types.makeSubtype(decl->name, base, base->low, base->high);
    decl->declaredType = type;
    Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::TypeName, decl->lower, decl->name);
    symbol->type = type;
    symbol->location = decl->location;
    scope->add(symbol);
}

// Lays out a record: the discriminants first, then the components every value
// has, then the variant part.  The alternatives of a variant part all start at
// the same offset and share their storage, so the record has one size whichever
// discriminant it was made with.
void Sema::layoutRecord(TypeDecl* decl, TypeDefinition* definition, Type* type, Scope* scope)
{
    long long offset = 0;
    int fieldIndex = 0;

    auto place = [&](RecordField& field, int variantIndex, bool isDiscriminant) {
        for (const FieldInfo& seen : type->fields) {
            if (seen.name == field.lower) {
                m_diagnostics.error(field.location, "'" + field.name + "' is named twice in '" + type->name + "'");
                return;
            }
        }

        FieldInfo info;
        info.name = field.lower;
        info.displayName = field.name;
        info.type = resolveSubtypeIndication(field.subtype.get(), scope);
        if (info.type != nullptr && info.type->kind == TypeKind::Array && !info.type->constrained) {
            m_diagnostics.error(field.subtype->location, "record components require a constrained array subtype");
        }
        info.index = fieldIndex++;
        info.variant = variantIndex;
        info.isDiscriminant = isDiscriminant;
        if (field.defaultValue) {
            analyzeExpr(field.defaultValue.get(), scope, info.type);
            adaptUniversal(field.defaultValue.get(), info.type);
            info.defaultValue = field.defaultValue.get();
        }

        long long alignment = typeAlignment(info.type);
        if (alignment > 0 && offset % alignment != 0) {
            offset += alignment - offset % alignment;
        }
        info.offset = offset;
        offset += typeSize(info.type);
        type->fields.push_back(info);
    };

    for (RecordField& discriminant : decl->discriminants) {
        // A discriminant is fixed when the object is declared and never changes,
        // so it has no value of its own to fall back on.
        if (discriminant.defaultValue) {
            m_diagnostics.error(discriminant.location,
                                "a discriminant is fixed when the object is declared, so it takes no default");
            discriminant.defaultValue.reset();
        }
        place(discriminant, -1, true);
        Type* discriminantType = type->fields.back().type;
        if (!isDiscrete(discriminantType)) {
            m_diagnostics.error(discriminant.location, "a discriminant has to be discrete, and '"
                                                           + (discriminantType != nullptr ? discriminantType->name
                                                                                          : std::string("it"))
                                                           + "' is not");
        }
    }
    type->discriminantCount = static_cast<int>(type->fields.size());

    for (RecordField& field : definition->fields) {
        place(field, -1, false);
    }

    if (definition->variant == nullptr) {
        return;
    }

    VariantPart& part = *definition->variant;
    for (const FieldInfo& field : type->fields) {
        if (field.isDiscriminant && field.name == part.discriminantLower) {
            type->variantOn = field.index;
            part.discriminantIndex = field.index;
        }
    }
    if (type->variantOn < 0) {
        m_diagnostics.error(part.location,
                            "'" + part.discriminant + "' is not a discriminant of '" + type->name + "'");
        return;
    }

    // Every alternative starts where the components common to all of them left
    // off: only one alternative exists in any one value, so they share the
    // storage and the record keeps a single size.
    Type* selector = type->fields[type->variantOn].type;
    long long base = offset;
    std::vector<CaseChoice> covered;
    bool sawOthers = false;

    for (std::size_t v = 0; v < part.variants.size(); ++v) {
        RecordVariant& variant = part.variants[v];
        if (sawOthers) {
            m_diagnostics.error(variant.location, "'others' has to be the last alternative of a variant part");
        }

        VariantInfo info;
        info.isOthers = variant.isOthers;
        sawOthers = sawOthers || variant.isOthers;

        for (std::size_t k = 0; k < variant.choiceLows.size(); ++k) {
            CaseChoice choice;
            if (!resolveChoice(variant.choiceLows[k].get(), variant.choiceHighs[k].get(), selector, covered, scope,
                               choice)) {
                continue;
            }
            covered.push_back(choice);
            variant.choices.push_back(choice);
            info.choices.push_back(VariantChoice { choice.low, choice.high });
        }
        type->variants.push_back(info);

        offset = base;
        for (RecordField& field : variant.fields) {
            place(field, static_cast<int>(v), false);
        }
    }

    if (!sawOthers) {
        reportUncovered(covered, selector, part.location, "variant part");
    }
}

// A type may be named before it is described, but not left that way: an object
// of it could be declared with nothing to say how wide it is.  The visible part
// of a package is looked at only once its private part has had its say.
void Sema::reportIncompleteTypes(DeclList& declarations)
{
    for (const DeclPtr& decl : declarations) {
        if (decl->kind != DeclKind::Type) {
            continue;
        }
        auto* typeDecl = static_cast<TypeDecl*>(decl.get());
        if (typeDecl->declaredType != nullptr && typeDecl->declaredType->isIncomplete) {
            m_diagnostics.error(decl->location, "'" + typeDecl->name + "' is never described");
        }
    }
}

// A size clause settles how wide a type is laid out, which is the only way to
// say that a stream element occupies one byte rather than the four an integer
// type would otherwise take.
void Sema::analyzeRepresentation(RepresentationDecl* decl, Scope* scope)
{
    Symbol* symbol = lookupName(decl->lower, scope);
    if (symbol == nullptr || symbol->kind != SymbolKind::TypeName || symbol->type == nullptr) {
        m_diagnostics.error(decl->location, "'" + decl->name + "' is not a type declared here");
        return;
    }
    if (decl->attribute != "size") {
        m_diagnostics.error(decl->location, "only a 'Size clause is understood");
        return;
    }

    analyzeExpr(decl->value.get(), scope, m_types.integerType());
    if (!decl->value->isStatic) {
        m_diagnostics.error(decl->location, "a size clause needs a static number of bits");
        return;
    }

    long long bits = decl->value->staticValue;
    if (bits <= 0 || bits % 8 != 0 || bits > 64) {
        m_diagnostics.error(decl->location, "a size of " + std::to_string(bits)
                                                + " bits is not a whole number of storage units this machine can "
                                                  "address");
        return;
    }
    symbol->type->byteSize = static_cast<int>(bits / 8);
}

Type* Sema::resolveSubtypeIndication(SubtypeIndication* indication, Scope* scope, bool allowDynamic)
{
    if (indication == nullptr) {
        return nullptr;
    }
    Type* base = resolveTypeName(indication->lower, scope, indication->location);
    if (base == nullptr) {
        return nullptr;
    }
    indication->resolved = base;

    if (indication->digits) {
        analyzeExpr(indication->digits.get(), scope, m_types.integerType());
        long long digits = 0;
        if (!isReal(base)) {
            m_diagnostics.error(indication->location, "an accuracy constraint requires a floating point type");
        } else if (!foldStatic(indication->digits.get(), digits)) {
            m_diagnostics.error(indication->location, "an accuracy constraint must be static");
        } else if (digits < 1 || digits > base->digits) {
            m_diagnostics.error(indication->location,
                                "a subtype cannot ask for more digits than " + base->name + " provides");
        }
    }

    if (indication->rangeLow && indication->rangeHigh && isReal(base)) {
        analyzeExpr(indication->rangeLow.get(), scope, base);
        analyzeExpr(indication->rangeHigh.get(), scope, base);
        double low = 0.0;
        double high = 0.0;
        if (!foldStaticReal(indication->rangeLow.get(), low)
            || !foldStaticReal(indication->rangeHigh.get(), high)) {
            m_diagnostics.error(indication->location, "range constraints must be static");
            return base;
        }
        Type* subtype = m_types.makeSubtype(anonymousTypeName(), base, base->low, base->high);
        subtype->hasRealRange = true;
        subtype->lowReal = low;
        subtype->highReal = high;
        indication->resolved = subtype;
        return subtype;
    }

    if (indication->rangeLow && indication->rangeHigh) {
        analyzeExpr(indication->rangeLow.get(), scope, base);
        analyzeExpr(indication->rangeHigh.get(), scope, base);
        long long low = 0;
        long long high = 0;
        if (!foldStatic(indication->rangeLow.get(), low) || !foldStatic(indication->rangeHigh.get(), high)) {
            m_diagnostics.error(indication->location, "range constraints must be static");
            return base;
        }
        Type* subtype = m_types.makeSubtype(anonymousTypeName(), base, low, high);
        indication->resolved = subtype;
        return subtype;
    }

    if (!indication->indexLows.empty()) {
        Type* array = baseType(base);
        if (array != nullptr && array->kind == TypeKind::Record) {
            return constrainDiscriminants(indication, base, scope);
        }
        if (array == nullptr || array->kind != TypeKind::Array) {
            m_diagnostics.error(indication->location, "index constraints require an array type");
            return base;
        }
        if (indication->indexLows.size() != static_cast<std::size_t>(array->arrayRank)) {
            m_diagnostics.error(indication->location, "index constraint count must match the array rank");
            return base;
        }
        if (array->arrayRank > 1) {
            if (base->constrained) {
                m_diagnostics.error(indication->location, "an index constraint requires an unconstrained array subtype");
                return base;
            }
            Type* subtype = m_types.makeSubtype(anonymousTypeName(), base, base->low, base->high);
            Type* row = subtype;
            Type* original = base;
            bool allStatic = true;
            std::vector<Type*> rows;
            for (int dimension = 0; dimension < array->arrayRank; ++dimension) {
                Expr* low = indication->indexLows[dimension].get();
                Expr* high = indication->indexHighs[dimension].get();
                if (high == nullptr) {
                    m_diagnostics.error(indication->location, "array index constraints require ranges");
                    return base;
                }
                Type* lowType = analyzeExpr(low, scope, original->index);
                Type* highType = analyzeExpr(high, scope, original->index);
                if (!typesCompatible(original->index, lowType) || !typesCompatible(original->index, highType)) {
                    m_diagnostics.error(indication->location, "array index bounds have an incompatible type");
                }
                bool staticLow = foldStatic(low, row->indexLow);
                bool staticHigh = foldStatic(high, row->indexHigh);
                allStatic = allStatic && staticLow && staticHigh;
                rows.push_back(row);
                if (dimension + 1 < array->arrayRank) {
                    original = original->element;
                    row->element = m_types.makeSubtype(anonymousTypeName(), original, original->low, original->high);
                    row = row->element;
                }
            }
            if (!allStatic && !allowDynamic) {
                m_diagnostics.error(indication->location, "index constraints must be static");
                return base;
            }
            long long size = typeSize(row->element);
            for (auto axis = rows.rbegin(); axis != rows.rend(); ++axis) {
                (*axis)->constrained = allStatic;
                if (allStatic && ((*axis)->indexLow < INT32_MIN || (*axis)->indexLow > INT32_MAX
                    || (*axis)->indexHigh < INT32_MIN || (*axis)->indexHigh > INT32_MAX)) {
                    m_diagnostics.error(indication->location, "multidimensional bounds must fit a 32-bit index");
                    return base;
                }
                if (allStatic && __builtin_mul_overflow(size, arrayLength(*axis), &size)) {
                    m_diagnostics.error(indication->location, "multidimensional array storage size is too large");
                    return base;
                }
            }
            indication->resolved = subtype;
            return subtype;
        }
        analyzeExpr(indication->indexLows.front().get(), scope, array->index);
        long long low = 0;
        long long high = 0;
        bool ok = foldStatic(indication->indexLows.front().get(), low);
        if (indication->indexHighs.front()) {
            analyzeExpr(indication->indexHighs.front().get(), scope, array->index);
            ok = ok && foldStatic(indication->indexHighs.front().get(), high);
        }
        if (!ok && allowDynamic && indication->indexHighs.front() != nullptr) {
            if (!isDiscrete(indication->indexLows.front()->type)
                || !isDiscrete(indication->indexHighs.front()->type)) {
                m_diagnostics.error(indication->location, "array index bounds must be discrete");
                return base;
            }
            Type* subtype = m_types.makeSubtype(anonymousTypeName(), base, base->low, base->high);
            subtype->constrained = false;
            indication->resolved = subtype;
            return subtype;
        }
        if (!ok) {
            m_diagnostics.error(indication->location, "index constraints must be static");
            return base;
        }
        Type* subtype = m_types.makeSubtype(anonymousTypeName(), base, base->low, base->high);
        subtype->constrained = true;
        subtype->indexLow = low;
        subtype->indexHigh = high;
        indication->resolved = subtype;
        return subtype;
    }

    return base;
}

// 'Shape (Circle)': fixes the discriminants of a record subtype, which settles
// which variant a value of it has and so which components it carries.
Type* Sema::constrainDiscriminants(SubtypeIndication* indication, Type* base, Scope* scope)
{
    Type* record = baseType(base);
    if (record->discriminantCount == 0) {
        m_diagnostics.error(indication->location, "'" + record->name + "' has no discriminants to constrain");
        return base;
    }
    if (static_cast<int>(indication->indexLows.size()) != record->discriminantCount) {
        m_diagnostics.error(indication->location, "'" + record->name + "' has "
                                                      + std::to_string(record->discriminantCount)
                                                      + " discriminants, so that many values are expected");
        return base;
    }

    Type* subtype = m_types.makeSubtype(anonymousTypeName(), base, base->low, base->high);
    subtype->name = record->name;
    subtype->discriminantsKnown = true;

    for (int i = 0; i < record->discriminantCount; ++i) {
        Expr* value = indication->indexLows[static_cast<std::size_t>(i)].get();
        if (indication->indexHighs[static_cast<std::size_t>(i)] != nullptr) {
            m_diagnostics.error(value->location, "a discriminant takes one value, not a range");
        }
        Type* discriminantType = record->fields[static_cast<std::size_t>(i)].type;
        analyzeExpr(value, scope, discriminantType);

        long long fixed = 0;
        if (!foldStatic(value, fixed)) {
            m_diagnostics.error(value->location, "a discriminant has to be fixed with a static value");
            subtype->discriminantsKnown = false;
        } else if (discriminantType != nullptr && (fixed < discriminantType->low || fixed > discriminantType->high)) {
            m_diagnostics.error(value->location, describeValue(discriminantType, fixed) + " is not a value '"
                                                     + record->fields[static_cast<std::size_t>(i)].name
                                                     + "' can take");
        }
        subtype->discriminantValues.push_back(fixed);
    }

    indication->resolved = subtype;
    return subtype;
}

// Whether a constraint somewhere along the subtype chain has fixed the
// discriminants of the record.
bool Sema::hasKnownDiscriminants(Type* type) const
{
    for (Type* walk = type; walk != nullptr; walk = walk->isSubtype ? walk->base : nullptr) {
        if (walk->discriminantsKnown) {
            return true;
        }
    }
    return false;
}

// What a fixed discriminant of the record was fixed to, if anything.
bool Sema::discriminantValue(Type* type, int index, long long& value) const
{
    return discriminantValueOf(type, index, value);
}

// The alternative a record subtype's fixed discriminant selects, or -1 when
// nothing has fixed it.
int Sema::knownVariant(Type* type) const
{
    Type* record = baseType(type);
    if (record == nullptr || record->variantOn < 0) {
        return -1;
    }
    long long fixed = 0;
    if (!discriminantValue(type, record->variantOn, fixed)) {
        return -1;
    }
    return variantFor(record, fixed);
}

bool Sema::typesCompatible(Type* target, Type* source) const
{
    if (target == nullptr || source == nullptr) {
        return true;
    }
    Type* left = rootType(target);
    Type* right = rootType(source);
    if (left == right) {
        return true;
    }
    if (isUniversal(left) && isUniversal(right)) {
        return left->kind == right->kind;
    }
    if (isUniversal(left) || isUniversal(right)) {
        // Ada keeps the two families apart: a whole number literal never stands
        // for a real value, and the other way round.
        Type* universal = isUniversal(left) ? left : right;
        Type* concrete = isUniversal(left) ? right : left;
        if (universal->kind == TypeKind::UniversalInteger) {
            return concrete->kind == TypeKind::Integer;
        }
        return concrete->kind == TypeKind::Float;
    }
    if (left->kind == TypeKind::Array && right->kind == TypeKind::Array) {
        return left->arrayRank == right->arrayRank && rootType(left->element) == rootType(right->element);
    }
    return false;
}
