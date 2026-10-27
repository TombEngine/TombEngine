## General Project Information

- Language: **C++ (native)** with sol2-based **Lua API framework**, targeting multiple platforms (Windows, Mac, Linux).
- Domain: Real-time 3D engine and tooling for classic-era Tomb Raider–style games.
- Architecture: Grid-based, room-based world structure. Rooms are connected via portals (horizontal/vertical) strictly aligned to the grid.
- World units: 1 sector = 1024 world units = roughly 2 meters.
- Codebase is **performance-critical** and low-level. Unnecessary abstractions should be avoided.

## General Guidelines

- Prefer simple, explicit, low-overhead C++, with a preference for C-style patterns.
- Favor readability and predictability over abstraction.
- Minimize hidden behavior and implicit costs.
- Prefer grouping all feature-related functionality within self-contained modules. Avoid creating large code blocks over 10–15 lines in existing modules; instead, offload code to helper functions.
- Before implementing your own helper methods for common math operations, refer to existing utility modules in the `/Math` subdirectory (`Math/Geometry.cpp`, `Math/Legacy.cpp`, `Math/Random.cpp`, `Math/Solvers.cpp`, etc.) and use them instead, if applicable.
- Avoid duplicating and copy-pasting code. Implement helper methods instead whenever similar code is used in several places within a given module, class, or feature scope.
- Avoid using Windows-specific or MSVC compiler-specific code patterns, data types or functions. Prefer universal patterns that are tolerated by all platform-specific compilers (MSVC, g++, Clang).

## Casting Rules

- Prefer C-style casts:

    ```cpp
    int value = (int)someFloat;
    ```

- Avoid these C++-style casts unless absolutely necessary by design:

    ```cpp
    static_cast<int>(someFloat);
    reinterpret_cast<...>();
    const_cast<...>();
    ```

## Types

- Avoid types ending with `_t`, such as `size_t`, `uint32_t`, `int16_t`, in local code. Use these types only when referencing external library methods or functions.
- Prefer explicit types such as `char`, `unsigned char`, `short`, `unsigned short`, `int`, `unsigned int`.
- Prefer `unsigned char` and `char` over `unsigned byte` and `byte`.

## Namespaces

- Do not use anonymous namespaces, like in this example:

    ```cpp
    namespace
    {
        // code...
    }
    ```

## Includes

- Local includes must use quotes `""`.
- External library or system includes must use angle brackets `<>`.
- Refer to the `framework.h` file to determine system or external library includes that should not be explicitly added to modules.

- **Includes should be grouped in this order**:
  - Module's own `.h` include (for `.cpp` files).
  - External library or system includes.
  - Local project includes.

- Every include group must be sorted in alphabetical order unless a specific order is required for a successful build.
- Every include group must be separated from another group with a blank line.

## Formatting

- Indentation: 4 spaces (no tabs).
- Files must use Windows line endings.
- Only standard ASCII symbols are allowed; do not use Unicode symbols, even in comments.

- **Braces**:
  - Namespace declarations, type definitions, and `if`/`while`/`for` blocks should place the opening curly brace `{` on a new line.
  - Always use braces for multi-statement `if` blocks.
  - Do not use braces for single-statement `if` blocks unless the statement is part of a multi-branch conditional (`else if`, etc.).

- **Line breaks and spacing**:
  - A blank line separates logically distinct groups of members (fields, constructors, public methods, static helpers, etc.).
  - Use spaces around binary operators (`=`, `+`, `==`, etc.) and after commas.
  - A single space follows the keywords `if`/`for`/`while` before the opening parenthesis.
  - Expressions may be broken into multiple lines and aligned with the previous line's indentation level to improve readability.

- Do not collapse early exits or single-statement conditions into a single line:

    Bad example:
	
    ```cpp
    if (condition) return;
    ```

    Do this instead:
	
    ```cpp
    if (condition)
        return;
    ```

## Naming

- **PascalCase** for public types, methods, constants, properties, and events.
- **camelCase** for private fields and local variables. Private fields should start with an underscore (`_itemIndex`, `_rendererFont`). Local variables should not start with an underscore.
- Constants use ALL_CAPS.
- `enum class` members use PascalCase. Old-style `enum` members use ALL_CAPS.
- Methods and variables should use clear, descriptive names and generally avoid Hungarian notation. Avoid short, non-descriptive names such as `s2`, `rwh`, `fmp`, unless the underlying meaning is trivial (e.g., `x`, `i`).
- Do not use prefix-based method and field names, except `g_`, which indicates "global".
- Class method and field names should not repeat words from the class name itself (e.g., `InputHandler::InitializeInputHandler` is a bad name, but `InputHandler::Initialize` is a good name).
- Interfaces are prefixed with `I` and use PascalCase (`IScalable`).

## Members and Access

- `auto` should be preferred where possible when the right-hand type is evident from the initializer. Always use `*` and `&` with `auto` if the underlying type is a pointer or reference.
- `constexpr auto` should be preferred for constants unless it interferes with the compiler's ability to make them static.
- Explicit typing should be used only when required by logic or the compiler, or when the type name is shorter than 6 characters (e.g., `int`, `bool`, `float`).
- For floating-point numbers, always use the `f` postfix and include a decimal, even if the value is not fractional (e.g., `2.0f`).

## Control Flow and Syntax

- Avoid excessive condition nesting and use early exits or breaks where possible.
- Exception and error handling is done with `try`/`catch`, and caught exceptions are logged using `TENLog` where appropriate.
- Warnings or safeguards must also be logged using `TENLog` where possible if incorrect behavior is caused by user action.

## Comments

- Use single-line comments (`//`). Block comments (`/* ... */`) are rare.
- Comments should be sparse. Code should rely on meaningful names rather than inline documentation.
- If a module or function implements complex functionality, a brief description (2–3 lines) may be added before it, separated by a blank line from the function body.
- All descriptive comments should end with a full stop (`.`).

## Code Grouping

- Large methods should group related actions together, separated by blank lines.
- Constants and static helpers used multiple times should appear at the top of a class or module.
- Constants used only within a method should be declared within that method.
- One-line lambdas may be grouped together if they share similar meaning or functionality.

## Performance

- Prefer performant approaches and locally cache frequently used data within the function scope whenever possible.
- Use `g_Parallel` for bulk operations to maximize performance if parallelization overhead does not exceed the data size. Avoid using it in thread-unsafe contexts or when operating on inherently serial data sets.