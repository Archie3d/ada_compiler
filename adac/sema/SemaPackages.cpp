#include "Sema.h"
#include "SemaSupport.h"

using SemaSupport::splitDottedName;

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

    int savedHandlerDepth = m_handlerDepth;
    m_handlerDepth = 0;
    m_packages.push_back(symbol);
    analyzeDeclarativePart(decl->declarations, symbol->scope);
    analyzeStatements(decl->body, symbol->scope);
    analyzeHandlers(decl->handlers, symbol->scope);
    m_packages.pop_back();
    m_handlerDepth = savedHandlerDepth;

    for (std::size_t i = 0; i < pushed; ++i) {
        m_namePrefix.pop_back();
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
