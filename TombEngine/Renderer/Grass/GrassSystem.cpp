#include "framework.h"
#include "Renderer/Grass/GrassSystem.h"

#include <random>
#include <algorithm>
#include <map>

#include "Game/collision/floordata.h"
#include "Game/room.h"
#include "Specific/level.h"
#include "Math/Constants.h"
#include "Renderer/RenderView.h"
#include "Renderer/ConstantBuffers/ConstantBuffer.h"
#include "Renderer/ConstantBuffers/GrassBuffer.h"

using namespace TEN::Collision::Floordata;

namespace TEN::Renderer::Grass
{
	// Tile dimension in world units (4 blocks = 4096).
	static constexpr float TILE_SIZE = 4096.0f;

	// Hash-based PRNG for deterministic placement.
	static uint32_t HashPosition(int x, int z, uint32_t seed)
	{
		uint32_t h = seed;
		h ^= (uint32_t)x + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= (uint32_t)z + 0x9e3779b9 + (h << 6) + (h >> 2);
		return h;
	}

	static float HashToFloat01(uint32_t h)
	{
		return (float)(h & 0xFFFFFF) / (float)0xFFFFFF;
	}

	void GrassSystem::Initialize(ID3D11Device* device)
	{
		_initialized = true;
		Clear();
	}

	void GrassSystem::Clear()
	{
		_allBlades.clear();
		_tiles.clear();
		_nextInfluenceSlot = 0;
		_currentTime = 0.0f;
		_lastVisibleTileCount = 0;
		_lastVisibleBladeCount = 0;
		_lastDrawCallCount = 0;

		for (auto& inf : _influences)
			inf.Active = false;
	}

	void GrassSystem::GenerateForLevel()
	{
		Clear();

		for (int roomNum = 0; roomNum < (int)g_Level.Rooms.size(); roomNum++)
		{
			GenerateGrassForRoom(roomNum);
		}

		BuildTiles();
	}

	void GrassSystem::GenerateGrassForRoom(int roomNumber)
	{
		auto& room = g_Level.Rooms[roomNumber];

		// Skip underwater rooms.
		if (room.flags & ENV_FLAG_WATER)
			return;

		for (int gridX = 0; gridX < room.XSize; gridX++)
		{
			for (int gridZ = 0; gridZ < room.ZSize; gridZ++)
			{
				auto& sector = GetFloor(roomNumber, Vector2i(gridX, gridZ));

				// Skip walls.
				if (sector.IsWall(0) && sector.IsWall(1))
					continue;

				int worldX = room.Position.x + gridX * BLOCK_UNIT;
				int worldZ = room.Position.z + gridZ * BLOCK_UNIT;

				// Check both triangles of the floor for grass material.
				for (int tri = 0; tri < 2; tri++)
				{
					auto& triData = sector.FloorSurface.Triangles[tri];

					if (triData.Material != MaterialType::Grass)
						continue;

					// Get floor height at sector center for this triangle.
					int sampleX = worldX + BLOCK_UNIT / 2;
					int sampleZ = worldZ + BLOCK_UNIT / 2;
					int floorHeight = sector.GetSurfaceHeight(sampleX, sampleZ, true);

					if (floorHeight == NO_HEIGHT)
						continue;

					Vector3 normal = sector.GetSurfaceNormal(tri, true);

					// Skip steep slopes (TEN uses inverted Y: flat floor normal.y = -1).
					if (normal.y > -0.5f)
						continue;

					GenerateBladesForSector(
						roomNumber, worldX, worldZ, floorHeight, normal, triData.Material);

					break; // Only generate once per sector (both triangles share the space).
				}
			}
		}
	}

	void GrassSystem::GenerateBladesForSector(
		int roomNumber, int worldX, int worldZ,
		int floorHeight, const Vector3& normal, MaterialType material)
	{
		float density = _config.Density;
		float cellSize = BLOCK_UNIT / density;
		int cellsPerAxis = (int)density;

		uint32_t sectorSeed = HashPosition(worldX, worldZ, 0xDEAD);

		for (int cx = 0; cx < cellsPerAxis; cx++)
		{
			for (int cz = 0; cz < cellsPerAxis; cz++)
			{
				uint32_t cellHash = HashPosition(cx + worldX * 137, cz + worldZ * 251, sectorSeed);
				float rng0 = HashToFloat01(cellHash);
				float rng1 = HashToFloat01(cellHash * 2654435761u);
				float rng2 = HashToFloat01(cellHash * 340573321u);
				float rng3 = HashToFloat01(cellHash * 1664525u + 1013904223u);

				// Jittered position within cell.
				float jitterX = (rng0 - 0.5f) * _config.JitterAmount;
				float jitterZ = (rng1 - 0.5f) * _config.JitterAmount;

				float localX = (cx + 0.5f + jitterX) * cellSize;
				float localZ = (cz + 0.5f + jitterZ) * cellSize;

				float finalX = worldX + localX;
				float finalZ = worldZ + localZ;

				// Clamp to sector bounds.
				finalX = std::clamp(finalX, (float)worldX + 1.0f, (float)(worldX + BLOCK_UNIT) - 1.0f);
				finalZ = std::clamp(finalZ, (float)worldZ + 1.0f, (float)(worldZ + BLOCK_UNIT) - 1.0f);

				// Get the actual floor height at this exact position.
				auto& room = g_Level.Rooms[roomNumber];
				auto& sector = GetFloor(roomNumber, (int)finalX, (int)finalZ);

				if (sector.IsWall((int)finalX, (int)finalZ))
					continue;

				int height = sector.GetSurfaceHeight((int)finalX, (int)finalZ, true);
				if (height == NO_HEIGHT)
					continue;

				Vector3 localNormal = sector.GetSurfaceNormal((int)finalX, (int)finalZ, true);
				if (localNormal.y > -0.5f)
					continue;

				GrassBlade blade;
				blade.Position = Vector3(finalX, (float)height, finalZ);
				blade.Normal = localNormal;
				blade.Scale = _config.MinScale + rng2 * (_config.MaxScale - _config.MinScale);
				blade.Seed = rng3; // Use independent hash for rotation to decorrelate from position jitter.
				blade.Color = Vector4(0.75f + rng1 * 0.25f, 0.8f + rng2 * 0.2f, 0.65f + rng0 * 0.2f, 1.0f);

				// Select a sprite variant from the atlas.
				int totalSprites = _config.AtlasColumns * _config.AtlasRows;
				blade.SpriteIdx = (int)(rng2 * totalSprites) % totalSprites;

				_allBlades.push_back(blade);
			}
		}
	}

	void GrassSystem::BuildTiles()
	{
		if (_allBlades.empty())
			return;

		// Sort blades into spatial tiles based on XZ position.
		struct TileKey
		{
			int tx, tz;
			bool operator<(const TileKey& other) const
			{
				return (tx < other.tx) || (tx == other.tx && tz < other.tz);
			}
		};

		std::map<TileKey, std::vector<int>> tileBladeIndices;

		for (int i = 0; i < (int)_allBlades.size(); i++)
		{
			auto& blade = _allBlades[i];
			int tx = (int)std::floor(blade.Position.x / TILE_SIZE);
			int tz = (int)std::floor(blade.Position.z / TILE_SIZE);
			tileBladeIndices[{tx, tz}].push_back(i);
		}

		// Rebuild the blade array sorted by tile, and create tile descriptors.
		std::vector<GrassBlade> sortedBlades;
		sortedBlades.reserve(_allBlades.size());

		for (auto& [key, indices] : tileBladeIndices)
		{
			GrassTile tile;
			tile.StartIndex = (int)sortedBlades.size();
			tile.BladeCount = (int)indices.size();

			Vector3 bmin(FLT_MAX, FLT_MAX, FLT_MAX);
			Vector3 bmax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

			for (int idx : indices)
			{
				auto& blade = _allBlades[idx];
				sortedBlades.push_back(blade);

				bmin = Vector3::Min(bmin, blade.Position - Vector3(0, _config.BladeHeight, 0));
				bmax = Vector3::Max(bmax, blade.Position + Vector3(0, _config.BladeHeight * 1.5f, 0));
			}

			// Expand the bounding box slightly for wind sway.
			bmin -= Vector3(_config.BladeWidth, 0, _config.BladeWidth);
			bmax += Vector3(_config.BladeWidth, 0, _config.BladeWidth);

			tile.Bounds = BoundingBox(
				(bmin + bmax) * 0.5f,
				(bmax - bmin) * 0.5f);

			_tiles.push_back(tile);
		}

		_allBlades = std::move(sortedBlades);
	}

	void GrassSystem::Update(float time)
	{
		_currentTime = time;
	}

	void GrassSystem::AddInfluence(const Vector3& position, float radius, float intensity)
	{
		// Try to find an existing influence at a similar position to update.
		for (auto& inf : _influences)
		{
			if (inf.Active)
			{
				float dist = Vector3::Distance(inf.Position, position);
				if (dist < radius * 0.5f)
				{
					// Update existing.
					inf.Position = position;
					inf.Radius = radius;
					inf.Intensity = intensity;
					inf.Timestamp = _currentTime;
					return;
				}
			}
		}

		// Allocate a new slot (ring buffer with oldest eviction).
		auto& slot = _influences[_nextInfluenceSlot];
		slot.Position = position;
		slot.Radius = radius;
		slot.Intensity = intensity;
		slot.Timestamp = _currentTime;
		slot.Active = true;

		_nextInfluenceSlot = (_nextInfluenceSlot + 1) % MAX_INFLUENCES;
	}

	void GrassSystem::Draw(
		ID3D11DeviceContext* context,
		const RenderView& view,
		ConstantBuffer<CGrassSettingsBuffer>& cbSettings,
		ConstantBuffer<CGrassInstanceBuffer>& cbInstances,
		float time)
	{
		if (!_enabled || _allBlades.empty() || _tiles.empty())
			return;

		// Fill the settings constant buffer.
		CGrassSettingsBuffer settings = {};
		settings.MaxDrawDistance = _config.MaxDrawDistance;
		settings.FadeStartDistance = _config.FadeStartDistance;
		settings.WindStrength = _config.WindStrength;
		settings.WindFrequency = _config.WindFrequency;
		settings.WindDirection = _config.WindDirection;
		settings.WindDirection.Normalize();
		settings.Time = time;
		settings.BendRiseSpeed = _config.BendRiseSpeed;
		settings.BendDecaySpeed = _config.BendDecaySpeed;
		settings.BendMaxAngle = _config.BendMaxAngle;
		settings.BladeWidth = _config.BladeWidth;
		settings.BladeHeight = _config.BladeHeight;
		settings.DebugMode = _debugMode ? 1 : 0;

		// Pack influence spheres.
		int numInfluences = 0;
		for (int i = 0; i < MAX_INFLUENCES; i++)
		{
			if (_influences[i].Active)
			{
				// Expire old influences.
				float age = time - _influences[i].Timestamp;
				if (age > 5.0f) // 5 seconds max lifetime.
				{
					_influences[i].Active = false;
					continue;
				}

				auto& dst = settings.Influences[numInfluences];
				dst.Position = _influences[i].Position;
				dst.Radius = _influences[i].Radius;
				dst.Intensity = _influences[i].Intensity;
				dst.Timestamp = _influences[i].Timestamp;
				numInfluences++;

				if (numInfluences >= MAX_GRASS_INFLUENCE_SPHERES)
					break;
			}
		}
		settings.NumInfluences = numInfluences;

		cbSettings.UpdateData(settings, context);

		// Frustum cull tiles and draw visible ones in batches.
		const auto& frustum = view.Camera.Frustum;
		const Vector3 camPos = view.Camera.WorldPosition;

		_lastVisibleTileCount = 0;
		_lastVisibleBladeCount = 0;
		_lastDrawCallCount = 0;

		CGrassInstanceBuffer instanceBuffer = {};
		int batchCount = 0;

		auto flushBatch = [&]()
		{
			if (batchCount == 0)
				return;

			cbInstances.UpdateData(instanceBuffer, context);
			context->DrawInstanced(6, batchCount, 0, 0);
			_lastDrawCallCount++;
			batchCount = 0;
		};

		for (auto& tile : _tiles)
		{
			// Frustum culling.
			Vector3 tileMin(
				tile.Bounds.Center.x - tile.Bounds.Extents.x,
				tile.Bounds.Center.y - tile.Bounds.Extents.y,
				tile.Bounds.Center.z - tile.Bounds.Extents.z);
			Vector3 tileMax(
				tile.Bounds.Center.x + tile.Bounds.Extents.x,
				tile.Bounds.Center.y + tile.Bounds.Extents.y,
				tile.Bounds.Center.z + tile.Bounds.Extents.z);

			if (!frustum.AABBInFrustum(tileMin, tileMax))
				continue;

			// Distance culling: check tile center vs max draw distance.
			Vector3 tileCenter(tile.Bounds.Center.x, tile.Bounds.Center.y, tile.Bounds.Center.z);
			float tileDist = Vector3::Distance(camPos, tileCenter);

			if (tileDist > _config.MaxDrawDistance + TILE_SIZE)
				continue;

			_lastVisibleTileCount++;

			// Process blades in this tile.
			int end = tile.StartIndex + tile.BladeCount;
			for (int i = tile.StartIndex; i < end; i++)
			{
				auto& blade = _allBlades[i];

				// Per-blade distance culling.
				float bladeDist = Vector3::Distance(camPos, blade.Position);
				if (bladeDist > _config.MaxDrawDistance)
					continue;

				// LOD: reduce density at distance.
				if (bladeDist > _config.FadeStartDistance)
				{
					float lodFactor = (bladeDist - _config.FadeStartDistance) /
						(_config.MaxDrawDistance - _config.FadeStartDistance);
					// Probabilistic LOD: skip some blades at distance.
					if (blade.Seed < lodFactor * 0.6f)
						continue;
				}

				// Disable bending for distant blades (performance).
				// Bending is computed in the shader only for blades
				// within the influence sphere radii anyway.

				// Fill instance data.
				auto& inst = instanceBuffer.Instances[batchCount];
				inst.Position = blade.Position;
				inst.Scale = blade.Scale;
				inst.Normal = blade.Normal;
				inst.Seed = blade.Seed;
				inst.Color = blade.Color;

				// Calculate UV offset for sprite atlas variant.
				int col = blade.SpriteIdx % _config.AtlasColumns;
				int row = blade.SpriteIdx / _config.AtlasColumns;
				inst.UVScale = Vector2(
					1.0f / _config.AtlasColumns,
					1.0f / _config.AtlasRows);
				inst.UVOffset = Vector2(
					col * inst.UVScale.x,
					row * inst.UVScale.y);

				batchCount++;
				_lastVisibleBladeCount++;

				if (batchCount >= GRASS_INSTANCE_BUCKET_SIZE)
					flushBatch();
			}
		}

		// Flush remaining.
		flushBatch();
	}
}
