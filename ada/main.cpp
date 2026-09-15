#include "Toolchain.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace
{

enum class Stage
{
    IntermediateLanguage,
    Assembly,
    Executable
};

void printUsage()
{
    std::cerr << "AIDA Ada compiler\n"
              << "usage: ada [options] <source> [<source>...]\n"
              << "\n"
              << "  -o <file>     name of the produced file\n"
              << "  --emit-ir     stop after generating QBE intermediate language\n"
              << "  -S            stop after generating assembly\n"
              << "  -k            keep the intermediate files\n"
              << "  --no-stdlib   leave the predefined environment out of the unit search path\n"
              << "  -v            print each command before running it\n";
}

std::string baseName(const std::string& path)
{
    std::size_t slash = path.find_last_of('/');
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    std::size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

}

int main(int argc, char** argv)
{
    std::vector<std::string> sources;
    std::string output;
    Stage stage = Stage::Executable;
    bool keepIntermediates = false;
    bool verbose = false;
    bool useLibrary = true;

    for (int i = 1; i < argc; ++i) {
        std::string argument = argv[i];
        if (argument == "-o") {
            if (i + 1 >= argc) {
                std::cerr << "ada: error: missing file name after '-o'\n";
                return 2;
            }
            output = argv[++i];
        } else if (argument == "--emit-ir") {
            stage = Stage::IntermediateLanguage;
        } else if (argument == "-S") {
            stage = Stage::Assembly;
        } else if (argument == "-k" || argument == "--keep") {
            keepIntermediates = true;
        } else if (argument == "--no-stdlib") {
            useLibrary = false;
        } else if (argument == "-v") {
            verbose = true;
        } else if (argument == "-h" || argument == "--help") {
            printUsage();
            return 0;
        } else if (!argument.empty() && argument[0] == '-') {
            std::cerr << "ada: error: unknown option '" << argument << "'\n";
            return 2;
        } else {
            sources.push_back(argument);
        }
    }

    if (sources.empty()) {
        printUsage();
        return 2;
    }

    Toolchain toolchain;
    toolchain.locateFrom(argv[0]);
    toolchain.setVerbose(verbose);

    // Without -o the name of the last source gives the stem of every product.
    std::string stem = output.empty() ? baseName(sources.back()) : output;
    std::string irFile = stem + ".ssa";
    std::string assemblyFile = stem + ".s";
    if (!output.empty()) {
        if (stage == Stage::IntermediateLanguage) {
            irFile = output;
        } else if (stage == Stage::Assembly) {
            assemblyFile = output;
        }
    }

    // The driver knows where the predefined environment ended up, so it tells
    // the front end rather than leaving it to guess.  Dropping it affects the
    // unit search path alone: the run time is still linked, since a program
    // raising Constraint_Error calls into it whether or not it names a
    // predefined unit.
    std::vector<std::string> command = { toolchain.adac(), "-o", irFile };
    if (useLibrary) {
        command.push_back("--stdlib");
        command.push_back(toolchain.library());
    } else {
        command.push_back("--no-stdlib");
    }
    for (const std::string& source : sources) {
        command.push_back(source);
    }
    if (toolchain.run(command) != 0) {
        return 1;
    }
    if (stage == Stage::IntermediateLanguage) {
        return 0;
    }

    std::vector<std::string> qbeCommand = { toolchain.qbe() };
#ifdef _WIN32
    // qbe's own default target is the ELF sysv ABI even when built on Windows.
    qbeCommand.push_back("-t");
    qbeCommand.push_back("amd64_win");
#endif
    qbeCommand.push_back("-o");
    qbeCommand.push_back(assemblyFile);
    qbeCommand.push_back(irFile);
    if (toolchain.run(qbeCommand) != 0) {
        if (!keepIntermediates) {
            std::remove(irFile.c_str());
        }
        return 1;
    }
    if (stage == Stage::Assembly) {
        if (!keepIntermediates) {
            std::remove(irFile.c_str());
        }
        return 0;
    }

    std::vector<std::string> link = { toolchain.compiler(), "-o", stem, assemblyFile };
    for (const std::string& file : toolchain.runtime()) {
        link.push_back(file);
    }
    int status = toolchain.run(link);

    if (!keepIntermediates) {
        std::remove(irFile.c_str());
        std::remove(assemblyFile.c_str());
    }
    return status == 0 ? 0 : 1;
}
