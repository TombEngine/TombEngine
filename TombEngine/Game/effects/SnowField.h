#pragma once

#include <SimpleMath.h>
#include <vector>

struct ItemInfo;

namespace TEN::Effects::SnowField
{
	using namespace DirectX::SimpleMath;

	// R8 heightmap resolution. The grid covers Snow.FieldRadius * 2 world units
	// centered around the player.
	constexpr int RESOLUTION = 256;

	void Initialize();
	void Deinitialize();

	bool IsActive();

	// Per-tick update: recenters around player, injects foot/body deformations,
	// applies decay.
	void Update(const ItemInfo& player);

	// Renderer-side accessors.
	const std::vector<unsigned char>& GetHeightmap();
	Vector2 GetWorldCentre();
	float   GetWorldRadius();

	// Stamps a circular deformation into the heightmap. World units. `depth` is in
	// [0..1] where 1 pushes the snow surface fully down to the floor. Safe to call
	// any time after Initialize() - silently ignored outside the active field.
	// Use this for explosions, custom scripted impacts, etc.
	void Stamp(const Vector3& worldPos, float worldRadius, float depth);

	// Returns the effective snow surface Y at a world XZ position, derived from the
	// heightmap. Y is down: returned value is <= floorY (raised surface = lower Y).
	// Returns floorY unchanged when the snow field is inactive or the position is
	// outside the field bounds.
	float GetSnowSurfaceY(float worldX, float worldZ, float floorY);

	// Returns true when `worldPos` is inside the active snow layer: below the raised
	// snow surface and above (or at) the raw floor. Safe to call every tick; returns
	// false immediately when the snow system is inactive.
	bool IsPositionInSnow(const Vector3i& worldPos, int roomNumber);
}
