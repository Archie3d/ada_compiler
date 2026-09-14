#include "Type.h"

#include <limits>

Type* rootType(Type* type)
{
    while (type != nullptr && type->base != nullptr) {
        type = type->base;
    }
    return type;
}

Type* baseType(Type* type)
{
    while (type != nullptr && type->isSubtype && type->base != nullptr) {
        type = type->base;
    }
    return type;
}

bool isDiscrete(const Type* type)
{
    if (type == nullptr) {
        return false;
    }
    return type->kind == TypeKind::Integer || type->kind == TypeKind::Enumeration
           || type->kind == TypeKind::UniversalInteger;
}

bool isReal(const Type* type)
{
    if (type == nullptr) {
        return false;
    }
    return type->kind == TypeKind::Float || type->kind == TypeKind::UniversalReal;
}

bool isNumeric(const Type* type)
{
    if (type == nullptr) {
        return false;
    }
    return type->kind == TypeKind::Integer || type->kind == TypeKind::Float
           || type->kind == TypeKind::UniversalInteger || type->kind == TypeKind::UniversalReal;
}

bool isScalar(const Type* type)
{
    return isDiscrete(type) || (type != nullptr && (type->kind == TypeKind::Float || type->kind == TypeKind::Access
                                                    || type->kind == TypeKind::UniversalReal));
}

bool isComposite(const Type* type)
{
    return type != nullptr && (type->kind == TypeKind::Array || type->kind == TypeKind::Record);
}

bool needsZeroInit(const Type* type)
{
    if (type == nullptr) {
        return false;
    }
    if (type->needsZeroInit) {
        return true;
    }
    // An access object starts out null, so anything holding one has to be
    // cleared before it is used.
    if (type->kind == TypeKind::Access) {
        return true;
    }
    if (type->kind == TypeKind::Array) {
        return needsZeroInit(type->element);
    }
    if (type->kind == TypeKind::Record) {
        for (const FieldInfo& field : type->fields) {
            if (needsZeroInit(field.type)) {
                return true;
            }
        }
    }
    return false;
}

long long typeSize(const Type* type)
{
    if (type == nullptr) {
        return 0;
    }
    if (type->byteSize > 0) {
        return type->byteSize;
    }
    switch (type->kind) {
    case TypeKind::Void:
        return 0;
    case TypeKind::UniversalInteger:
        return 8;
    case TypeKind::UniversalReal:
        return 8;
    case TypeKind::Integer:
        return type->base != nullptr ? typeSize(type->base)
            : (type->low < -2147483648LL || type->high > 2147483647LL ? 8 : 4);
    case TypeKind::Enumeration:
        return type->literals.size() > 256 ? 4 : 1;
    case TypeKind::Float:
        // Six decimal digits are what a single precision number holds.
        return type->digits > 6 ? 8 : 4;
    case TypeKind::Access:
        return 8;
    case TypeKind::Array: {
        long long count = arrayLength(type);
        long long elementSize = typeSize(type->element);
        long long alignment = typeAlignment(type->element);
        if (elementSize % alignment != 0) {
            elementSize += alignment - elementSize % alignment;
        }
        return count * elementSize;
    }
    case TypeKind::Record: {
        long long size = 0;
        long long alignment = typeAlignment(type);
        for (const FieldInfo& field : type->fields) {
            // The alternatives of a variant part share their storage, so the
            // record reaches as far as the largest of them and no further.
            long long end = field.offset + typeSize(field.type);
            if (end > size) {
                size = end;
            }
        }
        if (alignment > 0 && size % alignment != 0) {
            size += alignment - size % alignment;
        }
        return size;
    }
    }
    return 0;
}

long long typeAlignment(const Type* type)
{
    if (type == nullptr) {
        return 1;
    }
    switch (type->kind) {
    case TypeKind::Array:
        return typeAlignment(type->element);
    case TypeKind::Record: {
        long long alignment = 1;
        for (const FieldInfo& field : type->fields) {
            long long fieldAlignment = typeAlignment(field.type);
            if (fieldAlignment > alignment) {
                alignment = fieldAlignment;
            }
        }
        return alignment;
    }
    default:
        return typeSize(type) == 0 ? 1 : typeSize(type);
    }
}

long long arrayLength(const Type* type)
{
    if (type == nullptr || type->kind != TypeKind::Array || !type->constrained) {
        return 0;
    }
    long long length = type->indexHigh - type->indexLow + 1;
    return length < 0 ? 0 : length;
}

char qbeClass(const Type* type)
{
    if (type == nullptr) {
        return 'w';
    }
    switch (type->kind) {
    case TypeKind::Float:
        return typeSize(type) == 8 ? 'd' : 's';
    case TypeKind::UniversalReal:
        return 'd';
    case TypeKind::Integer:
        return typeSize(type) == 8 ? 'l' : 'w';
    case TypeKind::UniversalInteger:
    case TypeKind::Access:
    case TypeKind::Array:
    case TypeKind::Record:
        return 'l';
    default:
        return 'w';
    }
}

const char* qbeLoadInstruction(const Type* type)
{
    switch (typeSize(type)) {
    case 1:
        return type->kind == TypeKind::Integer && type->low < 0 ? "loadsb" : "loadub";
    case 2:
        return type->kind == TypeKind::Integer && type->low < 0 ? "loadsh" : "loaduh";
    case 8:
        if (isReal(type)) {
            return "loadd";
        }
        return "loadl";
    default:
        if (isReal(type)) {
            return "loads";
        }
        return "loadsw";
    }
}

const char* qbeStoreInstruction(const Type* type)
{
    switch (typeSize(type)) {
    case 1:
        return "storeb";
    case 2:
        return "storeh";
    case 8:
        if (isReal(type)) {
            return "stored";
        }
        return "storel";
    default:
        if (isReal(type)) {
            return "stores";
        }
        return "storew";
    }
}

TypeTable::TypeTable()
{
    m_void = create(TypeKind::Void, "void");

    m_universalInteger = create(TypeKind::UniversalInteger, "universal_integer");
    m_universalInteger->low = std::numeric_limits<long long>::min();
    m_universalInteger->high = std::numeric_limits<long long>::max();

    m_universalReal = create(TypeKind::UniversalReal, "universal_real");

    m_integer = create(TypeKind::Integer, "Integer");
    m_integer->low = -2147483648LL;
    m_integer->high = 2147483647LL;

    m_longInteger = create(TypeKind::Integer, "Long_Integer");
    m_longInteger->low = std::numeric_limits<long long>::min();
    m_longInteger->high = std::numeric_limits<long long>::max();

    m_natural = makeSubtype("Natural", m_integer, 0, 2147483647LL);
    m_positive = makeSubtype("Positive", m_integer, 1, 2147483647LL);

    m_boolean = create(TypeKind::Enumeration, "Boolean");
    m_boolean->literals = { "false", "true" };
    m_boolean->low = 0;
    m_boolean->high = 1;

    m_character = create(TypeKind::Enumeration, "Character");
    m_character->low = 0;
    m_character->high = 255;

    m_float = create(TypeKind::Float, "Float");
    m_float->digits = 6;

    m_longFloat = create(TypeKind::Float, "Long_Float");
    m_longFloat->digits = 15;

    m_string = create(TypeKind::Array, "String");
    m_string->element = m_character;
    m_string->index = m_positive;
    m_string->constrained = false;
    m_string->indexLow = 1;
    m_string->indexHigh = 0;
}

Type* TypeTable::create(TypeKind kind, const std::string& name)
{
    m_types.push_back(std::make_unique<Type>(kind, name));
    return m_types.back().get();
}

Type* TypeTable::makeSubtype(const std::string& name, Type* parent, long long low, long long high)
{
    Type* subtype = create(parent->kind, name);
    subtype->base = parent;
    subtype->isSubtype = true;
    subtype->low = low;
    subtype->high = high;
    subtype->byteSize = parent->kind == TypeKind::Integer ? typeSize(parent) : parent->byteSize;
    subtype->needsZeroInit = parent->needsZeroInit;
    subtype->literals = parent->literals;
    subtype->digits = parent->digits;
    subtype->hasRealRange = parent->hasRealRange;
    subtype->lowReal = parent->lowReal;
    subtype->highReal = parent->highReal;
    subtype->arrayRank = parent->arrayRank;
    subtype->isArrayRow = parent->isArrayRow;
    subtype->element = parent->element;
    subtype->index = parent->index;
    subtype->indexLow = parent->indexLow;
    subtype->indexHigh = parent->indexHigh;
    subtype->constrained = parent->constrained;
    subtype->m_boundsSymbol = parent->m_boundsSymbol;
    subtype->fields = parent->fields;
    subtype->target = parent->target;
    subtype->privateTo = parent->privateTo;
    subtype->isLimited = parent->isLimited;
    subtype->discriminantCount = parent->discriminantCount;
    subtype->variantOn = parent->variantOn;
    subtype->variants = parent->variants;
    return subtype;
}

bool discriminantValueOf(const Type* type, int index, long long& value)
{
    for (const Type* walk = type; walk != nullptr; walk = walk->isSubtype ? walk->base : nullptr) {
        if (!walk->discriminantsKnown || index < 0
            || static_cast<std::size_t>(index) >= walk->discriminantValues.size()) {
            continue;
        }
        value = walk->discriminantValues[static_cast<std::size_t>(index)];
        return true;
    }
    return false;
}

int variantFor(const Type* record, long long discriminant)
{
    int fallback = -1;
    for (std::size_t v = 0; v < record->variants.size(); ++v) {
        if (record->variants[v].isOthers) {
            fallback = static_cast<int>(v);
            continue;
        }
        for (const VariantChoice& choice : record->variants[v].choices) {
            if (discriminant >= choice.low && discriminant <= choice.high) {
                return static_cast<int>(v);
            }
        }
    }
    return fallback;
}

bool TypeTable::isBoolean(const Type* type) const
{
    return rootType(const_cast<Type*>(type)) == m_boolean;
}

bool TypeTable::isCharacter(const Type* type) const
{
    return rootType(const_cast<Type*>(type)) == m_character;
}

bool TypeTable::isString(const Type* type) const
{
    const Type* root = rootType(const_cast<Type*>(type));
    return root != nullptr && root->kind == TypeKind::Array && isCharacter(root->element);
}

// The base subtype keeps type identity and representation, but drops the
// first subtype's constraint. Cache it on the type, not on each subtype.
Type* TypeTable::scalarBaseType(Type* type)
{
    type = baseType(type);
    if (type == nullptr || (!isDiscrete(type) && !isReal(type))) {
        return nullptr;
    }
    if (type->m_scalarBase != nullptr) {
        return type->m_scalarBase;
    }
    Type* result = makeSubtype(type->name + "'Base", type, type->low, type->high);
    if (type->kind == TypeKind::Integer) {
        bool wide = typeSize(type) == 8;
        result->low = wide ? std::numeric_limits<long long>::min() : -2147483648LL;
        result->high = wide ? std::numeric_limits<long long>::max() : 2147483647LL;
    } else if (type->kind == TypeKind::Float) {
        result->hasRealRange = false;
    }
    type->m_scalarBase = result;
    return result;
}
