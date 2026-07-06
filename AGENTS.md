# General Project Information

- Language: **C++ (native)** with sol2-based **Lua API framework**, targeting multiple platforms (Windows, Mac, Linux).
- Domain: Real-time 3D engine and tooling for classic-era Tomb Raider-style games.
- Architecture: Grid-based, room-based world structure. Rooms are connected via portals (horizontal and vertical) strictly aligned to the grid.
- World units: 1 sector = 1024 world units ≈ 2 meters.
- The codebase is performance-critical and low-level. Unnecessary abstractions should be avoided.

---

# General Guidelines

- Prefer simple, explicit, low-overhead C++ with a preference for C-style patterns.
- Favor readability, predictability, and maintainability over abstraction.
- Minimize hidden behavior and implicit costs.
- Prefer grouping feature-related functionality within self-contained modules.
- Avoid creating large code blocks (roughly over 10–15 lines) inside existing methods. Extract logically independent code into helper functions instead.
- Before implementing common math operations, inspect the existing utilities in the `/Math` directory (`Geometry.cpp`, `Legacy.cpp`, `Random.cpp`, `Solvers.cpp`, etc.) and reuse them whenever appropriate.
- Avoid duplicating code. If similar logic appears multiple times within a feature or module, create a helper function instead.
- Avoid Windows-specific or MSVC-specific code patterns, types, or APIs. Prefer portable C++ compatible with MSVC, Clang, and GCC.
- Never modify existing data layouts.
- Never add new struct or class members unless explicitly requested.
- Never assume a member exists.
- Always inspect the actual class or struct definition before accessing members.
- In TEN, the positive Y-axis points downward.
- Do **not** compile the project unless explicitly requested. If build errors occur, the user will report them.
- Read the user's request carefully before making changes.

---

# Casting Rules

Prefer C-style casts.

```cpp
int value = (int)someFloat;
```

Avoid C++ casts unless absolutely required.

```cpp
static_cast<int>(someFloat);
reinterpret_cast<SomeType*>(ptr);
const_cast<Type*>(ptr);
```

---

# Types

- Prefer `auto` whenever the type is obvious from the initializer.
- Otherwise prefer explicit primitive types (`char`, `unsigned char`, `short`, `unsigned short`, `int`, `unsigned int`, `float`, etc.).
- Avoid `_t` types (`size_t`, `uint32_t`, `int16_t`, etc.) in local code unless required by external APIs.
- Prefer `char` and `unsigned char` over `byte` or `unsigned byte`.
- Always use the `f` suffix for floating-point literals.

```cpp
float value = 2.0f;
```

---

# Namespaces

Do not use anonymous namespaces.

```cpp
namespace
{
}
```

---

# Includes

Local includes use quotes.

```cpp
#include "LocalHeader.h"
```

External or system includes use angle brackets.

```cpp
#include <vector>
```

Refer to `framework.h` before adding additional system or external includes.

Include order:

1. Module header (`.cpp` only).
2. External/system headers.
3. Local project headers.

Rules:

- Alphabetical order inside each group unless build order requires otherwise.
- Separate groups with one blank line.

---

# Formatting

- Four spaces for indentation.
- No tabs.
- Windows (CRLF) line endings.
- ASCII characters only.

## Braces

```cpp
if (condition)
{
    DoSomething();
}
```

Always use braces for multi-statement blocks.

Single-statement early exits should remain on separate lines.

```cpp
if (condition)
    return;
```

Never write:

```cpp
if (condition) return;
```

## Spacing

- One space after `if`, `for`, `while`.
- Spaces around binary operators.
- Spaces after commas.
- Separate logical groups with blank lines.
- Break long expressions across multiple lines when readability improves.

---

# Naming

| Item | Style |
|------|------|
| Classes | PascalCase |
| Structs | PascalCase |
| Methods | PascalCase |
| Public members | PascalCase |
| Local variables | camelCase |
| Parameters | camelCase |
| Private members | `_memberName` |
| Constants | ALL_CAPS |
| enum class | PascalCase |
| Legacy enum | ALL_CAPS |
| Interfaces | `IExample` |

Additional rules:

- Use descriptive names.
- Avoid Hungarian notation.
- Avoid meaningless abbreviations.
- Do not repeat the class name inside method names.
- Do not use prefix-based naming except `g_` for globals.

---

# Members and Access

- Prefer `auto` when the type is obvious.
- Always write `auto*` or `auto&` when storing pointers or references.
- Prefer `constexpr auto` for constants whenever practical.
- Use explicit types when required by logic or when the type name is shorter than approximately six characters (`int`, `bool`, `float`, etc.).

---

# Control Flow

- Prefer early exits.
- Avoid excessive nesting.
- Keep control flow linear and easy to read.

Exception handling should only be used where appropriate.

```cpp
try
{
    DoWork();
}
catch (...)
{
    TENLog("Error occurred.");
}
```

Warnings caused by invalid user actions should also be logged using `TENLog` whenever practical.

---

# Comments

- Use `//` comments.
- Avoid block comments.
- Keep comments minimal.
- Prefer expressive code over explanatory comments.
- Comments should end with a period.

Example:

```cpp
// Initialize player state.
```

Complex modules or functions may begin with a brief 2–3 line description separated from the implementation by one blank line.

---

# Code Organization

- Group related logic together.
- Separate logical sections with blank lines.
- Place frequently reused constants and static helpers near the top of the class or module.
- Constants used only within one function should remain inside that function.
- Small related lambdas may be grouped together.

---

# Performance

- Prefer stack allocation whenever practical.
- Avoid unnecessary heap allocations.
- Cache frequently used values locally.
- Use `g_Parallel` for sufficiently large bulk operations.
- Avoid thread-unsafe operations inside parallel code.
- Prefer cache-friendly and data-oriented implementations where appropriate.

---

# Existing Code

- Search for existing functionality before implementing new code.
- Reuse existing utilities whenever possible.
- Prefer extending existing helpers over creating duplicate implementations.

---

# Implementation

Allowed:

- Local variables.
- Temporary variables.
- Local helper functions.
- New private methods when necessary.

Not allowed:

- Modifying existing data layouts.
- Adding new struct or class fields without explicit permission.
- Assuming nonexistent members.

---

# Safety

- Never invent missing members.
- Always verify class and struct definitions before use.
- Ask for clarification whenever required information is unavailable.

---

# Persistent Project Memory

## Session Start

Always:

- Read `memory.md`.
- Read `progress.md`.

Treat both files as the authoritative project state.

---

## memory.md

`memory.md` stores permanent project knowledge.

After completing a task, evaluate whether new long-term knowledge has been discovered.

Update `memory.md` only when one or more of the following changed:

- Engine architecture
- Engine behavior discovered through code analysis
- Project conventions
- Coding conventions
- User preferences
- Engine limitations
- Important implementation details that will likely be useful in future sessions

Do NOT store:

- Temporary work
- Current tasks
- TODOs
- Progress information
- Experimental ideas
- Information already present in `progress.md`

Each entry should be concise and written as a lasting fact.

---

## progress.md

Contains the current work state.

Update whenever progress is made.

Include:

- Current task.
- Completed work.
- Modified files.
- Next step.
- Blockers.

Keep entries concise and remove outdated information.

---

# Critical Workflow Rule (Non-Negotiable)

The workflow is a strict state machine.

After **every successful file modification**:

1. Immediately update `progress.md`.
2. Verify the update completed successfully.
3. Confirm the update.
4. Only then continue with the next action.

Never batch multiple updates into a single entry.

If `progress.md` cannot be updated successfully:

- Stop immediately.
- Report the issue.
- Do not continue working until the update succeeds.

Missing or skipped `progress.md` updates place the workflow into a failure state.

5. Evaluate whether `memory.md` requires an update.
6. If new long-term knowledge exists, update `memory.md`.
6. Only then report the task as completed.