#include "Diagnostics.h"

#include <iostream>
#include <utility>

Diagnostics::Diagnostics()
{
    // Identifier zero belongs to whatever has no file of its own.
    m_files.push_back("<unknown>");
}

int Diagnostics::addFile(std::string name)
{
    m_files.push_back(std::move(name));
    return static_cast<int>(m_files.size()) - 1;
}

const std::string& Diagnostics::fileName(int file) const
{
    if (file < 0 || file >= static_cast<int>(m_files.size())) {
        return m_files.front();
    }
    return m_files[static_cast<std::size_t>(file)];
}

void Diagnostics::error(const SourceLocation& location, const std::string& message)
{
    ++m_errorCount;
    report(location, "error", message);
}

void Diagnostics::warning(const SourceLocation& location, const std::string& message)
{
    ++m_warningCount;
    report(location, "warning", message);
}

void Diagnostics::report(const SourceLocation& location, const char* severity, const std::string& message)
{
    std::cerr << fileName(location.file);
    if (location.line > 0) {
        std::cerr << ':' << location.line << ':' << location.column;
    }
    std::cerr << ": " << severity << ": " << message << '\n';
}
