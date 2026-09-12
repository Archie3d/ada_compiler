#include "QbeEmitter.h"

#include "Lexer.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <limits>

namespace
{

Value constantValue(long long value, char type)
{
    return Value { std::to_string(value), type };
}

// QBE reads these with scanf, so the full precision of the value is written out
// rather than the six decimals std::to_string would give.
std::string realLiteral(double value, char type)
{
    char buffer[40];
    std::snprintf(buffer, sizeof buffer, "%.17g", value);
    return (type == 's' ? "s_" : "d_") + std::string(buffer);
}

bool isFloatClass(char type)
{
    return type == 's' || type == 'd';
}

bool isUnconstrainedArray(const Type* type)
{
    return type != nullptr && type->kind == TypeKind::Array && !type->constrained;
}

bool isLiteralOperand(const std::string& operand)
{
    if (operand.empty()) {
        return false;
    }
    std::size_t start = operand[0] == '-' ? 1 : 0;
    if (start >= operand.size()) {
        return false;
    }
    for (std::size_t i = start; i < operand.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(operand[i])) == 0) {
            return false;
        }
    }
    return true;
}

std::string upperCase(const std::string& text)
{
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return result;
}

// Encodes an Ada string as a QBE data body, keeping printable runs quoted.
std::string encodeString(const std::string& text)
{
    std::string result;
    bool inQuotes = false;
    auto closeQuotes = [&]() {
        if (inQuotes) {
            result += "\"";
            inQuotes = false;
        }
    };

    for (unsigned char c : text) {
        bool printable = c >= 0x20 && c < 0x7f && c != '"' && c != '\\';
        if (printable) {
            if (!inQuotes) {
                if (!result.empty()) {
                    result += " ";
                }
                result += "\"";
                inQuotes = true;
            }
            result.push_back(static_cast<char>(c));
        } else {
            closeQuotes();
            if (!result.empty()) {
                result += " ";
            }
            result += std::to_string(static_cast<int>(c));
        }
    }
    closeQuotes();
    if (!result.empty()) {
        result += " ";
    }
    result += "0";
    return result;
}

const char* comparisonInstruction(BinaryOp op, char type)
{
    bool isFloat = type == 's' || type == 'd';
    switch (op) {
    case BinaryOp::Equal:
        return isFloat ? (type == 's' ? "ceqs" : "ceqd") : (type == 'l' ? "ceql" : "ceqw");
    case BinaryOp::NotEqual:
        return isFloat ? (type == 's' ? "cnes" : "cned") : (type == 'l' ? "cnel" : "cnew");
    case BinaryOp::Less:
        return isFloat ? (type == 's' ? "clts" : "cltd") : (type == 'l' ? "csltl" : "csltw");
    case BinaryOp::LessEqual:
        return isFloat ? (type == 's' ? "cles" : "cled") : (type == 'l' ? "cslel" : "cslew");
    case BinaryOp::Greater:
        return isFloat ? (type == 's' ? "cgts" : "cgtd") : (type == 'l' ? "csgtl" : "csgtw");
    case BinaryOp::GreaterEqual:
        return isFloat ? (type == 's' ? "cges" : "cged") : (type == 'l' ? "csgel" : "csgew");
    default:
        return "ceqw";
    }
}

}

QbeEmitter::QbeEmitter(Sema& sema, Diagnostics& diagnostics)
    : m_sema(sema)
    , m_diagnostics(diagnostics)
{
}

// ---------------------------------------------------------------------------
// Output helpers
// ---------------------------------------------------------------------------

std::string QbeEmitter::newTemp()
{
    return "%.t" + std::to_string(m_tempCounter++);
}

std::string QbeEmitter::newLabel(const char* prefix)
{
    return "@" + std::string(prefix) + "." + std::to_string(m_labelCounter++);
}

void QbeEmitter::line(const std::string& text)
{
    if (m_context->terminated) {
        m_context->body << newLabel("unreachable") << "\n";
        m_context->terminated = false;
    }
    m_context->body << "    " << text << "\n";
}

void QbeEmitter::label(const std::string& name)
{
    if (!m_context->terminated) {
        m_context->body << "    jmp " << name << "\n";
    }
    m_context->body << name << "\n";
    m_context->terminated = false;
}

void QbeEmitter::jump(const std::string& target)
{
    if (!m_context->terminated) {
        m_context->body << "    jmp " << target << "\n";
        m_context->terminated = true;
    }
}

void QbeEmitter::branch(const Value& condition, const std::string& ifTrue, const std::string& ifFalse)
{
    line("jnz " + condition.name + ", " + ifTrue + ", " + ifFalse);
    m_context->terminated = true;
}

std::string QbeEmitter::stringData(const std::string& text)
{
    auto it = m_stringPool.find(text);
    if (it != m_stringPool.end()) {
        return it->second;
    }
    std::string name = "$.str." + std::to_string(m_dataCounter++);
    m_data << "data " << name << " = { b " << encodeString(text) << " }\n";
    m_stringPool.emplace(text, name);
    return name;
}

// The literal names of an enumeration type, laid down once as an array of C
// strings so that the run time can write one out or read one back.
std::string QbeEmitter::enumTableFor(const Type* type)
{
    const Type* base = type;
    while (base->base != nullptr) {
        base = base->base;
    }

    auto it = m_enumTables.find(base);
    if (it != m_enumTables.end()) {
        return it->second;
    }

    std::string table = "$.enum." + std::to_string(m_dataCounter++);
    std::vector<std::string> names;
    for (std::size_t i = 0; i < base->literals.size(); ++i) {
        std::string name = table + "." + std::to_string(i);
        m_data << "data " << name << " = { b " << encodeString(base->literals[i]) << ", b 0 }\n";
        names.push_back(name);
    }

    m_data << "data " << table << " = {";
    for (std::size_t i = 0; i < names.size(); ++i) {
        m_data << (i > 0 ? ", l " : " l ") << names[i];
    }
    m_data << " }\n";

    m_enumTables.emplace(base, table);
    return table;
}

std::string QbeEmitter::allocScratch(long long size)
{
    std::string temp = newTemp();
    const char* instruction = size <= 4 ? "alloc4" : "alloc8";
    m_context->prologue << "    " << temp << " =l " << instruction << " " << size << "\n";
    return temp;
}

// ---------------------------------------------------------------------------
// Program structure
// ---------------------------------------------------------------------------

void QbeEmitter::emit(const std::vector<CompilationUnit*>& units, std::ostream& out)
{
    // The run time library reports failures through these, so they have to be
    // visible to the linker and not just to the generated code.
    m_data << "export data $__ada_exception = align 4 { z 4 }\n";
    m_data << "export data $__ada_exception_name = align 8 { z 8 }\n";

    // A generic package instantiated inside a subprogram is still elaborated
    // once, with the library, so it joins what the units themselves declare.
    const std::vector<GenericInstantiationDecl*>& instances = m_sema.libraryInstances();

    for (CompilationUnit* unit : units) {
        collectGlobals(unit->units);
    }
    for (GenericInstantiationDecl* instance : instances) {
        collectGlobals(instance->expansion);
    }

    emitElaboration(units);

    for (CompilationUnit* unit : units) {
        emitSubprogramsIn(unit->units);
    }
    for (GenericInstantiationDecl* instance : instances) {
        emitSubprogramsIn(instance->expansion);
    }

    emitMain();

    out << m_data.str() << "\n";
    for (const std::string& function : m_functions) {
        out << function << "\n";
    }
}

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
                if (object->initializer) {
                    m_globalInitializers.emplace_back(symbol, object->initializer.get());
                } else if (hasComponentDefaults(symbol->type)) {
                    // No value of its own, but its components have theirs.
                    m_globalInitializers.emplace_back(symbol, nullptr);
                }
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
        if (decl->kind == DeclKind::PackageBody) {
            auto* package = static_cast<PackageBodyDecl*>(decl.get());
            emitElaborationDeclarations(package->declarations);
            emitStatements(package->body);
        } else if (decl->kind == DeclKind::PackageSpecification) {
            auto* package = static_cast<PackageSpecDecl*>(decl.get());
            emitElaborationDeclarations(package->publicPart);
            emitElaborationDeclarations(package->privatePart);
        } else if (decl->kind == DeclKind::GenericInstantiation) {
            emitElaborationDeclarations(static_cast<GenericInstantiationDecl*>(decl.get())->expansion);
        }
    }
}

void QbeEmitter::emitElaboration(const std::vector<CompilationUnit*>& units)
{
    FunctionContext context;
    context.propagateLabel = newLabel("propagate");
    FunctionContext* saved = m_context;
    m_context = &context;

    for (const auto& initializer : m_globalInitializers) {
        Value address { initializer.first->qbeName, 'l' };
        if (initializer.second != nullptr) {
            assignInto(address, initializer.first->type, initializer.second);
        } else {
            emitDefaultInit(address, initializer.first->type);
        }
    }
    for (GenericInstantiationDecl* instance : m_sema.libraryInstances()) {
        emitElaborationDeclarations(instance->expansion);
    }
    for (CompilationUnit* unit : units) {
        emitElaborationDeclarations(unit->units);
    }

    finishFunction("function $__ada_elaborate()");
    m_context = saved;
}

void QbeEmitter::emitSubprogramsIn(DeclList& declarations)
{
    for (const DeclPtr& decl : declarations) {
        switch (decl->kind) {
        case DeclKind::SubprogramBody:
            emitSubprogram(static_cast<SubprogramBody*>(decl.get()));
            break;
        case DeclKind::PackageSpecification: {
            auto* package = static_cast<PackageSpecDecl*>(decl.get());
            emitSubprogramsIn(package->publicPart);
            emitSubprogramsIn(package->privatePart);
            break;
        }
        case DeclKind::PackageBody: {
            auto* package = static_cast<PackageBodyDecl*>(decl.get());
            emitSubprogramsIn(package->declarations);
            break;
        }
        case DeclKind::GenericInstantiation:
            emitSubprogramsIn(static_cast<GenericInstantiationDecl*>(decl.get())->expansion);
            break;
        default:
            break;
        }
    }
}

void QbeEmitter::finishFunction(const std::string& signature)
{
    FunctionContext& context = *m_context;

    if (!context.terminated) {
        if (context.symbol != nullptr && context.symbol->returnType != nullptr) {
            // A missing result is a failure for every result representation.
            line("call $__ada_raise(w 2)");
            char type = qbeClass(context.symbol->returnType);
            if (isComposite(context.symbol->returnType)) {
                line("ret");
            } else if (type == 's' || type == 'd') {
                std::string zero = newTemp();
                line(zero + " =" + std::string(1, type) + " copy " + (type == 's' ? "s_0" : "d_0"));
                line("ret " + zero);
            } else {
                line("ret 0");
            }
        } else {
            line("ret");
        }
        context.terminated = true;
    }

    if (context.usesPropagate) {
        context.body << context.propagateLabel << "\n";
        if (context.symbol != nullptr && context.symbol->returnType != nullptr) {
            char type = qbeClass(context.symbol->returnType);
            if (isComposite(context.symbol->returnType)) {
                context.body << "    ret\n";
            } else if (type == 's' || type == 'd') {
                context.body << "    ret " << (type == 's' ? "s_0" : "d_0") << "\n";
            } else {
                context.body << "    ret 0\n";
            }
        } else {
            context.body << "    ret\n";
        }
    }

    std::string text = signature + " {\n@start\n";
    if (context.hasFrame) {
        text += "    " + context.frameTemp + " =l alloc8 " + std::to_string(context.frameSize) + "\n";
        if (context.symbol != nullptr && context.symbol->level > 0) {
            text += "    storel %.link, " + context.frameTemp + "\n";
        } else {
            text += "    storel 0, " + context.frameTemp + "\n";
        }
    }
    text += context.prologue.str();
    text += context.body.str();
    text += "}\n";
    m_functions.push_back(text);
}

void QbeEmitter::emitSubprogram(SubprogramBody* body)
{
    Symbol* symbol = body->symbol;
    if (symbol == nullptr) {
        return;
    }

    FunctionContext context;
    context.symbol = symbol;
    context.hasFrame = symbol->needsFrame;
    context.frameTemp = "%.frame";
    context.propagateLabel = newLabel("propagate");
    FunctionContext* saved = m_context;
    m_context = &context;

    std::string signature = "function ";
    if (symbol->returnType != nullptr && !isComposite(symbol->returnType)) {
        signature += std::string(1, qbeClass(symbol->returnType)) + " ";
    }
    signature += symbol->qbeName + "(";
    bool first = true;
    if (isComposite(symbol->returnType)) {
        signature += "l %.result";
        first = false;
    }
    if (symbol->level > 0) {
        signature += (first ? "" : ", ") + std::string("l %.link");
        first = false;
    }
    for (Symbol* parameter : symbol->parameters) {
        if (!first) {
            signature += ", ";
        }
        first = false;
        char type = parameter->byReference ? 'l' : qbeClass(parameter->type);
        signature += std::string(1, type) + " %p." + parameter->name;
        if (isUnconstrainedArray(parameter->type)) {
            signature += ", w %p." + parameter->name + ".first, w %p." + parameter->name + ".last";
        }
    }
    signature += ")";

    for (Symbol* parameter : symbol->parameters) {
        std::string incoming = "%p." + parameter->name;
        bool unconstrained = isUnconstrainedArray(parameter->type);
        if (parameter->isUplevel) {
            parameter->frameOffset = context.frameSize;
            long long slotSize = parameter->byReference ? 8 : typeSize(parameter->type);
            if (unconstrained) {
                slotSize = 16;   // Pointer followed by the two bounds.
            }
            context.frameSize += slotSize;
            std::string address = newTemp();
            context.prologue << "    " << address << " =l add " << context.frameTemp << ", "
                             << parameter->frameOffset << "\n";
            if (parameter->byReference) {
                context.prologue << "    storel " << incoming << ", " << address << "\n";
            } else {
                context.prologue << "    " << qbeStoreInstruction(parameter->type) << " " << incoming << ", "
                                 << address << "\n";
            }
            if (unconstrained) {
                std::string firstAddress = newTemp();
                std::string lastAddress = newTemp();
                context.prologue << "    " << firstAddress << " =l add " << context.frameTemp << ", "
                                 << parameter->frameOffset + 8 << "\n";
                context.prologue << "    storew " << incoming << ".first, " << firstAddress << "\n";
                context.prologue << "    " << lastAddress << " =l add " << context.frameTemp << ", "
                                 << parameter->frameOffset + 12 << "\n";
                context.prologue << "    storew " << incoming << ".last, " << lastAddress << "\n";
            }
            continue;
        }
        if (parameter->byReference) {
            context.locals[parameter] = incoming;
            if (unconstrained) {
                context.bounds[parameter] = { incoming + ".first", incoming + ".last" };
            }
            continue;
        }
        std::string slot = "%v." + parameter->name + "." + std::to_string(m_tempCounter++);
        long long size = typeSize(parameter->type);
        context.prologue << "    " << slot << " =l " << (size <= 4 ? "alloc4" : "alloc8") << " "
                         << (size < 1 ? 1 : size) << "\n";
        context.prologue << "    " << qbeStoreInstruction(parameter->type) << " " << incoming << ", " << slot
                         << "\n";
        context.locals[parameter] = slot;
    }

    emitLocalDeclarations(body->declarations);

    if (body->handlers.empty()) {
        emitStatements(body->body);
    } else {
        std::string dispatch = newLabel("handler");
        std::string after = newLabel("handled");
        context.handlerLabels.push_back(dispatch);
        emitStatements(body->body);
        context.handlerLabels.pop_back();
        jump(after);
        emitHandlers(body->handlers, after, dispatch);
        label(after);
    }

    finishFunction(signature);
    std::vector<SubprogramBody*> nested = context.nested;
    m_context = saved;

    for (SubprogramBody* inner : nested) {
        emitSubprogram(inner);
    }
}

void QbeEmitter::emitMain()
{
    Symbol* main = m_sema.mainSubprogram();

    FunctionContext context;
    context.propagateLabel = newLabel("propagate");
    FunctionContext* saved = m_context;
    m_context = &context;

    std::string unhandled = newLabel("unhandled");
    std::string elaborated = newLabel("elaborated");
    line("call $__ada_elaborate()");
    std::string elaborationStatus = newTemp();
    line(elaborationStatus + " =w loadsw $__ada_exception");
    branch(Value { elaborationStatus, 'w' }, unhandled, elaborated);
    label(elaborated);
    if (main != nullptr) {
        line("call " + main->qbeName + "()");
    }

    std::string done = newLabel("done");
    std::string status = newTemp();
    line(status + " =w loadsw $__ada_exception");
    branch(Value { status, 'w' }, unhandled, done);

    label(unhandled);
    std::string name = newTemp();
    line(name + " =l loadl $__ada_exception_name");
    line("call $__ada_unhandled(l " + name + ")");
    line("ret 1");
    m_context->terminated = true;

    label(done);
    line("ret 0");
    m_context->terminated = true;

    finishFunction("export function w $main()");
    m_context = saved;
}

// ---------------------------------------------------------------------------
// Declarations inside a subprogram
// ---------------------------------------------------------------------------

void QbeEmitter::emitLocalDeclarations(DeclList& declarations)
{
    for (const DeclPtr& decl : declarations) {
        switch (decl->kind) {
        case DeclKind::Object: {
            auto* object = static_cast<ObjectDecl*>(decl.get());
            if (object->awaitsValue) {
                break;
            }
            for (Symbol* symbol : object->symbols) {
                if (symbol->isGlobal) {
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
        case DeclKind::GenericInstantiation: {
            // An instance of a generic package belongs to the library rather
            // than to the subprogram it was written in, and is emitted there.
            auto* instance = static_cast<GenericInstantiationDecl*>(decl.get());
            if (!instance->isPackage) {
                emitLocalDeclarations(instance->expansion);
            }
            break;
        }
        default:
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

void QbeEmitter::emitStatements(StmtList& statements)
{
    for (const StmtPtr& statement : statements) {
        emitStatement(statement.get());
    }
}

void QbeEmitter::emitRaise(Symbol* exception, const SourceLocation& location)
{
    if (exception == nullptr) {
        if (m_context->activeExceptions.empty()) {
            m_diagnostics.error(location, "internal error: bare raise without an active handler");
            return;
        }
        const auto& occurrence = m_context->activeExceptions.back();
        line("storew " + occurrence.first + ", $__ada_exception");
        line("storel " + occurrence.second + ", $__ada_exception_name");
    } else {
        line("storew " + std::to_string(exception->exceptionId) + ", $__ada_exception");
        line("storel " + stringData(upperCase(exception->displayName)) + ", $__ada_exception_name");
    }
    if (!m_context->handlerLabels.empty()) {
        jump(m_context->handlerLabels.back());
    } else {
        m_context->usesPropagate = true;
        jump(m_context->propagateLabel);
    }
}

void QbeEmitter::emitExceptionCheck()
{
    std::string status = newTemp();
    std::string next = newLabel("nothrow");
    line(status + " =w loadsw $__ada_exception");
    if (!m_context->handlerLabels.empty()) {
        branch(Value { status, 'w' }, m_context->handlerLabels.back(), next);
    } else {
        m_context->usesPropagate = true;
        branch(Value { status, 'w' }, m_context->propagateLabel, next);
    }
    label(next);
}

void QbeEmitter::emitHandlers(std::vector<ExceptionHandler>& handlers, const std::string& afterLabel,
                              const std::string& dispatchLabel)
{
    label(dispatchLabel);
    std::string status = newTemp();
    line(status + " =w loadsw $__ada_exception");

    // Save the occurrence before clearing the pending status. Nested handlers
    // and calls may replace both globals while this handler remains active.
    std::string name = newTemp();
    line(name + " =l loadl $__ada_exception_name");
    std::vector<std::string> bodyLabels;
    for (std::size_t i = 0; i < handlers.size(); ++i) {
        bodyLabels.push_back(newLabel("handle"));
    }

    for (std::size_t i = 0; i < handlers.size(); ++i) {
        ExceptionHandler& handler = handlers[i];
        if (handler.isOthers) {
            jump(bodyLabels[i]);
            break;
        }
        std::string match;
        for (std::size_t k = 0; k < handler.identifiers.size(); ++k) {
            std::string test = newTemp();
            line(test + " =w ceqw " + status + ", " + std::to_string(handler.identifiers[k]));
            if (match.empty()) {
                match = test;
            } else {
                std::string combined = newTemp();
                line(combined + " =w or " + match + ", " + test);
                match = combined;
            }
        }
        std::string next = newLabel("nexthandler");
        if (match.empty()) {
            jump(next);
        } else {
            branch(Value { match, 'w' }, bodyLabels[i], next);
        }
        label(next);
    }

    if (!m_context->terminated) {
        if (m_context->handlerLabels.empty()) {
            m_context->usesPropagate = true;
            jump(m_context->propagateLabel);
        } else {
            jump(m_context->handlerLabels.back());
        }
    }

    for (std::size_t i = 0; i < handlers.size(); ++i) {
        label(bodyLabels[i]);
        line("storew 0, $__ada_exception");
        m_context->activeExceptions.emplace_back(status, name);
        emitStatements(handlers[i].body);
        m_context->activeExceptions.pop_back();
        jump(afterLabel);
    }
}

void QbeEmitter::emitStatement(Stmt* statement)
{
    switch (statement->kind) {
    case StmtKind::Null:
        break;

    case StmtKind::Assign: {
        auto* assign = static_cast<AssignStmt*>(statement);
        Value address = emitAddress(assign->target.get());
        assignInto(address, assign->target->type, assign->value.get());
        break;
    }

    case StmtKind::ProcedureCall: {
        auto* call = static_cast<ProcedureCallStmt*>(statement);
        emitExpr(call->call.get());
        break;
    }

    case StmtKind::If: {
        auto* ifStatement = static_cast<IfStmt*>(statement);
        std::string end = newLabel("endif");
        for (IfBranch& branchStatement : ifStatement->branches) {
            Value condition = emitExpr(branchStatement.condition.get());
            std::string then = newLabel("then");
            std::string next = newLabel("elsif");
            branch(condition, then, next);
            label(then);
            emitStatements(branchStatement.body);
            jump(end);
            label(next);
        }
        if (ifStatement->hasElse) {
            emitStatements(ifStatement->elseBody);
        }
        jump(end);
        label(end);
        break;
    }

    case StmtKind::Loop: {
        auto* loop = static_cast<LoopStmt*>(statement);
        std::string head = newLabel("loop");
        std::string bodyLabel = newLabel("loopbody");
        std::string exit = newLabel("endloop");
        m_context->loopExits[loop] = exit;

        if (loop->loopKind == LoopKind::For) {
            Symbol* variable = loop->variableSymbol;
            long long size = typeSize(variable->type);
            if (size < 1) {
                size = 1;
            }
            if (variable->isUplevel) {
                variable->frameOffset = m_context->frameSize;
                m_context->frameSize += size;
            } else {
                std::string slot = "%v." + variable->name + "." + std::to_string(m_tempCounter++);
                m_context->prologue << "    " << slot << " =l " << (size <= 4 ? "alloc4" : "alloc8") << " " << size
                                    << "\n";
                m_context->locals[variable] = slot;
            }

            Value low = emitExpr(loop->rangeLow.get());
            Value high = emitExpr(loop->rangeHigh.get());
            char type = qbeClass(variable->type);
            std::string boundSlot = allocScratch(type == 'l' ? 8 : 4);
            Value address = addressOf(variable);
            storeInto(address, loop->isReverse ? high : low, variable->type);
            line(std::string(type == 'l' ? "storel " : "storew ")
                 + (loop->isReverse ? low.name : high.name) + ", " + boundSlot);

            label(head);
            Value current = loadFrom(addressOf(variable), variable->type);
            std::string bound = newTemp();
            line(bound + " =" + std::string(1, type) + (type == 'l' ? " loadl " : " loadsw ") + boundSlot);
            std::string test = newTemp();
            line(test + " =w " + comparisonInstruction(loop->isReverse ? BinaryOp::GreaterEqual : BinaryOp::LessEqual, type)
                 + " " + current.name + ", " + bound);
            branch(Value { test, 'w' }, bodyLabel, exit);
            label(bodyLabel);
            emitStatements(loop->body);
            Value step = loadFrom(addressOf(variable), variable->type);
            std::string lastIteration = newTemp();
            line(lastIteration + " =w " + comparisonInstruction(BinaryOp::Equal, type) + " " + step.name + ", " + bound);
            std::string advance = newLabel("loopstep");
            branch(Value { lastIteration, 'w' }, exit, advance);
            label(advance);
            std::string updated = newTemp();
            line(updated + " =" + std::string(1, type) + " " + (loop->isReverse ? "sub " : "add ") + step.name
                 + ", 1");
            storeInto(addressOf(variable), Value { updated, type }, variable->type);
            jump(head);
            label(exit);
            break;
        }

        label(head);
        if (loop->loopKind == LoopKind::While) {
            Value condition = emitExpr(loop->condition.get());
            branch(condition, bodyLabel, exit);
            label(bodyLabel);
        }
        emitStatements(loop->body);
        jump(head);
        label(exit);
        break;
    }

    case StmtKind::Exit: {
        auto* exitStatement = static_cast<ExitStmt*>(statement);
        auto it = m_context->loopExits.find(exitStatement->target);
        if (it == m_context->loopExits.end()) {
            break;
        }
        if (exitStatement->condition) {
            Value condition = emitExpr(exitStatement->condition.get());
            std::string next = newLabel("noexit");
            branch(condition, it->second, next);
            label(next);
        } else {
            jump(it->second);
        }
        break;
    }

    case StmtKind::Return: {
        auto* returnStatement = static_cast<ReturnStmt*>(statement);
        Type* resultType = m_context->symbol == nullptr ? nullptr : m_context->symbol->returnType;
        if (returnStatement->value && isComposite(resultType)) {
            if (isUnconstrainedArray(resultType)) {
                Value value = emitExpr(returnStatement->value.get());
                value = withBounds(value, returnStatement->value->type, nullptr);
                line("call $__ada_array_result(l %.result, l " + value.name + ", w " + value.first
                     + ", w " + value.last + ", l " + std::to_string(typeSize(resultType->element)) + ")");
                emitExceptionCheck();
            } else {
                assignInto(Value { "%.result", 'l' }, resultType, returnStatement->value.get());
            }
            line("ret");
        } else if (returnStatement->value) {
            Value value = emitExpr(returnStatement->value.get());
            if (m_context->symbol != nullptr) {
                emitRangeCheck(value, m_context->symbol->returnType, returnStatement->location);
            }
            line("ret " + value.name);
        } else {
            line("ret");
        }
        m_context->terminated = true;
        break;
    }

    case StmtKind::Case: {
        auto* caseStatement = static_cast<CaseStmt*>(statement);
        Value selector = emitExpr(caseStatement->selector.get());
        char type = selector.type;
        std::string end = newLabel("endcase");

        std::vector<std::string> bodyLabels;
        for (std::size_t i = 0; i < caseStatement->alternatives.size(); ++i) {
            bodyLabels.push_back(newLabel("casebody"));
        }

        std::string othersLabel;
        for (std::size_t i = 0; i < caseStatement->alternatives.size(); ++i) {
            CaseAlternative& alternative = caseStatement->alternatives[i];
            if (alternative.isOthers) {
                othersLabel = bodyLabels[i];
                continue;
            }
            std::string match;
            for (const CaseChoice& choice : alternative.choices) {
                std::string test = newTemp();
                if (choice.low == choice.high) {
                    line(test + " =w " + comparisonInstruction(BinaryOp::Equal, type) + " " + selector.name + ", "
                         + std::to_string(choice.low));
                } else {
                    std::string lowTest = newTemp();
                    std::string highTest = newTemp();
                    line(lowTest + " =w " + comparisonInstruction(BinaryOp::GreaterEqual, type) + " "
                         + selector.name + ", " + std::to_string(choice.low));
                    line(highTest + " =w " + comparisonInstruction(BinaryOp::LessEqual, type) + " " + selector.name
                         + ", " + std::to_string(choice.high));
                    line(test + " =w and " + lowTest + ", " + highTest);
                }
                if (match.empty()) {
                    match = test;
                } else {
                    std::string combined = newTemp();
                    line(combined + " =w or " + match + ", " + test);
                    match = combined;
                }
            }
            std::string next = newLabel("nextcase");
            branch(Value { match, 'w' }, bodyLabels[i], next);
            label(next);
        }

        jump(othersLabel.empty() ? end : othersLabel);

        for (std::size_t i = 0; i < caseStatement->alternatives.size(); ++i) {
            label(bodyLabels[i]);
            emitStatements(caseStatement->alternatives[i].body);
            jump(end);
        }
        label(end);
        break;
    }

    case StmtKind::Block: {
        auto* block = static_cast<BlockStmt*>(statement);
        emitLocalDeclarations(block->declarations);
        if (block->handlers.empty()) {
            emitStatements(block->body);
            break;
        }
        std::string dispatch = newLabel("handler");
        std::string after = newLabel("handled");
        m_context->handlerLabels.push_back(dispatch);
        emitStatements(block->body);
        m_context->handlerLabels.pop_back();
        jump(after);
        emitHandlers(block->handlers, after, dispatch);
        label(after);
        break;
    }

    case StmtKind::Raise: {
        auto* raise = static_cast<RaiseStmt*>(statement);
        emitRaise(raise->exceptionSymbol, raise->location);
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// Addresses and memory
// ---------------------------------------------------------------------------

Value QbeEmitter::staticLinkFor(int targetLevel)
{
    Symbol* current = m_context->symbol;
    int currentLevel = current != nullptr ? current->level : 0;

    if (targetLevel == currentLevel) {
        if (m_context->hasFrame) {
            return Value { m_context->frameTemp, 'l' };
        }
        return Value { "0", 'l' };
    }

    if (currentLevel == 0) {
        return Value { "0", 'l' };
    }

    std::string pointer = "%.link";
    for (int level = currentLevel - 1; level > targetLevel; --level) {
        std::string next = newTemp();
        line(next + " =l loadl " + pointer);
        pointer = next;
    }
    return Value { pointer, 'l' };
}

Value QbeEmitter::addressOf(Symbol* symbol)
{
    if (symbol->isGlobal) {
        return Value { symbol->qbeName, 'l' };
    }

    if (symbol->owner == m_context->symbol) {
        if (symbol->frameOffset >= 0) {
            std::string address = newTemp();
            line(address + " =l add " + m_context->frameTemp + ", " + std::to_string(symbol->frameOffset));
            return Value { address, 'l' };
        }
        auto it = m_context->locals.find(symbol);
        if (it != m_context->locals.end()) {
            return Value { it->second, 'l' };
        }
        m_diagnostics.error(symbol->location, "internal error: '" + symbol->displayName + "' has no storage");
        return Value { "0", 'l' };
    }

    Value frame = staticLinkFor(symbol->owner != nullptr ? symbol->owner->level : 0);
    std::string address = newTemp();
    line(address + " =l add " + frame.name + ", " + std::to_string(symbol->frameOffset));
    if (symbol->kind == SymbolKind::Parameter && symbol->byReference) {
        std::string pointer = newTemp();
        line(pointer + " =l loadl " + address);
        return Value { pointer, 'l' };
    }
    return Value { address, 'l' };
}

Value QbeEmitter::loadFrom(const Value& address, Type* type)
{
    char resultType = qbeClass(type);
    std::string temp = newTemp();
    line(temp + " =" + std::string(1, resultType) + " " + qbeLoadInstruction(type) + " " + address.name);
    return Value { temp, resultType };
}

void QbeEmitter::storeInto(const Value& address, const Value& value, Type* type)
{
    line(std::string(qbeStoreInstruction(type)) + " " + value.name + ", " + address.name);
}

void QbeEmitter::copyInto(const Value& destination, const Value& source, Type* type)
{
    long long size = typeSize(type);
    if (size <= 0) {
        return;
    }
    line("blit " + source.name + ", " + destination.name + ", " + std::to_string(size));
}

void QbeEmitter::assignInto(const Value& address, Type* type, Expr* value)
{
    if (value == nullptr) {
        return;
    }
    if (value->kind == ExprKind::Aggregate) {
        emitAggregateInto(static_cast<AggregateExpr*>(value), address, type);
        return;
    }
    if (type != nullptr && type->kind == TypeKind::Array) {
        if (!type->constrained) {
            m_diagnostics.error(value->location, "cannot assign to an unconstrained array object");
            return;
        }
        Value source = emitExpr(value);
        long long target = arrayLength(type);
        Value sourceLength = lengthOf(source, value->type);
        if (isLiteralOperand(sourceLength.name)) {
            if (std::stoll(sourceLength.name) != target) {
                m_diagnostics.error(value->location, "the assigned value has a different length");
                return;
            }
        } else {
            // Ada requires the lengths to match, so check them at run time.
            std::string same = newTemp();
            line(same + " =w ceqw " + sourceLength.name + ", " + std::to_string(target));
            std::string ok = newLabel("lengthok");
            std::string bad = newLabel("lengthbad");
            branch(Value { same, 'w' }, ok, bad);
            label(bad);
            raiseConstraintError();
            label(ok);
        }
        copyInto(address, source, type);
        return;
    }
    if (isComposite(type)) {
        Value source = emitExpr(value);
        copyInto(address, source, type);
        return;
    }
    Value result = emitExpr(value);
    emitRangeCheck(result, type, value->location);
    storeInto(address, result, type);
}

void QbeEmitter::emitRangeCheck(const Value& value, Type* type, const SourceLocation& location)
{
    if (type != nullptr && type->kind == TypeKind::Float) {
        if (!type->hasRealRange) {
            return;
        }
        char type_ = value.type;
        std::string lowTest = newTemp();
        std::string highTest = newTemp();
        std::string combined = newTemp();
        line(lowTest + " =w " + comparisonInstruction(BinaryOp::GreaterEqual, type_) + " " + value.name + ", "
             + realLiteral(type->lowReal, type_));
        line(highTest + " =w " + comparisonInstruction(BinaryOp::LessEqual, type_) + " " + value.name + ", "
             + realLiteral(type->highReal, type_));
        line(combined + " =w and " + lowTest + ", " + highTest);

        std::string ok = newLabel("inrange");
        std::string bad = newLabel("outofrange");
        branch(Value { combined, 'w' }, ok, bad);
        label(bad);
        raiseConstraintError();
        label(ok);
        return;
    }
    if (type == nullptr || !isDiscrete(type)) {
        return;
    }
    char type_ = value.type;
    std::string lowTest = newTemp();
    std::string highTest = newTemp();
    std::string combined = newTemp();
    line(lowTest + " =w " + comparisonInstruction(BinaryOp::GreaterEqual, type_) + " " + value.name + ", "
         + std::to_string(type->low));
    line(highTest + " =w " + comparisonInstruction(BinaryOp::LessEqual, type_) + " " + value.name + ", "
         + std::to_string(type->high));
    line(combined + " =w and " + lowTest + ", " + highTest);

    std::string ok = newLabel("inrange");
    std::string bad = newLabel("outofrange");
    branch(Value { combined, 'w' }, ok, bad);
    label(bad);
    (void)location;
    raiseConstraintError();
    label(ok);
}

// A component of a variant part is only there when the discriminant selects
// that alternative.  Where nothing fixed the discriminant, the value carries
// the answer and reaching for the wrong component raises Constraint_Error.
void QbeEmitter::checkVariant(const Value& address, Type* record, int variant)
{
    const FieldInfo& discriminant = record->fields[static_cast<std::size_t>(record->variantOn)];
    Value slot = address;
    if (discriminant.offset != 0) {
        std::string moved = newTemp();
        line(moved + " =l add " + address.name + ", " + std::to_string(discriminant.offset));
        slot = Value { moved, 'l' };
    }
    Value value = loadFrom(slot, discriminant.type);

    const VariantInfo& alternative = record->variants[static_cast<std::size_t>(variant)];
    std::string ok = newLabel("rightvariant");
    std::string bad = newLabel("wrongvariant");

    if (alternative.isOthers) {
        // 'when others' holds whatever no other alternative named, so the test
        // is that none of them does.
        std::string matched = newTemp();
        line(matched + " =w copy 0");
        for (const VariantInfo& other : record->variants) {
            if (other.isOthers) {
                continue;
            }
            for (const VariantChoice& choice : other.choices) {
                std::string within = newTemp();
                line(within + " =w " + rangeTest(value, choice.low, choice.high));
                std::string combined = newTemp();
                line(combined + " =w or " + matched + ", " + within);
                matched = combined;
            }
        }
        branch(Value { matched, 'w' }, bad, ok);
    } else {
        std::string matched = newTemp();
        line(matched + " =w copy 0");
        for (const VariantChoice& choice : alternative.choices) {
            std::string within = newTemp();
            line(within + " =w " + rangeTest(value, choice.low, choice.high));
            std::string combined = newTemp();
            line(combined + " =w or " + matched + ", " + within);
            matched = combined;
        }
        branch(Value { matched, 'w' }, ok, bad);
    }

    label(bad);
    raiseConstraintError();
    label(ok);
}

// The instruction that answers whether a discriminant value falls in a range.
std::string QbeEmitter::rangeTest(const Value& value, long long low, long long high)
{
    if (low == high) {
        return std::string(comparisonInstruction(BinaryOp::Equal, value.type)) + " " + value.name + ", "
            + std::to_string(low);
    }
    std::string atLeast = newTemp();
    std::string atMost = newTemp();
    line(atLeast + " =w " + comparisonInstruction(BinaryOp::GreaterEqual, value.type) + " " + value.name + ", "
         + std::to_string(low));
    line(atMost + " =w " + comparisonInstruction(BinaryOp::LessEqual, value.type) + " " + value.name + ", "
         + std::to_string(high));
    return "and " + atLeast + ", " + atMost;
}

void QbeEmitter::checkNotNull(const Value& pointer)
{
    // null designates no object at all, so reaching through it is an error.
    std::string isNull = newTemp();
    line(isNull + " =w ceql " + pointer.name + ", 0");

    std::string bad = newLabel("nullaccess");
    std::string ok = newLabel("notnull");
    branch(Value { isNull, 'w' }, bad, ok);
    label(bad);
    raiseConstraintError();
    label(ok);
}

void QbeEmitter::raiseConstraintError()
{
    line("storew 1, $__ada_exception");
    line("storel " + stringData("CONSTRAINT_ERROR") + ", $__ada_exception_name");
    if (!m_context->handlerLabels.empty()) {
        jump(m_context->handlerLabels.back());
    } else {
        m_context->usesPropagate = true;
        jump(m_context->propagateLabel);
    }
}

std::pair<std::string, std::string> QbeEmitter::boundsFor(Symbol* symbol)
{
    if (symbol->owner == m_context->symbol && symbol->frameOffset < 0) {
        auto it = m_context->bounds.find(symbol);
        if (it != m_context->bounds.end()) {
            return it->second;
        }
        return { "1", "0" };
    }

    if (symbol->frameOffset < 0) {
        return { "1", "0" };
    }

    // Unconstrained array parameters keep their bounds behind the pointer in
    // the owning frame, so nested subprograms can read them too.
    Value frame = symbol->owner == m_context->symbol ? Value { m_context->frameTemp, 'l' }
                                                     : staticLinkFor(symbol->owner->level);
    std::string firstAddress = newTemp();
    line(firstAddress + " =l add " + frame.name + ", " + std::to_string(symbol->frameOffset + 8));
    std::string first = newTemp();
    line(first + " =w loadsw " + firstAddress);
    std::string lastAddress = newTemp();
    line(lastAddress + " =l add " + frame.name + ", " + std::to_string(symbol->frameOffset + 12));
    std::string last = newTemp();
    line(last + " =w loadsw " + lastAddress);
    return { first, last };
}

Value QbeEmitter::withBounds(const Value& address, Type* type, Symbol* symbol)
{
    Value result = address;
    if (type == nullptr || type->kind != TypeKind::Array) {
        return result;
    }
    if (type->constrained) {
        result.first = std::to_string(type->indexLow);
        result.last = std::to_string(type->indexHigh);
        return result;
    }
    if (symbol != nullptr) {
        std::pair<std::string, std::string> bounds = boundsFor(symbol);
        result.first = bounds.first;
        result.last = bounds.second;
    }
    return result;
}

Value QbeEmitter::lengthOf(const Value& array, Type* type)
{
    if (type != nullptr && type->kind == TypeKind::Array && type->constrained) {
        return constantValue(arrayLength(type), 'w');
    }
    if (!array.hasBounds()) {
        return constantValue(0, 'w');
    }
    if (isLiteralOperand(array.first) && isLiteralOperand(array.last)) {
        return constantValue(std::max(0LL, std::stoll(array.last) - std::stoll(array.first) + 1), 'w');
    }
    std::string span = newTemp();
    line(span + " =w sub " + array.last + ", " + array.first);
    std::string length = newTemp();
    line(length + " =w add " + span + ", 1");
    std::string nonNull = newTemp();
    std::string normalized = newTemp();
    line(nonNull + " =w csgew " + array.last + ", " + array.first);
    line(normalized + " =w mul " + length + ", " + nonNull);
    return Value { normalized, 'w' };
}

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

Value QbeEmitter::emitAddress(Expr* expr)
{
    switch (expr->kind) {
    case ExprKind::Identifier: {
        auto* identifier = static_cast<IdentifierExpr*>(expr);
        if (identifier->symbol != nullptr
            && (identifier->symbol->kind == SymbolKind::Object
                || identifier->symbol->kind == SymbolKind::Parameter
                || identifier->symbol->kind == SymbolKind::LoopParameter)) {
            return addressOf(identifier->symbol);
        }
        break;
    }
    case ExprKind::Selected: {
        auto* selected = static_cast<SelectedExpr*>(expr);
        if (selected->symbol != nullptr
            && (selected->symbol->kind == SymbolKind::Object || selected->symbol->kind == SymbolKind::Parameter)) {
            return addressOf(selected->symbol);
        }
        if (selected->isDereference) {
            // The designated object lives where the access value points.
            Value pointer = emitExpr(selected->prefix.get());
            checkNotNull(pointer);
            return pointer;
        }
        if (selected->fieldIndex >= 0) {
            Value base = emitAddress(selected->prefix.get());
            Type* record = baseType(selected->prefix->type);
            if (record != nullptr && record->kind == TypeKind::Access) {
                base = loadFrom(base, record);
                checkNotNull(base);
                record = record->target;
            }
            record = baseType(record);
            if (selected->checkedVariant >= 0) {
                checkVariant(base, record, selected->checkedVariant);
            }
            long long offset = record->fields[selected->fieldIndex].offset;
            if (offset == 0) {
                return base;
            }
            std::string address = newTemp();
            line(address + " =l add " + base.name + ", " + std::to_string(offset));
            return Value { address, 'l' };
        }
        break;
    }
    case ExprKind::Call: {
        auto* call = static_cast<CallExpr*>(expr);
        if (call->form == CallForm::Slice) {
            return emitSlice(call);
        }
        if (call->form == CallForm::Indexing) {
            Value base = emitExpr(call->callee.get());
            Type* array = baseType(call->callee->type);
            if (array != nullptr && array->kind == TypeKind::Access) {
                // The access value is already the address of the array.
                checkNotNull(base);
                array = baseType(array->target);
            }
            Value index = emitExpr(call->resolvedArguments.front());
            long long elementSize = typeSize(array->element);
            std::string lowBound = array->constrained
                                       ? std::to_string(array->indexLow)
                                       : (base.hasBounds() ? base.first : std::string("1"));

            std::string offset = newTemp();
            line(offset + " =w sub " + index.name + ", " + lowBound);
            std::string wide = newTemp();
            line(wide + " =l extsw " + offset);
            std::string scaled = newTemp();
            line(scaled + " =l mul " + wide + ", " + std::to_string(elementSize));
            std::string address = newTemp();
            line(address + " =l add " + base.name + ", " + scaled);
            return Value { address, 'l' };
        }
        break;
    }
    default:
        break;
    }

    if (isComposite(baseType(expr->type))) {
        return emitExpr(expr);
    }

    m_diagnostics.error(expr->location, "this expression does not designate an object");
    return Value { "0", 'l' };
}

// Whether anything inside the type carries a value of its own to start from.
bool QbeEmitter::hasComponentDefaults(Type* type)
{
    Type* base = baseType(type);
    if (base == nullptr) {
        return false;
    }
    if (base->kind == TypeKind::Array) {
        return base->constrained && hasComponentDefaults(base->element);
    }
    if (base->kind != TypeKind::Record) {
        return false;
    }
    // A discriminant the subtype fixed is written into the object too, so that
    // reading it back gives what the declaration said.
    long long fixed = 0;
    for (int i = 0; i < base->discriminantCount; ++i) {
        if (discriminantValueOf(type, i, fixed)) {
            return true;
        }
    }
    for (const FieldInfo& field : base->fields) {
        if (field.defaultValue != nullptr || hasComponentDefaults(field.type)) {
            return true;
        }
    }
    return false;
}

// Gives an object the values its component declarations named.  Anything a
// component says nothing about is left as it was found, which for storage from
// an allocator means cleared.
void QbeEmitter::emitDefaultInit(const Value& address, Type* type)
{
    Type* base = baseType(type);
    if (base == nullptr || !hasComponentDefaults(type)) {
        return;
    }

    if (base->kind == TypeKind::Array) {
        long long count = arrayLength(base);
        long long elementSize = typeSize(base->element);
        for (long long i = 0; i < count; ++i) {
            std::string slot = newTemp();
            line(slot + " =l add " + address.name + ", " + std::to_string(i * elementSize));
            emitDefaultInit(Value { slot, 'l' }, base->element);
        }
        return;
    }

    for (const FieldInfo& field : base->fields) {
        Value slot = address;
        if (field.offset != 0) {
            std::string moved = newTemp();
            line(moved + " =l add " + address.name + ", " + std::to_string(field.offset));
            slot = Value { moved, 'l' };
        }
        long long fixed = 0;
        if (field.isDiscriminant && discriminantValueOf(type, field.index, fixed)) {
            storeInto(slot, constantValue(fixed, qbeClass(field.type)), field.type);
        } else if (field.defaultValue != nullptr) {
            assignInto(slot, field.type, field.defaultValue);
        } else {
            emitDefaultInit(slot, field.type);
        }
    }
}

Value QbeEmitter::emitAllocator(AllocatorExpr* expr)
{
    Type* designated = expr->designated;
    long long size = typeSize(designated);

    // The run time hands back cleared storage, so an access component of the
    // new object starts out null even when no value is given.
    std::string pointer = newTemp();
    line(pointer + " =l call $__ada_allocate(l " + std::to_string(size) + ")");
    emitExceptionCheck();

    Value address { pointer, 'l' };
    if (expr->value != nullptr) {
        assignInto(address, designated, expr->value.get());
    } else {
        emitDefaultInit(address, designated);
    }
    return address;
}

Value QbeEmitter::emitExpr(Expr* expr)
{
    if (expr == nullptr) {
        return Value { "0", 'w' };
    }

    if (expr->isStatic && isDiscrete(baseType(expr->type))) {
        // Check before narrowing the literal to a QBE word. Subtype bounds
        // are checked at value boundaries, not on intermediate expressions.
        if (qbeClass(expr->type) == 'w'
            && (expr->staticValue < -2147483648LL || expr->staticValue > 2147483647LL)) {
            raiseConstraintError();
        }
        return constantValue(expr->staticValue, qbeClass(expr->type));
    }
    if (expr->isStatic && isReal(expr->type)) {
        char type = qbeClass(expr->type);
        return Value { realLiteral(expr->staticReal, type), type };
    }

    switch (expr->kind) {
    case ExprKind::IntegerLiteral:
        return constantValue(static_cast<IntegerLiteralExpr*>(expr)->value, qbeClass(expr->type));
    case ExprKind::CharacterLiteral:
        return constantValue(static_cast<unsigned char>(static_cast<CharacterLiteralExpr*>(expr)->value), 'w');
    case ExprKind::RealLiteral: {
        char type = qbeClass(expr->type);
        return Value { realLiteral(static_cast<RealLiteralExpr*>(expr)->value, type), type };
    }
    case ExprKind::StringLiteral: {
        auto* literal = static_cast<StringLiteralExpr*>(expr);
        return Value { stringData(literal->value), 'l', "1", std::to_string(literal->value.size()) };
    }
    case ExprKind::Null:
        return Value { "0", 'l' };

    case ExprKind::Identifier: {
        auto* identifier = static_cast<IdentifierExpr*>(expr);
        Symbol* symbol = identifier->symbol;
        if (symbol == nullptr) {
            return Value { "0", 'w' };
        }
        if (symbol->kind == SymbolKind::Subprogram) {
            CallExpr call;
            call.location = expr->location;
            call.form = CallForm::Subprogram;
            call.subprogram = symbol;
            call.type = symbol->returnType;
            return emitCall(&call);
        }
        if (symbol->kind == SymbolKind::EnumerationLiteral) {
            return constantValue(symbol->enumerationValue, qbeClass(symbol->type));
        }
        if (symbol->kind == SymbolKind::Number) {
            if (isReal(symbol->type)) {
                char type = qbeClass(symbol->type);
                return Value { realLiteral(symbol->staticReal, type), type };
            }
            return constantValue(symbol->staticValue, qbeClass(symbol->type));
        }
        Value address = addressOf(symbol);
        if (isComposite(symbol->type)) {
            return withBounds(address, symbol->type, symbol);
        }
        return loadFrom(address, symbol->type);
    }

    case ExprKind::Selected: {
        auto* selected = static_cast<SelectedExpr*>(expr);
        Symbol* symbol = selected->symbol;
        if (symbol != nullptr && symbol->kind == SymbolKind::Subprogram) {
            CallExpr call;
            call.location = expr->location;
            call.form = CallForm::Subprogram;
            call.subprogram = symbol;
            call.type = symbol->returnType;
            return emitCall(&call);
        }
        if (symbol != nullptr && symbol->kind == SymbolKind::EnumerationLiteral) {
            return constantValue(symbol->enumerationValue, qbeClass(symbol->type));
        }
        Value address = emitAddress(expr);
        if (isComposite(expr->type)) {
            return withBounds(address, expr->type, selected->symbol);
        }
        return loadFrom(address, expr->type);
    }

    case ExprKind::Allocator:
        return emitAllocator(static_cast<AllocatorExpr*>(expr));

    case ExprKind::Call: {
        auto* call = static_cast<CallExpr*>(expr);
        if (call->form == CallForm::Subprogram) {
            return emitCall(call);
        }
        if (call->form == CallForm::Slice) {
            return emitSlice(call);
        }
        if (call->form == CallForm::Indexing) {
            Value address = emitAddress(expr);
            if (isComposite(expr->type)) {
                return withBounds(address, expr->type, nullptr);
            }
            return loadFrom(address, expr->type);
        }
        if (call->form == CallForm::Conversion) {
            Expr* operand = call->resolvedArguments.front();
            Value value = emitExpr(operand);
            char from = value.type;
            char to = qbeClass(expr->type);
            if (from == to) {
                emitRangeCheck(value, expr->type, expr->location);
                return value;
            }
            std::string temp = newTemp();
            if (isFloatClass(from) && !isFloatClass(to)) {
                // Ada rounds when a real value becomes an integer, while the
                // conversion instructions of the backend truncate.
                std::string wide = value.name;
                if (from == 's') {
                    wide = newTemp();
                    line(wide + " =d exts " + value.name);
                }
                std::string rounded = newTemp();
                line(rounded + " =l call $__ada_round_to_integer(d " + wide + ")");
                emitExceptionCheck();
                emitRangeCheck(Value { rounded, 'l' }, expr->type, expr->location);
                line(temp + " =" + std::string(1, to) + " copy " + rounded);
            } else {
                std::string instruction;
                if (!isFloatClass(from) && isFloatClass(to)) {
                    instruction = from == 'w' ? "swtof" : "sltof";
                } else if (from == 's' && to == 'd') {
                    instruction = "exts";
                } else if (from == 'd' && to == 's') {
                    instruction = "truncd";
                } else if (from == 'w' && to == 'l') {
                    instruction = "extsw";
                } else {
                    emitRangeCheck(value, expr->type, expr->location);
                    instruction = "copy";
                }
                line(temp + " =" + std::string(1, to) + " " + instruction + " " + value.name);
            }
            Value result { temp, to };
            emitRangeCheck(result, expr->type, expr->location);
            return result;
        }
        m_diagnostics.error(expr->location, "unsupported call form");
        return Value { "0", 'w' };
    }

    case ExprKind::Binary:
        return emitBinary(static_cast<BinaryExpr*>(expr));
    case ExprKind::Unary:
        return emitUnary(static_cast<UnaryExpr*>(expr));
    case ExprKind::Attribute:
        return emitAttribute(static_cast<AttributeExpr*>(expr));
    case ExprKind::Aggregate:
        return emitAggregate(static_cast<AggregateExpr*>(expr));
    case ExprKind::Qualified:
        return emitExpr(static_cast<QualifiedExpr*>(expr)->operand.get());

    case ExprKind::Membership: {
        auto* membership = static_cast<MembershipExpr*>(expr);
        Value operand = emitExpr(membership->operand.get());
        Value low = emitExpr(membership->low.get());
        Value high = emitExpr(membership->high.get());
        std::string lowTest = newTemp();
        std::string highTest = newTemp();
        std::string combined = newTemp();
        line(lowTest + " =w " + comparisonInstruction(BinaryOp::GreaterEqual, operand.type) + " " + operand.name
             + ", " + low.name);
        line(highTest + " =w " + comparisonInstruction(BinaryOp::LessEqual, operand.type) + " " + operand.name
             + ", " + high.name);
        line(combined + " =w and " + lowTest + ", " + highTest);
        if (!membership->negated) {
            return Value { combined, 'w' };
        }
        std::string negated = newTemp();
        line(negated + " =w ceqw " + combined + ", 0");
        return Value { negated, 'w' };
    }
    }

    return Value { "0", 'w' };
}

Value QbeEmitter::emitSlice(CallExpr* expr)
{
    Value base = emitExpr(expr->callee.get());
    Type* array = expr->callee->type;
    Value low = emitExpr(expr->resolvedArguments[0]);
    Value high = emitExpr(expr->resolvedArguments[1]);

    std::string lowBound = array != nullptr && array->constrained
                               ? std::to_string(array->indexLow)
                               : (base.hasBounds() ? base.first : std::string("1"));
    long long elementSize = array != nullptr ? typeSize(array->element) : 1;

    std::string offset = newTemp();
    line(offset + " =w sub " + low.name + ", " + lowBound);
    std::string wide = newTemp();
    line(wide + " =l extsw " + offset);
    std::string scaled = newTemp();
    line(scaled + " =l mul " + wide + ", " + std::to_string(elementSize));
    std::string address = newTemp();
    line(address + " =l add " + base.name + ", " + scaled);

    return Value { address, 'l', low.name, high.name };
}

// The run time formats every real value as a double.
std::string QbeEmitter::widenToDouble(const Value& value)
{
    if (value.type == 'd') {
        return value.name;
    }
    std::string wide = newTemp();
    line(wide + " =d exts " + value.name);
    return wide;
}

// Ada writes one digit before the point and the rest after it.
int QbeEmitter::defaultAft(const Type* type)
{
    int digits = type != nullptr && type->digits > 0 ? type->digits : 6;
    return digits - 1;
}

// Operands here are addresses, including when the element is a scalar.
Value QbeEmitter::compareObjects(const Value& left, const Value& right, Type* type)
{
    if (type->kind == TypeKind::Array) {
        return compareArrays(BinaryOp::Equal, left, type, right, type);
    }
    if (type->kind == TypeKind::Record) {
        return compareRecords(left, right, type);
    }
    Value leftValue = loadFrom(left, type);
    Value rightValue = loadFrom(right, type);
    std::string equal = newTemp();
    line(equal + " =w " + comparisonInstruction(BinaryOp::Equal, leftValue.type) + " "
         + leftValue.name + ", " + rightValue.name);
    return Value { equal, 'w' };
}

Value QbeEmitter::compareArrays(BinaryOp op, const Value& left, Type* leftType, const Value& right,
                                Type* rightType)
{
    // Normalize null ranges to length zero, even for runtime bounds such as
    // 10 .. 5. Widen before subtracting so the span does not overflow a word.
    auto length = [&](const Value& value, Type* type) {
        if (type->constrained) {
            return std::to_string(arrayLength(type));
        }
        std::string first = newTemp();
        std::string last = newTemp();
        std::string span = newTemp();
        std::string count = newTemp();
        std::string positive = newTemp();
        std::string widePositive = newTemp();
        std::string result = newTemp();
        line(first + " =l extsw " + value.first);
        line(last + " =l extsw " + value.last);
        line(span + " =l sub " + last + ", " + first);
        line(count + " =l add " + span + ", 1");
        line(positive + " =w csgtl " + count + ", 0");
        line(widePositive + " =l extuw " + positive);
        line(result + " =l mul " + count + ", " + widePositive);
        return result;
    };
    std::string leftLength = length(left, leftType);
    std::string rightLength = length(right, rightType);
    bool equality = op == BinaryOp::Equal || op == BinaryOp::NotEqual;
    std::string resultSlot = allocScratch(4);
    std::string indexSlot = allocScratch(8);
    std::string head = newLabel("comparearray");
    std::string body = newLabel("compareelement");
    std::string advance = newLabel("comparenext");
    std::string different = newLabel("comparedifferent");
    std::string exhausted = newLabel("compareend");
    std::string done = newLabel("comparedone");
    line("storel 0, " + indexSlot);

    if (equality) {
        std::string sameLength = newTemp();
        line(sameLength + " =w ceql " + leftLength + ", " + rightLength);
        branch(Value { sameLength, 'w' }, head, different);
    }
    label(head);
    std::string index = newTemp();
    std::string withinLeft = newTemp();
    std::string withinRight = newTemp();
    std::string withinBoth = newTemp();
    line(index + " =l loadl " + indexSlot);
    line(withinLeft + " =w csltl " + index + ", " + leftLength);
    line(withinRight + " =w csltl " + index + ", " + rightLength);
    line(withinBoth + " =w and " + withinLeft + ", " + withinRight);
    branch(Value { withinBoth, 'w' }, body, exhausted);

    label(body);
    auto elementAddress = [&](const Value& array, Type* type) {
        std::string offset = newTemp();
        std::string address = newTemp();
        line(offset + " =l mul " + index + ", " + std::to_string(typeSize(type->element)));
        line(address + " =l add " + array.name + ", " + offset);
        return Value { address, 'l' };
    };
    Value leftElement = elementAddress(left, leftType);
    Value rightElement = elementAddress(right, rightType);
    Value equal = compareObjects(leftElement, rightElement, leftType->element);
    branch(equal, advance, different);

    label(advance);
    std::string nextIndex = newTemp();
    line(nextIndex + " =l add " + index + ", 1");
    line("storel " + nextIndex + ", " + indexSlot);
    jump(head);

    label(different);
    if (equality) {
        line("storew " + std::string(op == BinaryOp::Equal ? "0" : "1") + ", " + resultSlot);
    } else {
        // Only discrete-element arrays have predefined ordering. The first
        // unequal element decides the result using its numeric/ordinal value.
        Value leftValue = loadFrom(leftElement, leftType->element);
        Value rightValue = loadFrom(rightElement, rightType->element);
        std::string ordered = newTemp();
        line(ordered + " =w " + comparisonInstruction(op, leftValue.type) + " "
             + leftValue.name + ", " + rightValue.name);
        line("storew " + ordered + ", " + resultSlot);
    }
    jump(done);

    label(exhausted);
    std::string orderedLengths = newTemp();
    line(orderedLengths + " =w " + comparisonInstruction(op, 'l') + " " + leftLength + ", " + rightLength);
    line("storew " + orderedLengths + ", " + resultSlot);
    jump(done);

    label(done);
    std::string result = newTemp();
    line(result + " =w loadsw " + resultSlot);
    return Value { result, 'w' };
}

// Compare common fields first, including every discriminant, then dispatch to
// the active variant. Neither padding nor inactive storage contributes to equality.
Value QbeEmitter::compareRecords(const Value& left, const Value& right, Type* type)
{
    Type* record = baseType(type);
    std::string resultSlot = allocScratch(4);
    std::string different = newLabel("recorddifferent");
    std::string done = newLabel("recordcompared");
    line("storew 1, " + resultSlot);

    auto fieldAddress = [&](const Value& address, const FieldInfo& field) {
        if (field.offset == 0) {
            return address;
        }
        std::string moved = newTemp();
        line(moved + " =l add " + address.name + ", " + std::to_string(field.offset));
        return Value { moved, 'l' };
    };
    auto compareField = [&](const FieldInfo& field) {
        Value leftField = fieldAddress(left, field);
        Value rightField = fieldAddress(right, field);
        Value equal = compareObjects(leftField, rightField, field.type);
        std::string next = newLabel("recordnextfield");
        branch(equal, next, different);
        label(next);
    };
    for (const FieldInfo& field : record->fields) {
        if (field.variant < 0) {
            compareField(field);
        }
    }

    if (record->variantOn >= 0) {
        const FieldInfo& discriminant = record->fields[static_cast<std::size_t>(record->variantOn)];
        Value value = loadFrom(fieldAddress(left, discriminant), discriminant.type);
        std::vector<std::string> alternatives;
        std::string fallback = done;
        for (const VariantInfo& variant : record->variants) {
            alternatives.push_back(newLabel("comparevariant"));
            if (variant.isOthers) {
                fallback = alternatives.back();
            }
        }
        for (std::size_t i = 0; i < record->variants.size(); ++i) {
            const VariantInfo& variant = record->variants[i];
            if (variant.isOthers) {
                continue;
            }
            for (const VariantChoice& choice : variant.choices) {
                std::string matches = newTemp();
                std::string next = newLabel("comparenextvariant");
                line(matches + " =w " + rangeTest(value, choice.low, choice.high));
                branch(Value { matches, 'w' }, alternatives[i], next);
                label(next);
            }
        }
        jump(fallback);
        for (std::size_t i = 0; i < record->variants.size(); ++i) {
            label(alternatives[i]);
            for (const FieldInfo& field : record->fields) {
                if (field.variant == static_cast<int>(i)) {
                    compareField(field);
                }
            }
            jump(done);
        }
    } else {
        jump(done);
    }

    label(different);
    line("storew 0, " + resultSlot);
    jump(done);
    label(done);
    std::string result = newTemp();
    line(result + " =w loadsw " + resultSlot);
    return Value { result, 'w' };
}

Value QbeEmitter::emitCall(CallExpr* expr)
{
    Symbol* subprogram = expr->subprogram;
    if (subprogram == nullptr) {
        return Value { "0", 'w' };
    }
    // Bare names arrive without an explicit argument list. Complete it from
    // the resolved declaration before either Ada or imported-call marshalling.
    expr->resolvedArguments.resize(subprogram->parameters.size(), nullptr);
    for (std::size_t i = 0; i < subprogram->parameters.size(); ++i) {
        if (expr->resolvedArguments[i] == nullptr) {
            expr->resolvedArguments[i] = subprogram->parameters[i]->defaultExpr;
        }
        if (expr->resolvedArguments[i] == nullptr) {
            m_diagnostics.error(expr->location, "internal error: missing resolved call argument");
            return Value { "0", 'w' };
        }
    }
    if (subprogram->builtin == BuiltinKind::Runtime) {
        Value result = emitRuntimeCall(expr, subprogram);
        if (subprogram->canRaise) {
            emitExceptionCheck();
        }
        return result;
    }

    std::vector<std::string> arguments;
    bool compositeResult = isComposite(subprogram->returnType);
    bool dynamicResult = isUnconstrainedArray(subprogram->returnType);
    std::string resultStorage;
    if (compositeResult) {
        resultStorage = allocScratch(dynamicResult ? 24 : std::max(1LL, typeSize(subprogram->returnType)));
        arguments.push_back("l " + resultStorage);
    }
    if (subprogram->level > 0) {
        Value link = staticLinkFor(subprogram->level - 1);
        arguments.push_back("l " + link.name);
    }

    for (std::size_t i = 0; i < subprogram->parameters.size(); ++i) {
        Symbol* parameter = subprogram->parameters[i];
        Expr* argument = expr->resolvedArguments[i];
        if (parameter->byReference) {
            // Composite values are already addresses and carry their bounds.
            Value value = isComposite(argument->type) ? emitExpr(argument) : emitAddress(argument);
            arguments.push_back("l " + value.name);
            if (isUnconstrainedArray(parameter->type)) {
                if (!value.hasBounds()) {
                    value = withBounds(value, argument->type, nullptr);
                }
                arguments.push_back("w " + (value.first.empty() ? std::string("1") : value.first));
                arguments.push_back("w " + (value.last.empty() ? std::string("0") : value.last));
            }
        } else {
            Value value = emitExpr(argument);
            emitRangeCheck(value, parameter->type, argument->location);
            arguments.push_back(std::string(1, qbeClass(parameter->type)) + " " + value.name);
        }
    }

    std::string argumentList;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        if (i > 0) {
            argumentList += ", ";
        }
        argumentList += arguments[i];
    }

    Value result { "0", 'w' };
    if (subprogram->returnType != nullptr && !compositeResult) {
        char type = qbeClass(subprogram->returnType);
        std::string temp = newTemp();
        line(temp + " =" + std::string(1, type) + " call " + subprogram->qbeName + "(" + argumentList + ")");
        result = Value { temp, type };
    } else {
        line("call " + subprogram->qbeName + "(" + argumentList + ")");
    }

    emitExceptionCheck();
    if (dynamicResult) {
        // The callee transfers a heap copy. Move it immediately to this
        // activation's stack, then release the transfer buffer before any
        // further Ada expression can raise an exception.
        std::string pointer = newTemp();
        line(pointer + " =l loadl " + resultStorage);
        std::string firstAddress = newTemp();
        std::string lastAddress = newTemp();
        std::string sizeAddress = newTemp();
        line(firstAddress + " =l add " + resultStorage + ", 8");
        line(lastAddress + " =l add " + resultStorage + ", 12");
        line(sizeAddress + " =l add " + resultStorage + ", 16");
        std::string first = newTemp();
        std::string last = newTemp();
        std::string size = newTemp();
        line(first + " =w loadw " + firstAddress);
        line(last + " =w loadw " + lastAddress);
        line(size + " =l loadl " + sizeAddress);
        std::string buffer = newTemp();
        line(buffer + " =l alloc8 " + size);
        line("call $memcpy(l " + buffer + ", l " + pointer + ", l " + size + ")");
        line("call $__ada_deallocate(l " + pointer + ")");
        result = Value { buffer, 'l', first, last };
    } else if (compositeResult) {
        result = withBounds(Value { resultStorage, 'l' }, subprogram->returnType, nullptr);
    }
    return result;
}

// Marshals a call into the run time library straight from the parameter list:
// arrays become a pointer and a length, anything writable or composite becomes
// an address, and everything else travels by value.
Value QbeEmitter::emitRuntimeCall(CallExpr* expr, Symbol* subprogram)
{
    std::vector<std::string> arguments;

    for (std::size_t i = 0; i < subprogram->parameters.size(); ++i) {
        Symbol* parameter = subprogram->parameters[i];
        Expr* argument = expr->resolvedArguments[i];
        Type* formal = baseType(parameter->type);
        char formalClass = qbeClass(parameter->type);

        if (formal != nullptr && formal->kind == TypeKind::Array) {
            Value pointer = emitExpr(argument);
            Value length = lengthOf(pointer, argument->type);
            arguments.push_back("l " + pointer.name);
            arguments.push_back("w " + length.name);
            continue;
        }

        if (parameter->byReference) {
            Value address = isComposite(argument->type) ? emitExpr(argument) : emitAddress(argument);
            arguments.push_back("l " + address.name);
            continue;
        }

        Value value = emitExpr(argument);
        emitRangeCheck(value, parameter->type, argument->location);
        // Every run time entry taking a real by value takes a double, whatever
        // precision the type the call named is held in.
        if (isReal(parameter->type)) {
            arguments.push_back("d " + widenToDouble(value));
            continue;
        }
        arguments.push_back(std::string(1, formalClass) + " " + value.name);
    }

    std::string argumentList;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        if (i > 0) {
            argumentList += ", ";
        }
        argumentList += arguments[i];
    }

    if (subprogram->returnType == nullptr) {
        line("call " + subprogram->runtimeSymbol + "(" + argumentList + ")");
        return Value { "0", 'w' };
    }

    if (isUnconstrainedArray(subprogram->returnType)) {
        // A run time function that yields a string hands back a C string, so
        // its bounds are recovered here.
        std::string pointer = newTemp();
        line(pointer + " =l call " + subprogram->runtimeSymbol + "(" + argumentList + ")");
        std::string size = newTemp();
        line(size + " =l call $strlen(l " + pointer + ")");
        std::string length = newTemp();
        line(length + " =w copy " + size);
        return Value { pointer, 'l', "1", length };
    }

    char type = qbeClass(subprogram->returnType);
    std::string temp = newTemp();
    line(temp + " =" + std::string(1, type) + " call " + subprogram->runtimeSymbol + "(" + argumentList + ")");
    return Value { temp, type };
}

Value QbeEmitter::emitShortCircuit(BinaryExpr* expr)
{
    std::string slot = allocScratch(4);
    Value left = emitExpr(expr->left.get());
    line("storew " + left.name + ", " + slot);

    std::string evaluate = newLabel("shortcircuit");
    std::string done = newLabel("shortdone");
    if (expr->op == BinaryOp::AndThen) {
        branch(left, evaluate, done);
    } else {
        branch(left, done, evaluate);
    }
    label(evaluate);
    Value right = emitExpr(expr->right.get());
    line("storew " + right.name + ", " + slot);
    jump(done);
    label(done);

    std::string result = newTemp();
    line(result + " =w loadsw " + slot);
    return Value { result, 'w' };
}

Value QbeEmitter::emitModulo(const Value& left, const Value& right, char type)
{
    std::string slot = allocScratch(type == 'l' ? 8 : 4);
    std::string remainder = newTemp();
    line(remainder + " =" + std::string(1, type) + " rem " + left.name + ", " + right.name);
    line(std::string(type == 'l' ? "storel " : "storew ") + remainder + ", " + slot);

    std::string sign = newTemp();
    line(sign + " =" + std::string(1, type) + " xor " + remainder + ", " + right.name);
    std::string negative = newTemp();
    line(negative + " =w " + std::string(type == 'l' ? "csltl" : "csltw") + " " + sign + ", 0");
    std::string nonZero = newTemp();
    line(nonZero + " =w " + std::string(type == 'l' ? "cnel" : "cnew") + " " + remainder + ", 0");
    std::string adjust = newTemp();
    line(adjust + " =w and " + negative + ", " + nonZero);

    std::string fix = newLabel("modfix");
    std::string done = newLabel("moddone");
    branch(Value { adjust, 'w' }, fix, done);
    label(fix);
    std::string adjusted = newTemp();
    line(adjusted + " =" + std::string(1, type) + " add " + remainder + ", " + right.name);
    line(std::string(type == 'l' ? "storel " : "storew ") + adjusted + ", " + slot);
    jump(done);
    label(done);

    std::string result = newTemp();
    line(result + " =" + std::string(1, type) + " " + (type == 'l' ? "loadl " : "loadsw ") + slot);
    return Value { result, type };
}

Value QbeEmitter::emitPower(const Value& left, const Value& right, char type)
{
    bool isFloat = isFloatClass(type);
    std::string resultSlot = allocScratch(type == 'l' || type == 'd' ? 8 : 4);
    std::string counterSlot = allocScratch(4);
    const char* storeInstruction = isFloat ? (type == 'd' ? "stored" : "stores")
                                          : (type == 'l' ? "storel" : "storew");
    const char* loadInstruction = isFloat ? (type == 'd' ? "loadd" : "loads")
                                          : (type == 'l' ? "loadl" : "loadsw");

    line(std::string(storeInstruction) + " " + (isFloat ? realLiteral(1.0, type) : std::string("1")) + ", "
         + resultSlot);
    line("storew " + right.name + ", " + counterSlot);

    std::string head = newLabel("power");
    std::string body = newLabel("powerbody");
    std::string done = newLabel("powerdone");

    label(head);
    std::string counter = newTemp();
    line(counter + " =w loadsw " + counterSlot);
    std::string test = newTemp();
    line(test + " =w csgtw " + counter + ", 0");
    branch(Value { test, 'w' }, body, done);

    label(body);
    std::string accumulator = newTemp();
    line(accumulator + " =" + std::string(1, type) + " " + loadInstruction + " " + resultSlot);
    std::string product = newTemp();
    line(product + " =" + std::string(1, type) + " mul " + accumulator + ", " + left.name);
    line(std::string(storeInstruction) + " " + product + ", " + resultSlot);
    std::string decremented = newTemp();
    line(decremented + " =w sub " + counter + ", 1");
    line("storew " + decremented + ", " + counterSlot);
    jump(head);

    label(done);
    std::string result = newTemp();
    line(result + " =" + std::string(1, type) + " " + loadInstruction + " " + resultSlot);
    return Value { result, type };
}

Value QbeEmitter::emitConcatenation(BinaryExpr* expr)
{
    std::vector<Expr*> parts;
    std::vector<Expr*> pending = { expr };
    while (!pending.empty()) {
        Expr* current = pending.back();
        pending.pop_back();
        if (current->kind == ExprKind::Binary
            && static_cast<BinaryExpr*>(current)->op == BinaryOp::Concatenate) {
            auto* binary = static_cast<BinaryExpr*>(current);
            pending.push_back(binary->right.get());
            pending.push_back(binary->left.get());
            continue;
        }
        parts.push_back(current);
    }

    struct Operand
    {
        Value value;
        Value length;
        bool isCharacter = false;
    };

    std::vector<Operand> operands;
    bool allStatic = true;
    long long staticTotal = 0;

    for (Expr* part : parts) {
        Operand operand;
        operand.value = emitExpr(part);
        operand.isCharacter = part->type == nullptr || part->type->kind != TypeKind::Array;
        if (operand.isCharacter) {
            operand.length = constantValue(1, 'w');
        } else {
            if (typeSize(part->type->element) != 1) {
                m_diagnostics.error(part->location, "'&' is only supported on arrays of characters");
                return Value { "0", 'l' };
            }
            operand.length = lengthOf(operand.value, part->type);
        }
        if (isLiteralOperand(operand.length.name)) {
            staticTotal += std::stoll(operand.length.name);
        } else {
            allStatic = false;
        }
        operands.push_back(operand);
    }

    std::string total = std::to_string(staticTotal);
    for (const Operand& operand : operands) {
        if (isLiteralOperand(operand.length.name)) {
            continue;
        }
        std::string sum = newTemp();
        line(sum + " =w add " + total + ", " + operand.length.name);
        total = sum;
    }

    std::string buffer;
    if (allStatic) {
        buffer = allocScratch(staticTotal > 0 ? staticTotal : 1);
    } else {
        std::string size = newTemp();
        line(size + " =l extsw " + total);
        buffer = newTemp();
        line(buffer + " =l alloc8 " + size);
    }

    std::string running = buffer;
    long long staticOffset = 0;
    bool dynamic = false;

    for (const Operand& operand : operands) {
        std::string target = running;
        if (!dynamic) {
            target = buffer;
            if (staticOffset > 0) {
                target = newTemp();
                line(target + " =l add " + buffer + ", " + std::to_string(staticOffset));
            }
        }

        if (operand.isCharacter) {
            line("storeb " + operand.value.name + ", " + target);
        } else if (isLiteralOperand(operand.length.name)) {
            line("blit " + operand.value.name + ", " + target + ", " + operand.length.name);
        } else {
            std::string bytes = newTemp();
            line(bytes + " =l extsw " + operand.length.name);
            line(newTemp() + " =l call $memcpy(l " + target + ", l " + operand.value.name + ", l " + bytes + ")");
        }

        if (!dynamic && isLiteralOperand(operand.length.name)) {
            staticOffset += std::stoll(operand.length.name);
            continue;
        }
        dynamic = true;
        std::string advance = newTemp();
        line(advance + " =l extsw " + operand.length.name);
        std::string next = newTemp();
        line(next + " =l add " + target + ", " + advance);
        running = next;
    }

    return Value { buffer, 'l', "1", total };
}

Value QbeEmitter::emitIntegerOperation(int operation, const Value& left, const Value& right, char type)
{
    auto widen = [&](const Value& value) {
        if (value.type == 'l') {
            return value.name;
        }
        std::string wide = newTemp();
        line(wide + " =l extsw " + value.name);
        return wide;
    };
    std::string leftWide = widen(left);
    std::string rightWide = widen(right);
    std::string result = newTemp();
    line(result + " =l call $__ada_integer_operation(w " + std::to_string(operation) + ", w "
         + (type == 'l' ? "64" : "32") + ", l " + leftWide + ", l " + rightWide + ")");
    emitExceptionCheck();
    if (type == 'w') {
        std::string narrowed = newTemp();
        line(narrowed + " =w copy " + result);
        result = narrowed;
    }
    return Value { result, type };
}

Value QbeEmitter::emitBinary(BinaryExpr* expr)
{
    switch (expr->op) {
    case BinaryOp::AndThen:
    case BinaryOp::OrElse:
        return emitShortCircuit(expr);
    case BinaryOp::Concatenate:
        return emitConcatenation(expr);
    default:
        break;
    }

    Value left = emitExpr(expr->left.get());
    Value right = emitExpr(expr->right.get());
    char type = left.type == 'l' || right.type == 'l' ? 'l' : left.type;
    if (isFloatClass(left.type)) {
        type = left.type;
    } else if (isFloatClass(right.type)) {
        type = right.type;
    }

    if (!isFloatClass(type)) {
        int operation = -1;
        switch (expr->op) {
        case BinaryOp::Add: operation = 0; break;
        case BinaryOp::Subtract: operation = 1; break;
        case BinaryOp::Multiply: operation = 2; break;
        case BinaryOp::Divide: operation = 3; break;
        case BinaryOp::Remainder: operation = 4; break;
        case BinaryOp::Modulo: operation = 5; break;
        case BinaryOp::Power: operation = 6; break;
        default: break;
        }
        if (operation >= 0) {
            return emitIntegerOperation(operation, left, right, left.type);
        }
    }

    switch (expr->op) {
    case BinaryOp::Equal:
    case BinaryOp::NotEqual:
    case BinaryOp::Less:
    case BinaryOp::LessEqual:
    case BinaryOp::Greater:
    case BinaryOp::GreaterEqual: {
        if (expr->left->type != nullptr && expr->left->type->kind == TypeKind::Array) {
            return compareArrays(expr->op, left, expr->left->type, right, expr->right->type);
        }
        if (baseType(expr->left->type) != nullptr && baseType(expr->left->type)->kind == TypeKind::Record) {
            // The operands are addresses, so comparing them directly would be
            // asking whether they are the same object rather than equal ones.
            Value equal = compareRecords(left, right, expr->left->type);
            if (expr->op == BinaryOp::NotEqual) {
                std::string negated = newTemp();
                line(negated + " =w ceqw " + equal.name + ", 0");
                return Value { negated, 'w' };
            }
            return equal;
        }
        std::string temp = newTemp();
        line(temp + " =w " + comparisonInstruction(expr->op, type) + " " + left.name + ", " + right.name);
        return Value { temp, 'w' };
    }
    case BinaryOp::Modulo:
        return emitModulo(left, right, type);
    case BinaryOp::Power:
        return emitPower(left, right, type);
    default:
        break;
    }

    const char* instruction = "add";
    switch (expr->op) {
    case BinaryOp::Add:
        instruction = "add";
        break;
    case BinaryOp::Subtract:
        instruction = "sub";
        break;
    case BinaryOp::Multiply:
        instruction = "mul";
        break;
    case BinaryOp::Divide:
        instruction = "div";
        break;
    case BinaryOp::Remainder:
        instruction = "rem";
        break;
    case BinaryOp::And:
        instruction = "and";
        break;
    case BinaryOp::Or:
        instruction = "or";
        break;
    case BinaryOp::Xor:
        instruction = "xor";
        break;
    default:
        break;
    }

    std::string temp = newTemp();
    line(temp + " =" + std::string(1, type) + " " + instruction + " " + left.name + ", " + right.name);
    return Value { temp, type };
}

Value QbeEmitter::emitUnary(UnaryExpr* expr)
{
    Value operand = emitExpr(expr->operand.get());
    std::string temp = newTemp();

    switch (expr->op) {
    case UnaryOp::Plus:
        return operand;
    case UnaryOp::Negate: {
        if (!isFloatClass(operand.type)) {
            return emitIntegerOperation(1, Value { "0", operand.type }, operand, operand.type);
        }
        std::string zero = isFloatClass(operand.type) ? realLiteral(0.0, operand.type) : std::string("0");
        line(temp + " =" + std::string(1, operand.type) + " sub " + zero + ", " + operand.name);
        return Value { temp, operand.type };
    }
    case UnaryOp::Not:
        line(temp + " =w ceqw " + operand.name + ", 0");
        return Value { temp, 'w' };
    case UnaryOp::Abs: {
        bool isFloat = isFloatClass(operand.type);
        long long slotSize = operand.type == 'l' || operand.type == 'd' ? 8 : 4;
        std::string slot = allocScratch(slotSize);
        const char* storeInstruction = isFloat ? (operand.type == 'd' ? "stored" : "stores")
                                               : (operand.type == 'l' ? "storel" : "storew");
        const char* loadInstruction = isFloat ? (operand.type == 'd' ? "loadd" : "loads")
                                              : (operand.type == 'l' ? "loadl" : "loadsw");
        std::string zero = isFloat ? realLiteral(0.0, operand.type) : std::string("0");
        line(std::string(storeInstruction) + " " + operand.name + ", " + slot);
        std::string negative = newTemp();
        line(negative + " =w " + comparisonInstruction(BinaryOp::Less, operand.type) + " " + operand.name + ", "
             + zero);
        std::string negate = newLabel("absneg");
        std::string done = newLabel("absdone");
        branch(Value { negative, 'w' }, negate, done);
        label(negate);
        std::string negated = newTemp();
        if (isFloat) {
            line(negated + " =" + std::string(1, operand.type) + " sub " + zero + ", " + operand.name);
        } else {
            negated = emitIntegerOperation(1, Value { "0", operand.type }, operand, operand.type).name;
        }
        line(std::string(storeInstruction) + " " + negated + ", " + slot);
        jump(done);
        label(done);
        std::string result = newTemp();
        line(result + " =" + std::string(1, operand.type) + " " + loadInstruction + " " + slot);
        return Value { result, operand.type };
    }
    }

    return operand;
}

// 'Read, 'Write, 'Output and 'Input move a value between an object and a
// stream as the bytes it occupies.  'Output and 'Input additionally carry the
// bounds of an array whose type does not fix them.
Value QbeEmitter::emitStreamAttribute(AttributeExpr* expr)
{
    const std::string& name = expr->lower;
    Type* prefixType = expr->prefixType;
    Value stream = emitExpr(expr->arguments.front().get());

    if (name == "input") {
        if (isUnconstrainedArray(prefixType)) {
            std::string firstSlot = allocScratch(4);
            std::string lastSlot = allocScratch(4);
            std::string buffer = newTemp();
            line(buffer + " =l call $__ada_stream_read_array(l " + stream.name + ", w "
                 + std::to_string(typeSize(prefixType->element)) + ", l " + firstSlot + ", l " + lastSlot + ")");
            emitExceptionCheck();

            std::string first = newTemp();
            line(first + " =w loadsw " + firstSlot);
            std::string last = newTemp();
            line(last + " =w loadsw " + lastSlot);
            return Value { buffer, 'l', first, last };
        }

        long long size = typeSize(prefixType);
        std::string slot = allocScratch(size < 1 ? 1 : size);
        line("call $__ada_stream_read(l " + stream.name + ", l " + slot + ", w " + std::to_string(size) + ")");
        emitExceptionCheck();

        Value address { slot, 'l' };
        if (isComposite(prefixType)) {
            return withBounds(address, prefixType, nullptr);
        }
        return loadFrom(address, prefixType);
    }

    Expr* item = expr->arguments[1].get();
    Type* type = item->type;
    bool reading = name == "read";

    Value address;
    if (isComposite(type)) {
        address = emitExpr(item);
    } else if (reading) {
        address = emitAddress(item);
    } else {
        // A value being written need not live anywhere of its own.
        long long size = typeSize(type);
        Value value = emitExpr(item);
        address = Value { allocScratch(size < 1 ? 1 : size), 'l' };
        storeInto(address, value, type);
    }

    // An array occupies as many bytes as it currently holds, which the type
    // alone does not say.
    std::string size = std::to_string(typeSize(type));
    Type* array = baseType(type);
    if (array != nullptr && array->kind == TypeKind::Array) {
        if (!address.hasBounds()) {
            address = withBounds(address, type, nullptr);
        }
        Value length = lengthOf(address, type);
        std::string bytes = newTemp();
        line(bytes + " =w mul " + length.name + ", " + std::to_string(typeSize(array->element)));
        size = bytes;

        if (name == "output" && isUnconstrainedArray(prefixType)) {
            line("call $__ada_stream_write_bounds(l " + stream.name + ", w " + address.first + ", w "
                 + address.last + ")");
            emitExceptionCheck();
        }
    }

    line(std::string("call ") + (reading ? "$__ada_stream_read" : "$__ada_stream_write") + "(l " + stream.name
         + ", l " + address.name + ", w " + size + ")");
    emitExceptionCheck();
    return Value { "0", 'w' };
}

Value QbeEmitter::emitAttribute(AttributeExpr* expr)
{
    const std::string& name = expr->lower;
    Type* prefixType = expr->prefixType;

    if (name == "read" || name == "write" || name == "input" || name == "output") {
        return emitStreamAttribute(expr);
    }
    if (name == "pos") {
        return emitExpr(expr->arguments.front().get());
    }
    if (name == "val") {
        Value value = emitExpr(expr->arguments.front().get());
        emitRangeCheck(value, prefixType, expr->location);
        char type = qbeClass(prefixType);
        if (value.type != type) {
            std::string converted = newTemp();
            line(converted + " =" + std::string(1, type)
                 + (type == 'l' ? " extsw " : " copy ") + value.name);
            return Value { converted, type };
        }
        return value;
    }
    if (name == "succ" || name == "pred") {
        Value value = emitExpr(expr->arguments.front().get());
        Value result = emitIntegerOperation(name == "succ" ? 0 : 1, value, Value { "1", value.type }, value.type);
        if (prefixType->kind == TypeKind::Enumeration) {
            emitRangeCheck(result, baseType(prefixType), expr->location);
        }
        return result;
    }
    if (name == "image") {
        Value value = emitExpr(expr->arguments.front().get());
        std::string temp = newTemp();
        Type* base = baseType(prefixType);
        if (isFloatClass(value.type)) {
            std::string wide = widenToDouble(value);
            line(temp + " =l call $__ada_image_float(d " + wide + ", w " + std::to_string(defaultAft(prefixType))
                 + ", w 3)");
        } else if (base != nullptr && base->kind == TypeKind::Enumeration && !base->literals.empty()) {
            // Ada spells the image of an enumeration value in upper case.
            line(temp + " =l call $__ada_image_enum(w " + value.name + ", l " + enumTableFor(base) + ", w "
                 + std::to_string(base->literals.size()) + ")");
        } else if (base != nullptr && base->kind == TypeKind::Enumeration) {
            // Character, whose image is the literal in its quotes.
            line(temp + " =l call $__ada_image_character(w " + value.name + ")");
        } else {
            line(temp + " =l call $" + (value.type == 'l' ? "__ada_image_long_integer(l " : "__ada_image_integer(w ")
                 + value.name + ")");
        }
        std::string size = newTemp();
        line(size + " =l call $strlen(l " + temp + ")");
        std::string length = newTemp();
        line(length + " =w copy " + size);
        return Value { temp, 'l', "1", length };
    }
    if (name == "address") {
        Value address = isComposite(prefixType) ? emitExpr(expr->prefix.get()) : emitAddress(expr->prefix.get());
        return Value { address.name, 'l' };
    }
    if (name == "size") {
        return constantValue(typeSize(prefixType) * 8, 'w');
    }
    if (name == "value") {
        Value text = emitExpr(expr->arguments.front().get());
        Value length = lengthOf(text, expr->arguments.front()->type);
        Type* base = baseType(prefixType);
        std::string temp = newTemp();
        if (base != nullptr && base->kind == TypeKind::Enumeration && !base->literals.empty()) {
            line(temp + " =w call $__ada_value_enum(l " + text.name + ", w " + length.name + ", l "
                 + enumTableFor(base) + ", w " + std::to_string(base->literals.size()) + ")");
        } else if (base != nullptr && base->kind == TypeKind::Enumeration) {
            // Character, whose values are named by their spelling.
            line(temp + " =w call $__ada_value_character(l " + text.name + ", w " + length.name + ")");
        } else if (qbeClass(prefixType) == 'l') {
            line(temp + " =l call $__ada_value_long_integer(l " + text.name + ", w " + length.name + ", l "
                 + std::to_string(prefixType->low) + ", l " + std::to_string(prefixType->high) + ")");
        } else {
            line(temp + " =w call $__ada_value_integer(l " + text.name + ", w " + length.name + ", w "
                 + std::to_string(prefixType->low) + ", w " + std::to_string(prefixType->high) + ")");
        }
        emitExceptionCheck();
        return Value { temp, qbeClass(prefixType) };
    }
    if (name == "first" || name == "last" || name == "length") {
        Type* base = prefixType;
        if (isUnconstrainedArray(base)) {
            Value array = emitExpr(expr->prefix.get());
            if (name == "length") {
                return lengthOf(array, base);
            }
            return Value { name == "first" ? array.first : array.last, 'w' };
        }
        long long value = 0;
        if (base != nullptr && base->kind == TypeKind::Array) {
            value = name == "first" ? base->indexLow : (name == "last" ? base->indexHigh : arrayLength(base));
        } else if (base != nullptr) {
            value = name == "first" ? base->low : base->high;
        }
        return constantValue(value, 'w');
    }

    m_diagnostics.error(expr->location, "unsupported attribute '" + expr->name + "'");
    return Value { "0", 'w' };
}

void QbeEmitter::emitAggregateInto(AggregateExpr* expr, const Value& address, Type* type)
{
    Type* target = type;
    if (target == nullptr) {
        return;
    }

    if (target->kind == TypeKind::Record) {
        for (std::size_t i = 0; i < target->fields.size() && i < expr->resolvedFields.size(); ++i) {
            const FieldInfo& field = target->fields[i];
            Value fieldAddress = address;
            if (field.offset != 0) {
                std::string temp = newTemp();
                line(temp + " =l add " + address.name + ", " + std::to_string(field.offset));
                fieldAddress = Value { temp, 'l' };
            }
            assignInto(fieldAddress, field.type, expr->resolvedFields[i]);
        }
        return;
    }

    if (target->kind != TypeKind::Array) {
        m_diagnostics.error(expr->location, "an aggregate requires an array or record type");
        return;
    }

    long long elementSize = typeSize(target->element);
    long long position = target->indexLow;
    AggregateComponent* others = nullptr;

    for (AggregateComponent& component : expr->components) {
        if (component.isOthers) {
            others = &component;
            continue;
        }
        std::vector<long long> indexes;
        if (component.choiceLows.empty()) {
            indexes.push_back(position++);
        } else {
            for (std::size_t i = 0; i < component.choiceLows.size(); ++i) {
                long long low = component.choiceLows[i]->staticValue;
                long long high = component.choiceHighs[i] ? component.choiceHighs[i]->staticValue : low;
                for (long long index = low; index <= high; ++index) {
                    indexes.push_back(index);
                }
            }
        }
        for (long long index : indexes) {
            long long offset = (index - target->indexLow) * elementSize;
            Value elementAddress = address;
            if (offset != 0) {
                std::string temp = newTemp();
                line(temp + " =l add " + address.name + ", " + std::to_string(offset));
                elementAddress = Value { temp, 'l' };
            }
            assignInto(elementAddress, target->element, component.value.get());
        }
    }

    if (others == nullptr) {
        return;
    }

    // Fill the remaining positions with a runtime loop.
    std::string indexSlot = allocScratch(8);
    line("storel " + std::to_string(position) + ", " + indexSlot);
    std::string head = newLabel("fill");
    std::string body = newLabel("fillbody");
    std::string done = newLabel("filldone");

    label(head);
    std::string index = newTemp();
    line(index + " =l loadl " + indexSlot);
    std::string test = newTemp();
    line(test + " =w cslel " + index + ", " + std::to_string(target->indexHigh));
    branch(Value { test, 'w' }, body, done);

    label(body);
    std::string offset = newTemp();
    line(offset + " =l sub " + index + ", " + std::to_string(target->indexLow));
    std::string scaled = newTemp();
    line(scaled + " =l mul " + offset + ", " + std::to_string(elementSize));
    std::string elementAddress = newTemp();
    line(elementAddress + " =l add " + address.name + ", " + scaled);
    assignInto(Value { elementAddress, 'l' }, target->element, others->value.get());
    std::string next = newTemp();
    line(next + " =l add " + index + ", 1");
    line("storel " + next + ", " + indexSlot);
    jump(head);

    label(done);
}

Value QbeEmitter::emitAggregate(AggregateExpr* expr)
{
    if (isUnconstrainedArray(expr->type)) {
        m_diagnostics.error(expr->location, "array aggregate requires a constrained subtype");
        return Value { "0", 'l', "1", "0" };
    }
    long long size = typeSize(expr->type);
    std::string buffer = allocScratch(size > 0 ? size : 1);
    emitAggregateInto(expr, Value { buffer, 'l' }, expr->type);
    return withBounds(Value { buffer, 'l' }, expr->type, nullptr);
}
