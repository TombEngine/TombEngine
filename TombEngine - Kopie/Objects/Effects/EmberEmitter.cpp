#include "framework.h"
#include "Objects/Effects/EmberEmitter.h"

#include "Game/effects/effects.h"
#include "Game/Setup.h"
#include "Math/Math.h"

using namespace TEN::Math;

namespace TEN::Effects::EmberEmitter
{
	void ControlEmberEmitter(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		unsigned char r = 0;
		unsigned char g = 0;
		unsigned char b = 0;

		float brightnessShift = Random::GenerateFloat(-0.1f, 0.1f);
		r = std::clamp(item.Model.Color.x / 2.0f + brightnessShift, 0.0f, 1.0f) * UCHAR_MAX;
		g = std::clamp(item.Model.Color.y / 2.0f + brightnessShift, 0.0f, 1.0f) * UCHAR_MAX;
		b = std::clamp(item.Model.Color.z / 2.0f + brightnessShift, 0.0f, 1.0f) * UCHAR_MAX;
		
		if (item.TriggerFlags < 0)
		{
			if (item.TriggerFlags > -11)
				item.TriggerFlags = -11;

			if (!item.ItemFlags[2])
			{
				int div = (abs(item.TriggerFlags) % 10) << 10;
				int mod = (abs(item.TriggerFlags) / 10) << 10;

				// TODO: Use Random::GenerateInt()
				item.ItemFlags[0] = GetRandomControl() % div;
				item.ItemFlags[1] = GetRandomControl() % mod;
				item.ItemFlags[2] = (GetRandomControl() & 0xF) + 15;
			}

			if (--item.ItemFlags[2] < 15)
			{
				auto& spark = *GetFreeParticle();

				spark.on = true;
				spark.sR = -1;
				spark.sB = 16;
				spark.sG = Random::GenerateFloat(0.2f, 0.3f) * UCHAR_MAX;
				spark.dR = (GetRandomControl() & 0x3F) - 64;
				spark.dB = 0;
				spark.dG = (GetRandomControl() & 0x3F) - 128;
				spark.fadeToBlack = 4;
				spark.colFadeSpeed = Random::GenerateFloat(4.0f, 8.0f);
				spark.blendMode = BlendMode::Additive;
				spark.life =
				spark.sLife = Random::GenerateFloat(24.0f, 28.0f);
				spark.x = item.ItemFlags[1] + Random::GenerateFloat(0.0f, 64.0f) + item.Pose.Position.x - 544;
				spark.y = item.Pose.Position.y;
				spark.z = item.ItemFlags[0] + Random::GenerateFloat(0.0f, 64.0f) + item.Pose.Position.z - 544;
				spark.xVel = Random::GenerateFloat(-BLOCK(0.25f), BLOCK(0.25f));
				spark.friction = 6.0f;
				spark.zVel = Random::GenerateFloat(-BLOCK(0.25f), BLOCK(0.25f));
				spark.rotAng = Random::GenerateAngle(ANGLE(0.0f), ANGLE(20.0f));
				spark.rotAdd = Random::GenerateAngle(ANGLE(-0.2f), ANGLE(0.2f));
				spark.maxYvel = 0.0f;
				spark.yVel = -512 - (GetRandomControl() & 0x3FF);
				spark.sSize =
				spark.size = Random::GenerateFloat(32.0f, 48.0f);
				spark.dSize = spark.size / 4;

				if (GetRandomControl() & 3)
				{
					spark.scalar = 3.0f;
					spark.gravity = Random::GenerateFloat(32.0f, 96.0f);
					spark.flags = SP_DAMAGE | SP_ROTATE | SP_DEF | SP_SCALE | SP_EXPDEF;
					spark.damage = 2;
				}
				else
				{
					spark.SpriteSeqID = ID_DEFAULT_SPRITES;
					spark.SpriteID = SPR_UNDERWATERDUST;
					spark.scalar = 1.0f;
					spark.gravity = Random::GenerateFloat(64.0f, 80.0f);
					spark.flags = SP_ROTATE | SP_DEF | SP_SCALE;
				}
			}
		}
		else
		{
			float size = 0.0f;
			if (item.TriggerFlags)
			{
				size = item.TriggerFlags / 10.0f;
			}
			else
			{
				size = 1.0f;
			}

			if (GetRandomControl() & 7)
			{
				auto& spark = *GetFreeParticle();

				spark.on = true;
				spark.fadeToBlack = 4.0f;
				spark.colFadeSpeed = Random::GenerateFloat(4.0f, 8.0f);

				// NOTE: Only black has subtractive blending mode.
				if (item.Model.Color.x == 0.0f &&
					item.Model.Color.y == 0.0f &&
					item.Model.Color.z == 0.0f)
				{
					spark.sR =
					spark.sG =
					spark.sB =
					spark.dR =
					spark.dG =
					spark.dB = 1.0f * UCHAR_MAX;
					spark.blendMode = BlendMode::Subtractive;
				}
				else
				{
					spark.sR = r;
					spark.sG = g;
					spark.sB = b;
					spark.dR = r;
					spark.dG = g;
					spark.dB = b;
					spark.blendMode = BlendMode::Additive;
				}

				spark.SpriteSeqID = ID_DEFAULT_SPRITES;
				spark.SpriteID = SPR_UNDERWATERDUST;
				spark.life =
				spark.sLife = Random::GenerateFloat(74.0f, 78.0f);
				spark.x = item.Pose.Position.x + Random::GenerateFloat(0.0f, 22.0f);
				spark.y = item.Pose.Position.y;
				spark.z = item.Pose.Position.z + Random::GenerateFloat(0.0f, 22.0f);
				spark.rotAng = Random::GenerateAngle();
				spark.yVel = -BLOCK(0.1f + size) - Random::GenerateFloat(0.0f, BLOCK(0.75f + size));
				spark.gravity = Random::GenerateFloat(64.0f, 80.0f);
				spark.xVel = Random::GenerateFloat(-368.0f * size, 368.0f * size);
				spark.friction = 15;
				spark.maxYvel = 0;
				spark.zVel = Random::GenerateFloat(-368.0f * size, 368.0f * size);
				spark.scalar = 1.0f;
				spark.sSize =
				spark.size = Random::GenerateFloat(32.0f, 48.0f);
				spark.dSize = spark.size;
				spark.flags = SP_DAMAGE | SP_ROTATE | SP_DEF | SP_SCALE;
				spark.damage = 2;
			}
		}
	}
}
