#pragma once

#include "Symbol.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Scope
{
public:
    explicit Scope(Scope* parent = nullptr)
        : m_parent(parent)
    {
    }

    void add(Symbol* symbol);
    void addUseScope(Scope* scope);

    std::vector<Symbol*> lookup(const std::string& name) const;
    std::vector<Symbol*> lookupLocal(const std::string& name) const;

    Scope* parent() const { return m_parent; }

private:
    Scope* m_parent = nullptr;
    std::unordered_map<std::string, std::vector<Symbol*>> m_symbols;
    std::vector<Scope*> m_useScopes;
};

// Owns every symbol and scope created while analysing a compilation.
class SymbolTable
{
public:
    Symbol* createSymbol(SymbolKind kind, const std::string& lowerName, const std::string& displayName);
    Scope* createScope(Scope* parent);

private:
    std::vector<std::unique_ptr<Symbol>> m_symbols;
    std::vector<std::unique_ptr<Scope>> m_scopes;
};
