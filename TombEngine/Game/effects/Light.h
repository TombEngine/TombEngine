#pragma once

namespace TEN::Effects::Light
{
	void SpawnDynamicPointLight(const Vector3& pos, const Color& color, float falloff, bool castShadows = false, int hash = 0);
	void SpawnDynamicSpotLight(const Vector3& pos, const Vector3& dir, const Color& color, float radius, float falloff, float dist, bool castShadows = false, int hash = 0);
	
	// DEPRECATED!!! Use SpawnDynamicPointLight() instead and phase out this legacy function.
	void SpawnDynamicLight(int x, int y, int z, short falloff, byte r, byte g, byte b);
}
