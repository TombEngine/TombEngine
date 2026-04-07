## General Project Information

- Language: **C++ (native)** with sol2-based **Lua API framework**.
- Domain: Real-time 3D engine and tooling for the classic-era Tomb Raider–style games.
- Architecture: Grid-based, room-based world structure. Rooms connected via portals (horizontal/vertical) strictly aligned to grid.
- World units: 1 sector = 1024 world units = roughly 2 meters.
- Codebase is **performance-critical** and low-level. Unnecessary abstractions should be avoided.

## General Guidelines

- Prefer simple, explicit, low-overhead C++, with a preference for C-styled patterns.
- Favor readability and predictability over abstraction.
- Minimize hidden behaviour and implicit costs.
- Prefer grouping all feature-related functionality within a self-contained module or modules. Avoid creating large code blocks over 10–15 lines in existing modules; instead, offload code to helper functions.
- Avoid duplicating and copypasting code. Implement helper methods instead, whenever similar code is used in several places within a given module, class or feature scope.

## Casting Rules

- Prefer C-style casts:

    ```cpp
    int value = (int)someFloat;
    ```
	
- Avoid these C++ styled casts, unless absolutely necessary by design:

     ```cpp
    static_cast<int>(someFloat);
    reinterpret_cast<...>();
    const_cast<...>();
    ```
## Types

- Avoid types ending with `_t`, such as `size_t`, `uint32_t`, `int16_t` in local code. Use these types only when referencing external library methods or functions.
- Prefer verbose types, such as `char`, `unsigned char`, `short`, `unsigned short`, `int`, `unsigned int`.
- Prefer `unsigned char` and `char` over `unsigned byte` and `byte`.

## Namespaces

- Do not use anonymous namespaces:

    ```cpp
    namespace
    {
        some code...
    }
    ```
	
## Includes

- Local includes must use quotes `"`.
- External library or system includes must use angle brackets `<>`.
- Refer to `framework.h` file to know system or external library includes that shouldn't be explicitly added to modules.

- **Includes should be grouped in this order**:
  - Module's own `.h` include (for `.cpp` files).
  - External library or system includes.
  - Local project includes.
  
- Every include group must be sorted in alphabetical order, unless any other specific order is necessary for successful build.
- Every include group must be separated from another group with a blank line.

## Formatting

- Indentation: 4 spaces (no tabs).
- Files must use Windows line endings.
- Only standard ASCII symbols are allowed; do not use Unicode symbols, even in comments.

- **Braces**:
  - Namespace declarations, type definitions, `if/while/for` blocks should place the opening curly brace `{` on a new line.
  - Always use braces for multi-statement `if` blocks.
  - Do not use braces for single-statement `if` blocks, unless the statement has multiple `else if` conditions with surrounding statements being multi-line.

- **Line breaks and spacing**:
  - A blank line separates logically distinct groups of members (fields, constructors, public methods, static helpers, etc.).
  - Spaces around binary operators (`=`, `+`, `==`, etc.) and after commas.
  - A single space follows keyword `if`/`for`/`while` before the opening parenthesis.
  - Expressions may be broken into multiple lines and aligned with the previous line's indentation level to improve readability.
  
- Do not collapse early exits or single-statement conditions into a single line: 
  
    Bad example:
      ```csharp
      if (condition) return;
      ```
    Do this instead:
      ```csharp
      if (condition)
          return;
      ```

## Naming

- **PascalCase** for public types, methods, constants, properties and events.
- **camelCase** for private fields and local variables. Private fields should start with an underscore (`_itemIndex`, `_rendererFont`). Local variables should not start with an underscore.
- Constants use ALL_CAPS.
- `enum class` members use PascalCase. Old-styled `enum` members use ALL_CAPS.
- Methods and variables should use clear, descriptive names and generally avoid Hungarian notation. Avoid using short non-descriptive names, such as `s2`, `rwh`, `fmp`, unless underlying meaning is brief (e.g. X coordinate is `x`, counter is `i`).
- Class method and field names should not repeat words from a class name itself (e.g. `InputHandler::InitializeInputHandler` is a bad name, but `InputHandler::Initialize` is a good name).
- Interfaces are prefixed with `I` and use PascalCase (`IScaleable`).

## Members and Access

- `auto` type should be preferred where possible, when the right-hand type is evident from the initializer. Always use `*` and `&` with `auto` if underlying type is pointer or reference.
- `constexpr auto` type should be preferred for constants, unless it interferes with compiler's ability to make them static.
- Explicit typing should be only used when it is required by logic or compiler, or when type name is shorter than 6 symbols (e.g. `int`, `bool`, `float`).
- For floating-point numbers, always use `f` postfix and decimal, even if value is not fractional (e.g. `2.0f`).

## Control Flow and Syntax

- Avoid excessive condition nesting and use early exits / breaks where possible.
- Exception and error handling is done with `try`/`catch`, and caught exceptions are logged with using `TENLog` where appropriate.
- Warnings or safeguards must also be logged by `TENLog` where possible, if cause for the incorrect behaviour is user action.

## Comments

- When comments appear they are single-line `//`. Block comments (`/* ... */`) are rare.
- Comments are sparse. Code relies on meaningful names rather than inline documentation.
- If module or function implements complex functionality, a brief description (2-3 lines) may be added in front of it, separated by a blank line from the function body.
- All descriptive comments should end with a full stop (`.`).

## Code Grouping

- Large methods should group related actions together, separated by blank lines.
- Constants and static helpers that are used several times should appear at the top of a class or module.
- Constants that are used only within a scope of a method, should be declared within this method.
- One-liner lambdas may be grouped together, if they share similar meaning or functionality.

## Performance

- Prefer more performant approaches and locally cache frequently used data within the function scope whenever possible.
- Use `g_Parallel` for bulk operations to maximize performance, if parallelization overhead does not exceed the data length. Avoid using it in thread-unsafe contexts or when operating on serial data sets.