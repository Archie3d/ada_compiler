#include "Scope.h"

void Scope::add(Symbol* symbol)
{
    m_symbols[symbol->name].push_back(symbol);
}

void Scope::addUseScope(Scope* scope)
{
    for (Scope* existing : m_useScopes) {
        if (existing == scope) {
            return;
        }
    }
    m_useScopes.push_back(scope);
}

std::vector<Symbol*> Scope::lookupLocal(const std::string& name) const
{
    auto it = m_symbols.find(name);
    if (it == m_symbols.end()) {
        return {};
    }
    return it->second;
}

namespace {

bool isOverloadable(const Symbol* symbol)
{
    return symbol->kind == SymbolKind::Subprogram || symbol->kind == SymbolKind::EnumerationLiteral;
}

}  // namespace

// A name that cannot be overloaded is hidden by the innermost declaration of
// it, but subprograms and enumeration literals gather from every enclosing
// scope at once: that is what lets a Put on one instance of Integer_IO be
// called where another instance is also in use.
std::vector<Symbol*> Scope::lookup(const std::string& name) const
{
    std::vector<Symbol*> candidates;

    for (const Scope* scope = this; scope != nullptr; scope = scope->m_parent) {
        std::vector<Symbol*> level = scope->lookupLocal(name);
        if (level.empty()) {
            for (const Scope* used : scope->m_useScopes) {
                std::vector<Symbol*> fromUse = used->lookupLocal(name);
                for (Symbol* symbol : fromUse) {
                    level.push_back(symbol);
                }
            }
        }

        bool hides = false;
        for (Symbol* symbol : level) {
            if (!isOverloadable(symbol)) {
                hides = true;
            }
            bool seen = false;
            for (Symbol* existing : candidates) {
                if (existing == symbol) {
                    seen = true;
                    break;
                }
            }
            if (!seen) {
                candidates.push_back(symbol);
            }
        }
        if (hides && !candidates.empty()) {
            break;
        }
    }

    return candidates;
}

Symbol* SymbolTable::createSymbol(SymbolKind kind, const std::string& lowerName, const std::string& displayName)
{
    auto symbol = std::make_unique<Symbol>();
    symbol->kind = kind;
    symbol->name = lowerName;
    symbol->displayName = displayName;
    m_symbols.push_back(std::move(symbol));
    return m_symbols.back().get();
}

Scope* SymbolTable::createScope(Scope* parent)
{
    m_scopes.push_back(std::make_unique<Scope>(parent));
    return m_scopes.back().get();
}
