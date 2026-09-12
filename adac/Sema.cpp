#include "Sema.h"

#include "Lexer.h"
#include "Parser.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <limits>

namespace
{

bool isUniversal(const Type* type)
{
    return type != nullptr
           && (type->kind == TypeKind::UniversalInteger || type->kind == TypeKind::UniversalReal);
}

// Gives literals and expressions built purely from literals the type imposed by
// their context.
void adaptUniversal(Expr* expr, Type* type)
{
    if (expr == nullptr || type == nullptr || !isUniversal(expr->type)) {
        return;
    }
    expr->type = type;
    if (expr->kind == ExprKind::Unary) {
        adaptUniversal(static_cast<UnaryExpr*>(expr)->operand.get(), type);
    } else if (expr->kind == ExprKind::Binary) {
        auto* binary = static_cast<BinaryExpr*>(expr);
        adaptUniversal(binary->left.get(), type);
        adaptUniversal(binary->right.get(), type);
    }
}

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

std::vector<std::string> splitDottedName(const std::string& name)
{
    std::vector<std::string> parts;
    std::string current;
    for (char c : name) {
        if (c == '.') {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.push_back(current);
    return parts;
}

}

Sema::Sema(Diagnostics& diagnostics)
    : m_diagnostics(diagnostics)
{
    setupStandardScope();
}

// ---------------------------------------------------------------------------
// Predefined environment
// ---------------------------------------------------------------------------

void Sema::setupStandardScope()
{
    m_standardScope = m_symbolTable.createScope(nullptr);
    m_globalScope = m_symbolTable.createScope(m_standardScope);

    auto addType = [&](Type* type) {
        Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::TypeName, toLower(type->name), type->name);
        symbol->type = type;
        m_standardScope->add(symbol);
    };

    addType(m_types.integerType());
    addType(m_types.longIntegerType());
    addType(m_types.naturalType());
    addType(m_types.positiveType());
    addType(m_types.booleanType());
    addType(m_types.characterType());
    addType(m_types.floatType());
    addType(m_types.longFloatType());
    addType(m_types.stringType());

    const char* booleanLiterals[] = { "False", "True" };
    for (int i = 0; i < 2; ++i) {
        Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::EnumerationLiteral, toLower(booleanLiterals[i]),
                                                    booleanLiterals[i]);
        symbol->type = m_types.booleanType();
        symbol->enumerationValue = i;
        m_standardScope->add(symbol);
    }

    // System describes the machine rather than the language.  Address is what
    // 'Address yields, so the compiler has to know the type whether or not a
    // program ever names the package.
    Symbol* system = m_symbolTable.createSymbol(SymbolKind::Package, "system", "System");
    system->scope = m_symbolTable.createScope(nullptr);
    m_standardScope->add(system);

    m_addressType = m_types.create(TypeKind::Access, "Address");
    addTypeTo(system->scope, m_addressType);

    Symbol* storageUnit = m_symbolTable.createSymbol(SymbolKind::Number, "storage_unit", "Storage_Unit");
    storageUnit->type = m_types.integerType();
    storageUnit->hasStaticValue = true;
    storageUnit->staticValue = 8;
    system->scope->add(storageUnit);

    addException(m_standardScope, "Constraint_Error");
    addException(m_standardScope, "Program_Error");
    addException(m_standardScope, "Storage_Error");
    addException(m_standardScope, "Numeric_Error");
    addException(m_standardScope, "Tasking_Error");
}

Symbol* Sema::addException(Scope* scope, const std::string& displayName)
{
    Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::Exception, toLower(displayName), displayName);
    symbol->exceptionId = m_exceptionCounter++;
    scope->add(symbol);
    return symbol;
}

Symbol* Sema::addTypeTo(Scope* scope, Type* type)
{
    Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::TypeName, toLower(type->name), type->name);
    symbol->type = type;
    scope->add(symbol);
    return symbol;
}

namespace {

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

Type* typeIn(Scope* scope, const std::string& name)
{
    for (Symbol* candidate : scope->lookupLocal(name)) {
        if (candidate->kind == SymbolKind::TypeName) {
            return candidate->type;
        }
    }
    return nullptr;
}

}

// Two units of the predefined environment hold things the compiler itself has
// to lay hands on: the exceptions the run time raises by number, and the types
// the Text_IO generics are written in terms of.  Both are picked up as the Ada
// source declaring them is analysed.
void Sema::adoptLibraryUnit(PackageSpecDecl* decl, Symbol* package)
{
    if (m_namePrefix.size() != 2 || m_namePrefix[0] != "ada" || package->scope == nullptr) {
        return;
    }

    if (m_namePrefix[1] == "io_exceptions") {
        // The run time raises these by number, so the order they were declared
        // in is the order they are kept in.
        for (const DeclPtr& item : decl->publicPart) {
            if (item->kind == DeclKind::Exception) {
                for (Symbol* exception : static_cast<ExceptionDecl*>(item.get())->symbols) {
                    m_ioExceptions.push_back(exception);
                }
            }
        }
        return;
    }

    if (m_namePrefix[1] == "text_io") {
        m_textFileType = typeIn(package->scope, "file_type");
        m_fieldType = typeIn(package->scope, "field");
        m_numberBaseType = typeIn(package->scope, "number_base");
        m_typeSetType = typeIn(package->scope, "type_set");
    }
}

// ---------------------------------------------------------------------------
// Compilation unit
// ---------------------------------------------------------------------------

void Sema::analyze(CompilationUnit& unit)
{
    // The loader has already read whatever it could find, so a name still
    // standing for nothing is one no unit answers to.
    for (const WithClause& clause : unit.withClauses) {
        for (std::size_t i = 0; i < clause.names.size(); ++i) {
            Symbol* symbol = lookupName(clause.namesLower[i], m_globalScope);
            if (symbol == nullptr || (symbol->kind != SymbolKind::Package && symbol->kind != SymbolKind::Generic)) {
                m_diagnostics.error(clause.location, "cannot find the unit '" + clause.names[i] + "'");
            }
        }
    }

    for (UseDecl& use : unit.useClauses) {
        analyzeUseClause(use, m_globalScope);
    }
    analyzeDeclarativePart(unit.units, m_globalScope);

    if (m_main == nullptr) {
        for (const DeclPtr& decl : unit.units) {
            if (decl->kind != DeclKind::SubprogramBody) {
                continue;
            }
            auto* body = static_cast<SubprogramBody*>(decl.get());
            if (body->symbol != nullptr && !body->spec.isFunction && body->spec.parameters.empty()) {
                m_main = body->symbol;
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Declarations
// ---------------------------------------------------------------------------

std::string Sema::mangle(const std::string& name) const
{
    std::string result;
    for (const std::string& part : m_namePrefix) {
        result += part + "__";
    }
    result += name;
    return result;
}

std::string Sema::anonymousTypeName()
{
    return "anon." + std::to_string(m_anonymousCounter++);
}

void Sema::analyzeDeclarativePart(DeclList& declarations, Scope* scope, bool reportIncomplete)
{
    for (const DeclPtr& decl : declarations) {
        analyzeDecl(decl.get(), scope);
    }
    if (reportIncomplete) {
        reportIncompleteTypes(declarations);
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

void Sema::analyzeDecl(Decl* decl, Scope* scope)
{
    switch (decl->kind) {
    case DeclKind::Object:
        analyzeObjectDecl(static_cast<ObjectDecl*>(decl), scope);
        break;
    case DeclKind::Number:
        analyzeNumberDecl(static_cast<NumberDecl*>(decl), scope);
        break;
    case DeclKind::Type:
        analyzeTypeDecl(static_cast<TypeDecl*>(decl), scope);
        break;
    case DeclKind::Subtype:
        analyzeSubtypeDecl(static_cast<SubtypeDecl*>(decl), scope);
        break;
    case DeclKind::SubprogramDeclaration: {
        auto* subprogram = static_cast<SubprogramDecl*>(decl);
        subprogram->symbol = declareSubprogram(subprogram->spec, scope, false);
        break;
    }
    case DeclKind::SubprogramBody:
        analyzeSubprogramBody(static_cast<SubprogramBody*>(decl), scope);
        break;
    case DeclKind::PackageSpecification:
        analyzePackageSpec(static_cast<PackageSpecDecl*>(decl), scope);
        break;
    case DeclKind::PackageBody:
        analyzePackageBody(static_cast<PackageBodyDecl*>(decl), scope);
        break;
    case DeclKind::Use:
        analyzeUseClause(*static_cast<UseDecl*>(decl), scope);
        break;
    case DeclKind::GenericDeclaration:
        analyzeGenericDecl(static_cast<GenericDecl*>(decl), scope);
        break;
    case DeclKind::GenericInstantiation:
        analyzeGenericInstantiation(static_cast<GenericInstantiationDecl*>(decl), scope);
        break;
    case DeclKind::Exception:
        analyzeExceptionDecl(static_cast<ExceptionDecl*>(decl), scope);
        break;
    case DeclKind::Pragma:
        analyzePragma(static_cast<PragmaDecl*>(decl), scope);
        break;
    case DeclKind::Representation:
        analyzeRepresentation(static_cast<RepresentationDecl*>(decl), scope);
        break;
    }
}

void Sema::analyzeObjectDecl(ObjectDecl* decl, Scope* scope)
{
    Type* type = resolveSubtypeIndication(decl->subtype.get(), scope);

    // A discriminant is fixed when the object is declared, so the declaration
    // has to say what to fix it to.
    Type* record = baseType(type);
    if (record != nullptr && record->discriminantCount > 0 && !hasKnownDiscriminants(type)) {
        m_diagnostics.error(decl->location, "an object of '" + record->name + "' has to fix its discriminants, as "
                                                + "in 'X : " + record->name + " (...)'");
    }

    if (decl->initializer) {
        Type* valueType = analyzeExpr(decl->initializer.get(), scope, type);
        if (!typesCompatible(type, valueType)) {
            m_diagnostics.error(decl->initializer->location,
                                "initial value is not compatible with the declared subtype");
        }
        adaptUniversal(decl->initializer.get(), type);
    }

    if (decl->isConstant && !decl->initializer) {
        // A constant with no value is a promise that the private part will give
        // it one, which lets a package name a constant of a type its users are
        // not shown the inside of.
        if (!m_inVisiblePart) {
            m_diagnostics.error(decl->location, "a constant needs a value, unless it is deferred to the private "
                                                "part of a package specification");
            return;
        }
        decl->awaitsValue = true;
    }

    for (std::size_t i = 0; i < decl->names.size(); ++i) {
        // The declaration in the private part gives a value to the constant the
        // visible part named rather than declaring a second one.
        Symbol* deferred = nullptr;
        for (Symbol* candidate : scope->lookupLocal(decl->namesLower[i])) {
            if (candidate->kind == SymbolKind::Object && candidate->awaitsValue) {
                deferred = candidate;
            }
        }
        if (deferred != nullptr) {
            if (decl->awaitsValue) {
                m_diagnostics.error(decl->location, "'" + decl->names[i] + "' has already been named here");
                continue;
            }
            if (rootType(deferred->type) != rootType(type)) {
                m_diagnostics.error(decl->location, "'" + decl->names[i] + "' was named as a '" + deferred->type->name
                                                        + "' in the visible part");
            }
            deferred->awaitsValue = false;
            decl->symbols.push_back(deferred);
            continue;
        }

        Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::Object, decl->namesLower[i], decl->names[i]);
        symbol->type = type;
        symbol->awaitsValue = decl->awaitsValue;
        symbol->isConstant = decl->isConstant;
        symbol->location = decl->location;
        symbol->owner = m_currentSubprogram;
        symbol->level = m_currentSubprogram != nullptr ? m_currentSubprogram->level : 0;
        symbol->isGlobal = m_currentSubprogram == nullptr;
        if (symbol->isGlobal) {
            symbol->qbeName = "$" + mangle(decl->namesLower[i]);
        }
        if (decl->isConstant && decl->initializer) {
            long long value = 0;
            double realValue = 0.0;
            if (isReal(type) && foldStaticReal(decl->initializer.get(), realValue)) {
                symbol->hasStaticValue = true;
                symbol->staticReal = realValue;
            } else if (foldStatic(decl->initializer.get(), value)) {
                symbol->hasStaticValue = true;
                symbol->staticValue = value;
            }
        }
        scope->add(symbol);
        decl->symbols.push_back(symbol);
    }
}

void Sema::analyzeNumberDecl(NumberDecl* decl, Scope* scope)
{
    Type* type = analyzeExpr(decl->value.get(), scope, nullptr);
    if (type != nullptr && type->kind == TypeKind::UniversalInteger) {
        type = m_types.integerType();
        adaptUniversal(decl->value.get(), type);
    }

    // A real named number keeps its universal type, so that every use of it
    // takes the precision of its context.
    long long value = 0;
    double realValue = 0.0;
    bool real = isReal(type);
    bool isStatic = real ? foldStaticReal(decl->value.get(), realValue) : foldStatic(decl->value.get(), value);
    if (!isStatic) {
        m_diagnostics.error(decl->value->location, "the value of a named number must be static");
    }

    for (std::size_t i = 0; i < decl->names.size(); ++i) {
        Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::Number, decl->namesLower[i], decl->names[i]);
        symbol->type = type;
        symbol->isConstant = true;
        symbol->hasStaticValue = isStatic;
        symbol->staticValue = value;
        symbol->staticReal = realValue;
        symbol->location = decl->location;
        scope->add(symbol);
        decl->symbols.push_back(symbol);
    }
}

// Whether the package a private type belongs to is one of those being analysed
// right now.  A child unit counts, since it is analysed inside its parent.
bool Sema::withinPackage(Symbol* package) const
{
    for (Symbol* open : m_packages) {
        for (Symbol* walk = open; walk != nullptr; walk = walk->parentPackage) {
            if (walk == package) {
                return true;
            }
        }
    }
    return false;
}

// The type as its user may see it: outside the package that declared it, a
// private type is a name and nothing more.
bool Sema::representationVisible(Type* type) const
{
    Type* base = baseType(type);
    return base == nullptr || base->privateTo == nullptr || withinPackage(base->privateTo);
}

bool Sema::checkNotPrivate(Type* type, const SourceLocation& location, const char* what)
{
    Type* base = baseType(type);
    if (representationVisible(base)) {
        return true;
    }
    m_diagnostics.error(location, std::string(what) + " of '" + base->name + "', whose representation '"
                                      + base->privateTo->displayName + "' keeps to itself");
    return false;
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
        if (definition->indexTypes.size() > 1) {
            m_diagnostics.error(definition->location, "multi-dimensional arrays are not supported");
        }
        type->element = resolveSubtypeIndication(definition->elementType.get(), scope);
        type->constrained = !definition->unconstrainedIndexes;

        SubtypeIndication* index = definition->indexTypes.front().get();
        Type* indexType = index->name.empty() ? m_types.integerType()
                                              : resolveTypeName(index->lower, scope, index->location);
        type->index = indexType;
        if (index->rangeLow && index->rangeHigh) {
            analyzeExpr(index->rangeLow.get(), scope, indexType);
            analyzeExpr(index->rangeHigh.get(), scope, indexType);
            long long low = 0;
            long long high = 0;
            if (!foldStatic(index->rangeLow.get(), low) || !foldStatic(index->rangeHigh.get(), high)) {
                m_diagnostics.error(index->location, "array index bounds must be static");
            }
            type->indexLow = low;
            type->indexHigh = high;
        } else if (indexType != nullptr && type->constrained) {
            type->indexLow = indexType->low;
            type->indexHigh = indexType->high;
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

Symbol* Sema::declareSubprogram(SubprogramSpec& spec, Scope* scope, bool isBody)
{
    std::vector<Type*> parameterTypes;
    for (ParameterDecl& parameter : spec.parameters) {
        parameterTypes.push_back(resolveSubtypeIndication(parameter.subtype.get(), scope));
    }
    Type* returnType = spec.isFunction ? resolveSubtypeIndication(spec.returnType.get(), scope) : nullptr;

    if (isBody) {
        for (Symbol* candidate : scope->lookupLocal(spec.lower)) {
            if (candidate->kind != SymbolKind::Subprogram || candidate->hasBody) {
                continue;
            }
            if (candidate->parameters.size() != parameterTypes.size()) {
                continue;
            }
            bool matches = rootType(candidate->returnType) == rootType(returnType);
            for (std::size_t i = 0; i < parameterTypes.size(); ++i) {
                if (rootType(candidate->parameters[i]->type) != rootType(parameterTypes[i])
                    || candidate->parameters[i]->mode != spec.parameters[i].mode) {
                    matches = false;
                    break;
                }
            }
            if (!matches) {
                continue;
            }
            candidate->hasBody = true;
            for (std::size_t i = 0; i < spec.parameters.size(); ++i) {
                Symbol* parameter = candidate->parameters[i];
                parameter->name = spec.parameters[i].lower;
                parameter->displayName = spec.parameters[i].name;
                spec.parameters[i].symbol = parameter;
            }
            return candidate;
        }
    }

    Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::Subprogram, spec.lower, spec.name);
    symbol->location = spec.location;
    symbol->returnType = returnType;
    symbol->hasBody = isBody;
    symbol->level = m_currentSubprogram != nullptr ? m_currentSubprogram->level + 1 : 0;
    symbol->owner = m_currentSubprogram;

    // Block scopes can also declare the same spelling. Reserve emitted names
    // across the compilation, so shadowing never creates duplicate QBE symbols.
    std::string name = "$" + mangle(spec.lower);
    std::size_t ordinal = ++m_subprogramNames[name];
    symbol->qbeName = name;
    if (ordinal > 1) {
        symbol->qbeName += "__" + std::to_string(ordinal);
    }

    for (std::size_t i = 0; i < spec.parameters.size(); ++i) {
        ParameterDecl& declaration = spec.parameters[i];
        Symbol* parameter = m_symbolTable.createSymbol(SymbolKind::Parameter, declaration.lower, declaration.name);
        parameter->type = parameterTypes[i];
        parameter->mode = declaration.mode;
        parameter->location = declaration.location;
        parameter->owner = symbol;
        parameter->level = symbol->level;
        parameter->isConstant = declaration.mode == ParameterMode::In;
        parameter->byReference = declaration.mode != ParameterMode::In || isComposite(parameterTypes[i]);
        if (declaration.defaultValue) {
            analyzeExpr(declaration.defaultValue.get(), scope, parameterTypes[i]);
            adaptUniversal(declaration.defaultValue.get(), parameterTypes[i]);
            parameter->hasDefault = true;
            parameter->defaultExpr = declaration.defaultValue.get();
            foldStatic(declaration.defaultValue.get(), parameter->defaultValue);
        }
        declaration.symbol = parameter;
        symbol->parameters.push_back(parameter);
    }

    scope->add(symbol);
    return symbol;
}

void Sema::analyzeSubprogramBody(SubprogramBody* body, Scope* scope)
{
    // The body of a generic subprogram, like that of a generic package, waits
    // in the tokens an instance parses rather than being analysed here.
    Symbol* named = lookupName(body->spec.lower, scope);
    if (named != nullptr && named->kind == SymbolKind::Generic && named->generic != nullptr
        && !named->generic->isPackage) {
        std::vector<Token>& tokens = named->generic->tokens;
        Token endOfFile = tokens.back();
        tokens.pop_back();
        tokens.insert(tokens.end(), body->tokens.begin(), body->tokens.end());
        tokens.push_back(endOfFile);
        return;
    }

    Symbol* symbol = declareSubprogram(body->spec, scope, true);
    symbol->hasBody = true;
    body->symbol = symbol;

    Scope* inner = m_symbolTable.createScope(scope);
    for (std::size_t i = 0; i < body->spec.parameters.size(); ++i) {
        Symbol* parameter = symbol->parameters[i];
        body->spec.parameters[i].symbol = parameter;
        inner->add(parameter);
    }

    Symbol* savedSubprogram = m_currentSubprogram;
    m_currentSubprogram = symbol;
    m_namePrefix.push_back(symbol->name);

    analyzeDeclarativePart(body->declarations, inner);
    analyzeStatements(body->body, inner);
    analyzeHandlers(body->handlers, inner);

    m_namePrefix.pop_back();
    m_currentSubprogram = savedSubprogram;
}

// A library unit written as Ada.Text_IO is a child declared inside Ada, so each
// part of the name is walked, and made if it has not been seen, before the unit
// itself.  Every part joins m_namePrefix, which is what gives the child's
// subprograms names of their own.
Symbol* Sema::declarePackagePath(const std::string& lower, const std::string& displayName, Scope* scope,
                                 const SourceLocation& location, std::size_t& pushed)
{
    std::vector<std::string> parts = splitDottedName(lower);
    std::vector<std::string> display = splitDottedName(displayName);

    Scope* enclosing = scope;
    Symbol* symbol = nullptr;
    Symbol* parent = nullptr;
    pushed = 0;

    for (std::size_t i = 0; i < parts.size(); ++i) {
        // A parent is looked for wherever it may be, a child only in its parent.
        std::vector<Symbol*> candidates =
            i == 0 ? enclosing->lookup(parts[i]) : enclosing->lookupLocal(parts[i]);
        symbol = nullptr;
        for (Symbol* candidate : candidates) {
            if (candidate->kind == SymbolKind::Package) {
                symbol = candidate;
                break;
            }
        }
        if (symbol == nullptr) {
            symbol = m_symbolTable.createSymbol(SymbolKind::Package, parts[i],
                                                i < display.size() ? display[i] : parts[i]);
            symbol->scope = m_symbolTable.createScope(enclosing);
            symbol->location = location;
            symbol->parentPackage = parent;
            enclosing->add(symbol);
        }
        m_namePrefix.push_back(symbol->name);
        ++pushed;
        parent = symbol;
        enclosing = symbol->scope;
    }

    return symbol;
}

void Sema::analyzePackageSpec(PackageSpecDecl* decl, Scope* scope)
{
    std::size_t pushed = 0;
    Symbol* symbol = declarePackagePath(decl->lower, decl->name, scope, decl->location, pushed);
    decl->symbol = symbol;

    // What a private type is made of is in reach from here down to the end of
    // the private part, and from the body, but nowhere else.
    m_packages.push_back(symbol);
    m_inVisiblePart = true;
    analyzeDeclarativePart(decl->publicPart, symbol->scope, false);
    m_inVisiblePart = false;
    analyzeDeclarativePart(decl->privatePart, symbol->scope, false);
    m_packages.pop_back();

    reportIncompleteTypes(decl->publicPart);
    reportIncompleteTypes(decl->privatePart);

    // Every promise the visible part made has to have been kept by now.
    for (const DeclPtr& item : decl->publicPart) {
        if (item->kind != DeclKind::Object) {
            continue;
        }
        auto* object = static_cast<ObjectDecl*>(item.get());
        for (Symbol* constant : object->symbols) {
            if (constant->awaitsValue) {
                m_diagnostics.error(item->location, "'" + constant->displayName + "' is never given a value");
            }
        }
    }

    adoptLibraryUnit(decl, symbol);

    for (std::size_t i = 0; i < pushed; ++i) {
        m_namePrefix.pop_back();
    }
}

void Sema::analyzePackageBody(PackageBodyDecl* decl, Scope* scope)
{
    // The body of a generic is not analysed where it stands, any more than the
    // specification was.  It joins the tokens an instance parses, right after
    // the specification they already end with.
    Symbol* named = lookupName(decl->lower, scope);
    if (named != nullptr && named->kind == SymbolKind::Generic && named->generic != nullptr) {
        std::vector<Token>& tokens = named->generic->tokens;
        Token endOfFile = tokens.back();
        tokens.pop_back();
        tokens.insert(tokens.end(), decl->tokens.begin(), decl->tokens.end());
        tokens.push_back(endOfFile);
        return;
    }

    std::size_t pushed = 0;
    Symbol* symbol = declarePackagePath(decl->lower, decl->name, scope, decl->location, pushed);
    decl->symbol = symbol;

    m_packages.push_back(symbol);
    analyzeDeclarativePart(decl->declarations, symbol->scope);
    analyzeStatements(decl->body, symbol->scope);
    analyzeHandlers(decl->handlers, symbol->scope);
    m_packages.pop_back();

    for (std::size_t i = 0; i < pushed; ++i) {
        m_namePrefix.pop_back();
    }
}

namespace
{

// Renames the unit a generic was written as to the name of the instance, so
// that its symbols and the names the emitter gives them are its own.
void renameUnit(Decl* decl, const std::string& name, const std::string& lower, const std::string& fromLower)
{
    switch (decl->kind) {
    case DeclKind::PackageSpecification: {
        auto* spec = static_cast<PackageSpecDecl*>(decl);
        if (spec->lower == fromLower) {
            spec->name = name;
            spec->lower = lower;
        }
        break;
    }
    case DeclKind::PackageBody: {
        auto* body = static_cast<PackageBodyDecl*>(decl);
        if (body->lower == fromLower) {
            body->name = name;
            body->lower = lower;
        }
        break;
    }
    case DeclKind::SubprogramDeclaration: {
        auto* declaration = static_cast<SubprogramDecl*>(decl);
        if (declaration->spec.lower == fromLower) {
            declaration->spec.name = name;
            declaration->spec.lower = lower;
        }
        break;
    }
    case DeclKind::SubprogramBody: {
        auto* body = static_cast<SubprogramBody*>(decl);
        if (body->spec.lower == fromLower) {
            body->spec.name = name;
            body->spec.lower = lower;
        }
        break;
    }
    default:
        break;
    }
}

// An unconstrained array has no width until an object of it exists, so it
// cannot be the element type of a file.
bool isDefinite(Type* type)
{
    Type* base = baseType(type);
    return base == nullptr || base->kind != TypeKind::Array || base->constrained;
}

// The symbol a unit declared, which is what makes an instance visible where it
// was written.
Symbol* declaredSymbol(Decl* decl)
{
    switch (decl->kind) {
    case DeclKind::PackageSpecification:
        return static_cast<PackageSpecDecl*>(decl)->symbol;
    case DeclKind::PackageBody:
        return static_cast<PackageBodyDecl*>(decl)->symbol;
    case DeclKind::SubprogramDeclaration:
        return static_cast<SubprogramDecl*>(decl)->symbol;
    case DeclKind::SubprogramBody:
        return static_cast<SubprogramBody*>(decl)->symbol;
    default:
        return nullptr;
    }
}

}

void Sema::analyzeGenericDecl(GenericDecl* decl, Scope* scope)
{
    // A generic unit is not analysed where it stands; only its instances are.
    // One with a dotted name is a child like any other, so it is declared
    // inside its parent rather than under a name with a dot in it.
    Scope* target = scope;
    std::string lower = decl->lower;
    std::string name = decl->name;

    std::size_t dot = decl->lower.find_last_of('.');
    if (dot != std::string::npos) {
        std::size_t pushed = 0;
        Symbol* parent = declarePackagePath(decl->lower.substr(0, dot), decl->name.substr(0, dot), scope,
                                            decl->location, pushed);
        for (std::size_t i = 0; i < pushed; ++i) {
            m_namePrefix.pop_back();
        }
        target = parent->scope;
        lower = decl->lower.substr(dot + 1);
        name = decl->name.substr(dot + 1);
    }

    Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::Generic, lower, name);
    symbol->location = decl->location;
    symbol->generic = decl;
    decl->symbol = symbol;
    target->add(symbol);
}

// A formal type says what kind of type the instantiation may supply, and a
// generic body is only sound for the kinds it was written for.
bool Sema::acceptsFormalType(const GenericFormal& formal, Type* actual, const std::string& genericName,
                             const SourceLocation& location)
{
    Type* base = baseType(actual);
    const char* wanted = nullptr;

    switch (formal.typeClass) {
    case FormalTypeClass::IntegerType:
        if (base->kind != TypeKind::Integer) {
            wanted = "an integer type";
        }
        break;
    case FormalTypeClass::FloatType:
        if (base->kind != TypeKind::Float) {
            wanted = "a floating point type";
        }
        break;
    case FormalTypeClass::Discrete:
        // Character is an enumeration whose literal names this compiler does
        // not record, and Text_IO writes one out directly anyway.
        if (base->kind == TypeKind::Enumeration && base->literals.empty()) {
            m_diagnostics.error(location, "'" + actual->name
                                              + "' names its literals by their spelling, so use Text_IO.Put on one "
                                                "instead of instantiating '"
                                              + genericName + "'");
            return false;
        }
        if (!isDiscrete(base)) {
            wanted = "a discrete type";
        }
        break;
    case FormalTypeClass::Any:
        if (!isDefinite(actual)) {
            wanted = "a type of a fixed size";
        }
        break;
    }

    if (wanted != nullptr) {
        m_diagnostics.error(location, "'" + genericName + "' expects " + wanted + ", and '" + actual->name
                                          + "' is not one");
        return false;
    }
    return true;
}

// Matches the actuals to the formals and declares each one in a scope of its
// own, which the copy of the generic is then analysed inside.
bool Sema::bindGenericFormals(GenericInstantiationDecl* decl, Symbol* generic, Scope* bindings, Scope* scope)
{
    const std::vector<GenericFormal>& formals = generic->generic->formals;
    std::vector<Expr*> actuals(formals.size(), nullptr);

    if (decl->arguments.size() > formals.size()) {
        m_diagnostics.error(decl->location, "too many actual parameters for generic '" + decl->genericName + "'");
        return false;
    }
    for (std::size_t i = 0; i < decl->arguments.size(); ++i) {
        std::size_t index = i;
        if (!decl->arguments[i].nameLower.empty()) {
            index = formals.size();
            for (std::size_t f = 0; f < formals.size(); ++f) {
                if (formals[f].lower == decl->arguments[i].nameLower) {
                    index = f;
                    break;
                }
            }
            if (index == formals.size()) {
                std::string names;
                for (const GenericFormal& candidate : formals) {
                    names += names.empty() ? "'" : "', '";
                    names += candidate.name;
                }
                m_diagnostics.error(decl->location,
                                    "'" + decl->arguments[i].name + "' is not a formal of '" + decl->genericName
                                        + "', which takes " + names + "'");
                return false;
            }
        }
        actuals[index] = decl->arguments[i].value.get();
    }

    for (std::size_t i = 0; i < formals.size(); ++i) {
        const GenericFormal& formal = formals[i];
        Expr* actual = actuals[i];
        if (actual == nullptr && formal.defaultValue) {
            actual = formal.defaultValue.get();
        }
        if (actual == nullptr) {
            m_diagnostics.error(decl->location, "no actual supplied for generic formal '" + formal.name + "'");
            return false;
        }

        if (formal.kind == GenericFormalKind::TypeFormal) {
            if (actual->kind != ExprKind::Identifier) {
                m_diagnostics.error(actual->location, "a generic formal type expects a type name");
                return false;
            }
            auto* identifier = static_cast<IdentifierExpr*>(actual);
            Type* type = resolveTypeName(identifier->lower, scope, actual->location);
            if (type == nullptr) {
                return false;
            }
            if (!acceptsFormalType(formal, type, decl->genericName, actual->location)) {
                return false;
            }
            Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::TypeName, formal.lower, formal.name);
            symbol->type = type;
            symbol->location = formal.location;
            bindings->add(symbol);
            continue;
        }

        Type* type = resolveSubtypeIndication(formal.subtype.get(), scope);
        analyzeExpr(actual, scope, type);
        Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::Number, formal.lower, formal.name);
        symbol->type = type;
        symbol->location = formal.location;
        long long value = 0;
        double real = 0.0;
        if (isReal(type) ? foldStaticReal(actual, real) : foldStatic(actual, value)) {
            symbol->hasStaticValue = true;
            symbol->staticValue = value;
            symbol->staticReal = real;
        } else {
            m_diagnostics.error(actual->location, "a generic formal object expects a static value");
        }
        bindings->add(symbol);
    }

    return true;
}

void Sema::analyzeGenericInstantiation(GenericInstantiationDecl* decl, Scope* scope)
{
    Symbol* generic = lookupName(decl->genericLower, scope);
    if (generic == nullptr || generic->kind != SymbolKind::Generic) {
        m_diagnostics.error(decl->location, "'" + decl->genericName + "' is not a generic unit");
        return;
    }
    if (m_instantiationDepth > 16) {
        m_diagnostics.error(decl->location, "generic instantiation nests too deeply");
        return;
    }
    // An instance with a dotted name belongs inside its parent, but it is
    // analysed against the formals it was bound with.  So it is given the
    // simple name here, and only put in its parent once it is built.
    Scope* home = scope;
    std::size_t pushed = 0;
    std::string instanceName = decl->lower;
    std::string instanceDisplay = decl->name;

    std::size_t dot = decl->lower.find_last_of('.');
    if (dot != std::string::npos) {
        Symbol* parent = declarePackagePath(decl->lower.substr(0, dot), decl->name.substr(0, dot), scope,
                                            decl->location, pushed);
        home = parent->scope;
        instanceName = decl->lower.substr(dot + 1);
        instanceDisplay = decl->name.substr(dot + 1);
    }

    Scope* bindings = m_symbolTable.createScope(scope);
    if (!bindGenericFormals(decl, generic, bindings, scope)) {
        for (std::size_t i = 0; i < pushed; ++i) {
            m_namePrefix.pop_back();
        }
        return;
    }

    Parser parser(generic->generic->tokens, m_diagnostics);
    decl->expansion = parser.parseDeclarations();
    for (const DeclPtr& unit : decl->expansion) {
        renameUnit(unit.get(), instanceDisplay, instanceName, generic->generic->lower);
    }

    // A generic body sees nothing but its own formals and its context, so an
    // instance written inside a subprogram is still elaborated once and its
    // objects still live at library level.  Forgetting the subprogram it was
    // written in is what says so.
    Symbol* enclosing = m_currentSubprogram;
    if (decl->isPackage) {
        m_currentSubprogram = nullptr;
    }

    ++m_instantiationDepth;
    analyzeDeclarativePart(decl->expansion, bindings);
    --m_instantiationDepth;

    m_currentSubprogram = enclosing;
    for (std::size_t i = 0; i < pushed; ++i) {
        m_namePrefix.pop_back();
    }

    // An instance written inside a subprogram is elaborated with the library,
    // so the emitter is told where to find it: nothing walks into a subprogram
    // looking for units of its own.
    if (decl->isPackage && enclosing != nullptr) {
        m_libraryInstances.push_back(decl);
    }

    // The instance lives in the bindings scope while it is analysed; here it
    // becomes visible where it was written.
    for (const DeclPtr& unit : decl->expansion) {
        Symbol* symbol = declaredSymbol(unit.get());
        if (symbol != nullptr && symbol->name == instanceName) {
            decl->symbol = symbol;
            home->add(symbol);
            break;
        }
    }
    if (decl->symbol == nullptr) {
        m_diagnostics.error(decl->location, "generic '" + decl->genericName + "' produced no instance");
    }
}

void Sema::analyzeUseClause(UseDecl& decl, Scope* scope)
{
    for (std::size_t i = 0; i < decl.namesLower.size(); ++i) {
        Symbol* symbol = lookupName(decl.namesLower[i], scope);
        if (symbol == nullptr) {
            m_diagnostics.error(decl.location, "unknown package '" + decl.names[i] + "' in use clause");
            continue;
        }
        if (symbol->kind == SymbolKind::Package && symbol->scope != nullptr) {
            scope->addUseScope(symbol->scope);
        }
    }
}

void Sema::analyzeExceptionDecl(ExceptionDecl* decl, Scope* scope)
{
    // A renaming has to name the very same exception, identity being what a
    // handler matches on.
    int renamed = 0;
    if (!decl->renamesLower.empty()) {
        Symbol* target = lookupName(decl->renamesLower, scope);
        if (target == nullptr || target->kind != SymbolKind::Exception) {
            m_diagnostics.error(decl->location, "'" + decl->renames + "' is not an exception");
            return;
        }
        renamed = target->exceptionId;
    }

    for (std::size_t i = 0; i < decl->names.size(); ++i) {
        Symbol* symbol = m_symbolTable.createSymbol(SymbolKind::Exception, decl->namesLower[i], decl->names[i]);
        symbol->exceptionId = renamed != 0 ? renamed : m_exceptionCounter++;
        symbol->location = decl->location;
        scope->add(symbol);
        decl->symbols.push_back(symbol);
    }
}

// pragma Import ties the declaration just given to an entry point in the C run
// time.  It applies to the most recent declaration of the name, so that the
// overloads of a subprogram can each name the routine that carries them out.
void Sema::analyzePragma(PragmaDecl* decl, Scope* scope)
{
    std::vector<Symbol*> candidates = scope->lookupLocal(decl->entityLower);
    Symbol* target = nullptr;
    for (Symbol* candidate : candidates) {
        if (candidate->kind == SymbolKind::Subprogram) {
            target = candidate;
        }
    }
    if (target == nullptr) {
        m_diagnostics.error(decl->location,
                            "pragma Import names '" + decl->entity + "', which is not a subprogram declared here");
        return;
    }

    target->builtin = BuiltinKind::Runtime;
    target->runtimeSymbol = "$" + decl->linkName;
    target->canRaise = true;
    target->hasBody = true;
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

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

void Sema::analyzeStatements(StmtList& statements, Scope* scope)
{
    for (const StmtPtr& statement : statements) {
        analyzeStatement(statement.get(), scope);
    }
}

void Sema::analyzeHandlers(std::vector<ExceptionHandler>& handlers, Scope* scope)
{
    for (ExceptionHandler& handler : handlers) {
        for (std::size_t i = 0; i < handler.namesLower.size(); ++i) {
            Symbol* symbol = lookupName(handler.namesLower[i], scope);
            if (symbol == nullptr || symbol->kind != SymbolKind::Exception) {
                m_diagnostics.error(handler.location, "unknown exception '" + handler.names[i] + "'");
                continue;
            }
            handler.identifiers.push_back(symbol->exceptionId);
        }
        analyzeStatements(handler.body, scope);
    }
}

void Sema::checkAssignable(Expr* target, Scope* scope)
{
    (void)scope;

    // A limited private type is not copied outside the package that declared
    // it; whatever it takes to make one is that package's to offer.
    Type* type = baseType(target->type);
    if (type != nullptr && type->isLimited && type->privateTo != nullptr && !withinPackage(type->privateTo)) {
        m_diagnostics.error(target->location, "'" + type->name + "' is limited private, so a value of it cannot "
                                                  + "be assigned outside '" + type->privateTo->displayName + "'");
        return;
    }

    // A discriminant is fixed when the object is declared and stays that way,
    // since the components the value has were settled by it.
    if (target->kind == ExprKind::Selected) {
        auto* selected = static_cast<SelectedExpr*>(target);
        Type* record = baseType(selected->prefix->type);
        if (record != nullptr && record->kind == TypeKind::Access) {
            record = baseType(record->target);
        }
        if (selected->fieldIndex >= 0 && record != nullptr
            && record->fields[static_cast<std::size_t>(selected->fieldIndex)].isDiscriminant) {
            m_diagnostics.error(target->location, "'" + selected->selector
                                                      + "' is a discriminant, fixed when the object was declared");
            return;
        }
    }

    switch (target->kind) {
    case ExprKind::Identifier: {
        Symbol* symbol = static_cast<IdentifierExpr*>(target)->symbol;
        if (symbol == nullptr) {
            return;
        }
        if (symbol->kind == SymbolKind::Number || symbol->kind == SymbolKind::LoopParameter
            || (symbol->kind == SymbolKind::Object && symbol->isConstant)
            || (symbol->kind == SymbolKind::Parameter && symbol->mode == ParameterMode::In)) {
            m_diagnostics.error(target->location, "'" + symbol->displayName + "' cannot be assigned to");
        }
        return;
    }
    case ExprKind::Selected:
    case ExprKind::Call:
        return;
    default:
        m_diagnostics.error(target->location, "the target of an assignment must be a variable");
        return;
    }
}

void Sema::analyzeStatement(Stmt* statement, Scope* scope)
{
    switch (statement->kind) {
    case StmtKind::Null:
        break;

    case StmtKind::Assign: {
        auto* assign = static_cast<AssignStmt*>(statement);
        Type* targetType = analyzeExpr(assign->target.get(), scope, nullptr);
        checkAssignable(assign->target.get(), scope);
        Type* valueType = analyzeExpr(assign->value.get(), scope, targetType);
        if (!typesCompatible(targetType, valueType)) {
            m_diagnostics.error(assign->location, "the assigned value has an incompatible type");
        }
        adaptUniversal(assign->value.get(), targetType);
        break;
    }

    case StmtKind::ProcedureCall: {
        auto* call = static_cast<ProcedureCallStmt*>(statement);
        analyzeExpr(call->call.get(), scope, nullptr);
        break;
    }

    case StmtKind::If: {
        auto* ifStatement = static_cast<IfStmt*>(statement);
        for (IfBranch& branch : ifStatement->branches) {
            Type* conditionType = analyzeExpr(branch.condition.get(), scope, m_types.booleanType());
            if (!m_types.isBoolean(conditionType)) {
                m_diagnostics.error(branch.condition->location, "condition must be of type Boolean");
            }
            analyzeStatements(branch.body, scope);
        }
        analyzeStatements(ifStatement->elseBody, scope);
        break;
    }

    case StmtKind::Loop: {
        auto* loop = static_cast<LoopStmt*>(statement);
        Scope* inner = m_symbolTable.createScope(scope);

        if (loop->loopKind == LoopKind::While) {
            Type* conditionType = analyzeExpr(loop->condition.get(), scope, m_types.booleanType());
            if (!m_types.isBoolean(conditionType)) {
                m_diagnostics.error(loop->condition->location, "condition must be of type Boolean");
            }
        } else if (loop->loopKind == LoopKind::For) {
            Type* variableType = nullptr;
            if (!loop->rangeTypeLower.empty()) {
                variableType = resolveTypeName(loop->rangeTypeLower, scope, loop->location);
            }
            if (loop->rangeLow && loop->rangeHigh) {
                Type* lowType = analyzeExpr(loop->rangeLow.get(), scope, variableType);
                Type* highType = analyzeExpr(loop->rangeHigh.get(), scope, variableType);
                if (variableType == nullptr) {
                    variableType = isUniversal(lowType) ? highType : lowType;
                    if (isUniversal(variableType)) {
                        variableType = m_types.integerType();
                    }
                }
                adaptUniversal(loop->rangeLow.get(), variableType);
                adaptUniversal(loop->rangeHigh.get(), variableType);
            } else if (variableType != nullptr) {
                auto makeBound = [&](long long value) {
                    auto literal = std::make_unique<IntegerLiteralExpr>();
                    literal->location = loop->location;
                    literal->value = value;
                    literal->type = variableType;
                    literal->isStatic = true;
                    literal->staticValue = value;
                    return ExprPtr(std::move(literal));
                };
                loop->rangeLow = makeBound(variableType->low);
                loop->rangeHigh = makeBound(variableType->high);
            }
            if (variableType == nullptr) {
                variableType = m_types.integerType();
            }

            Symbol* variable = m_symbolTable.createSymbol(SymbolKind::LoopParameter, loop->variableLower,
                                                          loop->variableName);
            variable->type = variableType;
            variable->isConstant = true;
            variable->location = loop->location;
            variable->owner = m_currentSubprogram;
            variable->level = m_currentSubprogram != nullptr ? m_currentSubprogram->level : 0;
            variable->isGlobal = m_currentSubprogram == nullptr;
            inner->add(variable);
            loop->variableSymbol = variable;
        }

        m_loops.push_back(loop);
        analyzeStatements(loop->body, inner);
        m_loops.pop_back();
        break;
    }

    case StmtKind::Exit: {
        auto* exitStatement = static_cast<ExitStmt*>(statement);
        if (exitStatement->condition) {
            Type* conditionType = analyzeExpr(exitStatement->condition.get(), scope, m_types.booleanType());
            if (!m_types.isBoolean(conditionType)) {
                m_diagnostics.error(exitStatement->condition->location, "condition must be of type Boolean");
            }
        }
        if (m_loops.empty()) {
            m_diagnostics.error(exitStatement->location, "exit statement outside of a loop");
            break;
        }
        if (exitStatement->labelLower.empty()) {
            exitStatement->target = m_loops.back();
        } else {
            for (auto it = m_loops.rbegin(); it != m_loops.rend(); ++it) {
                if ((*it)->labelLower == exitStatement->labelLower) {
                    exitStatement->target = *it;
                    break;
                }
            }
            if (exitStatement->target == nullptr) {
                m_diagnostics.error(exitStatement->location, "unknown loop label '" + exitStatement->label + "'");
                exitStatement->target = m_loops.back();
            }
        }
        break;
    }

    case StmtKind::Return: {
        auto* returnStatement = static_cast<ReturnStmt*>(statement);
        Type* expected = m_currentSubprogram != nullptr ? m_currentSubprogram->returnType : nullptr;
        if (returnStatement->value) {
            Type* valueType = analyzeExpr(returnStatement->value.get(), scope, expected);
            if (expected == nullptr) {
                m_diagnostics.error(returnStatement->location, "a procedure cannot return a value");
            } else if (!typesCompatible(expected, valueType)) {
                m_diagnostics.error(returnStatement->location, "the returned value has an incompatible type");
            }
            adaptUniversal(returnStatement->value.get(), expected);
        } else if (expected != nullptr) {
            m_diagnostics.error(returnStatement->location, "a function must return a value");
        }
        break;
    }

    case StmtKind::Case:
        analyzeCaseStatement(static_cast<CaseStmt*>(statement), scope);
        break;

    case StmtKind::Block: {
        auto* block = static_cast<BlockStmt*>(statement);
        Scope* inner = m_symbolTable.createScope(scope);
        analyzeDeclarativePart(block->declarations, inner);
        analyzeStatements(block->body, inner);
        analyzeHandlers(block->handlers, inner);
        break;
    }

    case StmtKind::Raise: {
        auto* raise = static_cast<RaiseStmt*>(statement);
        if (raise->lower.empty()) {
            break;
        }
        Symbol* symbol = lookupName(raise->lower, scope);
        if (symbol == nullptr || symbol->kind != SymbolKind::Exception) {
            m_diagnostics.error(raise->location, "unknown exception '" + raise->name + "'");
            break;
        }
        raise->exceptionSymbol = symbol;
        break;
    }
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

// Ada asks a case to account for every value its selector can take, each of
// them exactly once.  That is what lets a case compile into a plain choice
// with no run time check behind it and nothing to fall through to.
void Sema::analyzeCaseStatement(CaseStmt* statement, Scope* scope)
{
    Type* selectorType = analyzeExpr(statement->selector.get(), scope, nullptr);
    if (isUniversal(selectorType)) {
        selectorType = m_types.integerType();
        adaptUniversal(statement->selector.get(), selectorType);
    }

    bool discrete = selectorType != nullptr && isDiscrete(selectorType);
    if (selectorType != nullptr && !discrete) {
        m_diagnostics.error(statement->selector->location,
                            "a case selector has to be discrete, and '" + selectorType->name + "' is not");
    }

    // What the choices have accounted for so far, which is both how a value
    // covered twice is noticed and how the gaps are found at the end.
    std::vector<CaseChoice> covered;
    bool sawOthers = false;

    for (CaseAlternative& alternative : statement->alternatives) {
        if (sawOthers) {
            m_diagnostics.error(alternative.location, "'others' has to be the last alternative of a case");
        }
        if (alternative.isOthers) {
            sawOthers = true;
            analyzeStatements(alternative.body, scope);
            continue;
        }

        for (std::size_t i = 0; i < alternative.choiceLows.size(); ++i) {
            CaseChoice choice;
            if (!resolveChoice(alternative.choiceLows[i].get(), alternative.choiceHighs[i].get(), selectorType,
                               covered, scope, choice)) {
                continue;
            }
            covered.push_back(choice);
            alternative.choices.push_back(choice);
        }

        analyzeStatements(alternative.body, scope);
    }

    if (!sawOthers && discrete) {
        reportUncovered(covered, selectorType, statement->location, "case");
    }
}

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

Type* Sema::analyzeExpr(Expr* expr, Scope* scope, Type* expected)
{
    if (expr == nullptr) {
        return nullptr;
    }

    switch (expr->kind) {
    case ExprKind::IntegerLiteral: {
        auto* literal = static_cast<IntegerLiteralExpr*>(expr);
        Type* type = m_types.universalInteger();
        if (expected != nullptr && isDiscrete(expected)) {
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
        return type;
    }
    }

    return nullptr;
}

void Sema::noteReference(Symbol* symbol)
{
    if (symbol == nullptr || symbol->owner == nullptr || symbol->isGlobal) {
        return;
    }
    if (symbol->owner != m_currentSubprogram) {
        symbol->isUplevel = true;
        symbol->owner->needsFrame = true;
    }
}

Type* Sema::analyzeIdentifier(IdentifierExpr* expr, Scope* scope, Type* expected)
{
    std::vector<Symbol*> candidates = scope->lookup(expr->lower);
    if (candidates.empty()) {
        m_diagnostics.error(expr->location, "'" + expr->name + "' is not declared");
        return nullptr;
    }

    Symbol* chosen = candidates.front();
    if (expected != nullptr) {
        for (Symbol* candidate : candidates) {
            if (candidate->kind == SymbolKind::EnumerationLiteral
                && rootType(candidate->type) == rootType(expected)) {
                chosen = candidate;
                break;
            }
            if (candidate->kind == SymbolKind::Subprogram && candidate->parameters.empty()
                && candidate->returnType != nullptr && rootType(candidate->returnType) == rootType(expected)) {
                chosen = candidate;
                break;
            }
        }
    }
    expr->symbol = chosen;

    switch (chosen->kind) {
    case SymbolKind::Object:
    case SymbolKind::Parameter:
    case SymbolKind::LoopParameter:
        noteReference(chosen);
        expr->type = chosen->type;
        if (chosen->hasStaticValue) {
            expr->isStatic = true;
            expr->staticValue = chosen->staticValue;
            expr->staticReal = chosen->staticReal;
        }
        return expr->type;
    case SymbolKind::Number:
        expr->type = chosen->type;
        expr->isStatic = chosen->hasStaticValue;
        expr->staticValue = chosen->staticValue;
        expr->staticReal = chosen->staticReal;
        return expr->type;
    case SymbolKind::EnumerationLiteral:
        expr->type = chosen->type;
        expr->isStatic = true;
        expr->staticValue = chosen->enumerationValue;
        return expr->type;
    case SymbolKind::Subprogram:
        // A name on its own still calls, as long as nothing is left unsupplied.
        for (Symbol* parameter : chosen->parameters) {
            if (!parameter->hasDefault) {
                m_diagnostics.error(expr->location, "'" + expr->name + "' requires arguments");
                break;
            }
        }
        expr->type = chosen->returnType;
        return expr->type;
    case SymbolKind::TypeName:
        expr->type = chosen->type;
        return expr->type;
    default:
        expr->type = nullptr;
        return nullptr;
    }
}

Type* Sema::analyzeSelected(SelectedExpr* expr, Scope* scope, Type* expected)
{
    // A selected name is either a qualified entity (Package.Entity) or a record
    // component selection.
    Symbol* prefixSymbol = nullptr;
    if (expr->prefix->kind == ExprKind::Identifier) {
        auto* identifier = static_cast<IdentifierExpr*>(expr->prefix.get());
        std::vector<Symbol*> candidates = scope->lookup(identifier->lower);
        for (Symbol* candidate : candidates) {
            if (candidate->kind == SymbolKind::Package) {
                prefixSymbol = candidate;
                identifier->symbol = candidate;
                break;
            }
        }
    } else if (expr->prefix->kind == ExprKind::Selected) {
        auto* selected = static_cast<SelectedExpr*>(expr->prefix.get());
        analyzeSelected(selected, scope, nullptr);
        if (selected->symbol != nullptr && selected->symbol->kind == SymbolKind::Package) {
            prefixSymbol = selected->symbol;
        }
    }

    if (prefixSymbol != nullptr && prefixSymbol->scope != nullptr) {
        std::vector<Symbol*> candidates = prefixSymbol->scope->lookupLocal(expr->selectorLower);
        if (candidates.empty()) {
            m_diagnostics.error(expr->location, "'" + expr->selector + "' is not declared in '"
                                                    + prefixSymbol->displayName + "'");
            return nullptr;
        }
        Symbol* chosen = candidates.front();
        if (expected != nullptr) {
            for (Symbol* candidate : candidates) {
                if (candidate->type != nullptr && rootType(candidate->type) == rootType(expected)) {
                    chosen = candidate;
                    break;
                }
            }
        }
        expr->symbol = chosen;
        noteReference(chosen);
        if (chosen->kind == SymbolKind::Subprogram) {
            expr->type = chosen->returnType;
        } else {
            expr->type = chosen->type;
            if (chosen->kind == SymbolKind::EnumerationLiteral) {
                expr->isStatic = true;
                expr->staticValue = chosen->enumerationValue;
            } else if (chosen->hasStaticValue) {
                expr->isStatic = true;
                expr->staticValue = chosen->staticValue;
                expr->staticReal = chosen->staticReal;
            }
        }
        return expr->type;
    }

    Type* prefixType = analyzeExpr(expr->prefix.get(), scope, nullptr);

    if (expr->isDereference) {
        Type* access = baseType(prefixType);
        if (access == nullptr || access->kind != TypeKind::Access) {
            m_diagnostics.error(expr->location, "only an access value designates an object with '.all'");
            return nullptr;
        }
        expr->type = access->target;
        return expr->type;
    }

    Type* record = baseType(prefixType);
    if (record != nullptr && record->kind == TypeKind::Access && record->target != nullptr) {
        record = baseType(record->target);
    }
    if (record == nullptr || record->kind != TypeKind::Record) {
        m_diagnostics.error(expr->location, "'" + expr->selector + "' is not a component of a record");
        return nullptr;
    }
    if (!checkNotPrivate(record, expr->location, "'.' reaches inside a value")) {
        return nullptr;
    }
    for (const FieldInfo& field : record->fields) {
        if (field.name != expr->selectorLower) {
            continue;
        }
        expr->fieldIndex = field.index;
        expr->type = field.type;

        // A component of a variant part is only there when the discriminant
        // says so.  Where the subtype fixed it, the answer is known now; where
        // it did not, the emitter asks at run time.
        if (field.variant >= 0) {
            Type* designated = baseType(prefixType) != nullptr && baseType(prefixType)->kind == TypeKind::Access
                                   ? baseType(prefixType)->target
                                   : prefixType;
            int variant = knownVariant(designated);
            if (variant >= 0 && variant != field.variant) {
                m_diagnostics.error(expr->location, "'" + expr->selector + "' belongs to another variant of '"
                                                        + record->name + "' than the one this value has");
                return nullptr;
            }
            expr->checkedVariant = variant < 0 ? field.variant : -1;
        }
        return expr->type;
    }
    m_diagnostics.error(expr->location, "'" + expr->selector + "' is not a component of type '" + record->name + "'");
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

Type* Sema::analyzeCall(CallExpr* expr, Scope* scope, Type* expected)
{
    // Collect the entities the callee may denote.
    std::vector<Symbol*> candidates;
    if (expr->callee->kind == ExprKind::Identifier) {
        auto* identifier = static_cast<IdentifierExpr*>(expr->callee.get());
        candidates = scope->lookup(identifier->lower);
        if (candidates.empty()) {
            m_diagnostics.error(expr->callee->location, "'" + identifier->name + "' is not declared");
            return nullptr;
        }
    } else if (expr->callee->kind == ExprKind::Selected) {
        auto* selected = static_cast<SelectedExpr*>(expr->callee.get());
        Symbol* prefixSymbol = nullptr;
        if (selected->prefix->kind == ExprKind::Identifier) {
            auto* identifier = static_cast<IdentifierExpr*>(selected->prefix.get());
            for (Symbol* candidate : scope->lookup(identifier->lower)) {
                if (candidate->kind == SymbolKind::Package) {
                    prefixSymbol = candidate;
                    identifier->symbol = candidate;
                    break;
                }
            }
        } else if (selected->prefix->kind == ExprKind::Selected) {
            auto* nested = static_cast<SelectedExpr*>(selected->prefix.get());
            analyzeSelected(nested, scope, nullptr);
            if (nested->symbol != nullptr && nested->symbol->kind == SymbolKind::Package) {
                prefixSymbol = nested->symbol;
            }
        }
        if (prefixSymbol != nullptr && prefixSymbol->scope != nullptr) {
            candidates = prefixSymbol->scope->lookupLocal(selected->selectorLower);
            if (candidates.empty()) {
                m_diagnostics.error(expr->callee->location, "'" + selected->selector + "' is not declared in '"
                                                                + prefixSymbol->displayName + "'");
                return nullptr;
            }
        }
    }

    // A range as the only argument selects a slice of an array.
    if (expr->arguments.size() == 1 && expr->arguments.front().high) {
        Type* prefixType = analyzeExpr(expr->callee.get(), scope, nullptr);
        if (prefixType == nullptr || prefixType->kind != TypeKind::Array) {
            m_diagnostics.error(expr->location, "only an array can be sliced");
            return nullptr;
        }
        Expr* low = expr->arguments.front().value.get();
        Expr* high = expr->arguments.front().high.get();
        analyzeExpr(low, scope, prefixType->index);
        analyzeExpr(high, scope, prefixType->index);
        adaptUniversal(low, prefixType->index != nullptr ? prefixType->index : m_types.integerType());
        adaptUniversal(high, prefixType->index != nullptr ? prefixType->index : m_types.integerType());

        Type* result = m_types.makeSubtype(anonymousTypeName(), rootType(prefixType), 0, 0);
        result->element = prefixType->element;
        result->index = prefixType->index;
        long long lowBound = 0;
        long long highBound = 0;
        if (foldStatic(low, lowBound) && foldStatic(high, highBound)) {
            result->constrained = true;
            result->indexLow = lowBound;
            result->indexHigh = highBound;
        } else {
            result->constrained = false;
        }

        expr->form = CallForm::Slice;
        expr->resolvedArguments = { low, high };
        expr->type = result;
        return expr->type;
    }

    // Reject malformed associations before selecting a profile. In particular,
    // a repeated name must not overwrite an earlier actual argument.
    bool sawNamed = false;
    std::vector<std::string> names;
    for (const Association& association : expr->arguments) {
        if (association.nameLower.empty()) {
            if (sawNamed) {
                m_diagnostics.error(association.value->location, "a positional argument cannot follow a named argument");
                return nullptr;
            }
        } else {
            sawNamed = true;
            if (std::find(names.begin(), names.end(), association.nameLower) != names.end()) {
                m_diagnostics.error(association.value->location, "a parameter cannot be supplied more than once");
                return nullptr;
            }
            names.push_back(association.nameLower);
        }
    }

    // Result context can eliminate profiles before their arguments are resolved.
    bool hasSubprograms = std::any_of(candidates.begin(), candidates.end(), [](Symbol* candidate) {
        return candidate->kind == SymbolKind::Subprogram;
    });
    if (expected != nullptr && hasSubprograms) {
        std::erase_if(candidates, [&](Symbol* candidate) {
            return candidate->kind == SymbolKind::Subprogram
                && (candidate->returnType == nullptr || !typesCompatible(expected, candidate->returnType));
        });
        if (candidates.empty() && (expr->callee->kind == ExprKind::Identifier
                                  || expr->callee->kind == ExprKind::Selected)) {
            m_diagnostics.error(expr->location, "no visible subprogram matches this call");
            return nullptr;
        }
    }

    for (std::size_t argumentIndex = 0; argumentIndex < expr->arguments.size(); ++argumentIndex) {
        const Association& association = expr->arguments[argumentIndex];
        // An aggregate only means something once the parameter it fills is
        // known, so it waits until the profile has been chosen.
        if (association.value->kind == ExprKind::Aggregate) {
            continue;
        }
        if (association.value->type == nullptr) {
            // A shared formal type provides context to nested calls without
            // prematurely choosing between otherwise distinct overloads.
            Type* context = nullptr;
            bool differs = false;
            for (Symbol* candidate : candidates) {
                if (candidate->kind != SymbolKind::Subprogram) {
                    continue;
                }
                Symbol* parameter = nullptr;
                if (association.nameLower.empty()) {
                    if (argumentIndex < candidate->parameters.size()) {
                        parameter = candidate->parameters[argumentIndex];
                    }
                } else {
                    for (Symbol* formal : candidate->parameters) {
                        if (formal->name == association.nameLower) {
                            parameter = formal;
                            break;
                        }
                    }
                }
                if (parameter != nullptr) {
                    if (context != nullptr && rootType(context) != rootType(parameter->type)) {
                        differs = true;
                    }
                    context = parameter->type;
                }
            }
            ExprKind kind = association.value->kind;
            bool needsContext = kind == ExprKind::Call || kind == ExprKind::Identifier
                || kind == ExprKind::Selected || kind == ExprKind::Allocator || kind == ExprKind::Null;
            analyzeExpr(association.value.get(), scope, differs || !needsContext ? nullptr : context);
        }
    }

    // A type mark applied to one argument is a type conversion.
    if (candidates.size() == 1 && candidates.front()->kind == SymbolKind::TypeName) {
        if (expr->arguments.size() != 1) {
            m_diagnostics.error(expr->location, "a type conversion takes exactly one operand");
            return nullptr;
        }
        expr->form = CallForm::Conversion;
        expr->type = candidates.front()->type;
        Expr* operand = expr->arguments.front().value.get();
        expr->resolvedArguments.push_back(operand);
        // A literal only takes the type it is converted to when both belong to
        // the same family; crossing families is what the conversion is for.
        if (isReal(expr->type) == isReal(operand->type)) {
            adaptUniversal(operand, expr->type);
        }

        bool numeric = isNumeric(baseType(expr->type)) && isNumeric(baseType(operand->type));
        if (!numeric && !typesCompatible(expr->type, operand->type)) {
            m_diagnostics.error(expr->location, "this type conversion is not allowed");
        }
        if (operand->isStatic && isReal(expr->type) == isReal(operand->type)) {
            expr->isStatic = true;
            expr->staticValue = operand->staticValue;
            expr->staticReal = operand->staticReal;
        }
        return expr->type;
    }

    std::vector<Symbol*> subprograms;
    for (Symbol* candidate : candidates) {
        if (candidate->kind == SymbolKind::Subprogram) {
            subprograms.push_back(candidate);
        }
    }

    if (!subprograms.empty()) {
        Symbol* chosen = nullptr;
        for (Symbol* candidate : subprograms) {
            if (candidate->parameters.size() < expr->arguments.size()) {
                continue;
            }
            std::vector<bool> filled(candidate->parameters.size(), false);
            bool matches = true;
            for (std::size_t i = 0; i < expr->arguments.size(); ++i) {
                std::size_t index = i;
                if (!expr->arguments[i].nameLower.empty()) {
                    matches = false;
                    for (std::size_t p = 0; p < candidate->parameters.size(); ++p) {
                        if (candidate->parameters[p]->name == expr->arguments[i].nameLower) {
                            index = p;
                            matches = true;
                            break;
                        }
                    }
                    if (!matches) {
                        break;
                    }
                }
                // An argument still without a type is an aggregate, which suits
                // whichever composite parameter it lands on.
                if (expr->arguments[i].value->type != nullptr
                    && !typesCompatible(candidate->parameters[index]->type, expr->arguments[i].value->type)) {
                    matches = false;
                    break;
                }
                if (filled[index]) {
                    matches = false;
                    break;
                }
                filled[index] = true;
            }
            // Whatever the caller left out has to have a default of its own.
            for (std::size_t p = 0; matches && p < candidate->parameters.size(); ++p) {
                if (!filled[p] && !candidate->parameters[p]->hasDefault) {
                    matches = false;
                }
            }
            if (matches) {
                if (chosen != nullptr) {
                    m_diagnostics.error(expr->location, "ambiguous subprogram call");
                    return nullptr;
                }
                chosen = candidate;
            }
        }

        if (chosen == nullptr) {
            m_diagnostics.error(expr->location, "no visible subprogram matches this call");
            return nullptr;
        }

        expr->form = CallForm::Subprogram;
        expr->subprogram = chosen;
        expr->resolvedArguments.assign(chosen->parameters.size(), nullptr);
        for (std::size_t i = 0; i < expr->arguments.size(); ++i) {
            std::size_t index = i;
            if (!expr->arguments[i].nameLower.empty()) {
                for (std::size_t p = 0; p < chosen->parameters.size(); ++p) {
                    if (chosen->parameters[p]->name == expr->arguments[i].nameLower) {
                        index = p;
                        break;
                    }
                }
            }
            Expr* argument = expr->arguments[i].value.get();
            adaptUniversal(argument, chosen->parameters[index]->type);
            if (argument->kind == ExprKind::Aggregate || argument->kind == ExprKind::StringLiteral
                || argument->kind == ExprKind::Null || argument->kind == ExprKind::Allocator) {
                analyzeExpr(argument, scope, chosen->parameters[index]->type);
            }
            if (chosen->parameters[index]->mode != ParameterMode::In) {
                checkAssignable(argument, scope);
            }
            expr->resolvedArguments[index] = argument;
        }
        // Whatever the caller left out stands in as the expression its
        // declaration gives.
        for (std::size_t p = 0; p < chosen->parameters.size(); ++p) {
            if (expr->resolvedArguments[p] == nullptr) {
                expr->resolvedArguments[p] = chosen->parameters[p]->defaultExpr;
            }
        }
        expr->type = chosen->returnType;
        return expr->type;
    }

    // Otherwise this is an indexed component.
    Type* prefixType = analyzeExpr(expr->callee.get(), scope, nullptr);
    Type* array = prefixType;
    // Indexing an access value reads the array it designates, without '.all'.
    if (array != nullptr && baseType(array)->kind == TypeKind::Access) {
        array = baseType(array)->target;
    }
    if (array == nullptr || array->kind != TypeKind::Array) {
        m_diagnostics.error(expr->location, "only arrays and subprograms can be applied to arguments");
        return nullptr;
    }
    if (expr->arguments.size() != 1) {
        m_diagnostics.error(expr->location, "arrays with one index expect exactly one index value");
        return nullptr;
    }

    expr->form = CallForm::Indexing;
    Expr* index = expr->arguments.front().value.get();
    analyzeExpr(index, scope, array->index);
    adaptUniversal(index, array->index != nullptr ? array->index : m_types.integerType());
    expr->resolvedArguments.push_back(index);
    expr->type = array->element;
    (void)expected;
    return expr->type;
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
    expr->prefixType = prefixType;

    for (const ExprPtr& argument : expr->arguments) {
        // An aggregate waits for the stream attributes below, which know the
        // type it is meant to have.
        if (argument->kind != ExprKind::Aggregate) {
            analyzeExpr(argument.get(), scope, nullptr);
        }
    }

    // Subtypes carry their own constraint, so the prefix type is used as written.
    Type* base = prefixType;
    const std::string& name = expr->lower;

    if (name == "read" || name == "write" || name == "input" || name == "output") {
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
        analyzeExpr(component.value.get(), scope, target->element);
        adaptUniversal(component.value.get(), target->element);
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
        // The exponent is a whole number whatever the base is.
        Type* left = analyzeExpr(expr->left.get(), scope, expected);
        Type* right = analyzeExpr(expr->right.get(), scope, m_types.integerType());
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

// ---------------------------------------------------------------------------
// Names and types
// ---------------------------------------------------------------------------

Symbol* Sema::lookupName(const std::string& lower, Scope* scope)
{
    std::vector<std::string> names = splitDottedName(lower);

    std::vector<Symbol*> candidates = scope->lookup(names.front());
    if (candidates.empty()) {
        return nullptr;
    }
    Symbol* symbol = candidates.front();
    for (Symbol* candidate : candidates) {
        if (names.size() > 1 && candidate->kind == SymbolKind::Package) {
            symbol = candidate;
            break;
        }
    }

    for (std::size_t i = 1; i < names.size(); ++i) {
        if (symbol->scope == nullptr) {
            return nullptr;
        }
        std::vector<Symbol*> nested = symbol->scope->lookupLocal(names[i]);
        if (nested.empty()) {
            return nullptr;
        }
        symbol = nested.front();
    }
    return symbol;
}

std::vector<Symbol*> Sema::lookupAll(const std::string& lower, Scope* scope)
{
    return scope->lookup(lower);
}

Type* Sema::resolveTypeName(const std::string& lower, Scope* scope, const SourceLocation& location)
{
    Symbol* symbol = lookupName(lower, scope);
    if (symbol == nullptr) {
        m_diagnostics.error(location, "'" + lower + "' is not declared");
        return nullptr;
    }
    if (symbol->kind != SymbolKind::TypeName) {
        m_diagnostics.error(location, "'" + symbol->displayName + "' is not a type");
        return nullptr;
    }
    return symbol->type;
}

Type* Sema::resolveSubtypeIndication(SubtypeIndication* indication, Scope* scope)
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
        analyzeExpr(indication->indexLows.front().get(), scope, array->index);
        long long low = 0;
        long long high = 0;
        bool ok = foldStatic(indication->indexLows.front().get(), low);
        if (indication->indexHighs.front()) {
            analyzeExpr(indication->indexHighs.front().get(), scope, array->index);
            ok = ok && foldStatic(indication->indexHighs.front().get(), high);
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
        return rootType(left->element) == rootType(right->element);
    }
    return false;
}

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
