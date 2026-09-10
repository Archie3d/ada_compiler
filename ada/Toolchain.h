#pragma once

#include <string>
#include <vector>

// Locates and runs the programs that make up the compilation pipeline.
class Toolchain
{
public:
    const std::string& adac() const { return m_adac; }
    const std::string& qbe() const { return m_qbe; }
    const std::string& compiler() const { return m_compiler; }

    // Everything the linker is given besides the program's own assembly.
    const std::vector<std::string>& runtime() const { return m_runtime; }

    // The directory the predefined environment is read from.
    const std::string& library() const { return m_library; }

    void setVerbose(bool verbose) { m_verbose = verbose; }

    // Runs a program and returns its exit status, or -1 when it cannot start.
    int run(const std::vector<std::string>& command) const;

    // Works out where every part of the compiler is, starting from the driver
    // itself.  Nothing is usable until this has been called.
    void locateFrom(const std::string& executablePath);

private:
    bool adoptInstall(const std::string& binDirectory);
    void applyEnvironment();

    std::string m_adac;
    std::string m_qbe;
    std::string m_compiler;
    std::string m_library;
    std::vector<std::string> m_runtime;
    bool m_verbose = false;
};
