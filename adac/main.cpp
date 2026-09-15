#include "Diagnostics.h"
#include "QbeEmitter.h"
#include "Sema.h"
#include "UnitLoader.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#ifdef _WIN32
#   include <cstdlib>
#else
#   include <climits>
#endif

#ifndef ADA_LIBRARY_DEFAULT_PATH
#define ADA_LIBRARY_DEFAULT_PATH ""
#endif

#ifndef ADA_INSTALL_LIBDIR
#define ADA_INSTALL_LIBDIR "lib"
#endif

namespace
{

void printUsage()
{
    std::cerr << "AIDA Ada compiler to QBE IR\n"
              << "usage: adac [options] <source> [<source>...]\n"
              << "\n"
              << "  -o <file>       name of the produced file, '-' for standard output\n"
              << "  -I <dir>        another directory to look for units in\n"
              << "  --stdlib <dir>  where the predefined environment lives\n"
              << "  --no-stdlib     leave the predefined environment out altogether\n";
}

std::string defaultOutputName(const std::string& path)
{
    std::size_t dot = path.find_last_of('.');
    std::size_t slash = path.find_last_of("/\\");
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        return path.substr(0, dot) + ".ssa";
    }
    return path + ".ssa";
}

// Several directories travel in one string, separated the way a search path is.
void addSeparatedPaths(UnitLoader& loader, const char* text)
{
    if (text == nullptr) {
        return;
    }
    std::string list = text;
    std::size_t start = 0;
    while (start <= list.size()) {
        std::size_t separator = list.find(':', start);
        std::size_t end = separator == std::string::npos ? list.size() : separator;
        loader.addSearchPath(list.substr(start, end - start));
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }
}

bool directoryExists(const std::string& path)
{
    struct stat information;
    return !path.empty() && stat(path.c_str(), &information) == 0 && S_ISDIR(information.st_mode);
}

std::string directoryOf(const std::string& path)
{
    std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return std::string();
    }
    return path.substr(0, slash);
}

// A name invoked without a slash was found on PATH, and that is the only clue
// to where adac was installed.
std::string alongPath(const std::string& name)
{
    const char* path = std::getenv("PATH");
    if (path == nullptr) {
        return std::string();
    }
    std::string list = path;
    std::size_t start = 0;
    while (start <= list.size()) {
        std::size_t separator = list.find(':', start);
        std::size_t end = separator == std::string::npos ? list.size() : separator;
        std::string candidate = list.substr(start, end - start) + "/" + name;
        if (access(candidate.c_str(), X_OK) == 0) {
            return candidate;
        }
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }
    return std::string();
}

// Symbolic links are followed, so a link on PATH pointing into an install
// leads back to the install rather than to wherever the link happens to sit.
std::string resolvedPath(const std::string& path)
{
#ifdef _WIN32
    char resolved[_MAX_PATH];
    if (_fullpath(resolved, path.c_str(), _MAX_PATH) != nullptr) {
        return resolved;
    }
#else
    char resolved[PATH_MAX];
    if (realpath(path.c_str(), resolved) != nullptr) {
        return resolved;
    }
#endif
    return path;
}

// An install has bin/adac beside lib/ada/adainclude.  Run in place, in a
// build tree or from an unrelated directory, adac has no such neighbour and
// falls back to whatever ADA_LIBRARY_DEFAULT_PATH names instead.
std::string libraryBesideExecutable(const std::string& executablePath)
{
    std::string found = executablePath.find_first_of("/\\") == std::string::npos
        ? alongPath(executablePath)
        : executablePath;
    std::string directory = directoryOf(resolvedPath(found));
    if (directory.empty()) {
        return std::string();
    }
    std::string candidate = directory + "/../" ADA_INSTALL_LIBDIR "/ada/adainclude";
    return directoryExists(candidate) ? candidate : std::string();
}

}

int main(int argc, char** argv)
{
    std::vector<std::string> inputs;
    std::vector<std::string> includes;
    std::string output;
    std::string library = ADA_LIBRARY_DEFAULT_PATH;
    bool librarySelected = false;

    for (int i = 1; i < argc; ++i) {
        std::string argument = argv[i];
        if (argument == "-o") {
            if (i + 1 >= argc) {
                std::cerr << "adac: error: missing file name after '-o'\n";
                return 2;
            }
            output = argv[++i];
        } else if (argument == "-I") {
            if (i + 1 >= argc) {
                std::cerr << "adac: error: missing directory after '-I'\n";
                return 2;
            }
            includes.push_back(argv[++i]);
        } else if (argument.size() > 2 && argument.compare(0, 2, "-I") == 0) {
            includes.push_back(argument.substr(2));
        } else if (argument == "--stdlib") {
            if (i + 1 >= argc) {
                std::cerr << "adac: error: missing directory after '--stdlib'\n";
                return 2;
            }
            library = argv[++i];
            librarySelected = true;
        } else if (argument == "--no-stdlib") {
            library.clear();
            librarySelected = true;
        } else if (argument == "-h" || argument == "--help") {
            printUsage();
            return 0;
        } else if (!argument.empty() && argument[0] == '-' && argument != "-") {
            std::cerr << "adac: error: unknown option '" << argument << "'\n";
            return 2;
        } else {
            inputs.push_back(argument);
        }
    }

    if (inputs.empty()) {
        printUsage();
        return 2;
    }
    if (output.empty()) {
        output = defaultOutputName(inputs.back());
    }

    // Neither --stdlib nor --no-stdlib was given, so adac is on its own: an
    // installed adac looks beside itself before falling back to the path
    // settled at build time.
    if (!librarySelected) {
        std::string beside = libraryBesideExecutable(argv[0]);
        if (!beside.empty()) {
            library = beside;
        }
    }

    Diagnostics diagnostics;
    UnitLoader loader(diagnostics);

    // A unit is looked for beside the source that asked for it first, then
    // where the command line says, then the environment, then in the library
    // that came with the compiler.
    for (const std::string& path : inputs) {
        std::size_t slash = path.find_last_of("/\\");
        loader.addSearchPath(slash == std::string::npos ? "." : path.substr(0, slash));
    }
    for (const std::string& directory : includes) {
        loader.addSearchPath(directory);
    }
    addSeparatedPaths(loader, std::getenv("ADA_INCLUDE_PATH"));
    addSeparatedPaths(loader, library.c_str());

    // The run time raises its exceptions by number, so the package declaring
    // them is part of every program whether or not it was asked for.
    loader.loadUnit("Ada.IO_Exceptions", SourceLocation {});

    for (const std::string& path : inputs) {
        if (!loader.loadSource(path)) {
            return 2;
        }
    }
    if (diagnostics.hasErrors()) {
        return 1;
    }

    std::vector<CompilationUnit*> units = loader.units();

    Sema sema(diagnostics);
    for (CompilationUnit* unit : units) {
        sema.analyze(*unit);
    }
    if (diagnostics.hasErrors()) {
        return 1;
    }

    QbeEmitter emitter(sema, diagnostics);
    if (output == "-") {
        emitter.emit(units, std::cout);
    } else {
        std::ofstream stream(output);
        if (!stream) {
            std::cerr << "adac: error: cannot write '" << output << "'\n";
            return 2;
        }
        emitter.emit(units, stream);
    }

    return diagnostics.hasErrors() ? 1 : 0;
}
