#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

enum class TypeKind
{
    Void,
    UniversalInteger,
    UniversalReal,
    Integer,
    Enumeration,
    Float,
    Array,
    Record,
    Access
};

class Type;
struct Symbol;

struct FieldInfo
{
    std::string name;          // Lowered, which is how a selection looks it up.
    std::string displayName;   // As written, which is how a message spells it.
    Type* type = nullptr;
    int index = 0;
    long long offset = 0;

    // What the component declaration gives it, used when an object of the
    // record is made without a value of its own.
    struct Expr* defaultValue = nullptr;

    // A leading component named in the discriminant part, fixed when an object
    // of the type is declared.
    bool isDiscriminant = false;

    // Which alternative of the variant part the component belongs to, or -1
    // when every value of the record has it.
    int variant = -1;
};

// One value the discriminant may hold to select a variant.  A single value is
// a range of one.
struct VariantChoice
{
    long long low = 0;
    long long high = 0;
};

// One alternative of a variant part.  Its components share their storage with
// those of every other alternative, since only one of them exists at a time.
struct VariantInfo
{
    std::vector<VariantChoice> choices;
    bool isOthers = false;
};

class Type
{
public:
    Type(TypeKind typeKind, std::string typeName)
        : kind(typeKind)
        , name(std::move(typeName))
    {
    }

    TypeKind kind;
    std::string name;

    // A subtype or derived type points at the type it was built from.
    Type* base = nullptr;
    bool isSubtype = false;
    Type* m_scalarBase = nullptr;

    // Named but not yet described.  'type Node;' declares one so that an access
    // type can point at a record declared further down, and the full definition
    // fills this very object in, which keeps every pointer already taken good.
    bool isIncomplete = false;

    // Set for a type declared private: what it is made of is only in reach
    // inside the package that declared it.  A limited one lends not even
    // assignment or equality to the outside.
    Symbol* privateTo = nullptr;
    bool isLimited = false;

    // Discriminants and the variant part they may select.  The discriminants
    // are the first components of the record; variantOn is the index of the
    // one the variant part switches on, or -1 when there is no variant part.
    int discriminantCount = 0;
    int variantOn = -1;
    std::vector<VariantInfo> variants;

    // Set on a subtype whose discriminants a constraint has pinned down, as in
    // 'subtype Round is Shape (Circle)'.
    bool discriminantsKnown = false;
    std::vector<long long> discriminantValues;

    // Discrete types (integer and enumeration).
    long long low = 0;
    long long high = 0;

    // Enumeration types.
    std::vector<std::string> literals;

    // Floating point types.  The requested number of decimal digits selects the
    // machine representation, and the bounds are kept separately from the
    // discrete ones because they do not fit in an integer.
    int digits = 0;
    bool hasRealRange = false;
    double lowReal = 0.0;
    double highReal = 0.0;

    // Array types.
    // Multidimensional arrays use nested row layouts internally.
    int arrayRank = 1;
    bool isArrayRow = false;
    Type* element = nullptr;
    Type* index = nullptr;
    long long indexLow = 0;
    long long indexHigh = 0;
    bool constrained = true;

    // A dynamically constrained declaration keeps its bounds in its owner's
    // activation. `constrained` continues to mean a static storage layout.
    Symbol* m_boundsSymbol = nullptr;
    std::vector<std::pair<struct Expr*, struct Expr*>> m_boundExpressions;

    // Record types.
    std::vector<FieldInfo> fields;

    // Access types.
    Type* target = nullptr;

    // Objects of this type start out cleared.  A File_Type has to, because the
    // run time reads the handle before anything has opened it.
    bool needsZeroInit = false;

    // A width the type carries instead of the one its kind implies, which is
    // how a Stream_Element comes out a byte wide.
    long long byteSize = 0;
};

// True when an object of this type, or one holding it, must be cleared.
bool needsZeroInit(const Type* type);

// The root of a subtype or derived type chain.
Type* rootType(Type* type);

// Which alternative of a variant part a discriminant value picks, or -1 when
// it picks none.
int variantFor(const Type* record, long long discriminant);

// What a constraint along the subtype chain fixed a discriminant to, if
// anything did.
bool discriminantValueOf(const Type* type, int index, long long& value);

// The type that carries the value representation (skips subtypes only).
Type* baseType(Type* type);

bool isDiscrete(const Type* type);
bool isReal(const Type* type);
bool isNumeric(const Type* type);
bool isScalar(const Type* type);
bool isComposite(const Type* type);

long long typeSize(const Type* type);
long long typeAlignment(const Type* type);

// The QBE base type letter used for values of this type.
char qbeClass(const Type* type);

// The QBE load/store instruction suffix for values of this type.
const char* qbeLoadInstruction(const Type* type);
const char* qbeStoreInstruction(const Type* type);

long long arrayLength(const Type* type);

class TypeTable
{
public:
    TypeTable();

    Type* create(TypeKind kind, const std::string& name);
    Type* makeSubtype(const std::string& name, Type* parent, long long low, long long high);

    Type* scalarBaseType(Type* type);

    Type* voidType() const { return m_void; }
    Type* universalInteger() const { return m_universalInteger; }
    Type* universalReal() const { return m_universalReal; }
    Type* integerType() const { return m_integer; }
    Type* longIntegerType() const { return m_longInteger; }
    Type* naturalType() const { return m_natural; }
    Type* positiveType() const { return m_positive; }
    Type* booleanType() const { return m_boolean; }
    Type* characterType() const { return m_character; }
    Type* floatType() const { return m_float; }
    Type* longFloatType() const { return m_longFloat; }
    Type* stringType() const { return m_string; }

    bool isBoolean(const Type* type) const;
    bool isCharacter(const Type* type) const;
    bool isString(const Type* type) const;

private:
    std::vector<std::unique_ptr<Type>> m_types;
    Type* m_void = nullptr;
    Type* m_universalInteger = nullptr;
    Type* m_universalReal = nullptr;
    Type* m_integer = nullptr;
    Type* m_longInteger = nullptr;
    Type* m_natural = nullptr;
    Type* m_positive = nullptr;
    Type* m_boolean = nullptr;
    Type* m_character = nullptr;
    Type* m_float = nullptr;
    Type* m_longFloat = nullptr;
    Type* m_string = nullptr;
};
