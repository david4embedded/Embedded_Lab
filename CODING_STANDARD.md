# C/C++ Coding Standard

## 1. Language and toolchain

- Use C++23 as the C++ language standard.

## 2. Formatting and layout

- Use Allman brace style.
- Indent `case` labels four spaces inside `switch`; do not add braces around case blocks.
- Do not break lines solely to meet a line-length limit; keep expressions and calls intact unless wrapping improves readability.

## 3. Naming

- Class and struct names use UpperCamelCase.
- Class methods use lower camelCase and start with a verb. Functions declared in a namespace use UpperCamelCase and start with a verb (for example, `AppShell::Initialize`).
- Prefix non-static global functions declared outside namespaces with the uppercase module name derived from the source file name, followed by an underscore and a lower camelCase verb name (for example, `MQTT_beginClient` for the `mqtt_client` module). File-local `static` functions do not use a module prefix and use lower camelCase verb names. Keep externally required framework entry-point names unchanged.
- Use `result` as the default name for a local variable holding a function's return value.
- Class and struct member variables start with `m_`.
- Global variables start with `g_`.
- Static variables start with `s_`, including class and struct members; this takes precedence over the other variable prefixes.
- Constant names use UPPER_SNAKE_CASE.
- file names use snake case and start with a low case.

## 4. Types, declarations, and interfaces

- Define constants as named `constexpr` values near the top of the file when practical; clear delay or wait values may remain inline.
- Use `static_assert` to verify compile-time assumptions.
- Use inheritance specifiers such as `override` and `final` where appropriate; avoid multiple inheritance when practical.
- Mark constructors `explicit` when implicit conversions are not intended.
- Prefer `enum class` over unscoped `enum` whenever practical.
- Declare structures as `struct Name`; do not use `typedef struct Name`.
- Do not use global `using namespace std`; qualify standard library names with `std::`.
- Initialize class member variables in the class declaration using `{}` when practical.
- Do not use `{}` initialization for variables or objects inside function bodies; it hurts readability.
- Prefer suitable utility functions from `std::` over custom implementations.
- Prefer lambdas for code reused repeatedly within a function.
- Specify lambda return types using trailing return type syntax (`-> Type`).
- Capture one or two variables explicitly by reference; when capturing more than two, use `[&]`.
- Declare return-value locals near the start of a function when practical; declare other locals close to their first use.
- Use the appropriate C++ cast for the conversion.
- Declare file-local `static` functions before function definitions; define global functions first, followed by `static` functions.
- Avoid anonymous namespaces; use `static` for file-local symbols.
- Prefer fixed-width integer types from `stdint`.
- Prefer `auto` when the variable's type is clear from its initializer.
- Prefer references for function inputs; use `const` for values that are not modified.
- Prefer `std::` facilities; in RTOS and bare-metal applications, use only facilities that do not allocate from the heap.

## 5. Resource and lifetime management

- Avoid dynamic memory allocation where practical.

## 6. Control flow and error handling

- Prefer range-based `for` loops when applicable.

## 7. Comments and documentation

- Use Doxygen-style comments.
- Add a file header briefly describing the file's purpose, its original creation date, and author (`Sungsu Kim`).
- Use `//!<` for comments on class members.
- Document each function at its definition with a Doxygen block containing a concise `@brief` and `@retval`; use `@retval None` for `void` functions. Put documentation for out-of-line functions in the source file, not on header declarations. Retain function documentation for inline functions defined in headers, since their definition is in the header. Do not document parameters.
