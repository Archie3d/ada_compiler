#include "QbeEmitter.h"

namespace
{

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

}

QbeEmitter::QbeEmitter(Sema& sema, Diagnostics& diagnostics)
    : m_sema(sema)
    , m_diagnostics(diagnostics)
{
}

void QbeEmitter::emit(const std::vector<CompilationUnit*>& units, std::ostream& out)
{
    // The run time library reports failures through these, so they have to be
    // visible to the linker and not just to the generated code.
    m_data << "export data $__ada_exception = align 4 { z 4 }\n";
    m_data << "export data $__ada_exception_name = align 8 { z 8 }\n";

    for (CompilationUnit* unit : units) {
        collectGlobals(unit->units);
    }

    emitElaboration(units);

    for (CompilationUnit* unit : units) {
        emitSubprogramsIn(unit->units);
    }

    emitMain();

    out << m_data.str() << "\n";
    for (const std::string& function : m_functions) {
        out << function << "\n";
    }
}

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
