#pragma once

#include "Ast.h"
#include "Diagnostics.h"

#include <string>
#include <unordered_map>
#include <vector>

// Finds the Ada source of a library unit and reads in everything a program
// draws on, so that a `with` clause means what it says rather than obliging the
// caller to name every file on the command line.
//
// A unit is stored under the name GNAT gives it: lower cased, with each dot of
// the name written as a dash, so Ada.Text_IO.Integer_IO lives in
// ada-text_io-integer_io.ads with its body beside it in the .adb file.
class UnitLoader
{
public:
    explicit UnitLoader(Diagnostics& diagnostics);

    void addSearchPath(const std::string& directory);

    // Reads a file named on the command line along with everything it withs.
    bool loadSource(const std::string& path);

    // Reads a unit by name, doing nothing if it has already been read.
    bool loadUnit(const std::string& name, const SourceLocation& from);

    // The units in the order they have to be analysed: whatever a unit draws on
    // comes before it, and the library comes before the program.
    std::vector<CompilationUnit*> units() const;

private:
    enum class State
    {
        Loading,
        Loaded
    };

    bool readFile(const std::string& path, std::string& contents) const;
    std::string findUnit(const std::string& key, const char* extension) const;
    bool parseInto(const std::string& path, const std::string& contents);

    Diagnostics& m_diagnostics;
    std::vector<std::string> m_searchPaths;
    std::vector<CompilationUnitPtr> m_units;
    std::unordered_map<std::string, State> m_states;
};
