#include "Sema.h"
#include "SemaSupport.h"

using SemaSupport::adaptUniversal;

void Sema::analyzeDeclarativePart(DeclList& declarations, Scope* scope, bool reportIncomplete)
{
    for (const DeclPtr& decl : declarations) {
        analyzeDecl(decl.get(), scope);
    }
    if (reportIncomplete) {
        reportIncompleteTypes(declarations);
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
    Type* type = resolveSubtypeIndication(decl->subtype.get(), scope, m_currentSubprogram != nullptr);

    if (type != nullptr && type->kind == TypeKind::Array && !type->constrained) {
        if (m_currentSubprogram == nullptr) {
            m_diagnostics.error(decl->location, "an unconstrained array object is currently supported only inside a subprogram");
        } else if (decl->subtype->indexLows.empty() && !decl->initializer && type->m_boundsSymbol == nullptr) {
            m_diagnostics.error(decl->location, "an unconstrained array object needs an initializer or index constraint");
        }
    }

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

        if (!scope->lookupLocal(decl->namesLower[i]).empty()) {
            m_diagnostics.error(decl->location, "'" + decl->names[i] + "' has already been declared in this scope");
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

Symbol* Sema::declareSubprogram(SubprogramSpec& spec, Scope* scope, bool isBody)
{
    std::vector<Type*> parameterTypes;
    for (ParameterDecl& parameter : spec.parameters) {
        parameterTypes.push_back(resolveSubtypeIndication(parameter.subtype.get(), scope));
    }
    Type* returnType = spec.isFunction ? resolveSubtypeIndication(spec.returnType.get(), scope) : nullptr;

    if (spec.lower == "**") {
        if (!spec.isFunction || spec.parameters.size() != 2
            || spec.parameters[0].mode != ParameterMode::In
            || spec.parameters[1].mode != ParameterMode::In
            || spec.parameters[0].defaultValue || spec.parameters[1].defaultValue) {
            m_diagnostics.error(spec.location, "'**' requires two in parameters without defaults");
        }
    }
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
    if (symbol->owner != nullptr) {
        // Every intervening lexical level needs a link, even when it has no
        // captured variables of its own. Defaults can read through that level.
        symbol->owner->needsFrame = true;
    }

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
            if (declaration.mode != ParameterMode::In) {
                m_diagnostics.error(declaration.location, "only an in parameter can have a default expression");
            }
            Symbol* savedSubprogram = m_currentSubprogram;
            m_currentSubprogram = symbol;
            Type* defaultType = analyzeExpr(declaration.defaultValue.get(), scope, parameterTypes[i]);
            m_currentSubprogram = savedSubprogram;
            if (!typesCompatible(parameterTypes[i], defaultType)) {
                m_diagnostics.error(declaration.defaultValue->location, "the default expression has an incompatible type");
            }
            adaptUniversal(declaration.defaultValue.get(), parameterTypes[i]);
            parameter->hasDefault = true;
            parameter->defaultExpr = declaration.defaultValue.get();
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
    int savedHandlerDepth = m_handlerDepth;
    m_handlerDepth = 0;
    m_namePrefix.push_back(symbol->name);

    analyzeDeclarativePart(body->declarations, inner);
    analyzeStatements(body->body, inner);
    analyzeHandlers(body->handlers, inner);

    m_namePrefix.pop_back();
    m_currentSubprogram = savedSubprogram;
    m_handlerDepth = savedHandlerDepth;
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
