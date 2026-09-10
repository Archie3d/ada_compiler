#pragma once

#include <string>
#include <vector>

// Where something was written.  The file is an index into the table the
// Diagnostics below keeps, so that a location stays meaningful once the units
// of a program and the ones it draws from the library are compiled together.
struct SourceLocation
{
    int line = 0;
    int column = 0;
    int file = 0;
};

class Diagnostics
{
public:
    Diagnostics();

    // Records a file and returns the identifier its locations carry.
    int addFile(std::string name);
    const std::string& fileName(int file) const;

    void error(const SourceLocation& location, const std::string& message);
    void warning(const SourceLocation& location, const std::string& message);

    bool hasErrors() const { return m_errorCount > 0; }
    int errorCount() const { return m_errorCount; }

private:
    void report(const SourceLocation& location, const char* severity, const std::string& message);

    std::vector<std::string> m_files;
    int m_errorCount = 0;
    int m_warningCount = 0;
};
