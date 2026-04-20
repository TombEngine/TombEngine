#include "framework.h"
#include "Game/effects/Light.h"

#include "Renderer/Renderer.h"

using TEN::Renderer::g_Renderer;

namespace TEN::Effects::Light
{
	void SpawnDynamicPointLight(const Vector3& pos, const Color& color, float falloff, bool castShadows, int hash)
	{
		g_Renderer.AddDynamicPointLight(pos, falloff, color, castShadows, hash);
	}

	void SpawnDynamicSpotLight(const Vector3& pos, const Vector3& dir, const Color& color, float radius, float falloff, float dist, bool castShadows, int hash)
	{
		g_Renderer.AddDynamicSpotLight(pos, dir, radius, falloff, dist, color, castShadows, hash);
	}

	void SpawnDynamicLight(int x, int y, int z, short falloff, unsigned char r, unsigned char g, unsigned char b)
	{
		g_Renderer.AddDynamicPointLight(Vector3(x, y, z), float(falloff * UCHAR_MAX), Color(r / (float)UCHAR_MAX, g / (float)UCHAR_MAX, b / (float)UCHAR_MAX), false);
	}

	void SpawnDynamicFogBulb(const Vector3& pos, short radius, short density, const Color& color, int hash)
	{
		g_Renderer.AddDynamicFogBulb(pos, float(radius * UCHAR_MAX), float(density / (float)UCHAR_MAX), color, hash);
	}
}
