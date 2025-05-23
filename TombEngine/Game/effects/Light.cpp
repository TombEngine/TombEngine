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

	void SpawnDynamicLight(int x, int y, int z, short falloff, byte r, byte g, byte b)
	{
		g_Renderer.AddDynamicPointLight(Vector3(x, y, z), float(falloff * UCHAR_MAX), Color(r / (float)CHAR_MAX, g / (float)CHAR_MAX, b / (float)CHAR_MAX), false);
	}
}
