#include "framework.h"
#include "Game/effects/SnowDust.h"

#include <algorithm>

#include "Game/effects/effects.h"
#include "Game/effects/SnowField.h"
#include "Math/Math.h"
#include "Objects/objectslist.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Effects::SnowDust
{
	// Soft white snow tint used as a fallback when no level snow tint is configured.
	static const Vector3 DEFAULT_SNOW_TINT = Vector3(0.96f, 0.98f, 1.0f);

	// Reads the snow surface tint from Settings.Snow.Tint, or returns the default
	// when the level has no meaningful tint set.
	static Vector3 ResolveSnowTint()
	{
		const auto& settings = g_GameFlow->GetSettings()->Snow;
		auto tint = Vector3(
			(float)settings.Tint.GetR() / 255.0f,
			(float)settings.Tint.GetG() / 255.0f,
			(float)settings.Tint.GetB() / 255.0f);

		if (tint.LengthSquared() < 0.05f)
			return DEFAULT_SNOW_TINT;

		return tint;
	}

	void SpawnSnowCompressionPuff(const Vector3& worldPos, int roomNumber,
								  float worldRadius, float intensity)
	{
		// Light gate: ignore everything below a perceptual threshold so the system
		// stays silent when items barely graze the snow surface.
		constexpr float MIN_INTENSITY = 0.08f;

		return;

		if (intensity < MIN_INTENSITY || worldRadius <= 0.0f)
			return;

		float clamped = std::clamp(intensity, 0.0f, 1.0f);

		// Particle count scales with both compression intensity and footprint size.
		int count = (int)std::round(2.0f + clamped * (worldRadius / 24.0f));
		count = std::clamp(count, 1, 8);

		auto tint = ResolveSnowTint();
		unsigned char br = (unsigned char)std::clamp((int)(tint.x * 255.0f), 0, 255);
		unsigned char bg = (unsigned char)std::clamp((int)(tint.y * 255.0f), 0, 255);
		unsigned char bb = (unsigned char)std::clamp((int)(tint.z * 255.0f), 0, 255);

		for (int i = 0; i < count; i++)
		{
			auto* spark = GetFreeParticle();

			spark->on = true;
			spark->SpriteSeqID = ID_DEFAULT_SPRITES;
			spark->SpriteID = SPR_FIRE1;
			spark->blendMode = BlendMode::Additive;

			spark->sR = br;
			spark->sG = bg;
			spark->sB = bb;
			spark->dR = (unsigned char)(br / 2);
			spark->dG = (unsigned char)(bg / 2);
			spark->dB = (unsigned char)(bb / 2);
			spark->colFadeSpeed = 4;
			spark->fadeToBlack = 6 + (int)(clamped * 8.0f);
			spark->life = spark->sLife = 10 + (GetRandomControl() & 0xF) + (int)(clamped * 18.0f);

			// Random offset inside the footprint disc, kept tight to the impact point.
			float ang = Random::GenerateFloat(0.0f, PI * 2.0f);
			float r = Random::GenerateFloat(0.0f, worldRadius * 0.6f);
			spark->x = (int)(worldPos.x + std::cos(ang) * r);
			spark->y = (int)(worldPos.y - Random::GenerateFloat(0.0f, worldRadius * 0.25f));
			spark->z = (int)(worldPos.z + std::sin(ang) * r);

			// Outward + slight upward kick proportional to intensity. Y is down, so
			// negative yVel rises.
			float kick = 8.0f + clamped * 20.0f;
			spark->xVel = (short)(std::cos(ang) * kick + Random::GenerateFloat(-4.0f, 4.0f));
			spark->zVel = (short)(std::sin(ang) * kick + Random::GenerateFloat(-4.0f, 4.0f));
			spark->yVel = (short)(-8.0f - clamped * 24.0f);

			spark->friction = 5;
			spark->gravity = (short)(-2 - (GetRandomControl() & 1));
			spark->maxYvel = (short)(-2 - (GetRandomControl() & 1));

			spark->roomNumber = roomNumber;
			spark->flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;
			spark->rotAng = GetRandomControl() & 0xFFF;
			spark->rotAdd = (short)(((GetRandomControl() & 1) ? -1 : 1) * (8 + (GetRandomControl() & 7)));
			spark->scalar = 2;

			float sizeBase = worldRadius * (0.5f + clamped * 1.2f);
			spark->sSize = spark->size = sizeBase * 0.4f;
			spark->dSize = sizeBase * 1.4f;
		}
	}

	void SpawnSnowExplosionBurst(const Vector3& worldPos, int roomNumber, float worldRadius)
	{
		if (worldRadius <= 0.0f)
			return;

		// Deform the heightmap at the blast site so the visual carves a real crater
		// into any nearby active snow field. No-op outside snowy areas.
		SnowField::Stamp(worldPos, worldRadius, 1.0f);

		auto tint = ResolveSnowTint();
		unsigned char br = (unsigned char)std::clamp((int)(tint.x * 255.0f), 0, 255);
		unsigned char bg = (unsigned char)std::clamp((int)(tint.y * 255.0f), 0, 255);
		unsigned char bb = (unsigned char)std::clamp((int)(tint.z * 255.0f), 0, 255);

		// Slightly darker, bluer tint for wet slush clumps.
		unsigned char sr = (unsigned char)std::clamp((int)(br * 0.70f), 0, 255);
		unsigned char sg = (unsigned char)std::clamp((int)(bg * 0.78f), 0, 255);
		unsigned char sb = (unsigned char)std::clamp((int)(bb * 0.88f), 0, 255);

		// Counts scale with blast radius. Clamped to keep the particle pool sane.
		int slushCount = std::clamp((int)(worldRadius / 24.0f) + 8, 8, 64);
		int dustCount  = std::clamp((int)(worldRadius / 12.0f) + 16, 16, 96);

		// Wet slush chunks: heavy, ballistic, alpha-blended.
		for (int i = 0; i < slushCount; i++)
		{
			auto* spark = GetFreeParticle();

			spark->on = true;
			spark->SpriteSeqID = ID_DEFAULT_SPRITES;
			spark->SpriteID = SPR_UNDERWATERDUST;
			spark->blendMode = BlendMode::AlphaBlend;

			spark->sR = sr;
			spark->sG = sg;
			spark->sB = sb;
			spark->dR = (unsigned char)(sr / 3);
			spark->dG = (unsigned char)(sg / 3);
			spark->dB = (unsigned char)(sb / 3);
			spark->colFadeSpeed = 6;
			spark->fadeToBlack = 24;
			spark->life = spark->sLife = 60 + (GetRandomControl() & 0x1F);

			float ang = Random::GenerateFloat(0.0f, PI * 2.0f);
			float r = Random::GenerateFloat(0.0f, worldRadius * 0.3f);
			spark->x = (int)(worldPos.x + std::cos(ang) * r);
			spark->y = (int)(worldPos.y + Random::GenerateFloat(-worldRadius * 0.2f, 0.0f));
			spark->z = (int)(worldPos.z + std::sin(ang) * r);

			float vh = Random::GenerateFloat(worldRadius * 0.06f, worldRadius * 0.14f);
			float vv = Random::GenerateFloat(worldRadius * 0.06f, worldRadius * 0.18f);
			spark->xVel = (short)(std::cos(ang) * vh);
			spark->zVel = (short)(std::sin(ang) * vh);
			spark->yVel = (short)(-vv);

			spark->friction = 3;
			spark->gravity = (short)(3 + (GetRandomControl() & 3));
			spark->maxYvel = 0;

			spark->roomNumber = roomNumber;
			spark->flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;
			spark->rotAng = GetRandomControl() & 0xFFF;
			spark->rotAdd = (short)(((GetRandomControl() & 1) ? -1 : 1) * (16 + (GetRandomControl() & 0xF)));
			spark->scalar = 2;

			float sizeBase = worldRadius * 0.18f;
			spark->sSize = spark->size = sizeBase * Random::GenerateFloat(0.6f, 1.1f);
			spark->dSize = sizeBase * Random::GenerateFloat(1.2f, 2.0f);
		}

		// Dust cloud: light, rises, fades. Additive for a softer fog look.
		for (int i = 0; i < dustCount; i++)
		{
			auto* spark = GetFreeParticle();

			spark->on = true;
			spark->SpriteSeqID = ID_DEFAULT_SPRITES;
			spark->SpriteID = SPR_UNDERWATERDUST;
			spark->blendMode = BlendMode::Additive;

			spark->sR = br;
			spark->sG = bg;
			spark->sB = bb;
			spark->dR = (unsigned char)(br / 4);
			spark->dG = (unsigned char)(bg / 4);
			spark->dB = (unsigned char)(bb / 4);
			spark->colFadeSpeed = 4;
			spark->fadeToBlack = 32;
			spark->life = spark->sLife = 80 + (GetRandomControl() & 0x3F);

			float ang = Random::GenerateFloat(0.0f, PI * 2.0f);
			float r = Random::GenerateFloat(0.0f, worldRadius * 0.5f);
			spark->x = (int)(worldPos.x + std::cos(ang) * r);
			spark->y = (int)(worldPos.y - Random::GenerateFloat(0.0f, worldRadius * 0.3f));
			spark->z = (int)(worldPos.z + std::sin(ang) * r);

			float vh = Random::GenerateFloat(worldRadius * 0.02f, worldRadius * 0.08f);
			spark->xVel = (short)(std::cos(ang) * vh);
			spark->zVel = (short)(std::sin(ang) * vh);
			spark->yVel = (short)(-Random::GenerateFloat(worldRadius * 0.04f, worldRadius * 0.10f));

			spark->friction = 6;
			spark->gravity = (short)(-2 - (GetRandomControl() & 1));
			spark->maxYvel = (short)(-3 - (GetRandomControl() & 1));

			spark->roomNumber = roomNumber;
			spark->flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;
			spark->rotAng = GetRandomControl() & 0xFFF;
			spark->rotAdd = (short)(((GetRandomControl() & 1) ? -1 : 1) * (4 + (GetRandomControl() & 7)));
			spark->scalar = 3;

			float sizeBase = worldRadius * 0.35f;
			spark->sSize = spark->size = sizeBase * Random::GenerateFloat(0.4f, 0.8f);
			spark->dSize = sizeBase * Random::GenerateFloat(1.4f, 2.4f);
		}
	}
}
