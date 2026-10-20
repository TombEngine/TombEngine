# Transparent Sorting - Design Notes

## Overview

Faces with sorted blend modes (every mode except opaque, alpha test, additive, distortion and fast
alpha blend, see `IsSortedBlendMode`) and faces of objects faded below `ALPHA_BLEND_THRESHOLD`
cannot rely on the depth buffer, so the renderer draws them back to front at the end of the scene pass, after
the opaque, alpha edge, water and additive draws. The frontend collects and sorts them on the CPU; the scene
renderer turns the sorted order into draw runs. This document describes how that pass collects, groups, orders and
draws those faces, and why it is built this way.

![Sorted pass pipeline](TransparentSorting/pipeline.svg)

| Stage | Function | File |
|-------|----------|------|
| Collect | `CollectSortedBucket`, `CollectSortedSprite`, called from `Renderer::CollectSortedFaces` and the `CollectSorted*Faces` functions | `RendererSorted.cpp`, call sites in `RendererDraw.cpp`, `RendererLara.cpp`, `RendererDrawEffect.cpp`, `RendererSprites.cpp` |
| Group, cluster, sort | `SortTransparentFaces` | `RendererSorted.cpp` |
| Runs | `SceneRenderer::CollectSortedFaces`, `GetSortedObjectInstance`, `CanMergeSortedRoomRuns` | `Backend/SceneRenderer.cpp` |
| Draw | `SceneRenderer::DrawSortedFaces` | `Backend/SceneRenderer.cpp` |
| Data | `RendererSortableObject`, `RendererSortablePolygon`, `RendererSortKey` | `Structures/RendererSortableObject.h` |
| Containers | `TransparentObjectsToDraw`, `TransparentPolygonsToDraw`, `TransparentSortKeys` | `RenderView.h` |

Frame flow:

```cpp
// Renderer::RenderScene (frontend).
CollectSortedFaces(view);   // Collect.
SortTransparentFaces(view); // Group, cluster, sort.

// SceneRenderer::RenderFrame (backend).
CollectSortedFaces(view, interpolationFactor); // Runs, sorted index buffer and instances.
DrawSortedFaces(commandBuffer, renderPass);    // Draw, at the end of the scene pass (and once per mirror).
```

---

## Data Layout

The pass separates what is shared by a whole mesh bucket from what changes per polygon.

![Memory layout before and after](TransparentSorting/data-layout.svg)

| Struct | Size | Count | Contents |
|--------|------|-------|----------|
| `RendererSortableObject` | 136 B | one per object bucket (or per sprite) | Draw state: type, world matrix, blend and light mode, room, bucket, owner pointer. Also the group index, depth range and screen rectangle used by `SortTransparentFaces`. |
| `RendererSortablePolygon` | 16 B | one per polygon | Polygon pointer, distance to camera, index of its object. Sprites use a single entry with no polygon. |
| `RendererSortKey` | 16 B | one per polygon | 64-bit sort key and index of the polygon entry. |

Only the sort key array is reordered. Objects and polygon entries stay where collection wrote them,
and `DrawSortedFaces` reaches them through `key.Index` and `polygon.ObjectIndex`.

### Collection

Every collection site fills one `RendererSortableObject` per bucket and calls:

```cpp
void CollectSortedBucket(RenderView& view, const RendererSortableObject& object, const Matrix* world, const Vector3& cameraPosition);
void CollectSortedSprite(RenderView& view, const RendererSortableObject& object, int distance);
```

`CollectSortedBucket` transforms each polygon centre by `world` (or uses it as-is for rooms, which
pass `nullptr`), stores the distance to `cameraPosition`, and grows the object's depth range and
normalized screen rectangle by projecting the centre with `view.Camera.ViewProjection`. Points
behind the camera count as covering the whole screen. The transforms are written out by hand so the
per-polygon cost stays low in debug builds too.

Per-instance work happens once per mesh, never per polygon: the bone and world matrix product for
item meshes and the `Matrix::Lerp` of swarm instances are computed before the bucket loop.

All object types, sprites included, measure distance from `view.Camera.WorldPosition`.

---

## Grouping

A group is the unit that can draw as one batch. Group identity is a `GroupKey` built by
`GetSortedGroupKey`:

| Object type | Key |
|-------------|-----|
| Room | bucket |
| Moveable, hair | item + bucket + object type |
| Static | static + bucket |
| Effect | effect + bucket |
| Swarm (`MoveableAsStatic`) | bucket + rounded world translation (3 x 21 bits); swarms have no stable instance pointer |
| Sprite | texture + 4-block depth band + blend mode, sprite type, render type, soft particle flag |

All polygons of one object are contiguous, so the key and its hash map lookup are resolved only when
`ObjectIndex` changes. The other polygons only add their distance to the group.

![Grouping once per object](TransparentSorting/grouping.svg)

---

## Clusters

Group order alone is wrong when two transparent objects overlap: one draws entirely before the
other, regardless of which polygons are actually in front. Polygon order across objects only changes
the image when the objects overlap **both in depth and on screen**, so only those are interleaved.

![Clusters](TransparentSorting/clusters.svg)

1. `BuildSortedClusters` joins groups whose depth ranges and screen rectangles both overlap, using a
   union-find. Candidates are swept by near depth, so a group is only tested against groups that
   start before it ends. Rooms and sprites never join a cluster.
2. `RankSortedClusters` orders clusters with the same rules previously used for single groups:
   - rooms first, back to front by portal traversal order;
   - objects back to front by mean polygon distance, quantized to `GROUP_DEPTH_STEP` (128 units);
   - equal depth falls back to a fixed blend mode priority (issue #1793), then to the cluster id.

   A group outside any cluster is a cluster of one and keeps its previous position.
3. Inside a cluster, polygons are in exact far-to-near order, so batches break wherever the group
   changes.

`SplitBusyClusters` counts group switches inside each cluster after sorting. A cluster with more than
`MAX_CLUSTER_GROUP_SWITCHES` (64) switches is dissolved, its groups return to group order and the
keys are sorted again. This keeps a pathological overlap from turning into one draw call per
polygon; in the common case the sort runs once.

> **Limitations:** screen rectangles come from polygon centres, so a large low-polygon object (a
> two-triangle surface) has an almost point-sized rectangle and may miss an overlap. It then keeps
> group order, which is the previous behaviour. Centre-based ordering is still a painter's algorithm:
> intersecting polygons can sort wrong.

---

## Sort Key

![Sort key layout](TransparentSorting/sort-key.svg)

```cpp
key = ((unsigned long long)clusterRank << 32) | (unsigned int)(INT_MAX - distance);
```

Ascending keys give clusters in draw order and polygons far to near inside each cluster.
`RadixSortKeys` is a stable LSD radix sort with 8-bit digits. It builds all eight digit histograms
in one read pass and skips every digit that is equal for all keys, so a typical scene needs three
passes. The scratch buffer `_sortKeysScratch` is kept across frames.

---

## Drawing

`SceneRenderer::CollectSortedFaces` walks the sorted keys and builds one `SortedDraw` run for each sequence of
consecutive polygons that share a `GroupIndex`. The indices of the run's polygons are copied from the CPU copy of the
level index buffer into `_sortedIndexData`, which is uploaded once per frame into the sorted index buffer, so a run
costs one indexed draw. Sprite runs add their sprites to the frame sprite buffer and are drawn instanced like the
other sprites.

The state of an object comes from the frame instance buffer: `GetSortedObjectInstance` adds one instance (world
matrix, color, ambient light, lights and bones) the first time an object appears and reuses it when interleaved
clusters split the object into several runs. Hair faces reuse the hair instances built with the player. Room runs use
instance 0 and the room lights, and consecutive room runs with the same draw state and portal rectangle are merged
(`CanMergeSortedRoomRuns`).

`DrawSortedFaces` records the runs in order. It changes pipeline, vertex streams and bucket bindings only when they
differ from the previous run, and clips room runs to their portal rectangle with a scissor. Mirrors record their own
list of runs, collected by `CollectMirroredSorted`, with the mirrored object shader.

---

## Debug Overlay

The Statistics tab of the debug pages (F10, F11) shows the sorted faces in two places:

| Line | Meaning |
|------|---------|
| `Sorted faces` (frontend) | Number of sorted polygon entries (sprites count as one). |
| `Interleaved sorted groups` (frontend) | Groups that ended up in a cluster of two or more. |
| `Sorted collect`, `Sorted sort` (frontend) | CPU time of the collection and of `SortTransparentFaces`, in ms. |
| `Sorted draws`, `Sorted indices` (scene renderer) | Draw runs and indices uploaded for the frame. |
| `Collect and sort`, `Record` (scene renderer) | CPU time of building the runs and of recording them, in ms. |

---

## Background

The current design replaced a version that turned every transparent polygon into a full 136-byte
object and sorted those objects with `std::sort`. A scene with seven alpha-blended vases (about
12,600 triangles) ran a debug frame at 16 ms instead of 4 ms. A RenderDoc replay of that scene put
the whole GPU frame at 7.2 ms, with the seven sorted draws at 0.42 ms, so the cost was on the CPU.

![Frame budget before the rework](TransparentSorting/frame-budget.svg)

| Step, 13,000 polygons | Debug | Release |
|-----------------------|------:|--------:|
| `std::sort` of 136-byte objects (old) | 5.3 ms | 1.1 ms |
| Radix sort of 16-byte keys | 0.5 ms | 0.07 ms |
| Collect with per-polygon objects (old) | 9 ms | - |
| Sort with per-polygon grouping (old) | 2 ms | - |

Sort timings come from a standalone benchmark; collect and sort timings come from the debug overlay.

---

## Possible Improvements

- **32-bit sort entries.** A radix sort on the 32-bit inverted distance followed by one stable
  counting-sort pass by cluster rank yields the same order with 8-byte entries. Expected gain is
  small (around 0.2 ms in debug for 13,000 polygons), since the pass count stays the same.
- **Sprites in clusters.** Interleaving sprites with meshes would fix particles seen through glass,
  but mixes the sprite and mesh batch paths. Not enabled yet.
