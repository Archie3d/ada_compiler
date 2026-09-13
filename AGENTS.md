---
description: Ada compiler with QBE backend
globs: "**/*"
alwaysApply: false
---

# Code style
- Use 4 spaces indentation (for C, C++, and Ada code)
- Functions code open with curly brace on the a line:
```cpp
void func()
{
    // code
}
```

- Classes and struct definition curly braces start on a new line:
```cpp
class Compiler
{
public:
    Compiler();

protected:

private:
    std::string m_errorMessage;
};
```

In classes the sections should be in this order: `public`, `protected`, `private`.

- Class member variables have `m_` prefix.
- Identifiers use Camel case convention, with UpperCamelCase for types, and lowerCamelCase for variables.

- All other code scopes have curly brase put at the end of the statement line:
```cpp
if (condition) {
    doSomething();
    while (anotherCondition) {
        doSomethingElse();
    }
}
```

# Instructions
- `qbe` folder is a QBE backend - an external project, you can read it but you must not modify it.
- Start with a simple top level Ada program definitions.
- Gradually add support for more complex statements, data types, functions, procedures.
- Add simle test Ada programs to test the compiler development stages.
