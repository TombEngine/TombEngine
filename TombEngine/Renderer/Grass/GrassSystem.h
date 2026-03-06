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
		Vector3 Position  = Vector3::Zero;
		Vector3 Normal    = Vector3::UnitY;
		float   Scale     = 1.0f;
		float   Seed      = 0.0f;
		Vector4 Color     = Vector4::One;
		int     SpriteIdx = 0; // Index into grass atlas sprite variants.
	};

	// A spatial tile containing grass blades for culling.
	struct GrassTile
	{
		BoundingBox    Bounds;
		int            RoomNumber  = 0;
		int            StartIndex  = 0;  // Into the owning GrassGrid's blade array.
		int            BladeCount  = 0;
	};

	// Influence sphere for bending effect (e.g., player walking through grass).
	struct GrassInfluence
	{
		Vector3 Position   = Vector3::Zero;
		float   Radius     = 0.0f;
		float   Intensity  = 0.0f;
		float   Timestamp  = 0.0f;
		bool    Active     = false;
	};

	// Configuration for the grass system.
	struct GrassConfig
	{
		// Placement.
		float Density           = 16.0f;  // Blades per BLOCK(1) unit on each axis.
		float MinScale          = 0.6f;
		float MaxScale          = 1.5f;
		float JitterAmount      = 1.0f;   // 0 = grid, 1 = full jitter within cell.

		// Rendering.
		float BladeWidth        = 56.0f;  // World units.
		float BladeHeight       = 350.0f; // World units (~1/3 block).
		float MaxDrawDistance    = 16384.0f;  // ~16 blocks.
		float FadeStartDistance = 12288.0f;  // ~12 blocks.

		// Wind.
		float   WindStrength    = 25.0f;
		float   WindFrequency   = 1.5f;
		Vector3 WindDirection   = Vector3(1.0f, 0.0f, 0.3f);

		// Bending.
		float BendRiseSpeed     = 10.0f;
		float BendDecaySpeed    = 1.5f;
		float BendMaxAngle      = 250.0f; // World units of displacement at full bend.
		float PlayerBendRadius  = 512.0f; // ~2 clicks.
		float PlayerBendIntensity = 1.0f;

		// Atlas layout (assuming a simple grid of sprite variants in one texture).
		int   AtlasColumns      = 4;
		int   AtlasRows         = 1;
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

		// Active influence spheres (ring buffer).
		static constexpr int MAX_INFLUENCES = MAX_GRASS_INFLUENCE_SPHERES;
		std::array<GrassInfluence, MAX_GRASS_INFLUENCE_SPHERES> _influences = {};
		int _nextInfluenceSlot = 0;
		float _currentTime = 0.0f;

		// Debug counters.
		int _lastVisibleTileCount  = 0;
		int _lastVisibleBladeCount = 0;
		int _lastDrawCallCount     = 0;
	};
}
