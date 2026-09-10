#include "Diagnostics.h"
#include "QbeEmitter.h"
#include "Sema.h"
#include "UnitLoader.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifndef ADA_LIBRARY_DEFAULT_PATH
#define ADA_LIBRARY_DEFAULT_PATH ""
#endif

namespace
{

void printUsage()
{
    std::cerr << "usage: adac [options] <source> [<source>...]\n"
              << "  Translates Ada 83 source files into QBE intermediate language.\n"
              << "  Units named in a with clause are read from the library path.\n"
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

}

int main(int argc, char** argv)
{
    std::vector<std::string> inputs;
    std::vector<std::string> includes;
    std::string output;
    std::string library = ADA_LIBRARY_DEFAULT_PATH;

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
        } else if (argument == "--no-stdlib") {
            library.clear();
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
