#include "Sema.h"

#include "Lexer.h"

namespace
{

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

Sema::Sema(Diagnostics& diagnostics)
    : m_diagnostics(diagnostics)
{
    setupStandardScope();
}

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

std::string Sema::mangle(const std::string& name) const
{
    std::string result;
    for (const std::string& part : m_namePrefix) {
        result += part + "__";
    }
    result += name == "**" ? "operator_power" : name;
    return result;
}

std::string Sema::anonymousTypeName()
{
    return "anon." + std::to_string(m_anonymousCounter++);
}
