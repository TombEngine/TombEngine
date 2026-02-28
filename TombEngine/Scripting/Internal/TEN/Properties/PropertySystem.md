# Property System — C++ Usage Guide

## Overview

The property system provides a **two-layered** key-value store for attaching custom data to
game entities (Moveables and Statics). Properties set in Lua scripts are accessible from
C++ for gameplay logic, AI behavior, puzzle mechanics, etc.

| Layer | Scope | Storage | Purpose |
|-------|-------|---------|---------|
| **Layer 1 — Type** | All instances of a type | `PropertyHandler` (global static maps) | Default values shared by every instance of an object type |
| **Layer 2 — Instance** | Single entity | `ItemInfo::Properties` / `StaticMesh::Properties` (`PropertyMap` member) | Per-instance overrides |

Resolution order: **Instance → Type → caller-supplied default**.

---

## Supported Value Types

`PropertyValue` is a `std::variant` over the following types:

| C++ Type | Lua Type | Notes |
|----------|----------|-------|
| `bool` | `boolean` | |
| `float` | `number` | |
| `std::string` | `string` | |
| `Vec2` | `Vec2` | 2D vector |
| `Vec3` | `Vec3` | 3D vector |
| `ScriptColor` | `Color` | RGBA color |
| `Rotation` | `Rotation` | Euler rotation |
| `Time` | `Time` | Time value |

---

## Headers

```cpp
// The variant type alias.
#include "Scripting/Internal/TEN/Properties/PropertyValue.h"

// The per-entity property container.
#include "Scripting/Internal/TEN/Properties/PropertyMap.h"

// The global type-level registry + two-layer resolvers.
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
```

`PropertyMap.h` already includes `PropertyValue.h`, and `PropertyHandler.h` already
includes `PropertyMap.h`, so in practice you only need to include the highest-level header
you use.

---

## Accessing Instance Properties (`PropertyMap`)

Every `ItemInfo` (moveable) and `StaticMesh` (static mesh) has a public member:

```cpp
TEN::Scripting::Properties::PropertyMap Properties;
```

### Get with `std::optional` (explicit null handling)

```cpp
using namespace TEN::Scripting::Properties;

auto& item = g_Level.Items[itemNumber];

// By name (hashes internally every call):
std::optional<float> hp = item.Properties.Get<float>("health");
if (hp.has_value())
    DoSomething(*hp);

// By pre-computed hash (fast — use in hot code):
static const int kHealthHash = GetHash("health");
std::optional<float> hp2 = item.Properties.Get<float>(kHealthHash);
```

### Get with default value (`GetOr`)

When a sensible fallback exists, `GetOr` avoids the `optional` boilerplate:

```cpp
// By name — returns 100.0f if "health" is missing or is not a float:
float hp = item.Properties.GetOr<float>("health", 100.0f);

// By pre-computed hash:
static const int kHealthHash = GetHash("health");
float hp2 = item.Properties.GetOr<float>(kHealthHash, 100.0f);

// Works with every supported type:
bool aggressive  = item.Properties.GetOr<bool>("aggressive", false);
std::string name = item.Properties.GetOr<std::string>("display_name", "Unknown");
Vec3 offset      = item.Properties.GetOr<Vec3>("spawn_offset", Vec3(0, 0, 0));
```

> **Behavior:** `GetOr` returns the default value in **two** cases:
> 1. The property key does not exist.
> 2. The property exists but holds a **different type** than `T`.

### Get raw variant

When you need to inspect the type at runtime:

```cpp
const PropertyValue* raw = item.Properties.GetRaw("some_key");
if (raw != nullptr)
{
    if (auto* f = std::get_if<float>(raw))
        // use *f
    else if (auto* s = std::get_if<std::string>(raw))
        // use *s
}
```

### Setting, removing, querying

```cpp
auto& props = item.Properties;

props.Set("health", PropertyValue(100.0f));     // Create or overwrite.
props.Has("health");                             // true
props.Remove("health");                          // Returns true if removed.
props.Clear();                                   // Remove all.
props.IsEmpty();                                 // true after Clear().
props.GetCount();                                // Number of entries.
```

### Iteration

```cpp
for (const auto& [hash, value] : item.Properties)
{
    const std::string* name = item.Properties.GetName(hash);
    // 'value' is a PropertyValue variant.
}

// Or get all names:
std::vector<std::string> names = item.Properties.GetNames();
```

---

## Two-Layer Resolution (`PropertyHandler`)

`PropertyHandler::Get` checks the instance layer first, then the type layer. If neither
layer contains the property, resolution returns `nullptr` (raw form) or the caller's
default value (typed form).

All overloads accept the entity reference directly (`const ItemInfo&` or
`const StaticMesh&`) and derive the object number / slot internally.

### Using typed `Get` (recommended)

```cpp
using namespace TEN::Scripting::Properties;

auto& item = g_Level.Items[itemNumber];

// By name (simple, hashes internally):
float damage = PropertyHandler::Get<float>(item, "damage", 10.0f);

// By pre-computed hash (fast — use in hot code):
static const int kDamageHash   = GetHash("damage");
static const int kSpeedHash    = GetHash("speed");
static const int kFriendlyHash = GetHash("friendly");

// Resolves: instance property → type property → default.
float damage2  = PropertyHandler::Get<float>(item, kDamageHash, 10.0f);
float speed    = PropertyHandler::Get<float>(item, kSpeedHash, 1.0f);
bool  friendly = PropertyHandler::Get<bool>(item, kFriendlyHash, false);

// Default value is optional (zero-initialized if omitted):
float damage3 = PropertyHandler::Get<float>(item, kDamageHash); // 0.0f if not found
```

### Statics

```cpp
auto& staticObj = g_Level.Rooms[roomNumber].mesh[meshIndex];

static const int kFragileHash = GetHash("fragile");

bool fragile = PropertyHandler::Get<bool>(staticObj, kFragileHash, false);
```

### Raw pointer resolution (when you need to branch on presence)

```cpp
static const int kBehaviorHash = GetHash("behavior");

const PropertyValue* val = PropertyHandler::Get(item, kBehaviorHash);

if (val != nullptr)
{
    if (auto* str = std::get_if<std::string>(val))
        ApplyBehavior(*str);
}
else
{
    // Property not found in either layer — use hardcoded logic.
}

// Or by name:
const PropertyValue* val2 = PropertyHandler::Get(item, "behavior");
```

---

## Querying Type Properties Directly

For cases where you only want the type-level default (ignoring any instance override):

```cpp
// Returns nullptr if no type properties registered for this object ID.
const PropertyMap* typeProps = PropertyHandler::FindMoveableProperties(objectID);
if (typeProps)
{
    float baseDamage = typeProps->GetOr<float>("damage", 10.0f);
}

// GetMoveableProperties creates the map if it doesn't exist (use for writes):
PropertyMap& typeProps = PropertyHandler::GetMoveableProperties(objectID);
typeProps.Set("damage", PropertyValue(25.0f));
```

---

## Performance Tips

| Pattern | Cost | When to use |
|---------|------|-------------|
| `Get<T>("name")` / `GetOr<T>("name", def)` | O(1) amortized, but hashes the string each call | One-off reads, init code |
| `Get<T>(hash)` / `GetOr<T>(hash, def)` | O(1) with no hashing | Hot paths — AI ticks, control loops |
| `PropertyHandler::Get<T>(entity, hash, …)` | Two O(1) lookups worst-case | Hot paths needing two-layer resolution |

**Best practice:** Declare hashes as `static const` locals or class constants using `GetHash` from `Specific/trutils.h`:

```cpp
static const int kHealth = GetHash("health");
static const int kDamage = GetHash("damage");
static const int kSpeed  = GetHash("speed");
```

---

## Lifecycle

- **Level load:** Lua scripts populate type properties via `Moveable.SetTypeProperty()`
  and instance properties via `myObj:SetProperty()`.
- **Savegame:** Both layers are serialized/deserialized automatically through FlatBuffers.
- **Level unload:** `PropertyHandler::Clear()` is called from `ObjectsHandler::FreeEntities()`,
  and instance `PropertyMap` members are destroyed with their owning `ItemInfo`/`StaticMesh`.

---

## Quick Reference

```
PropertyMap::Get<T>(name)            → std::optional<T>
PropertyMap::Get<T>(hash)            → std::optional<T>
PropertyMap::GetOr<T>(name, def)     → T
PropertyMap::GetOr<T>(hash, def)     → T
PropertyMap::GetRaw(name)            → const PropertyValue*
PropertyMap::GetRaw(hash)            → const PropertyValue*
PropertyMap::Set(name, value)        → void
PropertyMap::Has(name/hash)          → bool
PropertyMap::Remove(name/hash)       → bool
PropertyMap::Clear()                 → void

PropertyHandler::Get(item, name)                    → const PropertyValue*
PropertyHandler::Get(item, hash)                    → const PropertyValue*
PropertyHandler::Get(staticMesh, name)              → const PropertyValue*
PropertyHandler::Get(staticMesh, hash)              → const PropertyValue*
PropertyHandler::Get<T>(item, name, def = T{})      → T
PropertyHandler::Get<T>(item, hash, def = T{})      → T
PropertyHandler::Get<T>(staticMesh, name, def = T{})→ T
PropertyHandler::Get<T>(staticMesh, hash, def = T{})→ T
PropertyHandler::FindMoveableProperties(objectID)   → const PropertyMap*
PropertyHandler::FindStaticProperties(slotID)       → const PropertyMap*
PropertyHandler::GetMoveableProperties(objectID)    → PropertyMap&
PropertyHandler::GetStaticProperties(slotID)        → PropertyMap&
PropertyHandler::Clear()                            → void

GetHash(name)                        → int   (from Specific/trutils.h)
```
