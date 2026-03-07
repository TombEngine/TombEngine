#pragma once

#include <array>
#include <vector>
#include <SimpleMath.h>
#include <DirectXCollision.h>
#include <d3d11.h>
#include <wrl/client.h>

#include "Renderer/ConstantBuffers/GrassBuffer.h"

using namespace DirectX::SimpleMath;
using namespace DirectX;
using Microsoft::WRL::ComPtr;

enum class MaterialType;

namespace TEN::Renderer
{
	struct RenderView;

	namespace ConstantBuffers
	{
		template <typename CBuff> class ConstantBuffer;
	}
}

namespace TEN::Renderer::Grass
{
	using namespace TEN::Renderer::ConstantBuffers;

	// A single grass blade instance (CPU side).
	struct GrassBlade
	{
		Vector3 Position     = Vector3::Zero;
		Vector3 Normal       = Vector3::UnitY;
		float   Scale        = 1.0f;
		float   Seed         = 0.0f;
		Vector4 Color        = Vector4::One;
		int     SpriteIdx    = 0; // Index into grass atlas sprite variants.
		int     RoomNumber   = 0; // Room this blade belongs to (for ambient lookup).
		bool    WindEnabled  = false; // True if blade is in an outdoor (skybox) room.
	};

	// A spatial tile containing grass blades for culling.
	struct GrassTile
	{
		BoundingBox    Bounds;
		int            StartIndex  = 0; // Into the owning GrassGrid's blade array.
		int            BladeCount  = 0;
	};

	// Influence sphere for bending effect (e.g., player walking through grass).
	struct GrassInfluence
	{
		Vector3 Position       = Vector3::Zero;
		float   Radius         = 0.0f;
		float   Intensity      = 0.0f;
		float   Timestamp      = 0.0f; // Last refresh time (used for decay).
		float   BirthTimestamp = 0.0f; // Creation time (used for rise).
		bool    Active         = false;
	};

	// Configuration for the grass system.
	struct GrassConfig
	{
		// -- Placement --
		// Number of grass blades placed per BLOCK unit on each axis within a sector.
		// E.g. 16 = 256 blades per sector (16x16 grid before jitter).
		float Density           = 16.0f;

		// Minimum and maximum random scale multiplier applied to each blade.
		float MinScale          = 0.6f;
		float MaxScale          = 1.5f;

		// Controls how much each blade is randomly offset from its grid cell centre.
		// 0.0 = perfectly uniform grid (no randomness).
		// 1.0 = blade can appear anywhere within its cell (fully random).
		float JitterAmount      = 1.0f;

		// -- Rendering --
		// Width and height of each grass quad in world units.
		float BladeWidth        = 64.0f;
		float BladeHeight       = 256.0f;

		// Maximum distance from the camera at which grass is drawn at all.
		// Blades beyond this distance are skipped entirely. Reducing this improves performance.
		float MaxDrawDistance   = 16384.0f;

		// Distance at which grass begins to fade out (alpha fades to 0 at MaxDrawDistance).
		float FadeStartDistance = 12288.0f;

		// -- Wind --
		// Maximum world-unit displacement of a blade tip caused by wind at full swing.
		// Higher values = more dramatic swaying. Too high can look unnatural.
		float WindStrength      = 128.0f;

		// How many full wind oscillation cycles occur per second.
		// Higher values = faster flickering; lower values = slow, rolling waves.
		float WindFrequency     = 1.5f;

		// The primary direction the wind blows.
		Vector3 WindDirection   = Vector3(1.0f, 0.0f, 0.3f);

		// -- Bending (player interaction) --
		// How fast grass blades bend down when Lara first steps into them.
		// This is a linear ramp: full bend is reached after (1.0 / BendRiseSpeed) seconds.
		// E.g. 10.0 = full bend in ~0.1s (snappy). 2.0 = full bend in ~0.5s (slow lean).
		float BendRiseSpeed     = 10.0f;

		// How fast bent blades spring back up after Lara leaves.
		// This is an exponential decay: bend halves every (ln2 / BendDecaySpeed) seconds.
		// E.g. 2.5 = blades mostly upright in ~2s. 0.1 = blades stay flat for ~45s.
		float BendDecaySpeed    = 2.5f;

		// Maximum world-unit displacement applied to a blade tip at full bend.
		// This should be roughly equal to BladeHeight for a convincing flat collapse.
		// Too low = blades barely react; too high = blades stretch past the floor.
		float BendMaxAngle      = 256.0f;

		// Radius around Lara (in world units) within which grass blades are bent each frame.
		float PlayerBendRadius  = 512.0f;

		// Multiplier on the overall bending strength. 1.0 = normal full bend.
		// Reduce below 1.0 for subtler interaction; raise above 1.0 to exaggerate.
		float PlayerBendIntensity = 1.0f;

		// Atlas layout (assuming a simple grid of sprite variants in one texture).
		int   AtlasColumns      = 4;
		int   AtlasRows         = 1;
	};

	// Per-room sun bulb data (one entry per room, built each frame).
	struct RoomSunData
	{
		Vector3 Direction = Vector3::Zero;
		Vector3 Color     = Vector3::Zero;
		float   Intensity = 0.0f;
	};

	class GrassSystem
	{
	public:
		GrassSystem() = default;

		void Initialize(ID3D11Device* device);
		void GenerateForLevel();
		void Clear();

		// Per-frame update: updates influence spheres.
		void Update(float time);

		// Renders all visible grass tiles.
		void Draw(
			ID3D11DeviceContext* context,
			const RenderView& view,
			ConstantBuffer<CGrassSettingsBuffer>& cbSettings,
			ConstantBuffer<CGrassInstanceBuffer>& cbInstances,
			const Vector4* roomAmbients,
			const RoomSunData* roomSuns,
			int numRooms,
			float time);

		// Add a bending influence (call each frame for active influences).
		void AddInfluence(const Vector3& position, float radius, float intensity);

		bool IsEnabled() const { return _enabled && !_allBlades.empty(); }
		void SetEnabled(bool enabled) { _enabled = enabled; }
		void SetDebugMode(bool debug) { _debugMode = debug; }

		// Debug info.
		int GetTotalBladeCount() const { return (int)_allBlades.size(); }
		int GetVisibleTileCount() const { return _lastVisibleTileCount; }
		int GetVisibleBladeCount() const { return _lastVisibleBladeCount; }
		int GetDrawCallCount() const { return _lastDrawCallCount; }

		GrassConfig& GetConfig() { return _config; }

	private:
		void GenerateGrassForRoom(int roomNumber);
		void GenerateBladesForSector(int roomNumber, int worldX, int worldZ, int floorHeight, const Vector3& normal, MaterialType material);
		void BuildTiles();

		bool _enabled       = true;
		bool _debugMode     = false;
		bool _initialized   = false;
		GrassConfig _config;

		// All blades across the entire level, sorted by tile.
		std::vector<GrassBlade> _allBlades;

		// Spatial tiles for frustum culling.
		std::vector<GrassTile> _tiles;

		// Active influence spheres.
		static constexpr int MAX_INFLUENCES = MAX_GRASS_INFLUENCE_SPHERES;
		std::array<GrassInfluence, MAX_GRASS_INFLUENCE_SPHERES> _influences = {};
		float _currentTime = 0.0f;

		// Debug counters.
		int _lastVisibleTileCount  = 0;
		int _lastVisibleBladeCount = 0;
		int _lastDrawCallCount     = 0;
	};
}
