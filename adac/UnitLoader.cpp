#include "UnitLoader.h"

#include "Lexer.h"
#include "Parser.h"

#include <fstream>
#include <sstream>
#include <utility>

namespace
{

// The file a unit lives in: Ada.Text_IO.Integer_IO is ada-text_io-integer_io.
std::string fileKey(const std::string& unitName)
{
    std::string key = toLower(unitName);
    for (char& c : key) {
        if (c == '.') {
            c = '-';
        }
    }
    return key;
}

std::string directoryOf(const std::string& path)
{
    std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return ".";
    }
    return path.substr(0, slash);
}

// A child unit cannot be looked at before its parent, since it is declared
// inside it.
std::string parentOf(const std::string& unitName)
{
    std::size_t dot = unitName.find_last_of('.');
    if (dot == std::string::npos) {
        return std::string();
    }
    return unitName.substr(0, dot);
}

}

UnitLoader::UnitLoader(Diagnostics& diagnostics)
    : m_diagnostics(diagnostics)
{
}

void UnitLoader::addSearchPath(const std::string& directory)
{
    if (directory.empty()) {
        return;
    }
    for (const std::string& existing : m_searchPaths) {
        if (existing == directory) {
            return;
        }
    }
    m_searchPaths.push_back(directory);
}

bool UnitLoader::readFile(const std::string& path, std::string& contents) const
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    contents = buffer.str();
    return true;
}

std::string UnitLoader::findUnit(const std::string& key, const char* extension) const
{
    for (const std::string& directory : m_searchPaths) {
        std::string candidate = directory + "/" + key + extension;
        std::ifstream probe(candidate, std::ios::binary);
        if (probe) {
            return candidate;
        }
    }
    return std::string();
}

bool UnitLoader::parseInto(const std::string& path, const std::string& contents)
{
    int file = m_diagnostics.addFile(path);
    Lexer lexer(contents, file, m_diagnostics);
    Parser parser(lexer.tokenize(), m_diagnostics);
    CompilationUnitPtr unit = parser.parseCompilation();
    unit->fileName = path;

    // Whatever this unit draws on is read first, so that it lands ahead of the
    // unit naming it and Sema meets a declaration before any use of it.
    for (const WithClause& clause : unit->withClauses) {
        for (const std::string& name : clause.names) {
            loadUnit(name, clause.location);
        }
    }

    m_units.push_back(std::move(unit));
    return true;
}

bool UnitLoader::loadSource(const std::string& path)
{
    // A program finds its own units beside it.
    addSearchPath(directoryOf(path));

    if (m_states.find(path) != m_states.end()) {
        return true;
    }
    m_states.emplace(path, State::Loaded);

    // The file name gives the unit away, so a source named here is not read a
    // second time on account of something withing it.
    std::string base = path.substr(directoryOf(path) == "." && path.find('/') == std::string::npos
                                       ? 0
                                       : directoryOf(path).size() + 1);
    std::size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) {
        m_states.emplace(toLower(base.substr(0, dot)), State::Loaded);
    }

    std::string contents;
    if (!readFile(path, contents)) {
        m_diagnostics.error(SourceLocation {}, "cannot read '" + path + "'");
        return false;
    }

    return parseInto(path, contents);
}

bool UnitLoader::loadUnit(const std::string& name, const SourceLocation& from)
{
    std::string key = fileKey(name);

    auto seen = m_states.find(key);
    if (seen != m_states.end()) {
        // A unit already being read is one that withs its way back to itself.
        if (seen->second == State::Loading) {
            m_diagnostics.error(from, "'" + name + "' depends on itself");
            return false;
        }
        return true;
    }

    std::string parent = parentOf(name);
    if (!parent.empty() && !loadUnit(parent, from)) {
        return false;
    }

    std::string specPath = findUnit(key, ".ads");
    std::string bodyPath = findUnit(key, ".adb");
    if (specPath.empty() && bodyPath.empty()) {
        // Standard and System are declared by the compiler and have no file to
        // read.  Sema reports the with clause if the name turns out to stand
        // for no unit at all.
        return false;
    }

    m_states.emplace(key, State::Loading);

    std::string contents;
    if (!specPath.empty()) {
        m_states.emplace(specPath, State::Loaded);
        if (!readFile(specPath, contents)) {
            m_diagnostics.error(from, "cannot read '" + specPath + "'");
            return false;
        }
        parseInto(specPath, contents);
    }

    // The specification is in place, so a body that draws on something naming
    // this unit back is no longer a circle.
    m_states[key] = State::Loaded;

    if (!bodyPath.empty()) {
        m_states.emplace(bodyPath, State::Loaded);
        if (!readFile(bodyPath, contents)) {
            m_diagnostics.error(from, "cannot read '" + bodyPath + "'");
            return false;
        }
        parseInto(bodyPath, contents);
    }

    return true;
}

std::vector<CompilationUnit*> UnitLoader::units() const
{
    std::vector<CompilationUnit*> result;
    for (const CompilationUnitPtr& unit : m_units) {
        result.push_back(unit.get());
    }
    return result;
}
