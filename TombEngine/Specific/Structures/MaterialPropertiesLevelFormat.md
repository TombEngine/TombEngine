# Material Properties Level Format

This document describes the binary layout consumed by `LoadMaterials()` in `Specific/level.cpp`.

## Primitive Encoding

- All integer and floating-point scalars are little-endian.
- `bool` is stored as `uint8` and read through `ReadBool()`.
- `string` is stored as:
  - `ULEB128 byteCount`
  - `byte[byteCount]` UTF-8 payload with no trailing null terminator
- `Vec2` is two consecutive `float32` values: `x`, `y`.
- `Vec3` is three consecutive `float32` values: `x`, `y`, `z`.
- `Color` is four consecutive `float32` values: `r`, `g`, `b`, `a`.

## Material Block

The material section starts with a counted array:

```text
int32 materialCount
MaterialEntry material[materialCount]
```

`materialCount` is validated with `ReadCount()` on the engine side.

## MaterialEntry Layout

Each material entry is serialized in this exact order:

```text
string  name
int32   shaderType
PropertySlot property[4]
bool    hasNormalMap
bool    hasHeightMap
bool    hasAmbientOcclusionMap
bool    hasRoughnessMap
bool    hasSpecularMap
bool    hasEmissiveMap
```

Notes:

- `name` is the Tomb Editor material name.
- `shaderType` maps to `MaterialShaderType`:
  - `0` = `Default`
  - `1` = `Reflective`
  - `2` = `SkyboxReflective`
- There are always exactly 4 property slots, matching `MaterialParameters[0..3]` in `CBPerDraw`.

## PropertySlot Layout

Each of the 4 slots is serialized as:

```text
string  propertyName
int32   propertyType
<typed default value, only if propertyType != 0>
```

`propertyType` maps to `MaterialPropertyType`:

| Value | Type  | Payload layout |
|------:|-------|----------------|
| `0` | `None`  | no payload |
| `1` | `Bool`  | `uint8` |
| `2` | `Int`   | `int32` |
| `3` | `Float` | `float32` |
| `4` | `Vec2`  | `float32 x`, `float32 y` |
| `5` | `Vec3`  | `float32 x`, `float32 y`, `float32 z` |
| `6` | `Color` | `float32 r`, `float32 g`, `float32 b`, `float32 a` |

Notes:

- `propertyName` may be blank when `propertyType == 0`.
- When `propertyType == 0`, the engine treats the slot as unused and immediately continues to the next slot.
- The serialized value is the property's default value. On level load, the engine initializes both the current and previous runtime values from this default value.

## Runtime Mapping

- Slot `0` maps to `MaterialParameters[0]`.
- Slot `1` maps to `MaterialParameters[1]`.
- Slot `2` maps to `MaterialParameters[2]`.
- Slot `3` maps to `MaterialParameters[3]`.

Packing rules used by the runtime:

- `Bool` -> `Vector4(value ? 1, 0, 0, 0)`
- `Int` -> `Vector4(value, 0, 0, 0)`
- `Float` -> `Vector4(value, 0, 0, 0)`
- `Vec2` -> `Vector4(x, y, 0, 0)`
- `Vec3` -> `Vector4(x, y, z, 0)`
- `Color` -> `Vector4(r, g, b, a)`

## Pseudocode Summary

```text
write int32 materialCount
repeat materialCount times:
    write string materialName
    write int32 shaderType

    repeat 4 times:
        write string propertyName
        write int32 propertyType

        if propertyType != 0:
            write typed default value

    write bool hasNormalMap
    write bool hasHeightMap
    write bool hasAmbientOcclusionMap
    write bool hasRoughnessMap
    write bool hasSpecularMap
    write bool hasEmissiveMap
```