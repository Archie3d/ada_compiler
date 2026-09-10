#include "Toolchain.h"

#include <climits>
#include <cstdlib>
#include <iostream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{

bool fileExists(const std::string& path)
{
    struct stat information;
    return !path.empty() && stat(path.c_str(), &information) == 0;
}

bool directoryExists(const std::string& path)
{
    struct stat information;
    return !path.empty() && stat(path.c_str(), &information) == 0 && S_ISDIR(information.st_mode);
}

std::string directoryOf(const std::string& path)
{
    std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return std::string();
    }
    return path.substr(0, slash);
}

// The run time paths arrive as one colon separated list, in the same shape as
// the search paths the shell uses.
std::vector<std::string> splitList(const std::string& text)
{
    std::vector<std::string> parts;
    std::string current;

    for (char c : text) {
        if (c == ':') {
            if (!current.empty()) {
                parts.push_back(current);
            }
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    return parts;
}

// A name invoked without a slash was found on PATH, and that is the only clue
// to where the compiler was installed.
std::string alongPath(const std::string& name)
{
    const char* path = std::getenv("PATH");
    if (path == nullptr) {
        return std::string();
    }
    for (const std::string& directory : splitList(path)) {
        std::string candidate = directory + "/" + name;
        if (access(candidate.c_str(), X_OK) == 0) {
            return candidate;
        }
    }
    return std::string();
}

// The directory the driver really sits in, symbolic links followed, so that a
// link on PATH pointing into an install leads back to the install.
std::string homeOf(const std::string& executablePath)
{
    std::string found = executablePath.find('/') == std::string::npos ? alongPath(executablePath) : executablePath;
    if (found.empty()) {
        return std::string();
    }

    char resolved[PATH_MAX];
    if (realpath(found.c_str(), resolved) != nullptr) {
        found = resolved;
    }
    return directoryOf(found);
}

void overrideFrom(const char* environmentName, std::string& value)
{
    if (const char* text = std::getenv(environmentName)) {
        if (*text != '\0') {
            value = text;
        }
    }
}

// The programs of an install stand beside the driver.  One that is missing is
// left as it was, so a half built install still runs on what it has.
void adoptProgram(const std::string& directory, const char* name, std::string& program)
{
    std::string beside = directory + "/" + name;
    if (fileExists(beside)) {
        program = beside;
    }
}

}

void Toolchain::locateFrom(const std::string& executablePath)
{
    // Where the build tree put everything, which is what a driver run from the
    // build tree should go on using.
    m_adac = ADAC_DEFAULT_PATH;
    m_qbe = QBE_DEFAULT_PATH;
    m_compiler = CC_DEFAULT_PATH;
    m_library = ADA_LIBRARY_DEFAULT_PATH;
    m_runtime = { ADA_RUNTIME_DEFAULT_PATH };

    // An install found beside the driver has the last word over the build
    // tree, since a driver standing there is the installed one.  The prefix
    // settled at configure time is tried only when the build tree is gone,
    // so that an old install cannot capture a driver still being developed.
    if (!adoptInstall(homeOf(executablePath)) && !directoryExists(m_library)) {
        if (directoryExists(ADA_LIBRARY_INSTALL_PATH)) {
            m_library = ADA_LIBRARY_INSTALL_PATH;
        }
        if (fileExists(ADA_RUNTIME_INSTALL_PATH)) {
            m_runtime = { ADA_RUNTIME_INSTALL_PATH };
        }
    }

    applyEnvironment();
}

// bin/ada, bin/adac, bin/qbe and lib/ada beside them.  The environment being
// there is what says this is an install and not just some directory.
bool Toolchain::adoptInstall(const std::string& binDirectory)
{
    if (binDirectory.empty()) {
        return false;
    }

    std::string root = binDirectory + "/../" + ADA_INSTALL_LIBDIR + "/ada";
    if (!directoryExists(root + "/adainclude")) {
        return false;
    }
    m_library = root + "/adainclude";

    if (fileExists(root + "/adalib/libadart.a")) {
        m_runtime = { root + "/adalib/libadart.a" };
    }
    adoptProgram(binDirectory, "adac", m_adac);
    adoptProgram(binDirectory, "qbe", m_qbe);
    return true;
}

void Toolchain::applyEnvironment()
{
    overrideFrom("ADAC", m_adac);
    overrideFrom("QBE", m_qbe);
    overrideFrom("CC", m_compiler);
    overrideFrom("ADA_LIBRARY", m_library);

    std::string runtime;
    overrideFrom("ADA_RUNTIME", runtime);
    if (!runtime.empty()) {
        m_runtime = splitList(runtime);
    }
}

int Toolchain::run(const std::vector<std::string>& command) const
{
    if (command.empty()) {
        return -1;
    }

    if (m_verbose) {
        for (std::size_t i = 0; i < command.size(); ++i) {
            std::cerr << (i == 0 ? "" : " ") << command[i];
        }
        std::cerr << '\n';
    }

    std::vector<char*> arguments;
    arguments.reserve(command.size() + 1);
    for (const std::string& argument : command) {
        arguments.push_back(const_cast<char*>(argument.c_str()));
    }
    arguments.push_back(nullptr);

    pid_t child = fork();
    if (child < 0) {
        return -1;
    }
    if (child == 0) {
        execvp(arguments[0], arguments.data());
        std::cerr << "ada: error: cannot run '" << command.front() << "'\n";
        _exit(127);
    }

    int status = 0;
    if (waitpid(child, &status, 0) < 0) {
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}
