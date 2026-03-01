# Property System — C++ Usage Guide

## Overview

The property system provides a **two-layered** key-value store for attaching custom data to
game entities (Moveables and Statics). Properties set in Lua scripts are accessible from
C++ and vice versa, and can be used as a modern and intuitive replacement for outdated
techniques, such as OCBs and ItemFlags.

| Layer | Scope | Storage | Purpose |
|-------|-------|---------|---------|
| **Layer 1 — Global** | All instances of a given type | `PropertyHandler` (global static maps) | Default values shared by every instance of this type |
| **Layer 2 — Instance** | Unique single instance | `ItemInfo::Properties` / `StaticMesh::Properties` (`PropertyMap` member) | Per-instance overrides of global values |

Resolution order: **Instance → Type → caller-supplied harcoded default**.

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

Additionally, existing value may be set to `nil`. In this case, the property is removed from the map and resolution falls back to the next layer.

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

## Accessing the property via `PropertyHandler`

`PropertyHandler::Get` is the main method to get property value on the C++ side.
It automatically handles property value priority: it checks the instance layer first, then
the global type layer. If neither layer contains the property, method either returns
`nullptr` (raw form) or the caller's default hardcoded value.

All overloads accept the entity reference / pointer directly (`const ItemInfo&/*` or
`const StaticMesh&/*`) and derive the object number / static mesh slot internally.

### Using `Get`

```cpp
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"

using namespace TEN::Scripting::Properties;
using namespace TEN::Utils;

auto& item = g_Level.Items[itemNumber];

// Method 1: by name (simple, hashes internally every call):
int damage1 = PropertyHandler::Get(item, "damage", 10);

// Method 2: by pre-computed hash (faster — use in performance critical code):
int damage2 = PropertyHandler::Get(item, GetHash("damage"), 10);

// Method 3: default value is not provided (std::variant pointer, will be nullptr if not found):
auto damage3 = PropertyHandler::Get(item, "pincer_damage");
```

Methods 1 and 2 are template-based and will automatically resolve property data type from the
caller's expected type (`int` in the example). Method 3 is the raw form that returns a pointer
to the underlying `PropertyValue` variant, which should be resolved manually and can be `nullptr`
if the property is not found in either layer.

#### Statics

```cpp
auto& staticObj = g_Level.Rooms[roomNumber].mesh[meshIndex];
bool fragile = PropertyHandler::Get(staticObj, GetHash("fragile"), false);
```

#### Raw pointer resolution (when you need to branch on presence)

```cpp
const PropertyValue* val = PropertyHandler::Get(item, GetHash("behaviour"));

if (val != nullptr)
{
    if (auto* str = std::get_if<std::string>(val))
        ApplyBehaviour(*str);
}
else
{
    // Property not found in either layer — use hardcoded logic.
}

// Or by name:
const PropertyValue* val2 = PropertyHandler::Get(item, "behavior");
```

---

## Querying instance properties directly via `PropertyMap`

Every `ItemInfo` (moveable) and `StaticMesh` (static mesh) has a public member:

```cpp
TEN::Scripting::Properties::PropertyMap Properties;
```

### Get with `std::optional` (explicit null handling)

```cpp
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"

using namespace TEN::Scripting::Properties;

auto& item = g_Level.Items[itemNumber];

// By name (hashes internally every call):
std::optional<float> hp = item.Properties.Get<float>("health");
if (hp.has_value())
    DoSomething(*hp);

// By pre-computed hash (faster — use in performance critical code):
std::optional<float> hp2 = item.Properties.Get<float>(GetHash("health"));
```

### Get with default value (`GetOr`)

When a sensible fallback exists, `GetOr` avoids the `optional` boilerplate:

```cpp
// By name — returns 100.0f if "health" is missing or is not a float:
float hp1 = item.Properties.GetOr<float>("health", 100.0f);

// By pre-computed hash:
float hp2 = item.Properties.GetOr<float>(GetHash("health"), 100.0f);

// Works with every supported type:
bool aggressive  = item.Properties.GetOr<bool>("aggressive", false);
std::string name = item.Properties.GetOr<std::string>("display_name", "Unknown");
Vec3 offset      = item.Properties.GetOr<Vec3>("spawn_offset", Vec3(0, 0, 0));
```

> **Behaviour:** `GetOr` returns the default value in **two** cases:
> 1. The property key does not exist.
> 2. The property exists but holds a **different type** than `T`.

### Get raw variant

When you need to inspect the type at runtime:
## Accessing the property via `PropertyHandler`

```cpp
const PropertyValue* raw = item.Properties.GetRaw("some_key");
if (raw != nullptr)
{
    if (auto* f = std::get_if<float>(raw))
        // Use float path
    else if (auto* s = std::get_if<std::string>(raw))
        // Use string path
}
```

---

## Querying global properties directly via `PropertyMap`

For cases where you only want to access the global property, ignoring any instance override:

```cpp
// Returns nullptr if no type properties registered for this object ID.
const PropertyMap* typeProps = PropertyHandler::FindMoveableProperties(objectID);
if (typeProps)
{
    float baseDamage = typeProps->GetOr<float>("damage", 10.0f);
}

// GetMoveableProperties internally is an `unordered_map` and automatically creates the map
// if it doesn't exist (use for writes):
PropertyMap& typeProps = PropertyHandler::GetMoveableProperties(objectID);
typeProps.Set("damage", PropertyValue(25.0f));
```

---

## Performance Tips

| Pattern | Cost | When to use |
|---------|------|-------------|
| `Get<T>("name")` / `GetOr<T>("name", def)` | O(1) amortized, but hashes the string each call | One-off reads, init code |
| `Get<T>(hash)` / `GetOr<T>(hash, def)`     | O(1) with no hashing | Hot paths — AI ticks, control loops |
| `PropertyHandler::Get<T>(entity, hash, …)` | Two O(1) lookups worst-case | Hot paths needing two-layer resolution |

**Best practice:** Use `GetHash` directly with a string literal (e.g. `GetHash("damage")` or declare hashes as `constexpr`
constants using `GetHash`. In both cases the hash is computed at **compile time** — zero runtime cost:

```cpp
// Use directly:
float hp = PropertyHandler::Get(item, GetHash("health"), 100.0f);

// Explicitly define hash:
constexpr int kDamage = GetHash("damage");
float damage = item.Properties.GetOr<float>(kDamage, 10.0f);
```

> `GetHash(const std::string&)` is the runtime overload for dynamic strings and will calculate hash every call. It should
be avoided in frequently called code paths, except cases when accessed via Lua API methods.
> Both overloads will produce identical FNV-1a hashes.

---

## Lua Binding Helpers (`PropertyLuaConverters.h`)

These inline functions live in `Scripting/Internal/TEN/Properties/PropertyLuaConverters.h`
and are used by the Lua-facing property implementations in `MoveableObject.cpp`,
`StaticObject.cpp`, and `ObjectsHandler.h`:

| Function | Purpose |
|----------|---------|
| `ValidatePropertyName(name)` | Returns `false` and logs a script error if name is empty. Guards all Lua entry points. |
| `PropertyValueFromLua(obj)` | Converts a `sol::object` to a `PropertyValue`. Returns `std::nullopt` on unsupported types. |
| `PropertyValueToLua(state, value)` | Converts a `PropertyValue` variant to a `sol::object` via `std::visit`. |

---

## Savegame Serialization (`PropertySavegame.h`)

Both layers are serialized/deserialized automatically through FlatBuffers. The helpers
live in `Scripting/Internal/TEN/Properties/PropertySavegame.h`:

| Function | Direction | Purpose |
|----------|-----------|---------|
| `BuildPropertyValue(fbb, value, outType)` | Save | Serialize a single `PropertyValue` to FlatBuffers. |
| `BuildPropertyMap(fbb, propMap)` | Save | Serialize a `PropertyMap` to a `PropertyMapData` offset. |
| `BuildTypeProperties(fbb, typeProps)` | Save | Serialize a full `unordered_map<int, PropertyMap>` for type properties. |
| `ParsePropertyEntry(entry)` | Load | Deserialize a single `PropertyEntry` to `std::optional<PropertyValue>`. |
| `ParsePropertyMap(data, outMap)` | Load | Deserialize `PropertyMapData` into a `PropertyMap`. |
| `ParseTypeProperties(vec, outMap)` | Load | Deserialize type property vectors back into global maps. |

In `savegame.cpp`:

```cpp
// Saving:
auto moveableTypePropsOffset = BuildTypeProperties(fbb, PropertyHandler::GetAllMoveableProperties());
auto staticTypePropsOffset   = BuildTypeProperties(fbb, PropertyHandler::GetAllStaticProperties());

// Loading:
ParseTypeProperties(s->moveable_type_properties(), PropertyHandler::GetMutableMoveableProperties());
ParseTypeProperties(s->static_type_properties(),   PropertyHandler::GetMutableStaticProperties());
```

Instance properties (`ItemInfo::Properties` / `StaticMesh::Properties`) are serialized
per-entity alongside other item/static data using `BuildPropertyMap` / `ParsePropertyMap`.

---

## Lifecycle

- **Level load:** Lua scripts populate type properties via `Objects.SetMoveableProperty()` /
  `Objects.SetStaticProperty()` and instance properties via `myObj:SetProperty()`.
- **Savegame save:** Both layers serialized through FlatBuffers (see above).
- **Savegame load:** Both layers deserialized back from FlatBuffers.
- **Level unload:** `PropertyHandler::Clear()` is called from `ObjectsHandler::FreeEntities()`,
  and instance `PropertyMap` members are destroyed with their owning `ItemInfo` / `StaticMesh`.

---

## File Map

| File | Purpose |
|------|---------|
| `Properties/PropertyValue.h` | `PropertyValue` variant type alias |
| `Properties/PropertyMap.h` | Per-entity hash-based property container (header + templates) |
| `Properties/PropertyMap.cpp` | Non-template `PropertyMap` method implementations |
| `Properties/PropertyHandler.h` | Type-level registry + two-layer `Get` overloads (header + templates) |
| `Properties/PropertyHandler.cpp` | Non-template `PropertyHandler` method implementations |
| `Properties/PropertyLuaConverters.h` | `ValidatePropertyName`, `PropertyValueFromLua`, `PropertyValueToLua` |
| `Properties/PropertySavegame.h` | FlatBuffers Build/Parse helpers for both layers |

All paths relative to `Scripting/Internal/TEN/`.

---

## Quick Reference

```
PropertyMap
  ::Get<T>(name)             → std::optional<T>
  ::Get<T>(hash)             → std::optional<T>
  ::GetOr<T>(name, def)      → T
  ::GetOr<T>(hash, def)      → T
  ::GetRaw(name)             → const PropertyValue*
  ::GetRaw(hash)             → const PropertyValue*
  ::Set(name, value)         → void
  ::Has(name)                → bool
  ::Has(hash)                → bool
  ::Remove(name)             → bool
  ::Remove(hash)             → bool
  ::Clear()                  → void
  ::IsEmpty()                → bool
  ::GetCount()               → size_t
  ::GetNames()               → vector<string>
  ::GetName(hash)            → const string*
  ::begin() / end()          → iterator (over hash→value pairs)

PropertyHandler
  ::Get(item, name)                     → const PropertyValue*
  ::Get(item, hash)                     → const PropertyValue*
  ::Get(staticMesh, name)               → const PropertyValue*
  ::Get(staticMesh, hash)               → const PropertyValue*
  ::Get<T>(item, name, def = T{})       → T
  ::Get<T>(item, hash, def = T{})       → T
  ::Get<T>(staticMesh, name, def = T{}) → T
  ::Get<T>(staticMesh, hash, def = T{}) → T
  ::GetMoveableProperties(objectID)     → PropertyMap&
  ::FindMoveableProperties(objectID)    → const PropertyMap*
  ::GetStaticProperties(slotID)         → PropertyMap&
  ::FindStaticProperties(slotID)        → const PropertyMap*
  ::Clear()                             → void
  ::GetAllMoveableProperties()          → const unordered_map&
  ::GetAllStaticProperties()            → const unordered_map&
  ::GetMutableMoveableProperties()      → unordered_map&
  ::GetMutableStaticProperties()        → unordered_map&

GetHash(const string& name)  → int            (FNV-1a, runtime, from Specific/trutils.h)
GetHash(const char* name)    → constexpr int   (FNV-1a, compile-time, from Specific/trutils.h)
ValidatePropertyName(name)   → bool            (from PropertyLuaConverters.h)
```
