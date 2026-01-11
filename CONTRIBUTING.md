What you will read under has been agreed by the current active team in order to keep a good flow of work and proper management. You are welcome to put suggestions in the comments in order to discuss it with the TEN team.

# Contributor General rules

**1. Developer**

- If you wish to contribute to the TEN project, you must read and follow the coding convention used by the team, available here: https://github.com/MontyTRC89/TombEngine/blob/master/CONTRIBUTING.md#coding-conventions
- Do not work directly on master nor any branch which may be developed by several members. Create your branch, and name it with a clear description of the feature/set of features you are working on. You may regularly merge/rebase master or the active development branch with yours.
- Be respectful to other developers. Disagreements may occur on code style, changes, and should be discussed directly with the person who pushed or merged the commit/branch. If you are unsure of what some parts of the code means, or why a decision was made, contact the team as a whole so they can be explained to you.
- Do not make a pull request until your branch and features are extensively tested by the QA team. You are invited to test your feature set as much as you can too.
- If you are allowed to merge PRs, do not do such until code has been properly reviewed, preferably by more than one person.
- Merge PRs using **squash and merge** mode to preserve main branch history in original state. For squash merge commit, leave list of commits in commit description (GitHub does it automatically in both desktop client and web interface).
- Do not revert active development branch to a previous commit unless you are absolutely sure there will be no regressions. Do not revert parts of code to a previous state for the same reasons. In both of those cases, the entire team needs to be aware of it and agree on the decision.
- There are **two key branches**: `master` and `develop`. `master` is to be used for releases, and nothing else. `develop` is based on `master` and is used for active development, **Every PR should use it as its base**. To keep in sync, for release period they are to be merged in the following order: `develop` -> `master`.
 
**2. Tester**

- Be respectful with the developer you are testing the feature of.
- New features or bug fixes should be tested with several use cases, if not all possible use cases you can think of, of this feature. This avoids going back to it in the future because of missed test cases. 
- Pay attention to any regressions. Developer may have touched part of the code that could break other linked or unlinked features. Features or bug fixes can't be considered done if there are any regression elsewhere.

## Issues handling

**1. General**

- Preferably, write one issue for one specific bug. List of bugs should stay rare, unless they are closely related to each other.
- Follow the progress of features/fixes on the project board.
- Do not assign a dev to an issue. They will do so themselves
- Carefully choose the labels you add to the issue

**2. Developer**

- Assign yourself to an issue you want to work on available in the To do column of the project board. Add this feature to the "In progress" column. Make the team know you are working on it both by adding a comment to it and via Discord, on the strategy channel of Tomb Engine.
- Regularly update the status of the task by adding comments. If you are stuck, let other people know if they can help.
- Do not pass an issue as done unless fixes or features are fully tested and merged with active dev branch

**3. Tester**

- Give the Developer as much detail as you can about the bug you're writing for, and an easy way to reproduce it.
- Update and comment the issue with the tests you've done and your findings after fixes.

# Coding Conventions

## General Rules

* **Do not silently comment out any lines or change operators or conditional statements**, even for testing purposes. Should you comment out certain code, also leave a comment beginning with `TODO` prefix, and date-author signature, like this:

``` c
    // TODO: This code caused NPCs to run into walls, so it is disabled for now. -- Lwmte, 21.08.19
    // GetBoundsAccurate(blah);
```

* **Do not modify existing code without a clear understanding of what it does.** Even if you think you are fixing bugs or restoring original behavior by reverting the code, consult the TombEngine discord before committing any changes. It is possible you may ruin the work of other people since the original Core Design codebase is very fragile and it is very easy to falsely identify code as bugged.

* **Do not introduce quick hacks to fix issues**. Hacks like `if (room == 314)` or `if (direction == WEST)` should be left in the history of bad Core Design coding practices.

* If code remains unimplemented or you need to visit another part of the code for a considerable amount of time to implement missing functionality for that first part of code, **leave a comment** prefixed `FIXME` and with a date-author signature, describing missing functionality that you are about to add:

``` c
    // FIXME: I need to precisely calculate floor height, but for now let's keep original.  -- Lwmte, 21.08.19
    foo = GetFloor(blah);
```

* Make sure that **any new code is warning-free** before committing. Exceptions to this guideline include cases where fixing warnings would require you to make many changes outside of the new code, where the warning originates from a 3rd-party library, or where fixing the warning would make the code significantly less readable or performant.

* Avoid using magic numbers. Use constants or enums instead.

* Use **American English**.

## Parenthesis and new lines

Please do not write like this:

``` c
if (blah) {
    foo;
    foo_foo;
} else {
    bar;
    bar_bar; }
```

Write like this instead:

``` c
if (blah)
{
    foo;
    foo_foo;
} 
else
{
    bar;
    bar_bar;
}
```

However, if you have only one line enclosed (i. e. only `foo`), and there are no following `else` or `else if` conditions, you may omit brackets and do it like this:

``` c
if (blah)
    foo;
```

For one-line if statements, if the condition itself is multi-line, brackets should be added anyway to maintain readability:

```
if (item.RoomNumber == 123 || IsHeld(In::Action) &&
    LaraHasEatenSandwich == SANDWICH_BACON)
{
        CurrentLevel = 4;
}
```

Same rule applies to loops.

Avoid multiple nested conditional statements. Where possible, do an early exit from the code using the opposite statement instead of enclosing the statement body in an extra set of brackets. For example, instead of this:

``` c
if (blah)
{
   if (foo)
       bar;
}
```

Write this:

``` c
if (!blah || !foo)
   return;

bar;
```

## Spacing

Don't condense arguments, loop statements, or any other code which requires splitting with commas/semicolons or operators like this:

``` c
void SomeLoop(int blah,int blah2)
{
    for(int i=0;i<4;i++)
    {
        bar =blah;
        foo= blah2;
    }
}
```

Instead, separate everything with spaces **on the both sides** of operator, and **after** the comma or semicolon, like this:

``` c
void SomeLoop(int blah, int blah2)
{
    for (int i = 0; i < 4; i++)
    {
        bar = blah;
        foo = blah2;
    }
}
```

The same rule applies to any code body, not only function declarations or loop statements.

## Tabulation and indents

When you have numerous assignment operations in a row, and/or their names are somewhat of equal length, and their data types are similar, align "left" and "right" parts of the assignment using tabs most of the way and spaces the rest of the way, like this:

``` c
    float foo      = 1.0;
    float bar_bar  = 1.0;
    float foo_o_o  = 1.0;
```

Or:

``` c
    bar      = foo-foo-foo;
    foo_o    = bar;
    bar_r_r  = foo;
```

In case you have pointers defined along with "normal" variables, the asterisk symbol must be placed instead of the last tab's space symbol (this also applies for class declarations and/or implementations), like this:

``` c
    bar     = foo_foo;
   *foo     = &bar_bar;
```

Of course, if one's left part is way longer than another one's left part, there's no need for such alignment, so you can leave it like this:

``` c
    *foo_foo = &bar_foo;
    bar->foo_foo_foo.blah_blah_bar_foo = 1.0;
    foo_bar = 1.0;
```
In a switch case, each case body must be one tab further case's own label. Like this:

``` c
switch (blah)
{
case foo:
    foo - foo - foo;
    foo - foo - foo - foo;
    break;

case bar:
    bar - bar;
    bar - bar - bar;
    break;
}
```

If you need to enclose a particular case into a bracket block, put the body one tab further:

``` c
switch (blah)
{
case foo:
    {
        float bar;
        bar = foo - foo - foo;
        foo - foo - foo - foo = bar;
        break;
    }

case bar:
    bar - bar;
    bar - bar - bar;
    break;
}
```

## Code splitting

If a code block becomes too large, it is recommended to split it into several "sub-blocks" with empty lines, taking each sub-block's meaning into account, like this:

``` c
    foo = foo_foo + foo_foo_foo;
    if (foo)
        foo - foo - foo - foo - foo;
    foo - foo = foo;

    bar = (bar - bar > bar) ? (bar - bar) : 0;
    bar - bar - bar = bar - bar - bar;
```

Conditional statements, if there are many, should be grouped according to their meaning. That is, if you are doing early exit from the function because of different conditions, group them as such:

``` c
if (coll.Floor > CLICK(1) && coll.Ceiling < CLICK(3) && bounds.Y1 > WALL_SIZE)
   return false;
   
if (enemy.health <= 0 || lara.health <= 0 || collidedItems[0] == nullptr)
   return false;
```

However, if  there are few conditional statements, you can group them together. The rule of thumb is if there are more than three conditional statements and two of them are of a different kind, split them. This variant is allowed:

``` c
if (coll.Floor > CLICK(2) && coll.Ceiling < CLICK(4) || lara.health <= 0)
   return false;
```

Sometimes IDA decompiled output generates "pascal-styled" or "basic-styled" variable declarations at the beginning of the function or code block. Like this:

``` c
    int foo, bar = 0;
    float blah;
    ...
    blah = foo;
    foo = bar;
    ...
```

**Please**, get rid of this style everywhere you see it and declare variables in the place narrowest to its actual usage, like this:

``` c
    int foo = foo;
    float blah = bar;
    ...
```

Let's cite [Google Style Guide](https://google-styleguide.googlecode.com/svn/trunk/cppguide.html#Local_Variables) here:

> Place a function's variables in the narrowest scope possible, and initialize variables in the declaration.

## Naming

* Use `auto` where possible and if the original type name is longer than `auto`. E.g. there is no point in changing `bool enabled = true` to `auto enabled = true`. Remember that C++ `auto` **is not similar to C#** `var`, and for referencing existing value with `auto`, you should add `&` to it in the end, e.g. `auto& item = g_Level.Items[itemIndex];` Also, for underlying pointer types, please write `auto*` instead of `auto`, even if it seems redundant.

* Avoid using Hungarian notation for function names. Currently, inconsistent notation indicates an unrefactored coding style, because the original code used inconsistent naming, like `SOUND_Stop()`, `have_i_got_object()` or `gar_SuperpackYXZ()`. These should eventually be eradicated along the course of code restyling. For new function names, `PascalCase` should be used.

* For global struct or class members or methods, `PascalCase` should be used. For local variable names, `camelCase` should be used. For global variables, which are temporary until full refactoring occurs, an exclusive case of Hungarian notation with the `g_` prefix (e.g. `g_Foo`) is permitted.

* Functions designed to take an enum argument should take the enum type itself instead of an int or short. `PassInfo(InfoEnum type)` is more readable and error-proof than `PassInfo(int type)`.

* Functions designed to return boolean value (0 or 1) should define return type as bool. Please don't use `int` return type and don't write `return 0` or `return 1` in bool return functions, use `return false` or `return true`.

* Use the following convention for Pointers and References: `Type* somePointer` / `Type& someReference`. Do not write this: `Type * somePointer` / `Type & someReference` or `Type *somePointer` / `Type &someReference`. Pointers and references are distinct types, hence why the notation for them should have the token on the side of the type.

* Avoid unscoped enum types. For scoped `enum class` types, use `PascalCase` without including enum prefix:

``` c
enum class WeatherType
{
    None,
    Rain,
    Snow,
    Cats,
    Dogs
};
```

 `ENUM_ALL_CAPS` primarily indicates old C-styled Core notation. For C-styled (unscoped) enum values themselves, `ALL_CAPS` may be used for now, along with enum prefix:

``` c
enum LaraWeaponType
{
    WEAPON_NONE,
    WEAPON_PISTOLS,
    WEAPON_REVOLVER
};
```

## Data types

Avoid using `_t` data types, such as `uint8_t`, `int8_t`, `int16_t` etc. Use `unsigned char`, `signed char`, `short` etc. instead. If new variables or fields are introduced, prefer longer and more contemporary data types over deprecated ones. That is, if integer value is used, use `int` instead of `char` or `short`. If potentially fractional value is used (such as coordinates which are eventually multiplied or divided or transformed otherwise), prefer `float` over `int`.

For legacy functions and code paths, preserving original data types may be necessary. Special case are angle values - original games used weird `signed short` angle convention. So extra caution must be taken when writing code which operates on native TR angles, and it should always be kept in variables of `signed short` data type.

Prefer using references over pointers, both in function body and as arguments. When using references or pointers, prefix with `const` for read-only safety if the variable is not being written to.

## Casting

Prefer using C-styled casting instead of C++-styled casting where it is safe. While using it, avoid separating casting operator and variable with space:

`bar = (int)foo;`

For expressions, you can enclose expression into brackets to cast it:

`bar = int(foo + blah * 4);`

Using C++-styled casting is allowed when C-styled casting provides undefined or unacceptable behaviour.

## Includes

For header files from project itself, always use includes with quotes, not with brackets. Also avoid using Windows-specific `\` backslash symbols to specify subpaths, only use normal `/` slashes. Also please include full path to a header file and order includes alphabetically:

```c
#include "Game/effects/lightning.h"
#include "Specific/phd_math.h"
```

Includes with brackets are only allowed when using external libraries:

```c
#include <algorithm>
#include "Game/collision/collide.h"
```

## Namespaces

Don't shorten `std` namespace types and methods by using `using` directive. This is bad: `auto x = vector<int>();`
Leave them as is. This is good: `auto x = std::vector<int>();`

## Comments

Use `//`-styled comments where possible. 
Only use `/* */` style in case you are about to temporarily comment certain block for testing purposes or when writing a comment that will serve as the source for generated documentation.

Use a `NOTE: ` prefix in your comment if you want to highlight something particularly noteworthy:
```c
// NOTE: Will not work for bones at ends of hierarchies.
float GetBoneLength(GAME_OBJECT_ID objectID, int boneIndex)
{
    const auto& object = Objects[objectID];
    
    if (object.nmeshes == boneIndex)
        return 0.0f;
    
    auto nextBoneOffset = GetJointOffset(objectID, boneIndex + 1);
    return nextBoneOffset.Length();
}
```

Use a `FAILSAFE: ` prefix in your comment if you want to highlight a particularly quirky solution without an obvious and clear purpose:
```c
if (portalRoomNumber != NO_VALUE &&
    rayRoomNumber != portalRoomNumber) // FAILSAFE: Prevent infinite loop if room portal leads back to itself.
{
    player.Explode();
}
```

## Branches and pull requests

Make sure that epic branches (tens or hundreds of files changed due to renames, namespace wrappings, etc) **are focused on a single feature or task**. Don't jump in to others epic branches with another round of your epic changes. It masks bugs and makes review process very cumbersome.

Avoid making new branches based on unapproved epic PRs which are in the process of review. It may render your work useless if parent epic branch is unapproved and scrapped.
