#include "QbeEmitter.h"
#include "QbeSupport.h"

using QbeSupport::isUnconstrainedArray;

void QbeEmitter::collectGlobals(DeclList& declarations)
{
    for (const DeclPtr& decl : declarations) {
        switch (decl->kind) {
        case DeclKind::Object: {
            auto* object = static_cast<ObjectDecl*>(decl.get());
            if (object->awaitsValue) {
                // The declaration in the private part is the one that counts.
                break;
            }
            for (Symbol* symbol : object->symbols) {
                if (!symbol->isGlobal) {
                    continue;
                }
                long long size = typeSize(symbol->type);
                long long alignment = typeAlignment(symbol->type);
                m_data << "data " << symbol->qbeName << " = align " << (alignment < 1 ? 1 : alignment) << " { z "
                       << (size < 1 ? 1 : size) << " }\n";
            }
            break;
        }
        case DeclKind::PackageSpecification: {
            auto* package = static_cast<PackageSpecDecl*>(decl.get());
            collectGlobals(package->publicPart);
            collectGlobals(package->privatePart);
            break;
        }
        case DeclKind::PackageBody: {
            auto* package = static_cast<PackageBodyDecl*>(decl.get());
            collectGlobals(package->declarations);
            break;
        }
        case DeclKind::GenericInstantiation:
            collectGlobals(static_cast<GenericInstantiationDecl*>(decl.get())->expansion);
            break;
        default:
            break;
        }
    }
}

void QbeEmitter::emitElaborationDeclarations(DeclList& declarations)
{
    for (const DeclPtr& decl : declarations) {
        if (decl->kind == DeclKind::Object) {
            auto* object = static_cast<ObjectDecl*>(decl.get());
            if (object->awaitsValue) {
                continue;
            }
            for (Symbol* symbol : object->symbols) {
                if (!symbol->isGlobal) {
                    continue;
                }
                Value address { symbol->qbeName, 'l' };
                if (object->initializer) {
                    assignInto(address, symbol->type, object->initializer.get());
                } else {
                    emitDefaultInit(address, symbol->type);
                }
            }
        } else if (decl->kind == DeclKind::PackageBody) {
            auto* package = static_cast<PackageBodyDecl*>(decl.get());
            // Declaration failures bypass this package's handlers, but may
            // reach a surrounding handled sequence containing the package.
            emitElaborationDeclarations(package->declarations);
            if (package->handlers.empty()) {
                emitStatements(package->body);
            } else {
                std::string dispatch = newLabel("packagehandler");
                std::string after = newLabel("packagehandled");
                m_context->handlerStorage[dispatch] = storageCheckpoint();
                m_context->handlerLabels.push_back(dispatch);
                emitStatements(package->body);
                m_context->handlerLabels.pop_back();
                jump(after);
                emitHandlers(package->handlers, after, dispatch);
                label(after);
            }
        } else if (decl->kind == DeclKind::PackageSpecification) {
            auto* package = static_cast<PackageSpecDecl*>(decl.get());
            emitElaborationDeclarations(package->publicPart);
            emitElaborationDeclarations(package->privatePart);
        } else if (decl->kind == DeclKind::GenericInstantiation) {
            emitElaborationDeclarations(static_cast<GenericInstantiationDecl*>(decl.get())->expansion);
        }
    }
}

void QbeEmitter::emitLocalDeclarations(DeclList& declarations)
{
    std::string temporaryMark = newTemp();
    line(temporaryMark + " =l loadl " + storageArena(true));
    for (const DeclPtr& decl : declarations) {
        switch (decl->kind) {
        case DeclKind::Type: {
            auto* typeDecl = static_cast<TypeDecl*>(decl.get());
            if (typeDecl->definition != nullptr && typeDecl->definition->kind == TypeDefKind::Array) {
                emitTypeBounds(typeDecl->declaredType, decl->location);
            }
            break;
        }
        case DeclKind::Subtype:
            emitTypeBounds(static_cast<SubtypeDecl*>(decl.get())->declaredType, decl->location);
            break;
        case DeclKind::Object: {
            auto* object = static_cast<ObjectDecl*>(decl.get());
            if (object->awaitsValue) {
                break;
            }
            for (Symbol* symbol : object->symbols) {
                if (symbol->isGlobal) {
                    continue;
                }
                if (isUnconstrainedArray(symbol->type)) {
                    emitDynamicArray(object, symbol);
                    continue;
                }
                long long size = typeSize(symbol->type);
                if (size < 1) {
                    size = 1;
                }
                if (symbol->isUplevel) {
                    symbol->frameOffset = m_context->frameSize;
                    m_context->frameSize += size;
                } else {
                    std::string slot = "%v." + symbol->name + "." + std::to_string(m_tempCounter++);
                    m_context->prologue << "    " << slot << " =l " << (size <= 4 ? "alloc4" : "alloc8") << " "
                                        << size << "\n";
                    m_context->locals[symbol] = slot;
                }
                if (object->initializer) {
                    assignInto(addressOf(symbol), symbol->type, object->initializer.get());
                } else {
                    if (needsZeroInit(symbol->type)) {
                        // A file handle has to read as closed before anything
                        // opens it, so its storage cannot be left as it was
                        // found.
                        Value slot = addressOf(symbol);
                        line("call $memset(l " + slot.name + ", w 0, l " + std::to_string(size) + ")");
                    }
                    emitDefaultInit(addressOf(symbol), symbol->type);
                }
            }
            break;
        }
        case DeclKind::SubprogramBody:
            m_context->nested.push_back(static_cast<SubprogramBody*>(decl.get()));
            break;
        case DeclKind::PackageSpecification: {
            auto* package = static_cast<PackageSpecDecl*>(decl.get());
            emitLocalDeclarations(package->publicPart);
            emitLocalDeclarations(package->privatePart);
            break;
        }
        case DeclKind::PackageBody: {
            auto* package = static_cast<PackageBodyDecl*>(decl.get());
            emitLocalDeclarations(package->declarations);
            if (package->handlers.empty()) {
                emitStatements(package->body);
            } else {
                std::string dispatch = newLabel("packagehandler");
                std::string after = newLabel("packagehandled");
                m_context->handlerStorage[dispatch] = storageCheckpoint();
                m_context->handlerLabels.push_back(dispatch);
                emitStatements(package->body);
                m_context->handlerLabels.pop_back();
                jump(after);
                emitHandlers(package->handlers, after, dispatch);
                label(after);
            }
            break;
        }
        case DeclKind::GenericInstantiation:
            emitLocalDeclarations(static_cast<GenericInstantiationDecl*>(decl.get())->expansion);
            break;
        default:
            break;
        }
    }
    if (!m_context->terminated) {
        line("call $__ada_array_rewind(l " + storageArena(true) + ", l " + temporaryMark + ")");
    }
}
