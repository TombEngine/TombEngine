#include "framework.h"
#include "Game/effects/effects.h"

#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Game/animation.h"
#include "Game/control/box.h"
#include "Game/control/los.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/effects/Blood.h"
#include "Game/effects/Bubble.h"
#include "Game/effects/Drip.h"
#include "Game/effects/explosion.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/Light.h"
#include "Game/effects/Ripple.h"
#include "Game/effects/smoke.h"
#include "Game/effects/spark.h"
#include "Game/effects/Splash.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/objectslist.h"
#include "Objects/TR5/Emitter/Waterfall.h"
#include "Renderer/Renderer.h"
#include "Sound/sound.h"
#include "Specific/clock.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Blood;
using namespace TEN::Effects::Bubble;
using namespace TEN::Effects::Drip;
using namespace TEN::Effects::Environment;
using namespace TEN::Effects::Explosion;
using namespace TEN::Effects::Items;
using namespace TEN::Effects::Light;
using namespace TEN::Effects::Ripple;
using namespace TEN::Effects::Spark;
using namespace TEN::Effects::Splash;
using namespace TEN::Effects::WaterfallEmitter;
using namespace TEN::Math;
using namespace TEN::Math::Random;

using TEN::Renderer::g_Renderer;

constexpr int WIBBLE_SPEED = 4;
constexpr int WIBBLE_MAX = UCHAR_MAX - WIBBLE_SPEED + 1;

// New particle class
Particle Particles[MAX_PARTICLES];
ParticleDynamic ParticleDynamics[MAX_PARTICLE_DYNAMICS];

FX_INFO EffectList[MAX_SPAWNED_ITEM_COUNT];

GameBoundingBox DeadlyBounds;

int Wibble = 0;

Vector3i NodeVectors[ParticleNodeOffsetIDs::NodeMax];
NODEOFFSET_INFO NodeOffsets[ParticleNodeOffsetIDs::NodeMax] =
{
	{ -16, 40, 160, 13, false },		// TR5 offset 0
	{ -16, -8, 160, 0, false },			// TR5 offset 1
	{ 0, 0, 256, 8, false },			// TR5 offset 2
	{ 0, 0, 256, 17, false },			// TR5 offset 3
	{ 0, 0, 256, 26, false },			// TR5 offset 4
	{ 0, 144, 40, 10, false },			// TR5 offset 5
	{ -40, 64, 360, 14, false },		// TR5 offset 6
	{ 0, -600, -40, 0, false },			// TR5 offset 7
	{ 0, 32, 16, 9, false },			// TR5 offset 8
	{ 0, 340, 64, 7, false },			// TR3 offset 9
	{ 0, 0, -96, 10, false },			// TR3 offset 10
	{ 16, 48, 320, 13, false },			// TR3 offset 11
	{ 0, -256, 0, 5, false },			// TR3 offset 12
	{ 0, 64, 0, 10, false },			// TR3 offset 13
	{ 0, 64, 0, 13, false },			// TR3 offset 14
	{ -32, -16, -192, 13, false },		// TR3 offset 15
	{ -64, 410, 0, 20, false },			// TR3 offset 16
	{ 64, 410, 0, 23, false },			// TR3 offset 17
	{ 0, 0, 0, 0, false }				// Empty offset 18
};

void DetatchSpark(int number, SpriteEnumFlag type)
{
	auto* sptr = &Particles[0];

	for (int lp = 0; lp < MAX_PARTICLES; lp++, sptr++)
	{
		if (sptr->on && (sptr->flags & type) && sptr->fxObj == number)
		{
			switch (type)
			{
				case SP_FX:
					if (sptr->flags & SP_DAMAGE)
						sptr->on = false;
					else
					{
						auto* fx = &EffectList[number];

						sptr->x += fx->pos.Position.x;
						sptr->y += fx->pos.Position.y;
						sptr->z += fx->pos.Position.z;
						sptr->flags &= ~SP_FX;
					}

					break;

				case SP_ITEM:
					if (sptr->flags & SP_DAMAGE)
						sptr->on = false;
					else
					{
						auto* item = &g_Level.Items[number];

						sptr->x += item->Pose.Position.x;
						sptr->y += item->Pose.Position.y;
						sptr->z += item->Pose.Position.z;
						sptr->flags &= ~SP_ITEM;
					}

					break;
			}
		}
	}
}

Particle* GetFreeParticle()
{
	int partID = NO_VALUE;

	// Get first free available particle.
	for (int i = 0; i < MAX_PARTICLES; i++)
	{
		const auto& part = Particles[i];
		if (!part.on)
		{
			partID = i;
			break;
		}
	}

	// No free particles; get particle with shortest life.
	float shortestLife = INFINITY;
	if (partID == NO_VALUE)
	{
		for (int i = 0; i < MAX_PARTICLES; i++)
		{
			const auto& part = Particles[i];

			if (part.life < shortestLife && part.dynamic == NO_VALUE && !(part.flags & SP_EXPLOSION))
			{
				partID = i;
				shortestLife = part.life;
			}
		}
	}

	auto& part = Particles[partID];
	part.SpriteSeqID = ID_DEFAULT_SPRITES;
	part.SpriteID = 0;
	part.blendMode = BlendMode::Additive;
	part.extras = 0;
	part.dynamic = NO_VALUE;

	return &part;
}

void SetSpriteSequence(Particle& particle, GAME_OBJECT_ID objectID)
{
	if (particle.life <= 0)
	{
		particle.on = false;
		ParticleDynamics[particle.dynamic].On = false;
	}

	float particleAge = particle.sLife - particle.life;
	if (particleAge > particle.life )
		return;	

	int spriteCount = -Objects[objectID].nmeshes - 1;
	float normalizedAge = particleAge / particle.life;
	particle.SpriteSeqID = objectID;
	particle.SpriteID = (int)round(Lerp(0.0f, spriteCount, normalizedAge));
}

void SetAdvancedSpriteSequence(Particle& particle, GAME_OBJECT_ID objectID,	ParticleAnimType animationType, float frameRate)
{
	// Ensure valid lifespan
	if (particle.life <= 0)
	{
		particle.on = false;
		ParticleDynamics[particle.dynamic].On = false;
		return;
	}

	// Calculate particle's age and normalized progress
	float particleAge = particle.sLife - particle.life;  // Elapsed time since spawn
	float normalizedAge = particleAge / particle.sLife;  // Progress as a fraction [0.0, 1.0]

	// Retrieve sprite sequence information
	//int firstFrame = Objects[objectID].meshIndex;          // Starting sprite index
	int totalFrames = -Objects[objectID].nmeshes;          // Total frames (assuming nmeshes is negative)
	if (totalFrames <= 0)
	{
		particle.SpriteSeqID = objectID;
		particle.SpriteID = 0;  // Default to the first frame if no valid frames exist
		return;
	}

	particle.SpriteSeqID = objectID;

	// Handle animation modes
	switch (animationType)
	{
	case ParticleAnimType::Loop:  // Frames loop sequentially
	{
		float frameDuration = frameRate > 0 ? 1.0f / frameRate : 1.0f / totalFrames;  // Duration per frame
		int currentFrame = (int)(particleAge / frameDuration) % totalFrames;  // Wrap frames
		particle.SpriteID = currentFrame;
		break;
	}

	case ParticleAnimType::OneShot:  // Frames play once, then freeze on the last frame
	{
		float totalDuration = frameRate > 0 ? totalFrames / frameRate : particle.sLife;
		int currentFrame = (int)(particleAge / (totalDuration / totalFrames));
		if (currentFrame >= totalFrames)
			currentFrame = totalFrames - 1;  // Clamp to the last frame
		particle.SpriteID = currentFrame;
		break;
	}

	case ParticleAnimType::BackAndForth:  // Frames go forward and then backward
	{
		float frameDuration = frameRate > 0 ? 1.0f / frameRate : 1.0f / totalFrames;
		int totalFrameSteps = totalFrames * 2 - 2;  // Forward and backward frames (avoiding double-count of last frame)
		int step = (int)(particleAge / frameDuration) % totalFrameSteps;
		int currentFrame = step < totalFrames ? step : totalFrames - (step - totalFrames) - 1;
		particle.SpriteID = currentFrame;
		break;
	}

	case ParticleAnimType::LifetimeSpread:  // Distribute all frames evenly over lifetime
	{
		int currentFrame = (int)(normalizedAge * totalFrames);
		if (currentFrame >= totalFrames)
			currentFrame = totalFrames - 1;  // Clamp to the last frame
		particle.SpriteID = currentFrame;
		break;
	}

	case ParticleAnimType::None:  // Distribute all frames evenly over lifetime
	{
		particle.SpriteID = 0;
		break;
	}


	default:  // Default behavior: keep the first frame
		particle.SpriteID = 0;
		break;
	}
}


void UpdateWibble()
{
	// Update oscillator seed.
	Wibble = (Wibble + WIBBLE_SPEED) & WIBBLE_MAX;
}

void UpdateSparks()
{
	auto bounds = GameBoundingBox(LaraItem);
	DeadlyBounds = GameBoundingBox(
		LaraItem->Pose.Position.x + bounds.X1,
		LaraItem->Pose.Position.x + bounds.X2,
		LaraItem->Pose.Position.y + bounds.Y1,
		LaraItem->Pose.Position.y + bounds.Y2,
		LaraItem->Pose.Position.z + bounds.Z1,
		LaraItem->Pose.Position.z + bounds.Z2);

	for (int i = 0; i < MAX_PARTICLES; i++)
	{
		auto& spark = Particles[i];

		if (spark.on)
		{
			spark.StoreInterpolationData();

			spark.life--;

			if (!spark.life)
			{
				if (spark.dynamic != -1)
					ParticleDynamics[spark.dynamic].On = false;

				spark.on = false;
				continue;
			}

			if (HandleWaterfallParticle(spark))
				continue;
			
			int life = spark.sLife - spark.life;
			if (life < spark.colFadeSpeed)
			{
				int dl = (life << 16) / spark.colFadeSpeed;
				spark.r = spark.sR + (dl * (spark.dR - spark.sR) >> 16);
				spark.g = spark.sG + (dl * (spark.dG - spark.sG) >> 16);
				spark.b = spark.sB + (dl * (spark.dB - spark.sB) >> 16);
			}
			else if (spark.life >= spark.fadeToBlack)
			{
				spark.r = spark.dR;
				spark.g = spark.dG;
				spark.b = spark.dB;
			}
			else
			{
				spark.r = (spark.dR * (((spark.life - spark.fadeToBlack) << 16) / spark.fadeToBlack + 0x10000)) >> 16;
				spark.g = (spark.dG * (((spark.life - spark.fadeToBlack) << 16) / spark.fadeToBlack + 0x10000)) >> 16;
				spark.b = (spark.dB * (((spark.life - spark.fadeToBlack) << 16) / spark.fadeToBlack + 0x10000)) >> 16;

				if (spark.r < 8 && spark.g < 8 && spark.b < 8)
				{
					spark.on = 0;
					continue;
				}
			}

			if (spark.life == spark.colFadeSpeed)
			{
				if (spark.flags & SP_UNDERWEXP)
					spark.dSize /= 4;
			}

			if (spark.flags & SP_ROTATE)
				spark.rotAng = (spark.rotAng + spark.rotAdd) & 0x0FFF;

			if (spark.sLife - spark.life == spark.extras >> 3 &&
					spark.extras & 7)
			{
				int explosionType;

				if (spark.flags & SP_UNDERWEXP)
				{
					explosionType = 1;
				}
				else if (spark.flags & SP_PLASMAEXP)
				{
					explosionType = 2;
				}
				else
				{
					explosionType = 0;
				}

				for (int j = 0; j < (spark.extras & 7); j++)
				{
					if (spark.flags & SP_COLOR)
					{
						TriggerExplosionSparks(
							spark.x, spark.y, spark.z,
							(spark.extras & 7) - 1,
							spark.dynamic,
							explosionType,
							spark.roomNumber,
							Vector3(spark.dR, spark.dG, spark.dB),
							Vector3(spark.sR, spark.sG, spark.sB));
					}
					else
					{
						TriggerExplosionSparks(
							spark.x, spark.y, spark.z,
							(spark.extras & 7) - 1,
							spark.dynamic,
							explosionType,
							spark.roomNumber);
					}

					spark.dynamic = -1;
				}

				if (explosionType == 1)
				{
					TriggerExplosionBubble(
						spark.x,
						spark.y,
						spark.z,
						spark.roomNumber);
				}

				spark.extras = 0;
			}

			spark.yVel += spark.gravity;
			if (spark.maxYvel)
			{
				if (spark.yVel > spark.maxYvel)
					spark.yVel = spark.maxYvel;
			}

			if (spark.friction & 0xF)
			{
				spark.xVel -= spark.xVel >> (spark.friction & 0xF);
				spark.zVel -= spark.zVel >> (spark.friction & 0xF);
			}

			if (spark.friction & 0xF0)
				spark.yVel -= spark.yVel >> (spark.friction >> 4);

			spark.x += spark.xVel >> 5;
			spark.y += spark.yVel >> 5;
			spark.z += spark.zVel >> 5;

			if (spark.flags & SP_WIND)
			{
				spark.x += Weather.Wind().x;
				spark.z += Weather.Wind().z;
			}

			int dl = ((spark.sLife - spark.life) * 65536) / spark.sLife;
			spark.size = (spark.sSize + ((dl * (spark.dSize - spark.sSize)) / 65536));

			if (spark.flags & SP_EXPLOSION)
				SetSpriteSequence(spark, ID_EXPLOSION_SPRITES);


			if (spark.flags & SP_ANIMATED)
			{
				ParticleAnimType animationType = static_cast<ParticleAnimType>(spark.animationType);
				GAME_OBJECT_ID spriteObject = static_cast<GAME_OBJECT_ID>(spark.SpriteSeqID);
				SetAdvancedSpriteSequence(spark, spriteObject,  animationType, spark.framerate);
			}

			if (spark.flags & SP_SOUND)
				SoundEffect(spark.sound, &Pose(Vector3(spark.x, spark.y, spark.z)), SoundEnvironment::Always);

			if (spark.flags & SP_LIGHT)
			{
				float radius = spark.lightRadius * spark.size / spark.sSize;
				// Decrease flicker timer if set
				if (spark.lightFlicker > 0)
				{
					spark.lightFlicker--;

					if (spark.lightFlicker <= 0)
					{
						// Apply random flicker effect
						int random = GetRandomControl();
						int colorOffset = (random % 21) - 10; // Random change between -10 and +10

						byte r = std::clamp(spark.r + colorOffset, 0, 255);
						byte g = std::clamp(spark.g + colorOffset, 0, 255);
						byte b = std::clamp(spark.b + colorOffset, 0, 255);

						// Reset flicker timer
						spark.lightFlicker = spark.lightFlickerS;

						// Emit flickering light
						SpawnDynamicPointLight(Vector3(spark.x, spark.y, spark.z), ScriptColor(r, g, b), radius, false, GetHash(std::string()));
					}
					else
					{
						// Normal light emission while flicker is counting down
						SpawnDynamicPointLight(Vector3(spark.x, spark.y, spark.z), ScriptColor(spark.r, spark.g, spark.b), radius, false, GetHash(std::string()));
					}
				}
				else
				{
					// If flicker is disabled or 0, just emit normal light
					SpawnDynamicPointLight(Vector3(spark.x, spark.y, spark.z), ScriptColor(spark.r, spark.g, spark.b), radius, false, GetHash(std::string()));
				}
			}

			if ((spark.flags & SP_FIRE && LaraItem->Effect.Type == EffectType::None) ||
				(spark.flags & SP_DAMAGE) || 
				(spark.flags & SP_POISON))
			{
				int ds = spark.size * (spark.scalar / 2.0);

				if (spark.x + ds > DeadlyBounds.X1 && spark.x - ds < DeadlyBounds.X2)
				{
					if (spark.y + ds > DeadlyBounds.Y1 && spark.y - ds < DeadlyBounds.Y2)
					{
						if (spark.z + ds > DeadlyBounds.Z1 && spark.z - ds < DeadlyBounds.Z2)
						{
							if (spark.flags & SP_FIRE)
								ItemBurn(LaraItem);

							if (spark.flags & SP_DAMAGE)
								DoDamage(LaraItem, spark.damage);

							if (spark.flags & SP_POISON)
								Lara.Status.Poison += spark.damage;
						}
					}
				}
			}
		}
	}


	for (int i = 0; i < MAX_PARTICLES; i++)
	{
		auto& spark = Particles[i];

		if (spark.on && spark.dynamic != -1)
		{
			auto* dynsp = &ParticleDynamics[spark.dynamic];
			
			if (dynsp->Flags & 3)
			{
				int random = GetRandomControl();

				int x = spark.x + 16 * (random & 0xF);
				int y = spark.y + (random & 0xF0);
				int z = spark.z + ((random >> 4) & 0xF0);

				byte r, g, b;

				int dl = spark.sLife - spark.life - 1;
				if (dl >= 2)
				{
					if (dl >= 4)
					{
						if (dynsp->Falloff)
							dynsp->Falloff--;

						b = ((random >> 4) & 0x1F) + 128;
						g = (random & 0x1F) + 224;
						r = (random >> 8) & 0x3F;
					}
					else
					{
						if (dynsp->Falloff < 28)
							dynsp->Falloff += 6;

						b = -8 * dl + 128;
						g = -8 * dl - (random & 0x1F) + 255;
						r = 32 * (4 - dl);

						if (32 * (4 - dl) < 0)
							r = 0;
					}
				}
				else
				{
					if (dynsp->Falloff < 28)
						dynsp->Falloff += 6;

					g = 255 - 8 * dl - (random & 0x1F);
					b = 255 - 16 * dl - (random & 0x1F);
					r = 255 - (dl << 6) - (random & 0x1F);
				}

				if (spark.flags & SP_PLASMAEXP)
				{
					int falloff;
					if (dynsp->Falloff <= 28)
						falloff = dynsp->Falloff;
					else
						falloff = 31;

					SpawnDynamicLight(x, y, z, falloff, r, g, b);
				}
				else
				{
					int falloff = (dynsp->Falloff <= 28) ? dynsp->Falloff : 31;

					if (spark.flags & SP_COLOR)
					{
						SpawnDynamicLight(x, y, z, falloff, spark.dR, spark.dG, spark.dB);
					}
					else
					{
						SpawnDynamicLight(x, y, z, falloff, g, b, r);
					}
				}
			}
		}
	}
}

void TriggerRicochetSpark(const GameVector& pos, short angle, bool sound)
{
	int count = Random::GenerateInt(3, 8);
	TriggerRicochetSpark(pos, angle, count);
	SoundEffect(SFX_TR4_WEAPON_RICOCHET, &Pose(pos.ToVector3i()));
}

void TriggerCyborgSpark(int x, int y, int z, short xv, short yv, short zv)
{
	int dx = LaraItem->Pose.Position.x - x;
	int dz = LaraItem->Pose.Position.z - z;

	if (dx >= -BLOCK(16) && dx <= BLOCK(16) &&
		dz >= -BLOCK(16) && dz <= BLOCK(16))
	{
		auto* spark = GetFreeParticle();

		int random = rand();
		
		spark->sR = -1;
		spark->sB = -1;
		spark->sG = -1;
		spark->dR = -1;
		spark->on = true;
		spark->colFadeSpeed = 3;
		spark->fadeToBlack = 5;
		spark->dG = (random & 0x7F) + 64;
		spark->dB = -64 - ((random & 0x7F) + 64);
		spark->life = 10;
		spark->sLife = 10;
		spark->blendMode = BlendMode::Additive;
		spark->friction = 34;
		spark->scalar = 1;
		spark->x = (random & 7) + x - 3;
		spark->y = ((random >> 3) & 7) + y - 3;
		spark->z = ((random >> 6) & 7) + z - 3;
		spark->flags = SP_SCALE;
		spark->xVel = (random >> 2) + xv - 128;
		spark->yVel = (random >> 4) + yv - 128;
		spark->zVel = (random >> 6) + zv - 128;
		spark->sSize = spark->size = ((random >> 9) & 3) + 4;
		spark->dSize = ((random >> 12) & 1) + 1;
		spark->maxYvel = 0;
		spark->gravity = 0;
	}
}

void TriggerExplosionSparks(int x, int y, int z, int extraTrig, int dynamic, int uw, int roomNumber, const Vector3& mainColor, const Vector3& secondColor)
{
	constexpr auto LIFE_MAX		= 44.0f;
	constexpr auto ROTATION_MAX = ANGLE(0.15f);

	static const auto EXTRAS_TABLE = std::array<unsigned char, 4>{ 0, 4, 7, 10 };

	int scalar = 1;

	if (roomNumber < 0)
	{
		roomNumber = -roomNumber;
		scalar = 1;
	}

	auto& spark = *GetFreeParticle();
	spark.on = true;
	spark.sR = 255;

	if (uw == 1)
	{
		spark.sG = (GetRandomControl() & 0x3F) + 128;
		spark.sB = 32;
		spark.dR = 192;
		spark.dG = (GetRandomControl() & 0x1F) + 64;
		spark.dB = 0;
		spark.colFadeSpeed = 7;
		spark.fadeToBlack = 8;
		spark.life = (GetRandomControl() & 7) + 16;
		spark.sLife = spark.life;
		spark.roomNumber = roomNumber;
	}
	else
	{
		if (mainColor == Vector3::Zero)
		{
			spark.sG = (GetRandomControl() & 0xF) + 32;
			spark.sB = 0;
			spark.dR = (GetRandomControl() & 0x3F) + 192;
			spark.dG = (GetRandomControl() & 0x3F) + 128;
			spark.dB = 32;
			spark.colFadeSpeed = 8;
			spark.fadeToBlack = 16;
			spark.life = (GetRandomControl() & 7) + LIFE_MAX;
			spark.sLife = spark.life;
		}
		else
		{
			// New colored flame processing.
			int colorS[3] = { int(mainColor.x * UCHAR_MAX), int(mainColor.y * UCHAR_MAX), int(mainColor.z * UCHAR_MAX) };
			int colorD[3] = { int(secondColor.x * UCHAR_MAX), int(secondColor.y * UCHAR_MAX), int(secondColor.z * UCHAR_MAX) };

			// Determine weakest RGB component.
			int lowestS = UCHAR_MAX;
			int lowestD = UCHAR_MAX;
			for (int i = 0; i < 3; i++)
			{
				if (lowestS > colorS[i]) lowestS = colorS[i];
				if (lowestD > colorD[i]) lowestD = colorD[i];
			}

			// Introduce random color shift for non-weakest RGB components.
			constexpr auto CHROMA_SHIFT = 32;
			constexpr auto LUMA_SHIFT	= 0.5f;

			for (int i = 0; i < 3; i++)
			{
				if (colorS[i] != lowestS)
					colorS[i] = int(colorS[i] + GenerateInt(-CHROMA_SHIFT, CHROMA_SHIFT));

				if (colorD[i] != lowestD)
					colorD[i] = int(colorD[i] + GenerateInt(-CHROMA_SHIFT, CHROMA_SHIFT));

				colorS[i] = int(colorS[i] * (1.0f + GenerateFloat(-LUMA_SHIFT, 0)));
				colorD[i] = int(colorD[i] * (1.0f + GenerateFloat(-LUMA_SHIFT, 0)));

				colorS[i] = std::clamp(colorS[i], 0, UCHAR_MAX);
				colorD[i] = std::clamp(colorD[i], 0, UCHAR_MAX);
			}

			spark.sR = colorS[0];
			spark.sG = colorS[1];
			spark.sB = colorS[2];
			spark.dR = colorD[0];
			spark.dG = colorD[1];
			spark.dB = colorD[2];
			spark.colFadeSpeed = 8;
			spark.fadeToBlack = 16;
			spark.life = (GetRandomControl() & 7) + LIFE_MAX;
			spark.sLife = spark.life;
		}
	}

	spark.extras = unsigned char(extraTrig | ((EXTRAS_TABLE[extraTrig] + (GetRandomControl() & 7) + 28) << 3));
	spark.dynamic = (char)dynamic;

	if (dynamic == -2)
	{
		int i = 0;
		for (i = 0; i < 8; i++)
		{
			auto dynsp = &ParticleDynamics[i];

			if (!dynsp->On)
			{
				dynsp->On = true;
				dynsp->Falloff = 4;

				if (uw == 1)
					dynsp->Flags = 2;
				else
					dynsp->Flags = 1;

				spark.dynamic = (char)i;
				break;
			}							
		}
		
		if (i == 8)
			spark.dynamic = -1;			
	}

	spark.xVel = (GetRandomControl() & 0xFFF) - 2048;
	spark.yVel = (GetRandomControl() & 0xFFF) - 2048;
	spark.zVel = (GetRandomControl() & 0xFFF) - 2048;

	if (dynamic != -2 || uw == 1)
	{
		spark.x = (GetRandomControl() & 0x1F) + x - 16;
		spark.y = (GetRandomControl() & 0x1F) + y - 16;
		spark.z = (GetRandomControl() & 0x1F) + z - 16;
	}
	else
	{
		spark.x = (GetRandomControl() & 0x1FF) + x - 256;
		spark.y = (GetRandomControl() & 0x1FF) + y - 256;
		spark.z = (GetRandomControl() & 0x1FF) + z - 256;
	}

	if (uw == 1)
	{
		spark.friction = 17;
	}
	else
	{
		spark.friction = 51;
	}

	if (GetRandomControl() & 1)
	{
		if (uw == 1)
			spark.flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF | SP_UNDERWEXP;
		else
		{
			if (mainColor == Vector3::Zero)
			{
				spark.flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF | SP_EXPLOSION;
			}
			else
			{
				spark.flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF | SP_EXPLOSION | SP_COLOR;
			}
		}

		spark.rotAng = GetRandomControl() & 0xF;
		spark.rotAdd = (GetRandomControl() & 0xF) + ROTATION_MAX;
	}
	else if (uw == 1)
	{
		spark.flags = SP_SCALE | SP_DEF | SP_EXPDEF | SP_UNDERWEXP;
	}
	else
	{
		if (mainColor == Vector3::Zero)
		{
			spark.flags = SP_SCALE | SP_DEF | SP_EXPDEF | SP_EXPLOSION;
		}
		else
		{
			spark.flags = SP_SCALE | SP_DEF | SP_EXPDEF | SP_EXPLOSION | SP_COLOR;
		}
	}

	spark.scalar = 3;
	spark.gravity = 0;
	spark.size = (GetRandomControl() & 0xF) + 40;
	spark.sSize = spark.size * scalar;
	spark.dSize = spark.size * (scalar + 1);
	spark.size *= scalar;
	GetRandomControl();
	spark.maxYvel = 0;

	if (mainColor != Vector3::Zero)
	{
		if (extraTrig)
			TriggerExplosionSmokeEnd(x, y, z, uw);

		return;
	}

	if (uw == 2)
	{
		unsigned char r = spark.sR;
		unsigned char g = spark.sG;
		unsigned char b = spark.sB;
		spark.sR = b;
		spark.sG = r;
		spark.sB = g;

		r = spark.dR;
		g = spark.dG;
		b = spark.dB;
		spark.dR = b;
		spark.dG = r;
		spark.dB = g;

		spark.flags |= SP_PLASMAEXP;
	}
	else if (extraTrig)
	{
		TriggerExplosionSmoke(x, y, z, uw);
	}
	else
	{
		TriggerExplosionSmokeEnd(x, y, z, uw);
	}
}

void TriggerExplosionBubbles(int x, int y, int z, short roomNumber)
{
	int dx = LaraItem->Pose.Position.x - x;
	int dz = LaraItem->Pose.Position.z - z;

	if (dx >= -BLOCK(16) && dx <= BLOCK(16) &&
		dz >= -BLOCK(16) && dz <= BLOCK(16))
	{
		auto* spark = GetFreeParticle();

		spark->sR = 128;
		spark->dR = 128;
		spark->dG = 128;
		spark->dB = 128;
		spark->on = 1;
		spark->life = 24;
		spark->sLife = 24;
		spark->sG = 64;
		spark->sB = 0;
		spark->colFadeSpeed = 8;
		spark->fadeToBlack = 12;
		spark->blendMode = BlendMode::Additive;
		spark->x = x;
		spark->y = y;
		spark->z = z;
		spark->xVel = 0;
		spark->yVel = 0;
		spark->zVel = 0;
		spark->friction = 0;
		spark->flags = SP_UNDERWEXP | SP_DEF | SP_SCALE; 
		spark->SpriteSeqID = ID_DEFAULT_SPRITES;
		spark->SpriteID = SPR_BUBBLES;
		spark->scalar = 3;
		spark->gravity = 0;
		spark->maxYvel = 0;

		int size = (GetRandomControl() & 7) + 63;
		spark->sSize = size >> 1;
		spark->size = size >> 1;
		spark->dSize = 2 * size;

		for (int i = 0; i < 8; i++)
		{
			auto pos = Vector3(
				(GetRandomControl() & 0x1FF) + x - 256,
				(GetRandomControl() & 0x7F) + y - 64,
				(GetRandomControl() & 0x1FF) + z - 256);
			SpawnBubble(pos, roomNumber, (int)BubbleFlags::HighAmplitude);
		}
	}
}

void TriggerExplosionSmokeEnd(int x, int y, int z, int uw)
{
	auto* spark = GetFreeParticle();

	spark->on = true;

	if (uw)
	{
		spark->sR = 0;
		spark->sG = 0;
		spark->sB = 0;
		spark->dR = 192;
		spark->dG = 192;
		spark->dB = 208;
	}
	else
	{
		spark->dR = 64;
		spark->sR = 144;
		spark->sG = 144;
		spark->sB = 144;
		spark->dG = 64;
		spark->dB = 64;
	}

	spark->colFadeSpeed = 8;
	spark->fadeToBlack = 64;
	spark->life = spark->sLife= (GetRandomControl() & 0x1F) + 96;

	if (uw)
		spark->blendMode = BlendMode::Additive;
	else
		spark->blendMode = BlendMode::Subtractive;

	spark->x = (GetRandomControl() & 0x1F) + x - 16;
	spark->y = (GetRandomControl() & 0x1F) + y - 16;
	spark->z = (GetRandomControl() & 0x1F) + z - 16;
	spark->xVel = ((GetRandomControl() & 0xFFF) - 2048) >> 2;
	spark->yVel = GetRandomControl() - 128;
	spark->zVel = ((GetRandomControl() & 0xFFF) - 2048) >> 2;

	if (uw)
	{
		spark->friction = 20;
		spark->yVel >>= 4;
		spark->y += 32;
	}
	else
		spark->friction = 6;
	
	spark->flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;
	spark->rotAng = GetRandomControl() & 0xFFF;

	if (GetRandomControl() & 1)
		spark->rotAdd = -16 - (GetRandomControl() & 0xF);
	else
		spark->rotAdd = (GetRandomControl() & 0xF) + 16;

	spark->scalar = 3;

	if (uw)
	{
		spark->maxYvel = 0;
		spark->gravity = 0;
	}
	else
	{
		spark->gravity = -3 - (GetRandomControl() & 3);
		spark->maxYvel = -4 - (GetRandomControl() & 3);
	}

	spark->dSize = (GetRandomControl() & 0x1F) + 128;
	spark->sSize = spark->dSize / 4;
	spark->size = spark->dSize / 4;
}

void TriggerExplosionSmoke(int x, int y, int z, int uw)
{
	int dx = LaraItem->Pose.Position.x - x;
	int dz = LaraItem->Pose.Position.z - z;
	
	if (dx >= -BLOCK(16) && dx <= BLOCK(16) &&
		dz >= -BLOCK(16) && dz <= BLOCK(16))
	{
		auto* spark = GetFreeParticle();

		spark->sR = 144;
		spark->sG = 144;
		spark->sB = 144;
		spark->on = 1;
		spark->dR = 64;
		spark->dG = 64;
		spark->dB = 64;
		spark->colFadeSpeed = 2;
		spark->fadeToBlack = 8;
		spark->blendMode = BlendMode::Subtractive;
		spark->life = spark->sLife = (GetRandomControl() & 3) + 10;
		spark->x = (GetRandomControl() & 0x1FF) + x - 256;
		spark->y = (GetRandomControl() & 0x1FF) + y - 256;
		spark->z = (GetRandomControl() & 0x1FF) + z - 256;
		spark->xVel = ((GetRandomControl() & 0xFFF) - 2048) >> 2;
		spark->yVel = GetRandomControl() - 128;
		spark->zVel = ((GetRandomControl() & 0xFFF) - 2048) >> 2;

		if (uw)
			spark->friction = 2;
		else
			spark->friction = 6;

		spark->flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;
		spark->rotAng = GetRandomControl() & 0xFFF;
		spark->scalar = 1;
		spark->rotAdd = (GetRandomControl() & 0xF) + 16;
		spark->gravity = -3 - (GetRandomControl() & 3);
		spark->maxYvel = -4 - (GetRandomControl() & 3);
		spark->dSize = (GetRandomControl() & 0x1F) + 128;
		spark->sSize = spark->dSize / 4;
		spark->size = spark->dSize / 4;
	}
}

void TriggerSuperJetFlame(ItemInfo* item, int yvel, int deadly)
{
	long dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
	long dz = LaraItem->Pose.Position.z - item->Pose.Position.z;

	if (dx >= -BLOCK(16) && dx <= BLOCK(16) &&
		dz >= -BLOCK(16) && dz <= BLOCK(16))
	{
		int size = (GetRandomControl() & 0x1FF) - yvel;
		auto* sptr = GetFreeParticle();

		if (size < 512)
			size = 512;

		sptr->on = 1;
		sptr->sR = sptr->sG = (GetRandomControl() & 0x1F) + 48;
		sptr->sB = (GetRandomControl() & 0x3F) - 64;
		sptr->dR = (GetRandomControl() & 0x3F) - 64;
		sptr->dG = (GetRandomControl() & 0x3F) - 128;
		sptr->dB = 32;
		sptr->colFadeSpeed = 8;
		sptr->fadeToBlack = 8;
		sptr->blendMode = BlendMode::Additive;
		sptr->life = sptr->sLife = (size >> 9) + (GetRandomControl() & 7) + 16;
		sptr->x = (GetRandomControl() & 0x1F) + item->Pose.Position.x - 16;
		sptr->y = (GetRandomControl() & 0x1F) + item->Pose.Position.y - 16;
		sptr->z = (GetRandomControl() & 0x1F) + item->Pose.Position.z - 16;
		sptr->friction = 51;
		sptr->maxYvel = 0;
		sptr->flags = SP_EXPDEF | SP_ROTATE | SP_DEF | SP_SCALE;

		if (deadly)
			sptr->flags = SP_EXPDEF | SP_ROTATE | SP_DEF | SP_SCALE | SP_FIRE;

		sptr->scalar = 2;
		sptr->dSize = (GetRandomControl() & 0xF) + (size >> 6) + 16;
		sptr->sSize = sptr->size = sptr->dSize / 2;

		if ((-(item->TriggerFlags & 0xFF) & 7) == 1)
		{
			sptr->gravity = -16 - (GetRandomControl() & 0x1F);
			sptr->xVel = (GetRandomControl() & 0xFF) - 128;
			sptr->yVel = -size;
			sptr->zVel = (GetRandomControl() & 0xFF) - 128;
			sptr->dSize += sptr->dSize / 4;
			return;
		}

		sptr->y -= 64;
		sptr->gravity = -((size >> 9) + GetRandomControl() % (size >> 8));
		sptr->yVel = (GetRandomControl() & 0xFF) - 128;
		sptr->xVel = (GetRandomControl() & 0xFF) - 128;
		sptr->zVel = (GetRandomControl() & 0xFF) - 128;

		float xAngle = item->Pose.Orientation.x + ANGLE(180); // Nullmesh is rotated 180 degrees in editor
		float yAngle = item->Pose.Orientation.y;
		
		Vector3 dir;
		dir.x = phd_cos(xAngle) * phd_sin(yAngle);
		dir.y = phd_sin(xAngle);
		dir.z = phd_cos(xAngle) * phd_cos(yAngle);

		dir.Normalize();

		sptr->xVel += dir.x * (size - (size >> 2));
		sptr->yVel -= dir.y * (size - (size >> 2));
		sptr->zVel += dir.z * (size - (size >> 2));
	}
}

short DoBloodSplat(int x, int y, int z, short speed, short direction, short roomNumber)
{
	short probedRoomNumber = GetPointCollision(Vector3i(x, y, z), roomNumber).GetRoomNumber();
	if (TestEnvironment(ENV_FLAG_WATER, probedRoomNumber))
	{
		SpawnUnderwaterBlood(Vector3(x, y, z), probedRoomNumber, speed);
	}
	else
	{
		TriggerBlood(x, y, z, direction >> 4, speed);
	}

	return 0;
}

void DoLotsOfBlood(int x, int y, int z, int speed, short direction, short roomNumber, int count)
{
	for (int i = 0; i < count; i++)
	{
		DoBloodSplat(
			x + 256 - (GetRandomControl() * 512 / 0x8000),
			y + 256 - (GetRandomControl() * 512 / 0x8000),
			z + 256 - (GetRandomControl() * 512 / 0x8000),
			speed, direction, roomNumber);
	}
}

void TriggerLaraBlood()
{
	int node = 1;

	for (int i = 0; i < LARA_MESHES::LM_HEAD; i++)
	{
		if (node & LaraItem->TouchBits.ToPackedBits())
		{
			auto vec = GetJointPosition(LaraItem, 
				i,
				Vector3i(
					(GetRandomControl() & 31) - 16,
					(GetRandomControl() & 31) - 16,
					(GetRandomControl() & 31) - 16
				));
			DoBloodSplat(vec.x, vec.y, vec.z, (GetRandomControl() & 7) + 8, 2 * GetRandomControl(), LaraItem->RoomNumber);
		}

		node <<= 1;
	}
}

void Ricochet(Pose& pose)
{
	short angle = Geometry::GetOrientToPoint(pose.Position.ToVector3(), LaraItem->Pose.Position.ToVector3()).y;
	auto target = GameVector(pose.Position);
	TriggerRicochetSpark(target, angle / 16);
}

void ControlWaterfallMist(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (!TriggerActive(item))
		return;
	
	TriggerWaterfallMist(*item);
	SoundEffect(SFX_TR4_WATERFALL_LOOP, &item->Pose);
}

void TriggerWaterfallMist(const ItemInfo& item)
{
	static const int scale = 3;

	int size = 64;
	int width = 1;
	short angle = item.Pose.Orientation.y + ANGLE(180.0f);

	if (item.TriggerFlags != 0)
	{
		size = item.TriggerFlags % 100;
		width = std::clamp(int(round(item.TriggerFlags / 100) * 100) / 2, 0, BLOCK(8));
	}

	float cos = phd_cos(angle);
	float sin = phd_sin(angle);

	int maxPosX =  width * sin + item.Pose.Position.x;
	int maxPosZ =  width * cos + item.Pose.Position.z;
	int minPosX = -width * sin + item.Pose.Position.x;
	int minPosZ = -width * cos + item.Pose.Position.z;

	float fadeMin = GetParticleDistanceFade(Vector3i(minPosX, item.Pose.Position.y, minPosZ));
	float fadeMax = GetParticleDistanceFade(Vector3i(maxPosX, item.Pose.Position.y, maxPosZ));

	if ((fadeMin == 0.0f) && (fadeMin == fadeMax))
		return;

	float finalFade = ((fadeMin >= 1.0f) && (fadeMin == fadeMax)) ? 1.0f : std::max(fadeMin, fadeMax);

	auto startColor = item.Model.Color / 4.0f * finalFade * float(UCHAR_MAX);
	auto endColor   = item.Model.Color / 8.0f * finalFade * float(UCHAR_MAX);

	float step = size * scale;
	int currentStep = 0;

	while (true)
	{
		int offset = (step * currentStep) + Random::GenerateInt(-32, 32);

		if (offset > width)
			break;

		for (int sign = -1; sign <= 1; sign += 2)
		{
			auto* spark = GetFreeParticle();
			spark->on = true;

			char colorOffset = (Random::GenerateInt(-8, 8));
			spark->sR = std::clamp(int(startColor.x) + colorOffset, 0, UCHAR_MAX);
			spark->sG = std::clamp(int(startColor.y) + colorOffset, 0, UCHAR_MAX);
			spark->sB = std::clamp(int(startColor.z) + colorOffset, 0, UCHAR_MAX);
			spark->dR = std::clamp(int(endColor.x)   + colorOffset, 0, UCHAR_MAX);
			spark->dG = std::clamp(int(endColor.y)   + colorOffset, 0, UCHAR_MAX);
			spark->dB = std::clamp(int(endColor.z)   + colorOffset, 0, UCHAR_MAX);

			spark->colFadeSpeed = 1;
			spark->blendMode = BlendMode::Additive;
			spark->life = spark->sLife = Random::GenerateInt(8, 12);
			spark->fadeToBlack = spark->life - 6;

			spark->x = offset * sign * sin + Random::GenerateInt(-8, 8) + item.Pose.Position.x;
			spark->y = Random::GenerateInt(0, 16) + item.Pose.Position.y - 8;
			spark->z = offset * sign * cos + Random::GenerateInt(-8, 8) + item.Pose.Position.z;

			spark->xVel = 0;
			spark->yVel = Random::GenerateInt(-64, 64);
			spark->zVel = 0;

			spark->friction = 0;
			spark->rotAng = GetRandomControl() & 0xFFF;
			spark->scalar = scale;
			spark->maxYvel = 0;
			spark->rotAdd = Random::GenerateInt(-16, 16);
			spark->gravity = -spark->yVel >> 2;
			spark->sSize = spark->size = Random::GenerateInt(0, 3) * scale + size;
			spark->dSize = 2 * spark->size;

			spark->SpriteSeqID = ID_DEFAULT_SPRITES;
			spark->SpriteID = Random::GenerateInt(0, 100) > 95 ? 17 : 0;
			spark->flags = 538;

			if (sign == 1)
			{
				currentStep++;
				if (currentStep == 1)
					break;
			}
		}
	}
}

void TriggerWaterfallMist(Vector3 pos, int size, int width, float angle, Vector4 color)
{
	static const int scale = 3;

	float cos = phd_cos(angle);
	float sin = phd_sin(angle);

	int maxPosX = width * sin + pos.x;
	int maxPosZ = width * cos + pos.z;
	int minPosX = -width * sin + pos.x;
	int minPosZ = -width * cos + pos.z;

	float fadeMin = GetParticleDistanceFade(Vector3i(minPosX, pos.y, minPosZ));
	float fadeMax = GetParticleDistanceFade(Vector3i(maxPosX, pos.y, maxPosZ));

	if ((fadeMin == 0.0f) && (fadeMin == fadeMax))
		return;

	float finalFade = ((fadeMin >= 1.0f) && (fadeMin == fadeMax)) ? 1.0f : std::max(fadeMin, fadeMax);

	auto startColor = color / 4.0f * finalFade * float(UCHAR_MAX);
	auto endColor = color / 8.0f * finalFade * float(UCHAR_MAX);

	float step = size * scale;
	int currentStep = 0;

	while (true)
	{
		int offset = (step * currentStep) + Random::GenerateInt(-32, 32);

		if (offset > width)
			break;

		for (int sign = -1; sign <= 1; sign += 2)
		{
			auto* spark = GetFreeParticle();
			spark->on = true;

			char colorOffset = (Random::GenerateInt(-8, 8));
			spark->sR = std::clamp(int(startColor.x) + colorOffset, 0, UCHAR_MAX);
			spark->sG = std::clamp(int(startColor.y) + colorOffset, 0, UCHAR_MAX);
			spark->sB = std::clamp(int(startColor.z) + colorOffset, 0, UCHAR_MAX);
			spark->dR = std::clamp(int(endColor.x) + colorOffset, 0, UCHAR_MAX);
			spark->dG = std::clamp(int(endColor.y) + colorOffset, 0, UCHAR_MAX);
			spark->dB = std::clamp(int(endColor.z) + colorOffset, 0, UCHAR_MAX);

			spark->colFadeSpeed = 1;
			spark->blendMode = BlendMode::Additive;
			spark->life = spark->sLife = Random::GenerateInt(8, 12);
			spark->fadeToBlack = spark->life - 6;

			spark->x = offset * sign * sin + Random::GenerateInt(-8, 8) + pos.x;
			spark->y = Random::GenerateInt(0, 16) + pos.y - 8;
			spark->z = offset * sign * cos + Random::GenerateInt(-8, 8) + pos.z;

			spark->xVel = 0;
			spark->yVel = Random::GenerateInt(-64, 64);
			spark->zVel = 0;

			spark->friction = 0;
			spark->rotAng = GetRandomControl() & 0xFFF;
			spark->scalar = scale;
			spark->maxYvel = 0;
			spark->rotAdd = Random::GenerateInt(-16, 16);
			spark->gravity = -spark->yVel >> 2;
			spark->sSize = spark->size = Random::GenerateInt(0, 3) * scale + size;
			spark->dSize = 2 * spark->size;

			spark->SpriteSeqID = ID_DEFAULT_SPRITES;
			spark->SpriteID = Random::GenerateInt(0, 100) > 95 ? 17 : 0;
			spark->flags = 538;

			if (sign == 1)
			{
				currentStep++;
				if (currentStep == 1)
					break;
			}
		}
	}
}

void KillAllCurrentItems(short itemNumber)
{
	// TODO: Reimplement this functionality.
}

void TriggerRocketFlame(int x, int y, int z, int xv, int yv, int zv, int itemNumber)
{
	auto* sptr = GetFreeParticle();

	sptr->on = true;
	sptr->sR = 48 + (GetRandomControl() & 31);
	sptr->sG = sptr->sR;
	sptr->sB = 192 + (GetRandomControl() & 63);

	sptr->dR = 192 + (GetRandomControl() & 63);
	sptr->dG = 128 + (GetRandomControl() & 63);
	sptr->dB = 32;

	sptr->colFadeSpeed = 12 + (GetRandomControl() & 3);
	sptr->fadeToBlack = 12;
	sptr->sLife = sptr->life = (GetRandomControl() & 3) + 28;
	sptr->blendMode = BlendMode::Additive;
	sptr->extras = 0;
	sptr->dynamic = -1;

	sptr->x = x + ((GetRandomControl() & 31) - 16);
	sptr->y = y;
	sptr->z = z + ((GetRandomControl() & 31) - 16);

	sptr->xVel = xv;
	sptr->yVel = yv;
	sptr->zVel = zv;
	sptr->friction = 3 | (3 << 4);

	if (GetRandomControl() & 1)
	{
		sptr->flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_ITEM | SP_EXPDEF;
		sptr->fxObj = itemNumber;
		sptr->rotAng = GetRandomControl() & 4095;

		if (GetRandomControl() & 1)
			sptr->rotAdd = -(GetRandomControl() & 15) - 16;
		else
			sptr->rotAdd = (GetRandomControl() & 15) + 16;
	}
	else
	{
		sptr->flags = SP_SCALE | SP_DEF | SP_ITEM | SP_EXPDEF;
		sptr->fxObj = itemNumber;
	}
	
	sptr->gravity = 0;
	sptr->maxYvel = 0;

	// TODO: right sprite
	sptr->SpriteSeqID = ID_DEFAULT_SPRITES;
	sptr->SpriteID = 0;
	sptr->scalar = 2;

	int size = (GetRandomControl() & 7) + 32;
	sptr->size = sptr->sSize = size;
}

void TriggerRocketFire(int x, int y, int z)
{
	auto* sptr = GetFreeParticle();

	sptr->on = true;

	sptr->sR = sptr->sG = (GetRandomControl() & 0x1F) + 48;
	sptr->sB = (GetRandomControl() & 0x3F) - 64;
	sptr->dR = (GetRandomControl() & 0x3F) - 64;
	sptr->dG = (GetRandomControl() & 0x3F) - 128;
	sptr->dB = 32;

	sptr->colFadeSpeed = 4 + (GetRandomControl() & 3);
	sptr->fadeToBlack = 12;
	sptr->sLife = sptr->life = (GetRandomControl() & 3) + 20;
	sptr->blendMode = BlendMode::Additive;
	sptr->extras = 0;
	sptr->dynamic = -1;

	sptr->x = x + ((GetRandomControl() & 15) - 8);
	sptr->y = y + ((GetRandomControl() & 15) - 8);
	sptr->z = z + ((GetRandomControl() & 15) - 8);
	sptr->xVel = ((GetRandomControl() & 255) - 128);
	sptr->yVel = -(GetRandomControl() & 3) - 4;
	sptr->zVel = ((GetRandomControl() & 255) - 128);
	sptr->friction = 4;

	if (GetRandomControl() & 1)
	{
		sptr->flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF;
		sptr->rotAng = GetRandomControl() & 4095;

		if (GetRandomControl() & 1)
			sptr->rotAdd = -(GetRandomControl() & 15) - 16;
		else
			sptr->rotAdd = (GetRandomControl() & 15) + 16;
	}
	else
		sptr->flags = SP_SCALE | SP_DEF | SP_EXPDEF;

	// TODO: right sprite
	sptr->SpriteSeqID = ID_DEFAULT_SPRITES;
	sptr->SpriteID = 0;
	sptr->scalar = 1;
	sptr->gravity = -(GetRandomControl() & 3) - 4;
	sptr->maxYvel = -(GetRandomControl() & 3) - 4;

	int size = (GetRandomControl() & 7) + 128;
	sptr->size = sptr->sSize = size >> 2;
	sptr->dSize = size;
}


void TriggerRocketSmoke(int x, int y, int z)
{
	TEN::Effects::Smoke::TriggerRocketSmoke(x, y, z);
}

void SpawnCorpseEffect(const Vector3& pos)
{
	TEN::Effects::Smoke::SpawnCorpseEffect(pos);
}

void TriggerFlashSmoke(int x, int y, int z, short roomNumber)
{
	auto* room = &g_Level.Rooms[roomNumber];

	bool water = false;
	if (TestEnvironment(ENV_FLAG_WATER, room))
	{
		TriggerExplosionBubble(x, y, z, roomNumber);
		water = true;
	}

	auto* spark = &SmokeSparks[GetFreeSmokeSpark()];
	spark->on = true;
	spark->sShade = 0;
	spark->dShade = -128;
	spark->colFadeSpeed = 4;
	spark->fadeToBlack = 16;
	spark->blendMode = BlendMode::Additive;
	spark->life = spark->sLife = (GetRandomControl() & 0xF) + 64;
	spark->position.x = (GetRandomControl() & 0x1F) + x - 16;
	spark->position.y = (GetRandomControl() & 0x1F) + y - 16;
	spark->position.z = (GetRandomControl() & 0x1F) + z - 16;

	if (water)
	{
		spark->velocity.x = spark->velocity.y = GetRandomControl() & 0x3FF - 512;
		spark->velocity.z = (GetRandomControl() & 0x3FF) - 512;
		spark->friction = 68;
	}
	else
	{
		spark->velocity.x = 2 * (GetRandomControl() & 0x3FF) - 1024;
		spark->velocity.y = -512 - (GetRandomControl() & 0x3FF);
		spark->velocity.z = 2 * (GetRandomControl() & 0x3FF) - 1024;
		spark->friction = 85;
	}

	if (TestEnvironment(ENV_FLAG_WIND, room))
		spark->flags = 272;
	else
		spark->flags = 16;

	spark->rotAng = GetRandomControl() & 0xFFF;
	if (GetRandomControl() & 1)
		spark->rotAdd = -16 - (GetRandomControl() & 0xF);
	else
		spark->rotAdd = (GetRandomControl() & 0xF) + 16;
	
	spark->maxYvel = 0;
	spark->gravity = 0;
	spark->sSize = spark->size = (GetRandomControl() & 0x1F) + 64;
	spark->dSize = 2 * (spark->sSize + 4);
}

void TriggerFireFlame(int x, int y, int z, FlameType type, const Vector3& color1, const Vector3& color2)
{
	int dx = LaraItem->Pose.Position.x - x;
	int dz = LaraItem->Pose.Position.z - z;

	if (abs(dx) > BLOCK(16) || abs(dz) > BLOCK(16))
		return;

	auto* spark = GetFreeParticle();

	spark->on = true;

	if (color1 == Vector3::Zero || color2 == Vector3::Zero)
	{
		// Legacy default colours, for compatibility with TR4-TR5 objects.

		switch (type)
		{
		case FlameType::SmallFast:
			spark->sR = 48;
			spark->sG = 48;
			spark->sB = (GetRandomControl() & 0x1F) + 128;
			spark->dR = 32;
			spark->dG = (GetRandomControl() & 0x3F) - 64;
			spark->dB = (GetRandomControl() & 0x3F) + 64;
			break;

		case FlameType::Small:
			spark->sR = spark->sG = (GetRandomControl() & 0x1F) + 48;
			spark->sB = (GetRandomControl() & 0x3F) - 64;
			break;

		default:
			spark->sR = 255;
			spark->sB = 48;
			spark->sG = (GetRandomControl() & 0x1F) + 48;
			break;
		}

		if (type != FlameType::SmallFast)
		{
			spark->dR = (GetRandomControl() & 0x3F) - 64;
			spark->dG = (GetRandomControl() & 0x3F) + -128;
			spark->dB = 32;
		}
	}
	else
	{
		// New colored flame processing.

		int colorS[3] = { int(color1.x * UCHAR_MAX), int(color1.y * UCHAR_MAX), int(color1.z * UCHAR_MAX) };
		int colorD[3] = { int(color2.x * UCHAR_MAX), int(color2.y * UCHAR_MAX), int(color2.z * UCHAR_MAX) };

		// Determine weakest RGB component.

		int lowestS = UCHAR_MAX;
		int lowestD = UCHAR_MAX;
		for (int i = 0; i < 3; i++)
		{
			if (lowestS > colorS[i]) lowestS = colorS[i];
			if (lowestD > colorD[i]) lowestD = colorD[i];
		}

		// Introduce random color shift for non-weakest RGB components.

		static constexpr int CHROMA_SHIFT = 32;
		static constexpr float LUMA_SHIFT = 0.5f;

		for (int i = 0; i < 3; i++)
		{
			if (colorS[i] != lowestS)
				colorS[i] = int(colorS[i] + GenerateInt(-CHROMA_SHIFT, CHROMA_SHIFT));
			if (colorD[i] != lowestD)
				colorD[i] = int(colorD[i] + GenerateInt(-CHROMA_SHIFT, CHROMA_SHIFT));

			colorS[i] = int(colorS[i] * (1.0f + GenerateFloat(-LUMA_SHIFT, 0)));
			colorD[i] = int(colorD[i] * (1.0f + GenerateFloat(-LUMA_SHIFT, 0)));

			colorS[i] =	std::clamp(colorS[i], 0, UCHAR_MAX);
			colorD[i] =	std::clamp(colorD[i], 0, UCHAR_MAX);
		}

		spark->sR = colorS[0];
		spark->sG = colorS[1];
		spark->sB = colorS[2];

		spark->dR = colorD[0];
		spark->dG = colorD[1];
		spark->dB = colorD[2];
	}

	if (type == FlameType::Small ||
		type == FlameType::SmallFast ||
		type == FlameType::Static)
	{
		spark->fadeToBlack = 6;
		spark->colFadeSpeed = (GetRandomControl() & 3) + 5;
		spark->life = spark->sLife = (GetRandomControl() & 3) + 24;
	}
	else
	{
		spark->fadeToBlack = 8;
		spark->colFadeSpeed = (GetRandomControl() & 3) + 20;
		spark->life = spark->sLife = (GetRandomControl() & 7) + 40;
	}

	spark->blendMode = BlendMode::Additive;

	if (type != FlameType::Big && type != FlameType::Medium)
	{
		if (type < FlameType::SmallFast)
		{
			spark->x = (GetRandomControl() & 0xF) + x - 8;
			spark->y = y;
			spark->z = (GetRandomControl() & 0xF) + z - 8;
		}
		else
		{
			spark->x = (GetRandomControl() & 0x3F) + x - 32;
			spark->y = y;
			spark->z = (GetRandomControl() & 0x3F) + z - 32;
		}
	}
	else
	{
		spark->x = (GetRandomControl() & 0x1F) + x - 16;
		spark->y = y;
		spark->z = (GetRandomControl() & 0x1F) + z - 16;
	}

	if (type == FlameType::Small)
	{
		spark->xVel = (GetRandomControl() & 0x1F) - 16;
		spark->yVel = -1024 - (GetRandomControl() & 0x1FF);
		spark->zVel = (GetRandomControl() & 0x1F) - 16;
		spark->friction = 68;
	}
	else
	{
		spark->xVel = (GetRandomControl() & 0xFF) - 128;
		spark->yVel = -16 - (GetRandomControl() & 0xF);
		spark->zVel = (GetRandomControl() & 0xFF) - 128;

		if (type == FlameType::Medium)
			spark->friction = 51;
		else
			spark->friction = 5;
	}

	if (GetRandomControl() & 1)
	{
		spark->gravity = -16 - (GetRandomControl() & 0x1F);
		spark->maxYvel = -16 - (GetRandomControl() & 7);
		spark->flags = 538;

		spark->rotAng = GetRandomControl() & 0xFFF;

		if (GetRandomControl() & 1)
			spark->rotAdd = -16 - (GetRandomControl() & 0xF);
		else
			spark->rotAdd = (GetRandomControl() & 0xF) + 16;
	}
	else
	{
		spark->flags = SP_EXPDEF | SP_DEF | SP_SCALE;
		spark->gravity = -16 - (GetRandomControl() & 0x1F);
		spark->maxYvel = -16 - (GetRandomControl() & 7);
	}

	spark->scalar = 2;

	if (type != FlameType::Big)
	{
		if (type == FlameType::Medium)
			spark->sSize = spark->size = (GetRandomControl() & 0x1F) + 64;
		else if (type < FlameType::SmallFast)
		{
			spark->maxYvel = 0;
			spark->gravity = 0;
			spark->sSize = spark->size = (GetRandomControl() & 0x1F) + 32;
		}
		else
		{
			spark->dSize = spark->size / 16;

			if (type == FlameType::Pulse)
			{
				spark->colFadeSpeed >>= 2;
				spark->fadeToBlack = spark->fadeToBlack >> 2;
				spark->life = spark->life >> 2;
				spark->sLife = spark->life >> 2;
			}

			spark->sSize = spark->size = (GetRandomControl() & 0x0F) + 48;
		}
	}
	else
		spark->sSize = spark->size = (GetRandomControl() & 0x1F) + 128;

	if (type == FlameType::Small)
		spark->dSize = (spark->size / 4.0f);
	else
	{
		spark->dSize = (spark->size / 16.0f);

		if (type == FlameType::Pulse)
		{
			spark->colFadeSpeed >>= 2;
			spark->fadeToBlack = spark->fadeToBlack >> 2;
			spark->life = spark->life >> 2;
			spark->sLife = spark->life >> 2;
		}
	}
}

void TriggerMetalSparks(int x, int y, int z, int xv, int yv, int zv, const Vector3& color, int additional)
{
	int dx = LaraItem->Pose.Position.x - x;
	int dz = LaraItem->Pose.Position.z - z;
	int colorR = std::clamp(int(color.x * UCHAR_MAX), 0, UCHAR_MAX);
	int colorG = std::clamp(int(color.y * UCHAR_MAX), 0, UCHAR_MAX);
	int colorB = std::clamp(int(color.z * UCHAR_MAX), 0, UCHAR_MAX);

	if (dx >= -16384 && dx <= 16384 && dz >= -16384 && dz <= 16384)
	{
		int r = rand();

		auto* spark = GetFreeParticle();

		spark->dG =  colorG;
		spark->dB =  colorB;
		spark->life = 10;
		spark->sLife = 10;
		spark->sR = colorR;
		spark->sG = colorG;
		spark->sB = colorB;
		spark->dR = colorR;
		spark->x = (r & 7) + x - 3;
		spark->on = 1;
		spark->colFadeSpeed = 3;
		spark->fadeToBlack = 5;
		spark->y = ((r >> 3) & 7) + y - 3;
		spark->blendMode = BlendMode::Additive;
		spark->friction = 34;
		spark->scalar = 2;
		spark->z = ((r >> 6) & 7) + z - 3;
		spark->flags = 2;
		spark->xVel = (byte)(r >> 2) + xv - 128;
		spark->yVel = (byte)(r >> 4) + yv - 128;
		spark->zVel = (byte)(r >> 6) + zv - 128;
		spark->sSize = ((r >> 9) & 3) + 4;
		spark->size = ((r >> 9) & 3) + 4;
		spark->dSize = ((r >> 9) & 1) + 1;
		spark->maxYvel = 0;
		spark->gravity = 0;

		if (additional)
		{
			r = rand();
			spark = GetFreeParticle();
			spark->on = 1;
			spark->sR = spark->dR >> 1;
			spark->sG = spark->dG >> 1;
			spark->fadeToBlack = 4;
			spark->blendMode = BlendMode::Additive;
			spark->colFadeSpeed = (r & 3) + 8;
			spark->sB = spark->dB >> 1;
			spark->dR = 32;
			spark->dG = 32;
			spark->dB = 32;
			spark->yVel = yv;
			spark->life = ((r >> 3) & 7) + 13;
			spark->sLife = ((r >> 3) & 7) + 13;
			spark->friction = 4;
			spark->x = x + (xv >> 5);
			spark->y = y + (yv >> 5);
			spark->z = z + (zv >> 5);
			spark->xVel = (r & 0x3F) + xv - 32;
			spark->zVel = ((r >> 6) & 0x3F) + zv - 32;

			if (r & 1)
			{
				spark->flags = 538;
				spark->rotAng = r >> 3;

				if (r & 2)
					spark->rotAdd = -16 - (r & 0xF);
				else
					spark->rotAdd = (r & 0xF) + 16;
			}
			else
				spark->flags = 522;

			spark->gravity = -8 - (r >> 3 & 3);
			spark->scalar = 2;
			spark->maxYvel = -4 - (r >> 6 & 3);
			spark->sSize = (((r >> 8) & 0xF) + 24) >> 3;
			spark->size  = (((r >> 8) & 0xF) + 24) >> 3;
			spark->dSize =  ((r >> 8) & 0xF) + 24;
		}
	}
}

void ProcessEffects(ItemInfo* item)
{
	constexpr auto MAX_LIGHT_FALLOFF = 13;
	constexpr auto BURN_HEALTH_LARA = 7;
	constexpr auto BURN_HEALTH_NPC = 1;
	constexpr auto BURN_AFTERMATH_TIMEOUT = 4 * FPS;
	constexpr auto BURN_DAMAGE_PROBABILITY = 1 / 8.0f;

	if (item->Effect.Type == EffectType::None)
		return;

	if (item->Effect.Count > 0)
	{
		item->Effect.Count--;

		if (!item->Effect.Count)
		{
			if (item->Effect.Type == EffectType::Fire || 
				item->Effect.Type == EffectType::Custom || 
				item->Effect.Type == EffectType::ElectricIgnite || 
				item->Effect.Type == EffectType::RedIgnite)
			{
				item->Effect.Type = EffectType::Smoke;
				item->Effect.Count = BURN_AFTERMATH_TIMEOUT;
			}
			else
			{
				item->Effect.Type = EffectType::None;
				return;
			}
		}
	}

	int numMeshes = Objects[item->ObjectNumber].nmeshes;
	for (int i = 0; i < numMeshes; i++)
	{
		auto pos = GetJointPosition(item, i);

		switch (item->Effect.Type)
		{
		case EffectType::Fire:
			if (TestProbability(1 / 8.0f))
				TriggerFireFlame(pos.x, pos.y, pos.z, TestProbability(1 / 10.0f) ? FlameType::Trail : FlameType::Medium);
			
			break;

		case EffectType::Custom:
			if (TestProbability(1 / 8.0f))			
			{
				TriggerFireFlame(
					pos.x, pos.y, pos.z, TestProbability(1 / 10.0f) ? FlameType::Trail : FlameType::Medium,
					item->Effect.PrimaryEffectColor, item->Effect.SecondaryEffectColor);
			}

			break;

		case EffectType::Sparks:
			if (TestProbability(1 / 10.0f))
			{
				TriggerElectricSpark(
					GameVector(pos, item->RoomNumber),
					EulerAngles(0, Random::GenerateAngle(0, ANGLE(359.0f)), 0), 2);
			}

			if (TestProbability(1 / 64.0f))
				TriggerRocketSmoke(pos.x, pos.y, pos.z);

			break;

		case EffectType::ElectricIgnite:
			if (TestProbability(1 / 1.0f))
			{
				TriggerElectricSpark(
					GameVector(pos, item->RoomNumber),
					EulerAngles(0, Random::GenerateAngle(0, ANGLE(359.0f)), 0), 2);
			}

			if (TestProbability(1 / 1.0f))
			{
				TriggerFireFlame(
					pos.x, pos.y, pos.z, TestProbability(1 / 10.0f) ? FlameType::Medium : FlameType::Medium,
					Vector3(0.2f, 0.5f, 1.0f), Vector3(0.2f, 0.8f, 1.0f));
			}

			break;

		case EffectType::RedIgnite:
			if (TestProbability(1 / 1.0f))
			{
				TriggerFireFlame(
					pos.x, pos.y, pos.z, TestProbability(1 / 10.0f) ? FlameType::Medium : FlameType::Medium,
					Vector3(1.0f, 0.5f, 0.2f), Vector3(0.6f, 0.1f, 0.0f));
			}

			break;

		case EffectType::Smoke:
			if (TestProbability(1 / 32.0f))
				TriggerRocketSmoke(pos.x, pos.y, pos.z);
			
			break;

		}
	}

	if (item->Effect.Type != EffectType::Smoke)
	{
		int falloff = item->Effect.Count < 0 ? MAX_LIGHT_FALLOFF :
			MAX_LIGHT_FALLOFF - std::clamp(MAX_LIGHT_FALLOFF - item->Effect.Count, 0, MAX_LIGHT_FALLOFF);

		auto pos = GetJointPosition(item, 0);
		SpawnDynamicLight(
			pos.x, pos.y, pos.z, falloff,
			std::clamp(Random::GenerateInt(-32, 32) + int(item->Effect.LightColor.x * UCHAR_MAX), 0, UCHAR_MAX),
			std::clamp(Random::GenerateInt(-32, 32) + int(item->Effect.LightColor.y * UCHAR_MAX), 0, UCHAR_MAX),
			std::clamp(Random::GenerateInt(-32, 32) + int(item->Effect.LightColor.z * UCHAR_MAX), 0, UCHAR_MAX));
	}

	switch (item->Effect.Type)
	{
	case EffectType::ElectricIgnite:
	case EffectType::Sparks:
		SoundEffect(SOUND_EFFECTS::SFX_TR4_LARA_ELECTRIC_CRACKLES, &item->Pose);
		break;

	case EffectType::Fire: 
	case EffectType::Custom:
		SoundEffect(SOUND_EFFECTS::SFX_TR4_LOOP_FOR_SMALL_FIRES, &item->Pose);
		break;
	}

	if (item->Effect.Type != EffectType::Smoke)
	{
		if (item->IsLara() ||
			(item->IsCreature() && item->HitPoints > 0 && Random::TestProbability(BURN_DAMAGE_PROBABILITY)))
		{
			DoDamage(item, item->IsLara() ? BURN_HEALTH_LARA : BURN_HEALTH_NPC);
		}
	}
	
	if (item->Effect.Type != EffectType::Sparks && item->Effect.Type != EffectType::Smoke)
	{
		const auto& bounds = GameBoundingBox(item);
		int waterHeight = GetPointCollision(*item).GetWaterTopHeight();
		int itemLevel = item->Pose.Position.y + bounds.Y2 - (bounds.GetHeight() / 3);

		if (waterHeight != NO_HEIGHT && itemLevel > waterHeight)
		{
			item->Effect.Type = EffectType::Smoke;
			item->Effect.Count = 1 * FPS;
		}
	}

	if (item->IsLara() && GetLaraInfo(item)->Control.WaterStatus == WaterStatus::FlyCheat)
		item->Effect.Type = EffectType::None;
}

void TriggerAttackFlame(const Vector3i& pos, const Vector3& color, int scale)
{
	auto& spark = *GetFreeParticle();

	spark.on = true;
	spark.sR = 0;
	spark.sG = 0;
	spark.sB = 0;
	spark.dR = color.x;
	spark.dG = color.y;
	spark.dB = color.z;
	spark.fadeToBlack = 8;
	spark.colFadeSpeed = Random::GenerateInt(4, 8);
	spark.blendMode = BlendMode::Additive;
	spark.life = Random::GenerateInt(20, 28);
	spark.sLife = spark.life;
	spark.x = pos.x + Random::GenerateInt(-8, 8);
	spark.y = pos.y;
	spark.z = pos.z + Random::GenerateInt(-8, 8);
	spark.xVel = Random::GenerateInt(-128, 128);
	spark.yVel = 0;
	spark.zVel = Random::GenerateInt(-128, 128);
	spark.friction = 5;
	spark.flags = SP_EXPDEF | SP_DEF | SP_SCALE;
	spark.rotAng = Random::GenerateInt(0, 4096); // NOTE: Effect angles use [0, 4096] range.

	if (TestProbability(1 / 2.0f))
		spark.rotAdd = -32 - (GetRandomControl() & 0x1F);
	else
		spark.rotAdd = (GetRandomControl() & 0x1F) + 32;

	spark.maxYvel = 0;
	spark.gravity = Random::GenerateInt(16, 48);
	spark.scalar = 2;
	spark.size = Random::GenerateInt(0, 16) + scale;
	spark.sSize = spark.size;
	spark.dSize = spark.size / 4;
}

void SpawnPlayerWaterSurfaceEffects(const ItemInfo& item, int waterHeight, int waterDepth)
{
	const auto& player = GetLaraInfo(item);

	// Player underwater; return early.
	if (player.Control.WaterStatus == WaterStatus::Underwater)
		return;

	// Get point collision.
	auto pointColl0 = GetPointCollision(item, 0, 0, -(LARA_HEIGHT / 2));
	auto pointColl1 = GetPointCollision(item, 0, 0, item.Animation.Velocity.y);

	// In swamp; return early.
	if (TestEnvironment(ENV_FLAG_SWAMP, pointColl1.GetRoomNumber()))
		return;

	bool isWater0 = TestEnvironment(ENV_FLAG_WATER, pointColl0.GetRoomNumber());
	bool isWater1 = TestEnvironment(ENV_FLAG_WATER, pointColl1.GetRoomNumber());

	// Spawn splash.
	if (!isWater0 && isWater1 &&
		item.Animation.Velocity.y > 0.0f && SplashCount == 0 &&
		player.Control.WaterStatus != WaterStatus::TreadWater)
	{
		SplashSetup.Position = Vector3(item.Pose.Position.x, waterHeight - 1, item.Pose.Position.z);
		SplashSetup.InnerRadius = 16;
		SplashSetup.SplashPower = item.Animation.Velocity.z;

		SetupSplash(&SplashSetup, pointColl0.GetRoomNumber());
		SplashCount = 16;
	}
	// Spawn ripple.
	else if (isWater1)
	{
		if (Wibble & 0xF)
			return;

		if (Random::TestProbability(1 / 2000.0f) && item.Animation.ActiveState == LS_IDLE)
			return;

		int flags = (item.Animation.ActiveState == LS_IDLE) ?
			(int)RippleFlags::LowOpacity :
			(int)RippleFlags::SlowFade | (int)RippleFlags::LowOpacity;

		SpawnRipple(
			Vector3(item.Pose.Position.x, waterHeight - 1, item.Pose.Position.z),
			item.RoomNumber, Random::GenerateFloat(112.0f, 128.0f),
			flags);
	}
}
