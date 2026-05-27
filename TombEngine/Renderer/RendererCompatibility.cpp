#include "framework.h"
#include "Renderer/Renderer.h"

#include <execution>
#include <stack>
#include <tuple>

#include "Game/collision/floordata.h"
#include "Game/control/control.h"
#include "Game/effects/Decal.h"
#include "Game/effects/Hair.h"
#include "Game/Lara/lara_struct.h"
#include "Game/room.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Objects/Generic/Object/objects.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Specific/level.h"

using namespace TEN::Effects::Decal;
using namespace TEN::Effects::Hair;
using namespace TEN::Renderer::Graphics;

// =========================================================================================
// Snow overlay generation (Phase 2).
//
// At level load, floor polygons sitting on sectors flagged with MaterialType::Snow get a
// subdivided overlay mesh pinned MaxDepth units above the floor surface. The overlay
// inherits texture index, UVs, color and tangent space from the underlying floor polygon
// via bilinear (quad) or barycentric (triangle) interpolation. The geometry lives in the
// same global vertex/index buffer as regular room geometry; it is marked with
// RendererBucket::IsSnowOverlay so the main room render pass can skip it. The dedicated
// snow shader (added in a later phase) reads a deformation heightmap and pushes each
// snow vertex downward where Lara has walked.
// =========================================================================================

namespace TEN::Renderer
{
	// ----- Snow overlay helpers (file-local, kept inside namespace to access types). -----

	// Returns the room that owns the snow surface above this polygon, or nullptr if the
	// polygon is not a valid snow floor. The owner room is the room volume sitting above
	// the polygon's surface (which may or may not be the room that geometrically owns
	// the polygon vertices, e.g. when the polygon is the top of a wall in the lower
	// room of a vertical portal, or when an upper room has a portal-floor at the same
	// height as a partially-raised ramp). Routing snow to the correct owner room ensures
	// the snow inherits that room's ambient color and dynamic lights instead of the
	// (often unrelated) lighting state of the room that physically owns the geometry.
	static RoomData* FindSnowOwnerRoom(const POLYGON& poly, const RoomData& sourceRoom)
	{
		if (poly.indices.size() < 3)
			return nullptr;

		// Reject walls and ceiling polygons in one check. In TR's Y-down coordinate
		// system, floor surfaces have normals pointing toward +Y (downward direction),
		// so poly.normal.y > 0 for floors. The threshold 0.08 rejects near-vertical
		// walls (|normal.y| < 0.08), and the sign requirement (normal.y >= 0.08) rejects
		// ceiling polygons whose normals point toward -Y (normal.y < 0).
		if (poly.normal.y < 0.08f)
			return nullptr;

		auto centroid = Vector3::Zero;
		for (int idx : poly.indices)
			centroid += sourceRoom.positions[idx];
		centroid /= (float)poly.indices.size();

		int absX = (int)centroid.x + sourceRoom.Position.x;
		int absY = (int)centroid.y + sourceRoom.Position.y;
		int absZ = (int)centroid.z + sourceRoom.Position.z;

		int sgx = (absX - sourceRoom.Position.x) / BLOCK(1);
		int sgz = (absZ - sourceRoom.Position.z) / BLOCK(1);

		// Source-sector gate. The owner-routing below can fall through to a neighbour
		// room when the source room's volume check fails (e.g. for thin rooms, portal
		// sectors, or tilted floors where probe.y escapes ceiling/floor bounds). If
		// that neighbour happens to be a snow room, water/stone polygons that legally
		// live in a non-snow source sector would falsely inherit snow. Gate the entire
		// lookup on the polygon's OWN sector: only proceed if that sector either has
		// snow material itself, or is a wall sub-half (the legitimate wall-top case
		// where the polygon's footprint has no walkable floor in its source room and
		// the snow tag lives in a neighbour room's sector).
		if (sgx >= 0 && sgx < sourceRoom.XSize && sgz >= 0 && sgz < sourceRoom.ZSize)
		{
			const auto& srcSector = sourceRoom.Sectors[sgx * sourceRoom.ZSize + sgz];
			bool srcIsWall = srcSector.IsWall(absX, absZ);
			auto srcMaterial = srcSector.GetSurfaceMaterial(absX, absZ, true);
			if (!srcIsWall && srcMaterial != MaterialType::Snow)
				return nullptr;
		}

		// Owner = whichever room contains the volume immediately above the polygon
		// surface. FindRoomNumber() can't be used here because it tests only the room's
		// full AABB, which routinely overlaps neighbours (e.g. a tall lower room whose
		// ceiling extends above the wall top of a vertical portal still "contains" the
		// probe point by AABB) and returns the first matching room by index. Instead,
		// query the sector at the probe XZ and check the probe Y against the sector's
		// own floor and ceiling heights -- which respect walls and portal transitions.
		auto isInsideRoomVolume = [](const Vector3i& probe, int roomIdx)
		{
			const auto& r = g_Level.Rooms[roomIdx];
			int gx = (probe.x - r.Position.x) / BLOCK(1);
			int gz = (probe.z - r.Position.z) / BLOCK(1);
			if (gx < 0 || gx >= r.XSize || gz < 0 || gz >= r.ZSize)
				return false;

			const auto& s = r.Sectors[gx * r.ZSize + gz];
			if (s.IsWall(probe.x, probe.z))
				return false;

			int fH = s.GetSurfaceHeight(probe.x, probe.z, true);
			int cH = s.GetSurfaceHeight(probe.x, probe.z, false);
			if (fH == NO_HEIGHT || cH == NO_HEIGHT)
				return false;

			// Y-down: ceiling has smaller Y, floor has larger Y. Inside the volume means
			// strictly between them.
			return probe.y > cH && probe.y < fH;
		};

		Vector3i probe(absX, absY - 16, absZ);
		int sourceRoomIdx = (int)(&sourceRoom - g_Level.Rooms.data());

		int ownerRoomNumber = NO_VALUE;

		// Prefer the source room: if the polygon really is the source room's own floor,
		// the probe just above it is inside the source room's sector volume. This is the
		// common case (normal floor poly) and also handles upper-room portal-height polys
		// (Issue 2): the source room is the upper room, and the probe at portal_Y - 16
		// sits between the upper ceiling and the (transitive) floor.
		if (isInsideRoomVolume(probe, sourceRoomIdx))
		{
			ownerRoomNumber = sourceRoomIdx;
		}
		else
		{
			// Wall-top case (Issue 1): polygon is the top of a wall in the source room,
			// the probe above it lands in a neighbouring upper room's sector.
			for (int neighborIdx : sourceRoom.NeighborRoomNumbers)
			{
				if (neighborIdx != sourceRoomIdx && isInsideRoomVolume(probe, neighborIdx))
				{
					ownerRoomNumber = neighborIdx;
					break;
				}
			}
		}

		if (ownerRoomNumber == NO_VALUE)
			return nullptr;

		auto& ownerRoom = g_Level.Rooms[ownerRoomNumber];

		int ogx = (absX - ownerRoom.Position.x) / BLOCK(1);
		int ogz = (absZ - ownerRoom.Position.z) / BLOCK(1);
		if (ogx < 0 || ogx >= ownerRoom.XSize || ogz < 0 || ogz >= ownerRoom.ZSize)
			return nullptr;

		const auto& ownerSector = ownerRoom.Sectors[ogx * ownerRoom.ZSize + ogz];

		// Use the polygon centroid directly for the material check. The centroid of a
		// triangular polygon is always within its own sub-triangle, so GetSurfaceMaterial
		// reliably returns the correct per-half material for diagonally split sectors.
		// For quad polygons on non-split sectors both halves share the same material, so
		// the boundary-case determination does not matter.
		//if (ownerSector.GetSurfaceMaterial(absX, absZ, true) != MaterialType::Snow)
			//return nullptr;
	
		//if (ownerSector.IsSurfaceSplit(true))
		//{
		/*
			if (ownerSector.FloorSurface.Triangles[0].Material != MaterialType::Snow ||
				ownerSector.FloorSurface.Triangles[1].Material != MaterialType::Snow)
				return nullptr;
		}
		else*/
			if (ownerSector.GetSurfaceMaterial(absX, absZ, true) != MaterialType::Snow)
				return nullptr;
		//}

		// Reject fall-through floor triangles: if the owner sector's floor sub-triangle
		// at this XZ is a vertical portal to the room below, the polygon represents an
		// opening (not a walkable surface). No snow overlay should be generated there.
		const auto& ownerFloorTri = ownerSector.GetSurfaceTriangle(absX, absZ, true);
		if (ownerFloorTri.PortalRoomNumber != NO_VALUE)
			return nullptr;

		return &ownerRoom;
	}

	static bool IsSnowFloorPolygon(const POLYGON& poly, const RoomData& room)
	{
		return FindSnowOwnerRoom(poly, room) != nullptr;
	}

	static void GetSnowOverlayCounts(const POLYGON& poly, int subdivisions, int& outVerts, int& outIndices)
	{
		int n2 = subdivisions * subdivisions;
		if (poly.shape == 0)
		{
			outVerts   = n2 * 4;
			outIndices = n2 * 6;
		}
		else
		{
			outVerts   = n2 * 3;
			outIndices = n2 * 3;
		}
	}

	// Probes the floor height of the neighbor sector across a polygon edge. Returns the
	// absolute world-space Y of the neighbor's floor at the requested XZ, or NO_HEIGHT
	// if there is no valid neighbor sector inside this room.
	// sourceAbsY is the absolute world-space Y of the source polygon vertex; it is used
	// to locate the room above when the neighbor sector turns out to be a wall sector.
	static int GetSnowNeighborFloorAt(
		const RoomData& room,
		float localX, float localZ,
		float edgeMidX, float edgeMidZ,
		float outwardX, float outwardZ,
		int sourceAbsY)
	{
		// Probe outward from the vertex position so adjacent snow polys at ramp corners
		// return matching floor heights (rawDrop = 0) and don't erroneously reduce the
		// per-vertex lift scale. To make this work for INTERNAL diagonal-split edges --
		// where the vertex sits exactly on a sector corner and the outward perpendicular
		// points partly outside the sector -- first pull the probe a few world units
		// along the edge toward its midpoint. This shifts the sample off the corner
		// into the interior of the source sector, so the subsequent outward inset lands
		// in the OTHER sub-triangle of the same sector instead of going out of bounds.
		// The along-edge shift is small enough that on axis-aligned edges the probe
		// still ends up in the neighbour sector (PROBE_INSET dominates perpendicularly).
		constexpr float PROBE_INSET = 4.0f;
		constexpr float EDGE_INSET  = 8.0f;

		float toMidDX = edgeMidX - localX;
		float toMidDZ = edgeMidZ - localZ;
		float toMidLen = sqrtf(toMidDX * toMidDX + toMidDZ * toMidDZ);
		if (toMidLen > 1e-3f)
		{
			toMidDX /= toMidLen;
			toMidDZ /= toMidLen;
		}
		else
		{
			toMidDX = 0.0f;
			toMidDZ = 0.0f;
		}

		float probeMidX = localX + toMidDX * EDGE_INSET + outwardX * PROBE_INSET;
		float probeMidZ = localZ + toMidDZ * EDGE_INSET + outwardZ * PROBE_INSET;
		int gridX = (int)(probeMidX / BLOCK(1));
		int gridZ = (int)(probeMidZ / BLOCK(1));
		if (gridX < 0 || gridX >= room.XSize || gridZ < 0 || gridZ >= room.ZSize)
			return NO_HEIGHT;

		const auto& sector = room.Sectors[gridX * room.ZSize + gridZ];

		int absSampleX = (int)probeMidX + room.Position.x;
		int absSampleZ = (int)probeMidZ + room.Position.z;

		// Wall-sector neighbour: the neighbor's floor plane stores NO_HEIGHT, so
		// GetSurfaceHeight is meaningless here. Instead, look one step up to find
		// the room above the neighbour position and query its floor height. This
		// correctly handles step-downs between adjacent wall tops (sub-case below).
		// For all other wall cases (no room above, double wall, etc.) return sourceAbsY
		// so the drop is zero: skirts at walls are already suppressed by IsSnowNeighborAt,
		// and a non-zero fallback would incorrectly reduce the lift scale for floor
		// polygons adjacent to exterior walls, causing the snow to sink to floor level
		// and appear as holes.
		if (sector.IsWall(absSampleX, absSampleZ))
		{
			int aboveRoomNumber = FindRoomNumber(Vector3i(absSampleX, sourceAbsY - 16, absSampleZ));
			if (aboveRoomNumber == NO_VALUE)
				return sourceAbsY;

			const auto& roomAbove = g_Level.Rooms[aboveRoomNumber];
			int agx = (absSampleX - roomAbove.Position.x) / BLOCK(1);
			int agz = (absSampleZ - roomAbove.Position.z) / BLOCK(1);
			if (agx < 0 || agx >= roomAbove.XSize || agz < 0 || agz >= roomAbove.ZSize)
				return sourceAbsY;

			const auto& sectorAbove = roomAbove.Sectors[agx * roomAbove.ZSize + agz];
			if (sectorAbove.IsWall(absSampleX, absSampleZ))
				return sourceAbsY;

			int floorY = sectorAbove.GetSurfaceHeight(absSampleX, absSampleZ, true);
			if (floorY == NO_HEIGHT)
				return sourceAbsY;

			// Step-down between adjacent wall tops: return the actual floor height so the
			// correct drop is computed for both lift scale and skirt geometry.
			return floorY;
		}

		int floorY = sector.GetSurfaceHeight(absSampleX, absSampleZ, true);
		return floorY;
	}

	// For an edge from local vA to vB, determines whether a vertical skirt should be
	// emitted and, if so, returns the absolute world-space bottom Y for each endpoint.
	// Returns false if no skirt is needed (no neighbor, same height, or neighbor is
	// higher -- which is handled by the neighbor polygon's own skirt going the other way).
	// Returns true if the neighbor sector that lies PROBE_INSET world units beyond the
	// given local-space sample point (in the outward direction) has the Snow material on
	// its floor. Mirrors the probing geometry used by GetSnowNeighborFloorAt so the two
	// stay in sync.
	static bool IsSnowNeighborAt(
		const RoomData& room,
		float localX, float localZ,
		float edgeMidX, float edgeMidZ,
		float outwardX, float outwardZ)
	{
		constexpr float PROBE_INSET = 4.0f;
		constexpr float EDGE_INSET  = 8.0f;

		float toMidDX = edgeMidX - localX;
		float toMidDZ = edgeMidZ - localZ;
		float toMidLen = sqrtf(toMidDX * toMidDX + toMidDZ * toMidDZ);
		if (toMidLen > 1e-3f)
		{
			toMidDX /= toMidLen;
			toMidDZ /= toMidLen;
		}
		else
		{
			toMidDX = 0.0f;
			toMidDZ = 0.0f;
		}

		float probeX = localX + toMidDX * EDGE_INSET + outwardX * PROBE_INSET;
		float probeZ = localZ + toMidDZ * EDGE_INSET + outwardZ * PROBE_INSET;
		int gridX = (int)(probeX / BLOCK(1));
		int gridZ = (int)(probeZ / BLOCK(1));

		int absX = (int)probeX + room.Position.x;
		int absZ = (int)probeZ + room.Position.z;

		// Probe is inside the same room: query directly.
		if (gridX >= 0 && gridX < room.XSize && gridZ >= 0 && gridZ < room.ZSize)
		{
			const auto& sector = room.Sectors[gridX * room.ZSize + gridZ];

			// Wall neighbour: a solid wall closes the visual gap, so no drooping skirt
			// is needed. Exception: if the wall belongs to the wall sub-half of a
			// diagonal-step sector (IsSurfaceSplit), the probe stayed inside the same
			// sector and that partial wall does NOT close the gap under the lifted snow
			// edge along the diagonal cut -- allow the skirt to be emitted.
			if (sector.IsWall(absX, absZ))
				return !sector.IsSurfaceSplit(true);

			return sector.GetSurfaceMaterial(absX, absZ, true) == MaterialType::Snow;
		}

		return false;
	}

	static bool GetSnowSkirtForEdge(
		const Vector3& vA,
		const Vector3& vB,
		const Vector3& polyCentroidLocal,
		const RoomData& room,
		int& outBottomAbsYA,
		int& outBottomAbsYB)
	{
		constexpr int SKIRT_MIN_DROP = 8;         // World units: avoid hairline skirts on flat tiles.
		constexpr int SKIRT_MAX_DROP = CLICK(3);  // Beyond ~3 clicks the slope exceeds the angle of repose
		                                           // for snow -- no natural drift forms against a near-vertical wall.
		constexpr int SKIRT_CAP_DROP = CLICK(2);  // For cliff-edge wall tops (huge drops), clamp the skirt to a
		                                           // short drooping lip so the snow cap visibly overhangs the edge
		                                           // instead of cutting off flat.

		// Edge midpoint and outward direction (perpendicular to edge in XZ, away from centroid).
		float midX = (vA.x + vB.x) * 0.5f;
		float midZ = (vA.z + vB.z) * 0.5f;

		float ex = vB.x - vA.x;
		float ez = vB.z - vA.z;

		// Two possible perpendiculars; pick the one pointing AWAY from polygon centroid.
		float nx =  ez;
		float nz = -ex;
		float len = sqrtf(nx * nx + nz * nz);
		if (len < 1e-3f)
			return false;
		nx /= len;
		nz /= len;

		float toMidX = midX - polyCentroidLocal.x;
		float toMidZ = midZ - polyCentroidLocal.z;
		if (nx * toMidX + nz * toMidZ < 0.0f)
		{
			nx = -nx;
			nz = -nz;
		}

		int topAbsYA = (int)vA.y + room.Position.y;
		int topAbsYB = (int)vB.y + room.Position.y;

		int neighborYA = GetSnowNeighborFloorAt(room, vA.x, vA.z, midX, midZ, nx, nz, topAbsYA);
		int neighborYB = GetSnowNeighborFloorAt(room, vB.x, vB.z, midX, midZ, nx, nz, topAbsYB);

		// NO_HEIGHT means the probe went out of room bounds (level edge) -- treat it as a
		// cliff-edge drop large enough to trigger the cap-skirt path below.
		if (neighborYA == NO_HEIGHT) neighborYA = topAbsYA + SKIRT_MAX_DROP;
		if (neighborYB == NO_HEIGHT) neighborYB = topAbsYB + SKIRT_MAX_DROP;

		int dropA = neighborYA - topAbsYA;
		int dropB = neighborYB - topAbsYB;

		// Suppress skirts where the snow blanket continues seamlessly onto the neighbouring
		// sector (same height) or a solid wall closes the gap. When the neighbour is snow
		// but sits more than SKIRT_MIN_DROP below, the step face between the two snow
		// surface levels is exposed and a skirt is still required to cover it.
		bool neighborIsSnowA = IsSnowNeighborAt(room, vA.x, vA.z, midX, midZ, nx, nz);
		bool neighborIsSnowB = IsSnowNeighborAt(room, vB.x, vB.z, midX, midZ, nx, nz);
		if (neighborIsSnowA && neighborIsSnowB && dropA < SKIRT_MIN_DROP && dropB < SKIRT_MIN_DROP)
			return false;

		// Even at zero drop, an edge whose neighbour is NOT snow needs a skirt: the
		// snow surface sits `snowLift` above the underlying floor (shader-side), so
		// without a skirt the snow blanket appears to float at its borders. The skirt
		// top inherits the polygon's full liftScale and the bottom row sits at the
		// neighbour floor with zero lift, producing a natural drooping edge of height
		// snowLift in screen space. Only skip when BOTH endpoints have an upward
		// neighbour (drop < 0) -- those edges belong to the adjacent higher polygon.
		if (dropA <= -SKIRT_MIN_DROP && dropB <= -SKIRT_MIN_DROP)
			return false;

		// Cliff-edge case: when BOTH endpoints drop by more than SKIRT_MAX_DROP (or the
		// probe went out of bounds), the underlying surface is effectively a vertical wall.
		// A full-height skirt would be unrealistic, but cutting the snow off flat at the
		// edge looks wrong too. Clamp the bottom Y to a short overhang lip (SKIRT_CAP_DROP
		// below the top edge) so the snow cap visibly droops over the cliff.
		bool isCliffEdge = (dropA >= SKIRT_MAX_DROP && dropB >= SKIRT_MAX_DROP);
		if (isCliffEdge)
		{
			outBottomAbsYA = topAbsYA + SKIRT_CAP_DROP;
			outBottomAbsYB = topAbsYB + SKIRT_CAP_DROP;
			return true;
		}

		// Clamp each endpoint individually. If one side has a huge drop (NO_HEIGHT-replaced
		// or genuine cliff) while the other has a valid small drop, cap the large side to
		// SKIRT_CAP_DROP so the skirt remains visually proportional.
		outBottomAbsYA = std::max(topAbsYA, (dropA >= SKIRT_MAX_DROP ? topAbsYA + SKIRT_CAP_DROP : neighborYA));
		outBottomAbsYB = std::max(topAbsYB, (dropB >= SKIRT_MAX_DROP ? topAbsYB + SKIRT_CAP_DROP : neighborYB));
		return true;
	}

	static int CountSnowSkirtEdges(const POLYGON& poly, const RoomData& room)
	{
		int n = (int)poly.indices.size();
		if (n < 3)
			return 0;

		auto centroid = Vector3::Zero;
		for (int idx : poly.indices)
			centroid += room.positions[idx];
		centroid /= (float)n;

		int count = 0;
		for (int k = 0; k < n; k++)
		{
			const auto& vA = room.positions[poly.indices[k]];
			const auto& vB = room.positions[poly.indices[(k + 1) % n]];
			int bottomA, bottomB;
			if (GetSnowSkirtForEdge(vA, vB, centroid, room, bottomA, bottomB))
				count++;
		}
		return count;
	}

	// Vertex/index counts for a subdivided skirt patch: (N+1) x (N+1) grid -> N*N quads.
	static void GetSnowSkirtCounts(int skirtEdges, int subdivisions, int& outVerts, int& outIndices)
	{
		int nGrid = subdivisions + 1;
		outVerts   = skirtEdges * nGrid * nGrid;
		outIndices = skirtEdges * subdivisions * subdivisions * 6;
	}

	// Bilinear interpolation across the 4 sector-floor quad corners.
	// Index convention matches the main bucket loop: 0=NW, 1=NE, 2=SE, 3=SW.
	template <typename T>
	static T BilerpQuad(const T& nw, const T& ne, const T& se, const T& sw, float u, float v)
	{
		T top	 = nw * (1.0f - u) + ne * u;
		T bottom = sw * (1.0f - u) + se * u;
		return top * (1.0f - v) + bottom * v;
	}

	template <typename T>
	static T BaryTri(const T& a, const T& b, const T& c, float w0, float w1, float w2)
	{
		return a * w0 + b * w1 + c * w2;
	}

	// Returns the per-vertex lift scale k in [0, 1] for a snow overlay/skirt vertex
	// at posLocal. Smoothstep-faded over EDGE_FADE_RANGE inward from any drop edge.
	//
	// The value of k at the edge itself depends on the drop magnitude:
	//   - Drop >= 3 clicks: edgeScale=0 -> snow rolls all the way down to the floor.
	//     NOTE: edges with very large drops are probed directly (not via skirt eligibility),
	//     so even fully-elevated squares (all edges >= 3 clicks, no skirt emitted) are handled.
	//   - Drop in [1, lift]: edgeScale = 1 - drop/lift -> snow transitions to neighbor snow height.
	//   - Drop in (lift, 3 clicks): skirt handles geometry, no overlay depression (avoids
	//     triangulation artefacts at corners when lift < 1 click).
	// Outside the fade band k=1 (standard, undisturbed snow).
	static float GetSnowLiftScale(
		const POLYGON& poly,
		const RoomData& room,
		const Vector3& posLocal,
		float lift)
	{
		constexpr float EDGE_FADE_RANGE = (float)CLICK(2);
		constexpr int LARGE_DROP_THRESHOLD = CLICK(3);
		constexpr int MIN_DROP = 8; // Avoid hairline adjustments on flat tiles.

		int n = (int)poly.indices.size();
		if (n < 3)
			return 1.0f;

		Vector3 centroid = Vector3::Zero;
		for (int idx : poly.indices)
			centroid += room.positions[idx];
		centroid /= (float)n;

		float minScale = 1.0f;

		for (int e = 0; e < n; e++)
		{
			const auto& vA = room.positions[poly.indices[e]];
			const auto& vB = room.positions[poly.indices[(e + 1) % n]];

			// Compute outward normal for this edge (away from polygon centroid).
			float midX = (vA.x + vB.x) * 0.5f;
			float midZ = (vA.z + vB.z) * 0.5f;
			float ex = vB.x - vA.x;
			float ez = vB.z - vA.z;
			float nx =  ez;
			float nz = -ex;
			float nlen = sqrtf(nx * nx + nz * nz);
			if (nlen < 1e-3f)
				continue;
			nx /= nlen;
			nz /= nlen;
			float toMidX = midX - centroid.x;
			float toMidZ = midZ - centroid.z;
			if (nx * toMidX + nz * toMidZ < 0.0f)
			{
				nx = -nx;
				nz = -nz;
			}

			int topAbsYA = (int)vA.y + room.Position.y;
			int topAbsYB = (int)vB.y + room.Position.y;

			// Probe neighbor floor directly -- no SKIRT_MAX_DROP filter so large-drop
			// edges on fully elevated squares are detected.
			int neighborYA = GetSnowNeighborFloorAt(room, vA.x, vA.z, midX, midZ, nx, nz, topAbsYA);
			int neighborYB = GetSnowNeighborFloorAt(room, vB.x, vB.z, midX, midZ, nx, nz, topAbsYB);
			if (neighborYA == NO_HEIGHT || neighborYB == NO_HEIGHT)
				continue;
			int rawDropA = neighborYA - topAbsYA;
			int rawDropB = neighborYB - topAbsYB;

			// Skip edges with no meaningful downward drop on either endpoint.
			if (rawDropA < MIN_DROP && rawDropB < MIN_DROP)
				continue;

			int dropA = std::max(0, rawDropA);
			int dropB = std::max(0, rawDropB);

			// Skip internal diagonal edges of split sectors. These edges lie entirely within
			// one grid sector: the probe stays in the same cell as the polygon centroid.
			// Regardless of material on the other triangle, no liftScale reduction should
			// occur across an internal cut -- it creates holes between the two triangular halves.
			//
			// Also skip when the neighbour sector is itself diagonally split (diagonal wall
			// or diagonal elevation): the diagonal cut makes the sub-triangle sampled at the
			// edge probe ambiguous, and adjacent snow squares must keep their full lift along
			// edges touching such a sector -- no drop allowed.
			{
				constexpr float PROBE_INSET = 4.0f;
				float probeMidX = midX + nx * PROBE_INSET;
				float probeMidZ = midZ + nz * PROBE_INSET;
				int probeGridX = (int)(probeMidX / BLOCK(1));
				int probeGridZ = (int)(probeMidZ / BLOCK(1));
				int centGridX  = (int)(centroid.x / BLOCK(1));
				int centGridZ  = (int)(centroid.z / BLOCK(1));
				if (probeGridX == centGridX && probeGridZ == centGridZ)
					continue;

				if (probeGridX >= 0 && probeGridX < room.XSize &&
					probeGridZ >= 0 && probeGridZ < room.ZSize)
				{
					const auto& probeSector = room.Sectors[probeGridX * room.ZSize + probeGridZ];
					if (probeSector.IsSurfaceSplit(true))
						continue;
				}
			}

			// Distance from posLocal to nearest point on this edge in XZ.
			float elenSq = ex * ex + ez * ez;
			if (elenSq < 1e-3f)
				continue;

			float t = ((posLocal.x - vA.x) * ex + (posLocal.z - vA.z) * ez) / elenSq;
			t = std::clamp(t, 0.0f, 1.0f);

			float qx = vA.x + ex * t;
			float qz = vA.z + ez * t;
			float dx = posLocal.x - qx;
			float dz = posLocal.z - qz;
			float dist = sqrtf(dx * dx + dz * dz);
			if (dist >= EDGE_FADE_RANGE)
				continue;

			// Interpolate the drop at the nearest edge point (handles sloped ramps).
			int dropQ = (int)((float)dropA * (1.0f - t) + (float)dropB * t);

			// Determine edge scale based on drop magnitude.
			float edgeScale;
			if (dropQ >= LARGE_DROP_THRESHOLD || lift < 1.0f)
			{
				// Large drop: snow rolls all the way to the floor.
				edgeScale = 0.0f;
			}
			else if (dropQ <= (int)lift)
			{
				// Small drop reachable by lift: interpolate to neighbor snow surface.
				edgeScale = 1.0f - (float)dropQ / lift;
			}
			else if (dropQ >= CLICK(1))
			{
				// Medium drop (> 1 click, < 3 clicks): roll snow cap to floor so the
				// overlay forms a natural mound at raised platform edges (hill effect).
				edgeScale = 0.0f;
			}
			else
			{
				// Drop just above lift but <= 1 click: the skirt covers the geometry.
				// Skip the overlay depression to avoid triangulation artefacts at
				// corners when lift is smaller than the drop.
				continue;
			}

			// Smoothstep: edgeScale at dist=0, 1.0 at EDGE_FADE_RANGE inward.
			float u = dist / EDGE_FADE_RANGE;
			float fade = u * u * (3.0f - 2.0f * u);
			float scale = edgeScale + (1.0f - edgeScale) * fade;
			if (scale < minScale)
				minScale = scale;
		}

		// Per-vertex corner probe: the edge loop above only detects drops for tiles that
		// share an edge with this polygon. Tiles sharing only a corner (diagonal adjacency)
		// are not covered by any edge perpendicular, so their drop is never seen -- causing
		// the snow surface to remain fully lifted at the corner vertex even though the
		// diagonally adjacent tile is lower. Probe outward from each vertex (in the
		// centroid-to-vertex direction) to detect such diagonal drops and apply the same
		// liftScale fade so the snow surface rolls down to match the adjacent surfaces.
		for (int vi = 0; vi < n; vi++)
		{
			const auto& vc = room.positions[poly.indices[vi]];

			float cornerDirX = vc.x - centroid.x;
			float cornerDirZ = vc.z - centroid.z;
			float cornerLen = sqrtf(cornerDirX * cornerDirX + cornerDirZ * cornerDirZ);
			if (cornerLen < 1e-3f)
				continue;
			cornerDirX /= cornerLen;
			cornerDirZ /= cornerLen;

			// Small offset past the vertex to cross into the diagonal sector.
			constexpr float CORNER_PROBE = 4.0f;
			float probeX = vc.x + cornerDirX * CORNER_PROBE;
			float probeZ = vc.z + cornerDirZ * CORNER_PROBE;
			int probeGridX = (int)(probeX / BLOCK(1));
			int probeGridZ = (int)(probeZ / BLOCK(1));

			if (probeGridX < 0 || probeGridX >= room.XSize || probeGridZ < 0 || probeGridZ >= room.ZSize)
				continue;

			int topAbsY  = (int)vc.y + room.Position.y;
			int absProbeX = (int)probeX + room.Position.x;
			int absProbeZ = (int)probeZ + room.Position.z;
			int centGridX = (int)(centroid.x / BLOCK(1));
			int centGridZ = (int)(centroid.z / BLOCK(1));

			const auto& cornerSector = room.Sectors[probeGridX * room.ZSize + probeGridZ];
			int cornerNeighborY = NO_HEIGHT;

			if (probeGridX == centGridX && probeGridZ == centGridZ)
			{
				// The probe landed in the same sector as the polygon centroid. This is the
				// normal result for the internal diagonal edge of a split sector (both
				// sub-halves share the same grid cell) and should be skipped to avoid
				// suppressing lift on flat snow tiles. However, if the probe sub-half has
				// a floor portal going downward, the raised polygon vertex overhangs the
				// portal opening and must roll down to the floor of the room below.
				auto belowRoomNum = cornerSector.GetNextRoomNumber(absProbeX, absProbeZ, true);
				if (!belowRoomNum.has_value())
					continue;

				const auto& roomBelow = g_Level.Rooms[*belowRoomNum];
				int bgx = (absProbeX - roomBelow.Position.x) / BLOCK(1);
				int bgz = (absProbeZ - roomBelow.Position.z) / BLOCK(1);
				if (bgx < 0 || bgx >= roomBelow.XSize || bgz < 0 || bgz >= roomBelow.ZSize)
					continue;

				const auto& belowSector = roomBelow.Sectors[bgx * roomBelow.ZSize + bgz];
				if (!belowSector.IsWall(absProbeX, absProbeZ))
					cornerNeighborY = belowSector.GetSurfaceHeight(absProbeX, absProbeZ, true);
			}
			else if (cornerSector.IsWall(absProbeX, absProbeZ))
			{
				// Solid wall closes the visual gap: no corner drop.
				// Includes diagonal-step wall sub-halves: their step face is part of
				// the diagonal sector and must not deform the neighbour snow surface.
				continue;
			}
			else
			{
				// Diagonally split corner sector: the diagonal cut creates an ambiguous
				// sub-triangle to sample, and either half (raised or lowered) is part
				// of the diagonal feature. Adjacent snow squares must keep their full
				// lift at corners touching such a sector -- no corner drop allowed.
				if (cornerSector.IsSurfaceSplit(true))
					continue;

				cornerNeighborY = cornerSector.GetSurfaceHeight(absProbeX, absProbeZ, true);
			}

			if (cornerNeighborY == NO_HEIGHT)
				continue;

			int cornerDrop = cornerNeighborY - topAbsY;
			if (cornerDrop < MIN_DROP)
				continue;

			// Distance from posLocal to this corner vertex.
			float dx = posLocal.x - vc.x;
			float dz = posLocal.z - vc.z;
			float dist = sqrtf(dx * dx + dz * dz);
			if (dist >= EDGE_FADE_RANGE)
				continue;

			float edgeScale;
			if (cornerDrop >= LARGE_DROP_THRESHOLD || lift < 1.0f)
			{
				edgeScale = 0.0f;
			}
			else if (cornerDrop <= (int)lift)
			{
				edgeScale = 1.0f - (float)cornerDrop / lift;
			}
			else
			{
				// Drop exceeds lift but is below the large-drop threshold. For edge-adjacent
				// tiles the skirt covers the step face, so the overlay does a continue.
				// For corner-adjacent tiles there is no skirt, so force edgeScale to 0
				// to bring the snow surface down regardless of snow depth.
				edgeScale = 0.0f;
			}

			float u = dist / EDGE_FADE_RANGE;
			float fade = u * u * (3.0f - 2.0f * u);
			float scale = edgeScale + (1.0f - edgeScale) * fade;
			if (scale < minScale)
				minScale = scale;
		}

		return minScale;
	}

	// Writes one snow overlay vertex into the global buffer.
	static void EmitSnowVertex(
		Vertex&        out,
		const Vector3& posLocal,
		const Vector2& uv,
		const Vector3& nrm,
		const Vector3& tan,
		const Vector3& col,
		const Vector3& effects,
		const Vector3& faceNrm,
		const RoomData& room,
		float lift,
		int   animFrame,
		float shineStrength,
		int   vertIndexInPoly,
		float liftScale = 1.0f)
	{
		float k = std::clamp(liftScale, 0.0f, 1.0f);

		// 1 WU upward bias prevents Z-fighting with the underlying floor polygon when the
		// snow is fully deformed (h=1 in SnowOverlay.hlsl), which would otherwise bring
		// the snow surface to exactly the same Y as the floor geometry.
		constexpr float SNOW_Z_BIAS = 1.0f;

		out.Position.x = room.Position.x + posLocal.x;
		out.Position.y = room.Position.y + posLocal.y - lift * k - SNOW_Z_BIAS; // Lift snow above floor (Y is down).
		out.Position.z = room.Position.z + posLocal.z;

		out.Normal	   = Renderer::PackVector3(nrm);
		out.UV		   = uv;
		out.Color	   = VectorColorToRGBA(Vector4(col.x, col.y, col.z, k)); // alpha = per-vertex liftScale for SnowOverlay.hlsl.
		out.Tangent	   = Renderer::PackVector3(tan);
		out.FaceNormal = Renderer::PackVector3(faceNrm);

		constexpr unsigned long long PRIMES[] = { 73856093ULL, 19349663ULL, 83492791ULL };
		unsigned int hash =
			(unsigned int)std::hash<float>{}(out.Position.x * PRIMES[0]) ^
			((unsigned int)std::hash<float>{}(out.Position.y) * PRIMES[1]) ^
			((unsigned int)std::hash<float>{}(out.Position.z) * PRIMES[2]);

		out.AnimationFrameOffsetIndexHash = Renderer::PackAnimationFrameOffsetIndexHash(animFrame, 0, (int)hash);
		out.Effects						  = Renderer::PackEffectsAndIndexInPoly(effects, shineStrength, vertIndexInPoly);
	}

	// Emits a sloped, subdivided "snow drift" patch along polygon edges whose neighbor
	// sector floor sits below the current edge. The patch is an N x N grid:
	//   - The top row lies on the higher polygon's snow surface (s in [0,1] along edge).
	//   - The bottom row sits on the neighbor sector's snow surface, pushed OUTWARD
	//     horizontally to form a natural drift slope.
	//   - Every interior vertex samples the snow heightmap at its own XZ via the snow VS,
	//     so the patch curves with hills on the top edge and deforms with footprints on
	//     either side. With sufficient subdivisions the patch meets the parent floor and
	//     the neighbor floor without visible cracks.
	// UVs are interpolated along the edge but kept constant vertically to avoid sampling
	// outside the parent polygon's atlas tile.
	static void EmitSnowSkirtsForPolygon(
		const POLYGON&   poly,
		const RoomData&  room,
		int              subdivisions,
		float            lift,
		std::vector<Vertex>& vertices,
		std::vector<int>&    indices,
		int&             vertCursor,
		int&             indexCursor,
		std::vector<RendererPolygon>& outPolys,
		const Vector3*   colorOverride = nullptr)
	{
		const int N = std::max(1, subdivisions);
		const int nGrid = N + 1;
		int n = (int)poly.indices.size();
		if (n < 3)
			return;

		auto centroid = Vector3::Zero;
		for (int idx : poly.indices)
			centroid += room.positions[idx];
		centroid /= (float)n;

		for (int k = 0; k < n; k++)
		{
			int ia = poly.indices[k];
			int ib = poly.indices[(k + 1) % n];

			const auto& vA = room.positions[ia];
			const auto& vB = room.positions[ib];

			int bottomAbsYA, bottomAbsYB;
			if (!GetSnowSkirtForEdge(vA, vB, centroid, room, bottomAbsYA, bottomAbsYB))
				continue;

			// Outward horizontal normal (perpendicular to edge in XZ, away from centroid).
			float ex = vB.x - vA.x;
			float ez = vB.z - vA.z;
			float nx =  ez;
			float nz = -ex;
			float nlen = sqrtf(nx * nx + nz * nz);
			if (nlen < 1e-3f)
				continue;
			nx /= nlen;
			nz /= nlen;
			float toMidX = ((vA.x + vB.x) * 0.5f) - centroid.x;
			float toMidZ = ((vA.z + vB.z) * 0.5f) - centroid.z;
			if (nx * toMidX + nz * toMidZ < 0.0f)
			{
				nx = -nx;
				nz = -nz;
			}

			// Bottom endpoints: same XZ as the top edge (purely vertical skirt). This makes
			// the skirt connect to the neighbor overlay (or the neighbor floor) at exactly
			// the same XZ as the polygon edge -- no horizontal gap, no separate floating
			// surface. Y goes down to the neighbor floor.
			int dropA = bottomAbsYA - ((int)vA.y + room.Position.y);
			int dropB = bottomAbsYB - ((int)vB.y + room.Position.y);
			(void)dropA; (void)dropB;

			Vector3 bA = vA;
			bA.y = (float)(bottomAbsYA - room.Position.y);

			Vector3 bB = vB;
			bB.y = (float)(bottomAbsYB - room.Position.y);

			// Average slope direction (top -> bottom) for the patch normal.
			Vector3 slopeDir = (bA - vA) + (bB - vB);
			slopeDir *= 0.5f;
			if (slopeDir.LengthSquared() < 1e-6f)
				slopeDir = Vector3(nx, 0.0f, nz);
			else
				slopeDir.Normalize();

			Vector3 edgeDir = Vector3(ex, 0.0f, ez);
			if (edgeDir.LengthSquared() > 1e-6f)
				edgeDir.Normalize();
			Vector3 skirtNrm = edgeDir.Cross(slopeDir);
			if (skirtNrm.LengthSquared() < 1e-6f)
				skirtNrm = Vector3(0.0f, -1.0f, 0.0f);
			else
				skirtNrm.Normalize();
			if (skirtNrm.y > 0.0f)
				skirtNrm = -skirtNrm;

			// UVs are interpolated along the edge only (constant V across rows). This
			// keeps every skirt row inside the parent polygon's atlas tile and avoids
			// the stretched / garbage textures that come from extrapolating V beyond
			// the polygon's UV bounds.
			Vector2 uvA = poly.textureCoordinates[k];
			Vector2 uvB = poly.textureCoordinates[(k + 1) % n];

			Vector3 colA = colorOverride ? *colorOverride : room.colors[ia];
			Vector3 colB = colorOverride ? *colorOverride : room.colors[ib];
			Vector3 effA = room.effects[ia];
			Vector3 effB = room.effects[ib];

			int baseVertices = vertCursor;

			// Generate (N+1) x (N+1) vertex grid. Indexing: vert(i, j) at baseVertices + j * nGrid + i,
			// where i is along the edge (0..N) and j is from top (0) to bottom (N).
			for (int j = 0; j < nGrid; j++)
			{
				float tV = (float)j / (float)N;
				for (int i = 0; i < nGrid; i++)
				{
					float tU = (float)i / (float)N;

					// Top XZ at edge parameter tU.
					Vector3 topPt;
					topPt.x = vA.x * (1.0f - tU) + vB.x * tU;
					topPt.y = vA.y * (1.0f - tU) + vB.y * tU;
					topPt.z = vA.z * (1.0f - tU) + vB.z * tU;

					// Bottom XZ at the same edge parameter (pushed outward by lerp(pushA, pushB)).
					Vector3 botPt;
					botPt.x = bA.x * (1.0f - tU) + bB.x * tU;
					botPt.y = bA.y * (1.0f - tU) + bB.y * tU;
					botPt.z = bA.z * (1.0f - tU) + bB.z * tU;

					// Interpolate top -> bottom by tV.
					// Apply a small inward bias so the skirt sits just inside the wall face
					// geometry, preventing Z-fighting with the adjacent square textures.
					constexpr float SKIRT_INSET = 2.0f;
					Vector3 pt;
					pt.x = topPt.x * (1.0f - tV) + botPt.x * tV - nx * SKIRT_INSET;
					pt.y = topPt.y * (1.0f - tV) + botPt.y * tV;
					pt.z = topPt.z * (1.0f - tV) + botPt.z * tV - nz * SKIRT_INSET;

					Vector2 vertUV = Vector2(uvA.x * (1.0f - tU) + uvB.x * tU,
											 uvA.y * (1.0f - tU) + uvB.y * tU);
					Vector3 vertCol = colA * (1.0f - tU) + colB * tU;
					Vector3 vertEff = effA * (1.0f - tU) + effB * tU;

					int slot = ((j == 0) ? 0 : 2) + ((i == nGrid - 1) ? 1 : 0);

					// Per-vertex lift scale: top row matches the overlay edge at this XZ;
					// bottom row has no lift (k=0) so it sits flush with the neighbor floor.
					// Mid rows lerp linearly, keeping the skirt consistent with the overlay.
					float topScale  = GetSnowLiftScale(poly, room, topPt, lift);
					float vertScale = topScale * (1.0f - tV);

					EmitSnowVertex(vertices[vertCursor++], pt, vertUV, skirtNrm, edgeDir,
								   vertCol, vertEff, skirtNrm, room, lift,
								   poly.animatedFrame, poly.shineStrength, slot, vertScale);
				}
			}

			// Index the N x N grid as quads (two triangles each), winding matched to
			// EmitSnowOverlayPolygon's quad order so the outward face renders.
			for (int j = 0; j < N; j++)
			{
				for (int i = 0; i < N; i++)
				{
					int v00 = baseVertices + j       * nGrid + i;
					int v10 = baseVertices + j       * nGrid + (i + 1);
					int v01 = baseVertices + (j + 1) * nGrid + i;
					int v11 = baseVertices + (j + 1) * nGrid + (i + 1);

					RendererPolygon subPoly{};
					subPoly.Shape	  = 0;
					subPoly.Normal	  = skirtNrm;
					subPoly.Centre	  = (vertices[v00].Position + vertices[v10].Position +
										 vertices[v11].Position + vertices[v01].Position) * 0.25f;
					subPoly.BaseIndex = indexCursor;

					indices[indexCursor + 0] = v00;
					indices[indexCursor + 1] = v01;
					indices[indexCursor + 2] = v10;
					indices[indexCursor + 3] = v11;
					indices[indexCursor + 4] = v10;
					indices[indexCursor + 5] = v01;
					indexCursor += 6;

					outPolys.push_back(subPoly);
				}
			}
		}
	}

	// Emits a fully subdivided snow overlay for a single source floor polygon.
	// Appends interleaved per-quad / per-triangle vertices and indices in the same layout
	// used by the main bucket loop (so the existing index pattern keeps working).
	static void EmitSnowOverlayPolygon(
		const POLYGON&   poly,
		const RoomData&  room,
		int              subdivisions,
		float            lift,
		std::vector<Vertex>& vertices,
		std::vector<int>&    indices,
		int&             vertCursor,
		int&             indexCursor,
		std::vector<RendererPolygon>& outPolys,
		const Vector3*   colorOverride = nullptr)
	{
		const int N = subdivisions;

		// Cache per-input-vertex attributes.
		const int inCount = (int)poly.indices.size();
		std::array<Vector3, 4> p {}, nrm {}, tan {}, col {}, eff {};
		std::array<Vector2, 4> uv {};
		for (int k = 0; k < inCount && k < 4; k++)
		{
			int idx = poly.indices[k];
			p  [k] = room.positions[idx];
			nrm[k] = poly.normals[k];
			tan[k] = poly.tangents[k];
			uv [k] = poly.textureCoordinates[k];
			col[k] = colorOverride ? *colorOverride : room.colors[idx];
			eff[k] = room.effects[idx];
		}

		if (poly.shape == 0)
		{
			// Quad: subdivide into N x N sub-quads via bilinear interpolation.
			for (int j = 0; j < N; j++)
			{
				for (int i = 0; i < N; i++)
				{
					float u0 = (float)i       / (float)N;
					float u1 = (float)(i + 1) / (float)N;
					float v0 = (float)j       / (float)N;
					float v1 = (float)(j + 1) / (float)N;

					struct Corner { float u, v; };
					const Corner corners[4] = { {u0, v0}, {u1, v0}, {u1, v1}, {u0, v1} };

					int baseVertices = vertCursor;

					for (int k = 0; k < 4; k++)
					{
						auto cp  = BilerpQuad(p[0],   p[1],   p[2],   p[3],   corners[k].u, corners[k].v);
						auto cuv = BilerpQuad(uv[0],  uv[1],  uv[2],  uv[3],  corners[k].u, corners[k].v);
						auto cn  = BilerpQuad(nrm[0], nrm[1], nrm[2], nrm[3], corners[k].u, corners[k].v);
						cn.Normalize();
						auto ct  = BilerpQuad(tan[0], tan[1], tan[2], tan[3], corners[k].u, corners[k].v);
						ct.Normalize();
						auto cc  = BilerpQuad(col[0], col[1], col[2], col[3], corners[k].u, corners[k].v);
						auto ce  = BilerpQuad(eff[0], eff[1], eff[2], eff[3], corners[k].u, corners[k].v);

float liftScale = GetSnowLiftScale(poly, room, cp, lift);

						EmitSnowVertex(vertices[vertCursor], cp, cuv, cn, ct, cc, ce,
									   poly.normal, room, lift, poly.animatedFrame, poly.shineStrength, k, liftScale);
						vertCursor++;
					}

					RendererPolygon subPoly{};
					subPoly.Shape	 = 0;
					subPoly.Normal	 = poly.normal;
					subPoly.Centre	 = (vertices[baseVertices + 0].Position +
										vertices[baseVertices + 1].Position +
										vertices[baseVertices + 2].Position +
										vertices[baseVertices + 3].Position) * 0.25f;
					subPoly.BaseIndex = indexCursor;

					indices[indexCursor + 0] = baseVertices + 0;
					indices[indexCursor + 1] = baseVertices + 1;
					indices[indexCursor + 2] = baseVertices + 3;
					indices[indexCursor + 3] = baseVertices + 2;
					indices[indexCursor + 4] = baseVertices + 3;
					indices[indexCursor + 5] = baseVertices + 1;
					indexCursor += 6;

					outPolys.push_back(subPoly);
				}
			}
		}
		else
		{
			// Triangle: barycentric grid of (i, j) with 0 <= i, 0 <= j, i + j <= N.
			// Each grid cell (i, j) with i + j < N emits 1 upward sub-triangle and, if
			// i + j + 1 < N, 1 additional downward sub-triangle. Total: N * N triangles.
			auto sampleAt = [&](int gi, int gj, int kSlot, int& outBase)
			{
				float w0 = (float)(N - gi - gj) / (float)N;
				float w1 = (float)gi / (float)N;
				float w2 = (float)gj / (float)N;

				auto cp  = BaryTri(p[0],   p[1],   p[2],   w0, w1, w2);
				auto cuv = BaryTri(uv[0],  uv[1],  uv[2],  w0, w1, w2);
				auto cn  = BaryTri(nrm[0], nrm[1], nrm[2], w0, w1, w2); cn.Normalize();
				auto ct  = BaryTri(tan[0], tan[1], tan[2], w0, w1, w2); ct.Normalize();
				auto cc  = BaryTri(col[0], col[1], col[2], w0, w1, w2);
				auto ce  = BaryTri(eff[0], eff[1], eff[2], w0, w1, w2);

				float liftScale = GetSnowLiftScale(poly, room, cp, lift);

				EmitSnowVertex(vertices[vertCursor], cp, cuv, cn, ct, cc, ce,
							   poly.normal, room, lift, poly.animatedFrame, poly.shineStrength, kSlot, liftScale);
				outBase = vertCursor;
				vertCursor++;
			};

			for (int i = 0; i < N; i++)
			{
				for (int j = 0; j < N - i; j++)
				{
					// Upward sub-triangle: corners (i, j), (i+1, j), (i, j+1).
					int b0, b1, b2;
					sampleAt(i,     j,     0, b0);
					sampleAt(i + 1, j,     1, b1);
					sampleAt(i,     j + 1, 2, b2);

					RendererPolygon up{};
					up.Shape	 = 1;
					up.Normal	 = poly.normal;
					up.Centre	 = (vertices[b0].Position + vertices[b1].Position + vertices[b2].Position) / 3.0f;
					up.BaseIndex = indexCursor;

					indices[indexCursor + 0] = b0;
					indices[indexCursor + 1] = b1;
					indices[indexCursor + 2] = b2;
					indexCursor += 3;

					outPolys.push_back(up);

					if (i + j + 1 < N)
					{
						// Downward sub-triangle: corners (i+1, j), (i+1, j+1), (i, j+1).
						int d0, d1, d2;
						sampleAt(i + 1, j,     0, d0);
						sampleAt(i + 1, j + 1, 1, d1);
						sampleAt(i,     j + 1, 2, d2);

						RendererPolygon down{};
						down.Shape	   = 1;
						down.Normal	   = poly.normal;
						down.Centre	   = (vertices[d0].Position + vertices[d1].Position + vertices[d2].Position) / 3.0f;
						down.BaseIndex = indexCursor;

						indices[indexCursor + 0] = d0;
						indices[indexCursor + 1] = d1;
						indices[indexCursor + 2] = d2;
						indexCursor += 3;

						outPolys.push_back(down);
					}
				}
			}
		}
	}

	bool Renderer::PrepareDataForTheRenderer()
	{
		TENLog("Preparing renderer...", LogLevel::Info);

		_skinVertexBackups.clear();
		_lastBlendMode = BlendMode::Unknown;
		_lastCullMode = CullMode::Unknown;
		_lastDepthState = DepthState::Unknown;
		_lastMaterialIndex = NO_VALUE;

		_moveableObjects.resize(ID_NUMBER_OBJECTS);
		_spriteSequences.resize(ID_NUMBER_OBJECTS);
		_rooms.resize(g_Level.Rooms.size());

		_meshes.clear();

		_dynamicLightList = 0;
		for (auto& dynamicLightList : _dynamicLights)
			dynamicLightList.clear();

		int allocatedItemSize = (int)g_Level.Items.size() + MAX_SPAWNED_ITEM_COUNT;

		auto item = RendererItem();
		_items = std::vector<RendererItem>(allocatedItemSize, item);

		auto effect = RendererEffect();
		_effects = std::vector<RendererEffect>(allocatedItemSize, effect);
		
		auto emptyNormalMap = std::vector<unsigned char>{ 128, 128, 255, 255 };
		auto emptyORSHMap = std::vector<unsigned char>{ 255, 255, 0, 255 };
		auto emptyEmissiveMap = std::vector<unsigned char>{ 0, 0, 0, 0 };

		TENLog("Allocated renderer object memory.", LogLevel::Info);

		_animatedTextures.resize(g_Level.AnimatedTextures.size());
		for (int i = 0; i < g_Level.AnimatedTextures.size(); i++)
		{
			TEXTURE* texture = &g_Level.AnimatedTextures[i];
			
			std::unique_ptr<ITexture2D> color = _graphicsDevice->CreateTexture2DFromFileInMemory(
				(int)texture->colorMapData.size(), texture->colorMapData.data());

			std::unique_ptr<ITexture2D> normal;
			if (texture->normalMapData.size() < 1)
			{
				normal = CreateDefaultTexture(emptyNormalMap);
			}
			else
			{
				normal = _graphicsDevice->CreateTexture2DFromFileInMemory((int)texture->normalMapData.size(), texture->normalMapData.data());
			}

			std::unique_ptr<ITexture2D> ORSH;
			if (texture->ORSHMapData.size() < 1)
			{
				ORSH = CreateDefaultTexture(emptyORSHMap);
			}
			else
			{
				ORSH = _graphicsDevice->CreateTexture2DFromFileInMemory((int)texture->ORSHMapData.size(), texture->ORSHMapData.data());
			}

			std::unique_ptr<ITexture2D> emissive;
			if (texture->emissiveMapData.size() < 1)
			{
				emissive = CreateDefaultTexture(emptyEmissiveMap);
			}
			else
			{
				emissive = _graphicsDevice->CreateTexture2DFromFileInMemory((int)texture->emissiveMapData.size(), texture->emissiveMapData.data());
			}

			AtlasTexturesSet tex = std::make_tuple(
				std::move(color),
				std::move(normal),
				std::move(ORSH),
				std::move(emissive));

			_animatedTextures[i] = std::move(tex);
		}

		std::transform(g_Level.AnimatedTexturesSequences.begin(), g_Level.AnimatedTexturesSequences.end(), std::back_inserter(_animatedTextureSets), [](ANIMATED_TEXTURES_SEQUENCE& sequence)
		{  
			RendererAnimatedTextureSet set{};

			set.NumTextures = sequence.NumFrames;
			set.Type = (AnimatedTextureType)sequence.Type;
			set.Fps = sequence.Fps;
			set.UVRotateSpeed = sequence.UVRotateSpeed;
			set.UVRotateDirection = sequence.UVRotateDirection;

			std::transform(sequence.Frames.begin(), sequence.Frames.end(), std::back_inserter(set.Textures), [](ANIMATED_TEXTURES_FRAME& frm)
			{
				RendererAnimatedTexture tex{};

				tex.UV[0].x = frm.x1;
				tex.UV[0].y = frm.y1;
				tex.UV[1].x = frm.x2;
				tex.UV[1].y = frm.y2;
				tex.UV[2].x = frm.x3;
				tex.UV[2].y = frm.y3;
				tex.UV[3].x = frm.x4;
				tex.UV[3].y = frm.y4;

				float UMin = std::min({ tex.UV[0].x, tex.UV[1].x, tex.UV[2].x, tex.UV[3].x });
				float VMin = std::min({ tex.UV[0].y, tex.UV[1].y, tex.UV[2].y, tex.UV[3].y });
				float UMax = std::max({ tex.UV[0].x, tex.UV[1].x, tex.UV[2].x, tex.UV[3].x });
				float VMax = std::max({ tex.UV[0].y, tex.UV[1].y, tex.UV[2].y, tex.UV[3].y });

				for (int i = 0; i < 4; ++i)
				{
					tex.NormalizedUV[i].x = round((tex.UV[i].x - UMin) / (UMax - UMin));
					tex.NormalizedUV[i].y = round((tex.UV[i].y - VMin) / (VMax - VMin));
				}

				return tex;
			});

			return set;
		});

		if (_animatedTextureSets.size() > 0)
			TENLog("Generated " + std::to_string(_animatedTextureSets.size()) + " animated texture sets.", LogLevel::Info);

		_roomTextures.resize(g_Level.RoomTextures.size());
		for (int i = 0; i < g_Level.RoomTextures.size(); i++)
		{
			TEXTURE* texture = &g_Level.RoomTextures[i];

			std::unique_ptr<ITexture2D> color = _graphicsDevice->CreateTexture2DFromFileInMemory(
				(int)texture->colorMapData.size(), texture->colorMapData.data());

				std::unique_ptr<ITexture2D> normal;
			if (texture->normalMapData.size() < 1)
			{
				normal = CreateDefaultTexture(emptyNormalMap);
			}
			else
			{
				normal = _graphicsDevice->CreateTexture2DFromFileInMemory((int)texture->normalMapData.size(), texture->normalMapData.data());
			}

			std::unique_ptr<ITexture2D> ORSH;
			if (texture->ORSHMapData.size() < 1)
			{
				ORSH = CreateDefaultTexture(emptyORSHMap);
			}
			else
			{
				ORSH = _graphicsDevice->CreateTexture2DFromFileInMemory((int)texture->ORSHMapData.size(), texture->ORSHMapData.data());
			}

			std::unique_ptr<ITexture2D> emissive;
			if (texture->emissiveMapData.size() < 1)
			{
				emissive = CreateDefaultTexture(emptyEmissiveMap);
			}
			else
			{
				emissive = _graphicsDevice->CreateTexture2DFromFileInMemory((int)texture->emissiveMapData.size(), texture->emissiveMapData.data());
			}

			AtlasTexturesSet tex = std::make_tuple(
				std::move(color),
				std::move(normal),
				std::move(ORSH),
				std::move(emissive));
			
			_roomTextures[i] = std::move(tex);

#ifdef DUMP_TEXTURES
			char filename[255];
			sprintf(filename, "dump/room_%d.png", i);

			std::ofstream outfile(std::filesystem::path{filename}, std::ios::out | std::ios::binary);
			outfile.write(reinterpret_cast<const char*>(texture->colorMapData.data()), texture->colorMapData.size());
#endif
		}

		if (_roomTextures.size() > 0)
			TENLog("Generated " + std::to_string(_roomTextures.size()) + " room texture atlases.", LogLevel::Info);

		_moveablesTextures.resize(g_Level.MoveablesTextures.size());
		for (int i = 0; i < g_Level.MoveablesTextures.size(); i++)
		{
			TEXTURE* texture = &g_Level.MoveablesTextures[i];

			std::unique_ptr<ITexture2D> color = _graphicsDevice->CreateTexture2DFromFileInMemory(
				(int)texture->colorMapData.size(), texture->colorMapData.data());

				std::unique_ptr<ITexture2D> normal;
			if (texture->normalMapData.size() < 1)
			{
				normal = CreateDefaultTexture(emptyNormalMap);
			}
			else
			{
				normal = _graphicsDevice->CreateTexture2DFromFileInMemory((int)texture->normalMapData.size(), texture->normalMapData.data());
			}

			std::unique_ptr<ITexture2D> ORSH;
			if (texture->ORSHMapData.size() < 1)
			{
				ORSH = CreateDefaultTexture(emptyORSHMap);
			}
			else
			{
				ORSH = _graphicsDevice->CreateTexture2DFromFileInMemory((int)texture->ORSHMapData.size(), texture->ORSHMapData.data());
			}

			std::unique_ptr<ITexture2D> emissive;
			if (texture->emissiveMapData.size() < 1)
			{
				emissive = CreateDefaultTexture(emptyEmissiveMap);
			}
			else
			{
				emissive = _graphicsDevice->CreateTexture2DFromFileInMemory((int)texture->emissiveMapData.size(), texture->emissiveMapData.data());
			}

			AtlasTexturesSet tex = std::make_tuple(
				std::move(color),
				std::move(normal),
				std::move(ORSH),
				std::move(emissive));

			_moveablesTextures[i] = std::move(tex);

#ifdef DUMP_TEXTURES
			char filename[255];
			sprintf(filename, "dump/moveable_%d.png", i);

			std::ofstream outfile(std::filesystem::path{filename}, std::ios::out | std::ios::binary);
			outfile.write(reinterpret_cast<const char*>(texture->colorMapData.data()), texture->colorMapData.size());
#endif
		}

		if (_moveablesTextures.size() > 0)
			TENLog("Generated " + std::to_string(_moveablesTextures.size()) + " moveable texture atlases.", LogLevel::Info);

		_staticTextures.resize(g_Level.StaticsTextures.size());
		for (int i = 0; i < g_Level.StaticsTextures.size(); i++)
		{
			TEXTURE* texture = &g_Level.StaticsTextures[i];

			std::unique_ptr<ITexture2D> color = _graphicsDevice->CreateTexture2DFromFileInMemory(
				(int)texture->colorMapData.size(), texture->colorMapData.data());

			std::unique_ptr<ITexture2D> normal;
			if (texture->normalMapData.size() < 1)
			{
				normal = CreateDefaultTexture(emptyNormalMap);
			}
			else
			{
				normal = _graphicsDevice->CreateTexture2DFromFileInMemory((int)texture->normalMapData.size(), texture->normalMapData.data());
			}

			std::unique_ptr<ITexture2D> ORSH;
			if (texture->ORSHMapData.size() < 1)
			{
				ORSH = CreateDefaultTexture(emptyORSHMap);
			}
			else
			{
				ORSH = _graphicsDevice->CreateTexture2DFromFileInMemory((int)texture->ORSHMapData.size(), texture->ORSHMapData.data());
			}

			std::unique_ptr<ITexture2D> emissive;
			if (texture->emissiveMapData.size() < 1)
			{
				emissive = CreateDefaultTexture(emptyEmissiveMap);
			}
			else
			{
				emissive = _graphicsDevice->CreateTexture2DFromFileInMemory((int)texture->emissiveMapData.size(), texture->emissiveMapData.data());
			}

			AtlasTexturesSet tex = std::make_tuple(
				std::move(color),
				std::move(normal),
				std::move(ORSH),
				std::move(emissive));
			
			_staticTextures[i] = std::move(tex);

#ifdef DUMP_TEXTURES
			char filename[255];
			sprintf(filename, "dump/static_%d.png", i);

			std::ofstream outfile(std::filesystem::path{filename}, std::ios::out | std::ios::binary);
			outfile.write(reinterpret_cast<const char*>(texture->colorMapData.data()), texture->colorMapData.size());
#endif
		}

		if (_staticTextures.size() > 0)
			TENLog("Generated " + std::to_string(_staticTextures.size()) + " static mesh texture atlases.", LogLevel::Info);

		_spritesTextures.resize(g_Level.SpritesTextures.size());
		for (int i = 0; i < g_Level.SpritesTextures.size(); i++)
		{
			auto& texture = g_Level.SpritesTextures[i];
			_spritesTextures[i] = _graphicsDevice->CreateTexture2DFromFileInMemory((int)texture.colorMapData.size(), texture.colorMapData.data());
		}

		if (_spritesTextures.size() > 0)
			TENLog("Generated " + std::to_string((int)_spritesTextures.size()) + " sprite atlases.", LogLevel::Info);

		_skyTexture = _graphicsDevice->CreateTexture2DFromFileInMemory((int)g_Level.SkyTexture.colorMapData.size(), g_Level.SkyTexture.colorMapData.data());

		TENLog("Loaded sky texture.", LogLevel::Info);

		int totalVertices = 0;
		int totalIndices = 0;
		for (auto& room : g_Level.Rooms)
			for (auto& bucket : room.buckets)
			{ 
				totalVertices += bucket.numQuads * 4 + bucket.numTriangles * 3;
				totalIndices += bucket.numQuads * 6 + bucket.numTriangles * 3;
			}

		// Snow overlay (Phase 2): pre-count extra vertices and indices that snow-flagged
		// floor polygons will generate, so the upcoming single immutable buffer upload
		// can fit them in one shot.
		const auto& snowSettings = g_GameFlow->GetSettings()->Snow;
		int snowSubdivisions = std::clamp(snowSettings.Subdivisions, 1, 64);
		if (snowSettings.Enabled)
		{
			for (auto& room : g_Level.Rooms)
			{
				if (room.positions.empty())
					continue;

				for (auto& bucket : room.buckets)
				{
					for (auto& poly : bucket.polygons)
					{
						if (!IsSnowFloorPolygon(poly, room))
							continue;

						int extraVerts = 0;
						int extraIndices = 0;
						GetSnowOverlayCounts(poly, snowSubdivisions, extraVerts, extraIndices);

						// Subdivided skirts that bridge adjacent sectors at different floor heights.
						int skirtEdges = CountSnowSkirtEdges(poly, room);
						int skirtVerts = 0, skirtIndices = 0;
						GetSnowSkirtCounts(skirtEdges, snowSubdivisions, skirtVerts, skirtIndices);
						extraVerts   += skirtVerts;
						extraIndices += skirtIndices;

						totalVertices += extraVerts;
						totalIndices += extraIndices;
					}
				}
			}
		}

		if (!totalVertices || !totalIndices)
			throw std::exception("Level has no textured room geometry.");

		_roomsVertices.resize(totalVertices);
		_roomsIndices.resize(totalIndices);

		TENLog("Loaded total " + std::to_string(totalVertices) + " room vertices.", LogLevel::Info);

		int lastVertex = 0;
		int lastIndex = 0;

		TENLog("Preparing room data...", LogLevel::Info);

		for (int i = 0; i < g_Level.Rooms.size(); i++)
		{
			auto& room = g_Level.Rooms[i];
			auto& rendererRoom = _rooms[i];

			rendererRoom.RoomNumber = i;
			rendererRoom.AmbientLight = Vector4(room.ambient.x, room.ambient.y, room.ambient.z, 1.0f);
			rendererRoom.ItemsToDraw.reserve(MAX_ITEMS_DRAW);
			rendererRoom.EffectsToDraw.reserve(MAX_ITEMS_DRAW);
			rendererRoom.Decals.reserve(Decal::COUNT_MAX);

			auto boxMin = Vector3(room.Position.x + BLOCK(1), room.TopHeight - CLICK(1), room.Position.z + BLOCK(1));
			auto boxMax = Vector3(room.Position.x + (room.XSize - 1) * BLOCK(1), room.BottomHeight + CLICK(1), room.Position.z + (room.ZSize - 1) * BLOCK(1));
			auto center = (boxMin + boxMax) / 2.0f;
			auto extents = boxMax - center;
			rendererRoom.BoundingBox = BoundingBox(center, extents);

			rendererRoom.Neighbors.clear();
			for (int j : room.NeighborRoomNumbers)
			{
				if (g_Level.Rooms[j].Active())
					rendererRoom.Neighbors.push_back(j);
			}

			if (!room.Portals.empty())
			{
				rendererRoom.Doors.resize((int)room.Portals.size());

				for (int j = 0; j < room.Portals.size(); j++)
				{
					const auto& portal = room.Portals[j];
					auto& rendererDoor = rendererRoom.Doors[j];

					rendererDoor.RoomNumber = portal.RoomNumber;
					rendererDoor.Normal = portal.Normal;

					for (int k = 0; k < 4; k++)
					{
						rendererDoor.AbsoluteVertices[k] = Vector4(
							room.Position.x + portal.Vertices[k].x,
							room.Position.y + portal.Vertices[k].y,
							room.Position.z + portal.Vertices[k].z,
							1.0f);
					}
				}
			}

			if (room.mesh.size() != 0)
			{
				rendererRoom.Statics.resize(room.mesh.size());

				for (int l = 0; l < (int)room.mesh.size(); l++)
				{
					auto& rendererStatic = rendererRoom.Statics[l];
					auto& nativeStatic = room.mesh[l];

					nativeStatic.Dirty = true;

					rendererStatic.ObjectNumber = nativeStatic.Slot;
					rendererStatic.RoomNumber = nativeStatic.RoomNumber;
					rendererStatic.Color = nativeStatic.Color;
					rendererStatic.AmbientLight = rendererRoom.AmbientLight;
					rendererStatic.Pose =
					rendererStatic.PrevPose = nativeStatic.Pose;
					rendererStatic.OriginalSphere = Statics[rendererStatic.ObjectNumber].visibilityBox.ToLocalBoundingSphere();
					rendererStatic.IndexInRoom = l;

					rendererStatic.Update(GetInterpolationFactor());
				}
			}

			if (room.positions.empty())
				continue;
			
			for (auto& levelBucket : room.buckets)
			{
				RendererBucket bucket{};

				bucket.Animated = levelBucket.animated;
				bucket.BlendMode = static_cast<BlendMode>(levelBucket.blendMode);
				bucket.MaterialIndex = levelBucket.materialIndex;
				bucket.Texture = levelBucket.texture;
				bucket.StartVertex = lastVertex;
				bucket.StartIndex = lastIndex;
				bucket.NumVertices += levelBucket.numQuads * 4 + levelBucket.numTriangles * 3;
				bucket.NumIndices += levelBucket.numQuads * 6 + levelBucket.numTriangles * 3;
				bucket.Centre = Vector3::Zero;

				for (auto& poly : levelBucket.polygons)
				{
					RendererPolygon newPoly;

					newPoly.Shape = poly.shape;

					newPoly.Centre = (
						room.positions[poly.indices[0]] +
						room.positions[poly.indices[1]] +
						room.positions[poly.indices[2]]) / 3.0f;

					newPoly.Centre += room.Position.ToVector3();

					Vector3 p1 = room.positions[poly.indices[0]];
					Vector3 p2 = room.positions[poly.indices[1]];
					Vector3 p3 = room.positions[poly.indices[2]];

					Vector3 n = (p2 - p1).Cross(p3 - p1);
					n.Normalize();

					newPoly.Normal = n;

					int baseVertices = lastVertex;
					for (int k = 0; k < poly.indices.size(); k++)
					{
						Vertex* vertex = &_roomsVertices[lastVertex];
						int index = poly.indices[k];

						vertex->Position.x = room.Position.x + room.positions[index].x;
						vertex->Position.y = room.Position.y + room.positions[index].y;
						vertex->Position.z = room.Position.z + room.positions[index].z;

						bucket.Centre += vertex->Position;

						vertex->Normal = PackVector3(poly.normals[k]);
						vertex->UV = poly.textureCoordinates[k];
						vertex->Color = VectorColorToRGBA(Vector4(room.colors[index].x, room.colors[index].y, room.colors[index].z, 1.0f));
						vertex->Tangent = PackVector3(poly.tangents[k]);
						vertex->FaceNormal = PackVector3(poly.normal);

						const unsigned long long primes[]{ 73856093ULL, 19349663ULL, 83492791ULL };
						unsigned int hash = (unsigned int)std::hash<float>{}
						((vertex->Position.x) * primes[0]) ^
							((unsigned int)std::hash<float>{}(vertex->Position.y)* primes[1]) ^
							(unsigned int)std::hash<float>{}(vertex->Position.z)* primes[2];

						vertex->AnimationFrameOffsetIndexHash = PackAnimationFrameOffsetIndexHash(poly.animatedFrame, index, hash);
						vertex->Effects = PackEffectsAndIndexInPoly(room.effects[index], poly.shineStrength, k);
						
						lastVertex++;
					}

					if (poly.shape == 0)
					{
						newPoly.BaseIndex = lastIndex;

						_roomsIndices[lastIndex + 0] = baseVertices + 0;
						_roomsIndices[lastIndex + 1] = baseVertices + 1;
						_roomsIndices[lastIndex + 2] = baseVertices + 3;
						_roomsIndices[lastIndex + 3] = baseVertices + 2;
						_roomsIndices[lastIndex + 4] = baseVertices + 3;
						_roomsIndices[lastIndex + 5] = baseVertices + 1;

						lastIndex += 6;
					}
					else
					{
						newPoly.BaseIndex = lastIndex;
 
						_roomsIndices[lastIndex + 0] = baseVertices + 0;
						_roomsIndices[lastIndex + 1] = baseVertices + 1;
						_roomsIndices[lastIndex + 2] = baseVertices + 2;

						lastIndex += 3;
					}

					bucket.Polygons.push_back(newPoly);
				}

				bucket.Centre /= bucket.NumIndices;

				rendererRoom.Buckets.push_back(bucket);		
			}

			// Snow overlay (Phase 2): for every floor polygon sitting on a snow sector,
			// emit a subdivided overlay bucket inheriting the parent's texture/material.
			// These buckets are marked IsSnowOverlay and skipped by the regular room draw
			// pass; a dedicated snow shader pass (Phase 4) consumes them.
			//
			// Polygons are grouped by OWNER room (the room volume sitting above the
			// snow surface) rather than by the room whose geometry the polygon belongs
			// to. This matters whenever a snow polygon sits at a portal boundary --
			// e.g. the top of a wall in the lower room, or an upper-room ramp where
			// some sub-polys remain at portal height. In every case the overlay must
			// be pushed into the OWNER room's bucket list so it inherits that room's
			// ambient color, dynamic lights and fog state in DrawSnowOverlay.
			if (snowSettings.Enabled)
			{
				float snowLift = (float)std::max(0, snowSettings.MaxDepth)
					+ std::max(0.0f, snowSettings.HillHeight);

				for (auto& levelBucket : room.buckets)
				{
					// Group polygon indices by owner room number for this bucket.
					std::unordered_map<int, std::vector<int>> polysByOwner;
					for (int p = 0; p < (int)levelBucket.polygons.size(); p++)
					{
						auto* ownerRoomPtr = FindSnowOwnerRoom(levelBucket.polygons[p], room);
						if (ownerRoomPtr == nullptr)
							continue;
						int ownerIdx = (int)(ownerRoomPtr - g_Level.Rooms.data());
						polysByOwner[ownerIdx].push_back(p);
					}

					for (auto& kv : polysByOwner)
					{
						int ownerIdx = kv.first;
						auto& polyIdxs = kv.second;

						RendererBucket overlay{};
						overlay.Animated	  = levelBucket.animated;
						overlay.BlendMode	  = (BlendMode)levelBucket.blendMode;
						overlay.MaterialIndex = levelBucket.materialIndex;
						overlay.Texture		  = levelBucket.texture;
						overlay.StartVertex	  = lastVertex;
						overlay.StartIndex	  = lastIndex;
						overlay.IsSnowOverlay = true;
						overlay.Centre		  = Vector3::Zero;

						int startVertex = lastVertex;
						int startIndex	= lastIndex;

						// When the polygon's geometry belongs to a DIFFERENT room than the
						// snow owner, the source room's baked per-vertex colors reflect that
						// room's lighting (e.g. a dark cave below) and would clash with the
						// owner room's ambient. Replace per-vertex color with the owner's
						// flat ambient so DoModulateColor() in SnowOverlay.hlsl yields the
						// correct base lighting before dynamic lights are added on top.
						const auto& ownerRoom = g_Level.Rooms[ownerIdx];
						Vector3 ownerAmbient(ownerRoom.ambient.x, ownerRoom.ambient.y, ownerRoom.ambient.z);
						const Vector3* colorOverride = (ownerIdx != i) ? &ownerAmbient : nullptr;

						for (int p : polyIdxs)
						{
							auto& poly = levelBucket.polygons[p];

							EmitSnowOverlayPolygon(
								poly, room, snowSubdivisions, snowLift,
								_roomsVertices, _roomsIndices,
								lastVertex, lastIndex,
								overlay.Polygons,
								colorOverride);

							EmitSnowSkirtsForPolygon(
								poly, room, snowSubdivisions, snowLift,
								_roomsVertices, _roomsIndices,
								lastVertex, lastIndex,
								overlay.Polygons,
								colorOverride);
						}

						overlay.NumVertices = lastVertex - startVertex;
						overlay.NumIndices	= lastIndex - startIndex;

						if (overlay.NumVertices == 0)
							continue;

						// Compute centre from emitted vertices.
						Vector3 sum = Vector3::Zero;
						for (int v = startVertex; v < lastVertex; v++)
							sum += _roomsVertices[v].Position;
						overlay.Centre = sum / (float)overlay.NumVertices;

						_rooms[ownerIdx].Buckets.push_back(overlay);
					}
				}
			}

			if (room.lights.size() != 0)
			{
				rendererRoom.Lights.resize(room.lights.size());

				for (int l = 0; l < room.lights.size(); l++)
				{
					RendererLight* light = &rendererRoom.Lights[l];
					RoomLightData* oldLight = &room.lights[l];

					if (oldLight->type == 0)
					{
						light->Color = Vector3(oldLight->r, oldLight->g, oldLight->b) * oldLight->intensity;
						light->Intensity = oldLight->intensity;
						light->Direction = Vector3(oldLight->dx, oldLight->dy, oldLight->dz);
						light->CastShadows = oldLight->castShadows;
						light->Type = LightType::Sun;
						light->Luma = Luma(light->Color);
					}
					else if (oldLight->type == 1)
					{
						light->Position = Vector3(oldLight->x, oldLight->y, oldLight->z);
						light->Color = Vector3(oldLight->r, oldLight->g, oldLight->b) * oldLight->intensity;
						light->Intensity = oldLight->intensity;
						light->In = oldLight->in;
						light->Out = oldLight->out;
						light->CastShadows = oldLight->castShadows;
						light->Type = LightType::Point;
						light->Luma = Luma(light->Color);
					}
					else if (oldLight->type == 3)
					{
						light->Position = Vector3(oldLight->x, oldLight->y, oldLight->z);
						light->Color = Vector3(oldLight->r, oldLight->g, oldLight->b) * oldLight->intensity;
						light->Intensity = oldLight->intensity;
						light->In = oldLight->in;
						light->Out = oldLight->out;
						light->CastShadows = false;
						light->Type = LightType::Shadow;
						light->Luma = Luma(light->Color);
					}
					else if (oldLight->type == 2)
					{
						light->Position = Vector3(oldLight->x, oldLight->y, oldLight->z);
						light->Color = Vector3(oldLight->r, oldLight->g, oldLight->b) * oldLight->intensity;
						light->Intensity = oldLight->intensity;
						light->Direction = Vector3(oldLight->dx, oldLight->dy, oldLight->dz);
						light->In = oldLight->length;     
						light->Out = oldLight->cutoff;
						light->InRange = oldLight->in;
						light->OutRange = oldLight->out;
						light->CastShadows = oldLight->castShadows;
						light->Type = LightType::Spot;
						light->Luma = Luma(light->Color);
					}
					else if (oldLight->type == 4)
					{  
						light->Position = Vector3(oldLight->x, oldLight->y, oldLight->z);
						light->Color = Vector3(oldLight->r, oldLight->g, oldLight->b);
						light->Intensity = oldLight->intensity;
						light->In = oldLight->in;
						light->Out = oldLight->out;
						light->Type = LightType::FogBulb;
						light->Luma = Luma(light->Color);
					} 

					// Monty's temp variables for sorting
					light->LocalIntensity = 0;
					light->Distance = 0;
					light->RoomNumber = i;
					light->AffectNeighbourRooms = light->Type != LightType::Sun;

					oldLight++;
				}
			}
		}
		_roomsVertexBuffer = _graphicsDevice->CreateVertexBuffer((int)_roomsVertices.size(), sizeof(Vertex), _roomsVertices.data());
		_roomsIndexBuffer = _graphicsDevice->CreateIndexBuffer((int)_roomsIndices.size(), _roomsIndices.data());

		std::for_each(std::execution::par_unseq,
			_rooms.begin(),
			_rooms.end(),
			[](RendererRoom& room)
			{
				std::sort(
					room.Buckets.begin(),
					room.Buckets.end(),
					[](RendererBucket& a, RendererBucket& b)
					{
						if (a.BlendMode == b.BlendMode)
							return (a.Texture < b.Texture);
						else
							return (a.BlendMode < b.BlendMode);
					}
				);
			}
		);

		TENLog("Preparing object data...", LogLevel::Info);
			 
		bool isSkinPresent = false;

		totalVertices = 0;
		totalIndices = 0;
		for (int i = 0; i < MoveablesIds.size(); i++)
		{
			int objNum = MoveablesIds[i];
			ObjectInfo* obj = &Objects[objNum];

			int meshCount = (obj->skinIndex == NO_VALUE) ? obj->nmeshes : obj->nmeshes + 1;
			for (int j = 0; j < meshCount; j++)
			{
				MESH* mesh = &g_Level.Meshes[obj->meshIndex + j];

				for (auto& bucket : mesh->buckets)
				{
					totalVertices += bucket.numQuads * 4 + bucket.numTriangles * 3;
					totalIndices += bucket.numQuads * 6 + bucket.numTriangles * 3;
				}
			}
		}
		_moveablesVertices.resize(totalVertices);
		_moveablesIndices.resize(totalIndices);

		lastVertex = 0;
		lastIndex = 0;
		for (int i = 0; i < MoveablesIds.size(); i++)
		{
			int objNum = MoveablesIds[i];
			ObjectInfo *obj = &Objects[objNum];

			if (obj->nmeshes > 0)
			{
				_moveableObjects[MoveablesIds[i]] = RendererObject();
				RendererObject &moveable = *_moveableObjects[MoveablesIds[i]];
				moveable.Id = MoveablesIds[i];
				moveable.Hidden = obj->Hidden;
				moveable.ShadowType = obj->shadowType;
													   
				for (int j = 0; j < obj->nmeshes; j++)
				{              
					// HACK: mesh pointer 0 is the placeholder for Lara's body parts and is right hand with pistols
					// We need to override the bone index because the engine will take mesh 0 while drawing pistols anim,
					// and vertices have bone index 0 and not 10.
					auto* mesh = GetRendererMeshFromTrMesh(
						&moveable,
						&g_Level.Meshes[obj->meshIndex + j],
						j, MoveablesIds[i] == ID_LARA_SKIN_JOINTS,
						MoveablesIds[i] == ID_HAIR_PRIMARY || MoveablesIds[i] == ID_HAIR_SECONDARY, &lastVertex, &lastIndex);

					moveable.ObjectMeshes.push_back(mesh);
					_meshes.push_back(mesh);
				}

				if (obj->skinIndex != NO_VALUE)
				{
					auto* mesh = GetRendererMeshFromTrMesh(&moveable, &g_Level.Meshes[obj->skinIndex], 0, false, false, &lastVertex, &lastIndex);
					_meshes.push_back(mesh);
				}

				if (objNum == ID_IMP_ROCK || objNum == ID_ENERGY_BUBBLES || objNum == ID_BUBBLES || objNum == ID_BODY_PART)
				{
					// HACK: these objects must have nmeshes = 0 because engine will use them in a different way while drawing Effects.
					// In Core's code this was done in SETUP.C but we must do it here because we need to create renderer's meshes.
					obj->nmeshes = 0;
				}
				else
				{
					for (int j = 0; j < obj->nmeshes; j++)
					{
						moveable.LinearizedBones.push_back(new RendererBone(j));
						moveable.AnimationTransforms.push_back(Matrix::Identity);
						moveable.BindPoseTransforms.push_back(Matrix::Identity);
					}

					if (obj->nmeshes > 1)
					{
						int *bone = &g_Level.Bones[obj->boneIndex];

						std::stack<RendererBone *> stack;

						RendererBone *currentBone = moveable.LinearizedBones[0];
						RendererBone *stackBone = moveable.LinearizedBones[0];

						for (int mi = 0; mi < obj->nmeshes - 1; mi++)
						{
							int j = mi + 1;

							int opcode = *(bone++);
							int linkX = *(bone++);
							int linkY = *(bone++);
							int linkZ = *(bone++);

							byte flags = opcode & 0x1C;

							moveable.LinearizedBones[j]->ExtraRotationFlags = flags;

							switch (opcode & 0x03)
							{
							case 0:
								moveable.LinearizedBones[j]->Parent = currentBone;
								moveable.LinearizedBones[j]->Translation = Vector3(linkX, linkY, linkZ);
								currentBone->Children.push_back(moveable.LinearizedBones[j]);
								currentBone = moveable.LinearizedBones[j];
								break;

							case 1:
								if (stack.empty())
									continue;

								currentBone = stack.top();
								stack.pop();

								moveable.LinearizedBones[j]->Parent = currentBone;
								moveable.LinearizedBones[j]->Translation = Vector3(linkX, linkY, linkZ);
								currentBone->Children.push_back(moveable.LinearizedBones[j]);
								currentBone = moveable.LinearizedBones[j];
								break;

							case 2:
								stack.push(currentBone);

								moveable.LinearizedBones[j]->Translation = Vector3(linkX, linkY, linkZ);
								moveable.LinearizedBones[j]->Parent = currentBone;
								currentBone->Children.push_back(moveable.LinearizedBones[j]);
								currentBone = moveable.LinearizedBones[j];
								break;

							case 3:
								if (stack.empty())
									continue;

								RendererBone *theBone = stack.top();
								stack.pop();

								moveable.LinearizedBones[j]->Translation = Vector3(linkX, linkY, linkZ);
								moveable.LinearizedBones[j]->Parent = theBone;
								theBone->Children.push_back(moveable.LinearizedBones[j]);
								currentBone = moveable.LinearizedBones[j];
								stack.push(theBone);
								break;
							}
						}
					}

					for (int n = 0; n < obj->nmeshes; n++)
					{
						moveable.LinearizedBones[n]->Transform = Matrix::CreateTranslation(
							moveable.LinearizedBones[n]->Translation.x,
							moveable.LinearizedBones[n]->Translation.y,
							moveable.LinearizedBones[n]->Translation.z);
					}

					moveable.Skeleton = moveable.LinearizedBones[0];
					BuildHierarchy(&moveable);

					// Fix player skin joints and hair units.
					if (MoveablesIds[i] == ID_LARA_SKIN_JOINTS)
					{
						BackupObjectVertices(ID_LARA_SKIN_JOINTS);
						isSkinPresent = true;

						auto& jointsMoveable = moveable;
						auto& skinMoveable = GetRendererObject(GAME_OBJECT_ID::ID_LARA_SKIN);
						ProcessSkinJoints(jointsMoveable, const_cast<RendererObject&>(skinMoveable), *obj);
					}
					else if ((MoveablesIds[i] == ID_HAIR_PRIMARY || MoveablesIds[i] == ID_HAIR_SECONDARY) && isSkinPresent)
					{
						BackupObjectVertices((GAME_OBJECT_ID)MoveablesIds[i]);
						bool isYoung = (g_GameFlow->GetLevel(CurrentLevel)->GetLaraType() == LaraType::Young);
						bool isSecond = isYoung && MoveablesIds[i] == ID_HAIR_SECONDARY;
						auto& skinMoveable = GetRendererObject(GAME_OBJECT_ID::ID_LARA_SKIN);
						ProcessHair((GAME_OBJECT_ID)MoveablesIds[i], const_cast<RendererObject&>(skinMoveable), isSecond);
					}
				}
			}
		}

		_moveablesVertexBuffer = _graphicsDevice->CreateVertexBuffer((int)_moveablesVertices.size(), sizeof(Vertex), _moveablesVertices.data());
		_moveablesIndexBuffer = _graphicsDevice->CreateIndexBuffer((int)_moveablesIndices.size(), _moveablesIndices.data());

		TENLog("Preparing static mesh data...", LogLevel::Info);

		totalVertices = 0;
		totalIndices = 0;
		for (const auto& staticObj : Statics)
		{
			const auto& mesh = g_Level.Meshes[staticObj.meshNumber];
			for (const auto& bucket : mesh.buckets)
			{
				totalVertices += (bucket.numQuads * 4) + (bucket.numTriangles * 3);
				totalIndices += (bucket.numQuads * 6) + (bucket.numTriangles * 3);
			}
		}

		_staticsVertices.resize(totalVertices);
		_staticsIndices.resize(totalIndices);

		lastVertex = 0;
		lastIndex = 0;
		for (const auto& staticObj : Statics)
		{
			auto newStaticObj = RendererObject();
			newStaticObj.Type = 1;
			newStaticObj.Id = staticObj.ObjectNumber;

			auto& mesh = *GetRendererMeshFromTrMesh(&newStaticObj, &g_Level.Meshes[staticObj.meshNumber], 0, false, false, &lastVertex, &lastIndex);

			newStaticObj.ObjectMeshes.push_back(&mesh);
			_meshes.push_back(&mesh);

			_staticObjects.push_back(newStaticObj);
		}

		_staticsVertexBuffer = _graphicsDevice->CreateVertexBuffer((int)_staticsVertices.size(), sizeof(Vertex), _staticsVertices.data());
		_staticsIndexBuffer = _graphicsDevice->CreateIndexBuffer((int)_staticsIndices.size(), _staticsIndices.data());

		TENLog("Preparing sprite data...", LogLevel::Info);
		
		// Step 5: prepare sprites
		_sprites.resize(g_Level.Sprites.size());

		for (int i = 0; i < g_Level.Sprites.size(); i++)
		{
			SPRITE *oldSprite = &g_Level.Sprites[i];
			_sprites[i] = RendererSprite();
			RendererSprite &sprite = _sprites[i];

			sprite.UV[0] = Vector2(oldSprite->x1, oldSprite->y1);
			sprite.UV[1] = Vector2(oldSprite->x2, oldSprite->y2);
			sprite.UV[2] = Vector2(oldSprite->x3, oldSprite->y3);
			sprite.UV[3] = Vector2(oldSprite->x4, oldSprite->y4);
			sprite.Texture = _spritesTextures[oldSprite->tile].get();
			sprite.Width = round((oldSprite->x2 - oldSprite->x1) * (float)sprite.Texture->GetWidth() + 1.0f);
			sprite.Height = round((oldSprite->y3 - oldSprite->y2) * (float)sprite.Texture->GetHeight() + 1.0f);
			sprite.X = oldSprite->x1 * sprite.Texture->GetWidth();
			sprite.Y = oldSprite->y1 * sprite.Texture->GetHeight();
		}

		for (int i = 0; i < SpriteSequencesIds.size(); i++)
		{
			ObjectInfo *obj = &Objects[SpriteSequencesIds[i]];

			if (obj->nmeshes < 0)
			{
				short numSprites = abs(obj->nmeshes);
				short baseSprite = obj->meshIndex;
				_spriteSequences[SpriteSequencesIds[i]] = RendererSpriteSequence();

				// TODO: Why a custom =& operator is needed? It creates everytime new N null sprites
				RendererSpriteSequence &sequence = _spriteSequences[SpriteSequencesIds[i]];

				sequence.NumSprites = numSprites;
				sequence.SpritesList.resize(numSprites);
				for (int j = baseSprite; j < baseSprite + numSprites; j++)
				{
					sequence.SpritesList[j - baseSprite] = &_sprites[j];
				}

				_spriteSequences[SpriteSequencesIds[i]] = sequence;
			}
		}

		return true;
	}

	RendererMesh* Renderer::GetRendererMeshFromTrMesh(RendererObject* obj, MESH* meshPtr, short boneIndex, int isJoints, int isHairs, int* lastVertex, int* lastIndex)
	{
		RendererMesh* mesh = new RendererMesh();

		mesh->Sphere = meshPtr->sphere;
		mesh->LightMode = meshPtr->lightMode;

		if (meshPtr->positions.empty())
			return mesh;

		mesh->Positions.resize(meshPtr->positions.size());
		for (int i = 0; i < meshPtr->positions.size(); i++)
			mesh->Positions[i] = meshPtr->positions[i];

		for (int n = 0; n < meshPtr->buckets.size(); n++)
		{
			BUCKET* levelBucket = &meshPtr->buckets[n];
			RendererBucket bucket{};
			bucket.Animated = levelBucket->animated;
			bucket.Texture = levelBucket->texture;
			bucket.BlendMode = static_cast<BlendMode>(levelBucket->blendMode);
			bucket.MaterialIndex = levelBucket->materialIndex;
			bucket.StartVertex = *lastVertex;
			bucket.StartIndex = *lastIndex;
			bucket.NumVertices = levelBucket->numQuads * 4 + levelBucket->numTriangles * 3;
			bucket.NumIndices = levelBucket->numQuads * 6 + levelBucket->numTriangles * 3;

			for (int p = 0; p < (int)levelBucket->polygons.size(); p++)
			{
				POLYGON* poly = &levelBucket->polygons[p];
				RendererPolygon newPoly;

				newPoly.Shape = poly->shape;
				newPoly.Centre = (
					meshPtr->positions[poly->indices[0]] +
					meshPtr->positions[poly->indices[1]] +
					meshPtr->positions[poly->indices[2]]) / 3.0f;

				int baseVertices = *lastVertex;

				for (int k = 0; k < (int)poly->indices.size(); k++)
				{
					Vertex vertex;
					int v = poly->indices[k];
					
					vertex.Position.x = meshPtr->positions[v].x;
					vertex.Position.y = meshPtr->positions[v].y;
					vertex.Position.z = meshPtr->positions[v].z;
					 
					vertex.Normal = PackVector3(Vector3(poly->normals[k].x, poly->normals[k].y, poly->normals[k].z));
					vertex.Tangent = PackVector3(Vector3(poly->tangents[k].x, poly->tangents[k].y, poly->tangents[k].z));

					vertex.FaceNormal = PackVector3(poly->normal);

					vertex.UV.x = poly->textureCoordinates[k].x;
					vertex.UV.y = poly->textureCoordinates[k].y;
					
					vertex.Color = VectorColorToRGBA(Vector4(meshPtr->colors[v].x, meshPtr->colors[v].y, meshPtr->colors[v].z, 1.0f));
					
					vertex.BoneIndex  = meshPtr->boneIndices[v];
					vertex.BoneWeight = meshPtr->boneWeights[v];

					unsigned int hash = (unsigned int)std::hash<float>{}
						(vertex.Position.x) ^
						(unsigned int)std::hash<float>{}(vertex.Position.y) ^
						(unsigned int)std::hash<float>{}(vertex.Position.z);

					vertex.AnimationFrameOffsetIndexHash = PackAnimationFrameOffsetIndexHash(poly->animatedFrame, v, hash);	
					vertex.Effects = PackEffectsAndIndexInPoly(meshPtr->effects[v], poly->shineStrength, k);

					if (obj->Type == 0)
						_moveablesVertices[*lastVertex] = vertex;
					else
						_staticsVertices[*lastVertex] = vertex;

					*lastVertex = *lastVertex + 1;
				}

				if (poly->shape == 0)
				{
					newPoly.BaseIndex = *lastIndex;

					if (obj->Type == 0)
					{
						_moveablesIndices[newPoly.BaseIndex + 0] = baseVertices + 0;
						_moveablesIndices[newPoly.BaseIndex + 1] = baseVertices + 1;
						_moveablesIndices[newPoly.BaseIndex + 2] = baseVertices + 3;
						_moveablesIndices[newPoly.BaseIndex + 3] = baseVertices + 2;
						_moveablesIndices[newPoly.BaseIndex + 4] = baseVertices + 3;
						_moveablesIndices[newPoly.BaseIndex + 5] = baseVertices + 1;
					}
					else
					{
						_staticsIndices[newPoly.BaseIndex + 0] = baseVertices + 0;
						_staticsIndices[newPoly.BaseIndex + 1] = baseVertices + 1;
						_staticsIndices[newPoly.BaseIndex + 2] = baseVertices + 3;
						_staticsIndices[newPoly.BaseIndex + 3] = baseVertices + 2;
						_staticsIndices[newPoly.BaseIndex + 4] = baseVertices + 3;
						_staticsIndices[newPoly.BaseIndex + 5] = baseVertices + 1;
					}

					*lastIndex = *lastIndex + 6;
				}
				else
				{
					newPoly.BaseIndex = *lastIndex;

					if (obj->Type == 0)
					{
						_moveablesIndices[newPoly.BaseIndex + 0] = baseVertices + 0;
						_moveablesIndices[newPoly.BaseIndex + 1] = baseVertices + 1;
						_moveablesIndices[newPoly.BaseIndex + 2] = baseVertices + 2;
					}
					else
					{
						_staticsIndices[newPoly.BaseIndex + 0] = baseVertices + 0;
						_staticsIndices[newPoly.BaseIndex + 1] = baseVertices + 1;
						_staticsIndices[newPoly.BaseIndex + 2] = baseVertices + 2;
					}

					*lastIndex = *lastIndex + 3;
				}

				bucket.Polygons.push_back(newPoly);
			}

			mesh->Buckets.push_back(bucket);
		}

		return mesh;
	}

	void Renderer::ProcessSkinJoints(RendererObject& jointsMoveable, RendererObject& skinMoveable, ObjectInfo& jointsObj)
	{
		for (int j = 1; j < jointsObj.nmeshes; j++)
		{
			const auto* jointMesh = jointsMoveable.ObjectMeshes[j];
			const auto* jointBone = jointsMoveable.LinearizedBones[j];

			int bonesToCheck[2] = { jointBone->Parent->Index, j };

			for (int b1 = 0; b1 < jointMesh->Buckets.size(); b1++)
			{
				const auto* jointBucket = &jointMesh->Buckets[b1];

				for (int v1 = 0; v1 < jointBucket->NumVertices; v1++)
				{
					auto* jointVertex = &_moveablesVertices[jointBucket->StartVertex + v1];
					bool isDone = false;

					for (int k = 0; k < 2; k++)
					{
						const auto* skinMesh = skinMoveable.ObjectMeshes[bonesToCheck[k]];
						const auto* skinBone = skinMoveable.LinearizedBones[bonesToCheck[k]];

						for (int b2 = 0; b2 < skinMesh->Buckets.size(); b2++)
						{
							const auto* skinBucket = &skinMesh->Buckets[b2];
							for (int v2 = 0; v2 < skinBucket->NumVertices; v2++)
							{
								auto* skinVertex = &_moveablesVertices[skinBucket->StartVertex + v2];

								int x1 = _moveablesVertices[jointBucket->StartVertex + v1].Position.x + jointBone->GlobalTranslation.x;
								int y1 = _moveablesVertices[jointBucket->StartVertex + v1].Position.y + jointBone->GlobalTranslation.y;
								int z1 = _moveablesVertices[jointBucket->StartVertex + v1].Position.z + jointBone->GlobalTranslation.z;

								int x2 = _moveablesVertices[skinBucket->StartVertex + v2].Position.x + skinBone->GlobalTranslation.x;
								int y2 = _moveablesVertices[skinBucket->StartVertex + v2].Position.y + skinBone->GlobalTranslation.y;
								int z2 = _moveablesVertices[skinBucket->StartVertex + v2].Position.z + skinBone->GlobalTranslation.z;

								if (abs(x1 - x2) < 2 && abs(y1 - y2) < 2 && abs(z1 - z2) < 2)
								{
									jointVertex->BoneIndex[0] = bonesToCheck[k];
									jointVertex->Position = skinVertex->Position;
									jointVertex->Normal = skinVertex->Normal;

									isDone = true;
									break;
								}
							}

							if (isDone) break;
						}

						if (isDone) break;
					}

					if (!isDone)
					{
						jointVertex->BoneIndex[0] = j;
						jointVertex->BoneWeight[0] = 0.5f * UCHAR_MAX;
						jointVertex->BoneIndex[1] = jointBone->Parent->Index;
						jointVertex->BoneWeight[1] = 0.5f * UCHAR_MAX;
					}
				}
			}
		}
	}

	void Renderer::ProcessHair(GAME_OBJECT_ID hairID, RendererObject& skinMoveable,	bool isSecond)
	{
		if (!_moveableObjects[hairID].has_value())
			return;

		auto* hairObj = &Objects[hairID];
		if (!hairObj->loaded)
			return;

		auto& hairMoveable = _moveableObjects[hairID].value();
		const auto& settings = g_GameFlow->GetSettings()->Hair;

		// Flatten skinned hairmesh vertices.
		if (hairObj->skinIndex != NO_VALUE)
		{
			const auto* hairMesh = GetMesh(hairObj->skinIndex);

			for (const auto& bucket : hairMesh->Buckets)
			{
				for (int v = 0; v < bucket.NumVertices; v++)
				{
					auto& vertex = _moveablesVertices[bucket.StartVertex + v];

					for (int w = 0; w < 4; w++)
					{
						if (vertex.BoneWeight[w] == 0)
							continue;

						auto offset = Vector3::Zero;

						for (int b = 1; b < vertex.BoneIndex[w]; b++)
							offset += GetJointOffset(hairID, b, true);

						vertex.Position += offset * (vertex.BoneWeight[w] / (float)UCHAR_MAX);
					}
				}
			}
		}

		bool isYoung = (g_GameFlow->GetLevel(CurrentLevel)->GetLaraType() == LaraType::Young);

		for (int j = 0; j < hairObj->nmeshes; j++)
		{
			const auto* currentMesh = hairMoveable.ObjectMeshes[j];
			const auto* currentBone = hairMoveable.LinearizedBones[j];

			for (const auto& currentBucket : currentMesh->Buckets)
			{
				for (int v1 = 0; v1 < currentBucket.NumVertices; v1++)
				{
					auto* currentVertex = &_moveablesVertices[currentBucket.StartVertex + v1];
					currentVertex->BoneIndex[0] = j + 1;

					if (j == 0)
					{
						const auto& vertices0 = isYoung ? settings[(int)PlayerHairType::YoungLeft].Indices :
							settings[(int)PlayerHairType::Normal].Indices;
						const auto& vertices1 = isYoung ? settings[(int)PlayerHairType::YoungRight].Indices :
							settings[(int)PlayerHairType::Normal].Indices;

						int rootMesh = HairUnit::GetRootMeshID(isSecond ? 1 : 0);
						const auto* parentMesh = skinMoveable.ObjectMeshes[rootMesh];

						int currentOriginalIndex = GetOriginalIndex(currentVertex->AnimationFrameOffsetIndexHash);

						if ((!isSecond && currentOriginalIndex >= vertices0.size()) ||
							(isSecond && currentOriginalIndex >= vertices1.size()))
						{
							continue;
						}

						for (int b2 = 0; b2 < parentMesh->Buckets.size(); b2++)
						{
							const auto* parentBucket = &parentMesh->Buckets[b2];
							for (int v2 = 0; v2 < parentBucket->NumVertices; v2++)
							{
								const auto* parentVertex = &_moveablesVertices[parentBucket->StartVertex + v2];
								int parentOriginalIndex = GetOriginalIndex(parentVertex->AnimationFrameOffsetIndexHash);

								if ((parentOriginalIndex == vertices1[currentOriginalIndex] && isSecond) ||
									(parentOriginalIndex == vertices0[currentOriginalIndex] && !isSecond))
								{
									currentVertex->BoneIndex[0] = 0;
									currentVertex->Position = parentVertex->Position;
									currentVertex->Normal = parentVertex->Normal;
								}
							}
						}
					}
					else
					{
						const auto* parentMesh = hairMoveable.ObjectMeshes[j - 1];
						const auto* parentBone = hairMoveable.LinearizedBones[j - 1];

						for (int b2 = 0; b2 < parentMesh->Buckets.size(); b2++)
						{
							const auto* parentBucket = &parentMesh->Buckets[b2];
							for (int v2 = 0; v2 < parentBucket->NumVertices; v2++)
							{
								auto* parentVertex = &_moveablesVertices[parentBucket->StartVertex + v2];

								int x1 = _moveablesVertices[currentBucket.StartVertex + v1].Position.x + currentBone->GlobalTranslation.x;
								int y1 = _moveablesVertices[currentBucket.StartVertex + v1].Position.y + currentBone->GlobalTranslation.y;
								int z1 = _moveablesVertices[currentBucket.StartVertex + v1].Position.z + currentBone->GlobalTranslation.z;

								int x2 = _moveablesVertices[parentBucket->StartVertex + v2].Position.x + parentBone->GlobalTranslation.x;
								int y2 = _moveablesVertices[parentBucket->StartVertex + v2].Position.y + parentBone->GlobalTranslation.y;
								int z2 = _moveablesVertices[parentBucket->StartVertex + v2].Position.z + parentBone->GlobalTranslation.z;

								if (abs(x1 - x2) == 0 && abs(y1 - y2) == 0 && abs(z1 - z2) == 0)
								{
									currentVertex->BoneIndex[0] = j;
									currentVertex->Position = parentVertex->Position;
									currentVertex->Normal = parentVertex->Normal;
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	// Re-run vertex matching for player skin joints and hair objects at runtime, then re-upload the vertex buffer.
	void Renderer::UpdatePlayerSkinVertices(GAME_OBJECT_ID skinID, GAME_OBJECT_ID skinJointsID, GAME_OBJECT_ID hairPrimaryID, GAME_OBJECT_ID hairSecondaryID)
	{
		if (!_moveableObjects[skinID].has_value() || !_moveableObjects[skinJointsID].has_value())
			return;

		// Restore original vertex data for objects that were previously processed.
		RestoreObjectVertices(skinJointsID);
		RestoreObjectVertices(hairPrimaryID);
		RestoreObjectVertices(hairSecondaryID);

		// Backup original vertex data for objects being processed for the first time.
		BackupObjectVertices(skinJointsID);
		BackupObjectVertices(hairPrimaryID);
		BackupObjectVertices(hairSecondaryID);

		auto& jointsMoveable = _moveableObjects[skinJointsID].value();
		auto& skinMoveable = _moveableObjects[skinID].value();
		auto* jointsObj = &Objects[skinJointsID];

		ProcessSkinJoints(jointsMoveable, skinMoveable, *jointsObj);

		bool isYoung = (g_GameFlow->GetLevel(CurrentLevel)->GetLaraType() == LaraType::Young);
		ProcessHair(hairPrimaryID, skinMoveable, false);
		ProcessHair(hairSecondaryID, skinMoveable, isYoung);

		// Re-upload modified vertex data to the GPU.
		_graphicsDevice->UpdateVertexBuffer(_moveablesVertexBuffer.get(), 0, (int)_moveablesVertices.size(), _moveablesVertices.data());
	}

	void Renderer::BackupObjectVertices(GAME_OBJECT_ID objectID)
	{
		if (_skinVertexBackups.count((int)objectID))
			return;

		if (!_moveableObjects[objectID].has_value())
			return;

		auto& moveable = _moveableObjects[objectID].value();
		auto& backup = _skinVertexBackups[(int)objectID];

		for (auto* mesh : moveable.ObjectMeshes)
		{
			for (auto& bucket : mesh->Buckets)
			{
				for (int v = 0; v < bucket.NumVertices; v++)
					backup.push_back(_moveablesVertices[bucket.StartVertex + v]);
			}
		}

		auto* obj = &Objects[objectID];
		if (obj->skinIndex != NO_VALUE)
		{
			auto* skinMesh = GetMesh(obj->skinIndex);
			for (auto& bucket : skinMesh->Buckets)
			{
				for (int v = 0; v < bucket.NumVertices; v++)
					backup.push_back(_moveablesVertices[bucket.StartVertex + v]);
			}
		}
	}

	void Renderer::RestoreObjectVertices(GAME_OBJECT_ID objectID)
	{
		auto it = _skinVertexBackups.find((int)objectID);
		if (it == _skinVertexBackups.end())
			return;

		if (!_moveableObjects[objectID].has_value())
			return;

		auto& moveable = _moveableObjects[objectID].value();
		auto& backup = it->second;
		int idx = 0;

		for (auto* mesh : moveable.ObjectMeshes)
		{
			for (auto& bucket : mesh->Buckets)
			{
				for (int v = 0; v < bucket.NumVertices; v++)
					_moveablesVertices[bucket.StartVertex + v] = backup[idx++];
			}
		}

		auto* obj = &Objects[objectID];
		if (obj->skinIndex != NO_VALUE)
		{
			auto* skinMesh = GetMesh(obj->skinIndex);
			for (auto& bucket : skinMesh->Buckets)
			{
				for (int v = 0; v < bucket.NumVertices; v++)
					_moveablesVertices[bucket.StartVertex + v] = backup[idx++];
			}
		}
	}
}
