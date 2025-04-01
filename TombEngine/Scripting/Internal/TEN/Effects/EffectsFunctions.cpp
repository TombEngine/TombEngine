#include "framework.h"
#include "Scripting/Internal/TEN/Effects/EffectsFunctions.h"

#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/control/los.h"
#include "Game/effects/blood.h"
#include "Game/effects/Bubble.h"
#include "Game/effects/DisplaySprite.h"
#include "Game/effects/effects.h"
#include "Game/effects/Electricity.h"
#include "Game/effects/explosion.h"
#include "Game/effects/spark.h"
#include "Game/effects/Streamer.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Utils/object_helper.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Effects/BlendIDs.h"
#include "Scripting/Internal/TEN/Effects/EffectIDs.h"
#include "Scripting/Internal/TEN/Effects/ParticleAnimTypes.h"
#include "Scripting/Internal/TEN/Effects/FeatherModes.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"
#include "Sound/sound.h"
#include "Specific/clock.h"
#include "Specific/trutils.h"
#include <Scripting/Internal/TEN/Objects/Moveable/MoveableObject.h>


/// Functions to generate effects.
// @tentable Effects 
// @pragma nostrip

using namespace TEN::Effects::Blood;
using namespace TEN::Effects::Bubble;
using namespace TEN::Effects::DisplaySprite;
using namespace TEN::Effects::Electricity;
using namespace TEN::Effects::Environment;
using namespace TEN::Effects::Explosion;
using namespace TEN::Effects::Spark;
using namespace TEN::Effects::Streamer;
using namespace TEN::Math;
using namespace TEN::Scripting::Types;

namespace TEN::Scripting::Effects
{
	/// Emit a lightning arc.
	// @function EmitLightningArc
	// @tparam Vec3 origin
	// @tparam Vec3 target
	// @tparam Color color (default Color(255, 255, 255))
	// @tparam float life Lifetime in seconds. Clamped to [0, 4.233] for now because of strange internal maths. __default: 1__
	// @tparam int amplitude "strength" of the lightning - the higher the value, the "taller" the arcs. Clamped to [1, 255]. __default: 20__
	// @tparam int beamWidth Clamped to [1, 127]. __default 2__
	// @tparam int detail Higher numbers equal more segments, but it's not a 1:1 correlation. Clamped to [1, 127]. __default: 10__
	// @tparam bool smooth If true, the arc will have large, smooth curves; if false, it will have small, jagged spikes. __default: false__
	// @tparam bool endDrift If true, the end of the arc will be able to gradually drift away from its destination in a random direction __default: false__
	static void EmitLightningArc(const Vec3& origin, const Vec3& target, TypeOrNil<ScriptColor> color, TypeOrNil<float> life, TypeOrNil<int> amplitude,
								 TypeOrNil<int> beamWidth, TypeOrNil<int> segments, TypeOrNil<bool> smooth, TypeOrNil<bool> endDrift)
	{
		constexpr auto LIFE_SEC_MAX = 4.233f;

		auto convertedOrigin = origin.ToVector3();
		auto convertedTarget = target.ToVector3();

		int segs = ValueOr<int>(segments, 10);

		segs = std::clamp(segs, 1, 127);

		int width = ValueOr<int>(beamWidth, 2);

		width = std::clamp(width, 1, 127);

		// Nearest number of milliseconds equating to approx 254, the max even byte value for "life".
		// This takes into account a "hardcoded" FPS of 30 and the fact that
		// lightning loses two "life" each frame.
		float convertedLife = ValueOr<float>(life, 1.0f);
		convertedLife = std::clamp(convertedLife, 0.0f, LIFE_SEC_MAX);

		constexpr float secsPerFrame = 1.0f / (float)FPS;

		// This will put us in the range [0, 127]
		int lifeInFrames = (int)round(convertedLife / secsPerFrame);

		// Multiply by two since a) lightning loses two "life" each frame, and b) it must be
		// an even number to avoid overshooting a value of 0 and wrapping around.
		byte byteLife = lifeInFrames * 2;

		int amp = ValueOr<int>(amplitude, 20);
		byte byteAmp = std::clamp(amp, 1, 255);

		bool isSmooth = ValueOr<bool>(smooth, false);
		bool isDrift = ValueOr<bool>(endDrift, false);

		char flags = 0;
		if(isSmooth)
			flags |= 1;

		if(isDrift)
			flags |= 2;

		auto convertedColor = ValueOr<ScriptColor>(color, ScriptColor( 255, 255, 255 ));

		SpawnElectricity(convertedOrigin, convertedTarget, byteAmp, convertedColor.GetR(), convertedColor.GetG(), convertedColor.GetB(), byteLife, flags, width, segs);
	}

	/// Emit a particle.
	// @function EmitParticle
	// @tparam Vec3 pos World position.
	// @tparam Vec3 vel Directional velocity.
	// @tparam int spriteID Sprite ID in the sprite sequence slot.
	// @tparam float gravity Effect of gravity. Positive value ascends, negative value descends. __default: 0__
	// @tparam float rotVel Rotational velocity in degrees. __default: 0__
	// @tparam Color startColor Color at start of life. __default: Color(255, 255, 255)__
	// @tparam Color endColor Color at end of life. This will finish long before the end of the particle's life due to internal math. __default: Color(255, 255, 255)__
	// @tparam Effects.BlendID blendMode Render blend mode. __TEN.Effects.BlendID.ALPHA_BLEND__
	// @tparam float startSize Size at start of life. __default: 10__
	// @tparam float endSize Size at end of life. __default: 0__
	// @tparam float life Lifespan in seconds. __default: 2__
	// @tparam bool damage Harm the player on collision. __default: false__
	// @tparam bool poison Poison the player on collision. __default: false__
	// @tparam Objects.ObjID.SpriteConstants spriteSeqID Sprite sequence slot ID. __default: Objects.ObjID.DEFAULT_SPRITES__
	// @tparam float startRot Rotation at start of life. __default: random__
	// @usage
	// EmitParticle(
	//     pos,
	// 	   Vec3(math.random(), math.random(), math.random()),
	// 	   22, -- spriteID
	// 	   0, -- gravity
	// 	   -2, -- rotVel
	// 	   Color(255, 0, 0), -- startColor
	// 	   Color(0,  255, 0), -- endColor
	// 	   TEN.Effects.BlendID.ADDITIVE, -- blendMode
	// 	   15, -- startSize
	// 	   50, -- endSize
	// 	   20, -- life
	// 	   false, -- damage
	// 	   true, -- poison
	//     Objects.ObjID.DEFAULT_SPRITES, -- spriteSeqID
	//     180) -- startRot
	static void EmitParticle(const Vec3& pos, const Vec3& vel, int spriteID, TypeOrNil<float> gravity, TypeOrNil<float> rotVel,
							 TypeOrNil<ScriptColor> startColor, TypeOrNil<ScriptColor> endColor, TypeOrNil<BlendMode> blendMode, 
							 TypeOrNil<float> startSize, TypeOrNil<float> endSize, TypeOrNil<float> life,
							 TypeOrNil<bool> applyDamage, TypeOrNil<bool> applyPoison, TypeOrNil<GAME_OBJECT_ID> spriteSeqID, TypeOrNil<float> startRot)
	{
		constexpr auto DEFAULT_START_SIZE = 10.0f;
		constexpr auto DEFAULT_LIFE		  = 2.0f;
		constexpr auto SECS_PER_FRAME	  = 1.0f / (float)FPS;

		static const auto DEFAULT_COLOR = ScriptColor(255, 255, 255);

		auto convertedSpriteSeqID = ValueOr<GAME_OBJECT_ID>(spriteSeqID, ID_DEFAULT_SPRITES); 
		if (!CheckIfSlotExists(convertedSpriteSeqID, "EmitParticle() script function."))
			return;

		auto& part = *GetFreeParticle();

		part.on = true;
		part.SpriteSeqID = convertedSpriteSeqID;
		part.SpriteID = spriteID;

		auto convertedBlendMode = ValueOr<BlendMode>(blendMode, BlendMode::AlphaBlend);
		part.blendMode = BlendMode(std::clamp((int)convertedBlendMode, (int)BlendMode::Opaque, (int)BlendMode::AlphaBlend));

		part.x = pos.x;
		part.y = pos.y;
		part.z = pos.z;
		part.roomNumber = FindRoomNumber(Vector3i(pos.x, pos.y, pos.z));

		part.xVel = short(vel.x * 32);
		part.yVel = short(vel.y * 32);
		part.zVel = short(vel.z * 32);

		part.rotAng = ANGLE(ValueOr<float>(startRot, TO_DEGREES(Random::GenerateAngle()))) >> 4;
		part.rotAdd = ANGLE(ValueOr<float>(rotVel, 0.0f)) >> 4;
		
		part.sSize =
		part.size = ValueOr<float>(startSize, DEFAULT_START_SIZE);
		part.dSize = ValueOr<float>(endSize, 0.0f);
		part.scalar = 2;

		part.gravity = (short)std::clamp(ValueOr<float>(gravity, 0.0f), (float)SHRT_MIN, (float)SHRT_MAX);
		part.friction = 0;
		part.maxYvel = 0;

		auto convertedStartColor = ValueOr<ScriptColor>(startColor, DEFAULT_COLOR);
		part.sR = convertedStartColor.GetR();
		part.sG = convertedStartColor.GetG();
		part.sB = convertedStartColor.GetB();

		auto convertedEndColor = ValueOr<ScriptColor>(endColor, DEFAULT_COLOR);
		part.dR = convertedEndColor.GetR();
		part.dG = convertedEndColor.GetG();
		part.dB = convertedEndColor.GetB();

		float convertedLife = std::max(0.1f, ValueOr<float>(life, DEFAULT_LIFE));
		part.life =
		part.sLife = (int)round(convertedLife / SECS_PER_FRAME);
		part.colFadeSpeed = part.life / 2;
		part.fadeToBlack = part.life / 3;

		part.flags = SP_SCALE | SP_ROTATE | SP_DEF | SP_EXPDEF;

		bool convertedApplyPoison = ValueOr<bool>(applyPoison, false);
		if (convertedApplyPoison)
			part.flags |= SP_POISON;

		bool convertedApplyDamage = ValueOr<bool>(applyDamage, false);
		if (convertedApplyDamage)
		{
			part.flags |= SP_DAMAGE;
			part.damage = 2;
		}

		// TODO: Add option to turn off wind.
		if (TestEnvironment(RoomEnvFlags::ENV_FLAG_WIND, part.roomNumber))
			part.flags |= SP_WIND;
	}

	/// Emit a particle with extensive configuration options, including sprite sequence animation, lights, sounds, and damage effects.
	// @function EmitAdvancedParticle
	// @tparam ParticleData ParticleData Table containing particle data.
	// @usage
	// local particle =
	// {
	//     pos = GetMoveableByName("camera_target_6"):GetPosition(), 
	//     vel = Vec3(0, 0, 10),
	//     spriteSeqID = TEN.Objects.ObjID.CUSTOM_BAR_GRAPHIC,
	//     spriteID = 0,
	//     life = 10,
	//     maxYVel = 0,
	//     gravity = 0,
	//     friction = 10,
	//     startRot = 0,
	//     rotVel = 0,
	//     startSize = 80,
	//     endSize = 80,
	//     startColor = TEN.Color(128, 128, 128),
	//     endColor = TEN.Color(128, 128, 128),
	//     blendMode = TEN.Effects.BlendID.ADDITIVE,
	//     wind = false,
	//     damage = true,
	//     poison = false,
	//     burn = false,
	//     damageHit = 80,
	//     soundID = 197,
	//     light = true,
	//     lightRadius = 6, 
	//     lightFlicker = 5, 
	//     animated = true,
	//     frameRate = 0.25,
	//     animType = TEN.Effects.ParticleAnimationType.LOOP,
	// }
	// EmitAdvancedParticle(particle)

	/// Structure for EmitAdvancedParticle table.
	// @table ParticleData
	// @tfield Vec3 pos World position.
	// @tfield Vec3 vel Directional velocity in world units per second.
	// @tfield[opt] Objects.ObjID.SpriteConstants spriteSeqID Sprite sequence slot ID. __default: Objects.ObjID.DEFAULT_SPRITES__
	// @tfield[opt] int spriteID Sprite ID in the sprite sequence slot. __default: 0__
	// @tfield[opt] float life Lifespan in seconds. __default: 2__
	// @tfield[opt] float maxYVel Maximum vertical velocity in world units per second. __default: 0__
	// @tfield[opt] float gravity Effect of gravity in world units per second. Positive value ascend, negative value descend. __default: 0__
	// @tfield[opt] float friction Friction affecting velocity over time in world units per second. __default: 0__
	// @tfield[opt] float startRot Rotation at start of life. __default: random__
	// @tfield[opt] float rotVel Rotational velocity in degrees per second. __default: 0__
	// @tfield[opt] float startSize Size at start of life. __default: 10__
	// @tfield[opt] float endSize Size at end of life. __default: 0__
	// @tfield[opt] Color startColor Color at start of life. __default: Color(255, 255, 255)__
	// @tfield[opt] Color endColor Color at end of life. Note that this will finish long before the end of life due to internal math. __default: Color(255, 255, 255)__
	// @tfield[opt] Effects.BlendID blendMode Render blend mode. __default: TEN.Effects.BlendID.ALPHA_BLEND__
	// @tfield[opt] bool damage Harm the player on collision. __default: false__
	// @tfield[opt] bool poison Poison the player on collision. __default: false__
	// @tfield[opt] bool burn Burn the player on collision. __default: false__
	// @tfield[opt] bool wind Affect position by wind in outside rooms. __default: false__
	// @tfield[opt] int damageHit Player damage amount on collision. __default: 2__
	// @tfield[opt] bool light Emit a colored light. CAUTION: Recommended only for a single particle. Too many particles with lights can overwhelm the lighting system. __default: false__
	// @tfield[opt] int lightRadius Light radius in 1/4 blocks. __default: 0__
	// @tfield[opt] int lightFlicker Interval at which the light should flicker. __default: 0__
	// @tfield[opt] int soundID Sound ID to play. CAUTION: Recommended only for a single particle. Too many particles with sounds can overwhelm the sound system. __default: none__
	// @tfield[opt] bool animated Play animates sprite sequence. __default: false__
	// @tfield[opt] Effects.ParticleAnimationType animType Animation type of the sprite sequence. __default: TEN.Effects.ParticleAnimationType.LOOP__
	// @tfield[opt] float frameRate Sprite sequence animation framerate. __default: 1__
	static void EmitAdvancedParticle(const sol::table& table)
	{
		constexpr auto DEFAULT_START_SIZE = 10.0f;
		constexpr auto DEFAULT_LIFE		  = 2.0f;

		auto convertedSpriteSeqID = table.get_or("spriteSeqID", ID_DEFAULT_SPRITES);
		if (!CheckIfSlotExists(convertedSpriteSeqID, "EmitParticle() script function."))
			return;

		auto& part = *GetFreeParticle();

		part.on = true;
		part.SpriteSeqID = convertedSpriteSeqID;
		part.SpriteID = table.get_or("spriteID", 0);

		auto bMode = table.get_or("blendMode", BlendMode::AlphaBlend);
		part.blendMode = bMode;

		auto pos = (Vec3)table["pos"];
		part.x = pos.x;
		part.y = pos.y;
		part.z = pos.z;
		part.roomNumber = FindRoomNumber(pos.ToVector3i());

		auto vel = ((Vec3)table["vel"]) / (float)FPS;
		part.xVel = short(vel.x * 32);
		part.yVel = short(vel.y * 32);
		part.zVel = short(vel.z * 32);

		float startRot = table.get_or("startRot", TO_DEGREES(Random::GenerateAngle()));
		float rotVel = table.get_or("rotVel", 0.0f) / (float)FPS;
		part.rotAng = ANGLE(startRot) >> 4;
		part.rotAdd = ANGLE(rotVel) >> 4;

		part.sSize =
		part.size = table.get_or("startSize", DEFAULT_START_SIZE);
		part.dSize = table.get_or("endSize", 0.0f);
		part.scalar = 2;

		part.gravity = (short)(std::clamp<float>(table.get_or("gravity", 0.0f), SHRT_MIN, SHRT_MAX) / (float)FPS);
		part.friction = table.get_or("friction", 0.0f) / (float)FPS;
		part.maxYvel = table.get_or("maxYVel", 0.0f) / (float)FPS;

		auto convertedStartColor = table.get_or("startColor", ScriptColor(255, 255, 255));
		part.sR = convertedStartColor.GetR();
		part.sG = convertedStartColor.GetG();
		part.sB = convertedStartColor.GetB();

		auto convertedEndColor = table.get_or("endColor", ScriptColor(255, 255, 255));
		part.dR = convertedEndColor.GetR();
		part.dG = convertedEndColor.GetG();
		part.dB = convertedEndColor.GetB();

		float convertedLife = std::max(0.1f, (float)table.get_or("life", DEFAULT_LIFE));
		part.life =
		part.sLife = (int)round(convertedLife / (1.0f / (float)FPS));
		part.colFadeSpeed = part.life / 2;
		part.fadeToBlack = part.life / 3;

		part.flags = SP_SCALE | SP_ROTATE | SP_DEF | SP_EXPDEF;

		part.damage = table.get_or("damageHit", 2);

		bool convertedApplyPoison = table.get_or("poison", false);
		if (convertedApplyPoison)
			part.flags |= SP_POISON;

		bool convertedApplyDamage = table.get_or("damage", false);
		if (convertedApplyDamage)
			part.flags |= SP_DAMAGE;

		bool convertedApplyBurn = table.get_or("burn", false);
		if (convertedApplyBurn)
			part.flags |= SP_FIRE;

		int convertedSoundID = table.get_or("soundID", NO_VALUE);
		if (convertedSoundID != NO_VALUE)
		{
			part.flags |= SP_SOUND;
			part.sound = convertedSoundID;
		}
		bool convertedApplyLight = table.get_or("light", false);
		if (convertedApplyLight)
		{
			part.flags |= SP_LIGHT;
			int lightRadius = table.get_or("lightRadius", 0);
			part.lightRadius = lightRadius * BLOCK(0.25f);
			int flicker = table.get_or("lightFlicker", 0);
			
			if (flicker > 0)
			{
				part.lightFlicker = table.get_or("lightFlicker", 0);
				part.lightFlickerS = table.get_or("lightFlicker", 0);
			}
		}
		bool animatedSpr = table.get_or("animated", false);
		if (animatedSpr)
		{
			auto applyAnim = (ParticleAnimType)table.get_or("animType", ParticleAnimType::Loop);
			float applyFramerate = table.get_or("frameRate", 1.0f);
			part.flags |= SP_ANIMATED;
			part.framerate = applyFramerate;
			part.animationType = ParticleAnimType(std::clamp(int(applyAnim), int(ParticleAnimType::None), int(ParticleAnimType::LifetimeSpread)));

		}

		bool convertedApplyWind = table.get_or("wind", false);
		if (convertedApplyWind)
		{
			if (TestEnvironment(RoomEnvFlags::ENV_FLAG_WIND, part.roomNumber))
				part.flags |= SP_WIND;
		}
	}
	
	/// Emit a shockwave, similar to that seen when a harpy projectile hits something.
	// @function EmitShockwave
	// @tparam Vec3 pos Origin position
	// @tparam int innerRadius (default 0) Initial inner radius of the shockwave circle - 128 will be approx a click, 512 approx a block
	// @tparam int outerRadius (default 128) Initial outer radius of the shockwave circle
	// @tparam Color color (default Color(255, 255, 255))
	// @tparam float lifetime (default 1.0) Lifetime in seconds (max 8.5 because of inner maths weirdness)
	// @tparam int speed (default 50) Initial speed of the shockwave's expansion (the shockwave will always slow as it goes)
	// @tparam int angle (default 0) Angle about the X axis - a value of 90 will cause the shockwave to be entirely vertical
	// @tparam bool hurtsLara (default false) If true, the shockwave will hurt Lara, with the damage being relative to the shockwave's current speed
	static void EmitShockwave(Vec3 pos, TypeOrNil<int> innerRadius, TypeOrNil<int> outerRadius, TypeOrNil<ScriptColor> col,
							  TypeOrNil<float> lifetime, TypeOrNil<int> speed, TypeOrNil<int> angle, TypeOrNil<bool> hurtPlayer)
	{
		constexpr auto LIFE_IN_SECONDS_MAX = 8.5f;
		constexpr auto SECONDS_PER_FRAME   = 1 / (float)FPS;

		auto pose = Pose(Vector3i(pos.x, pos.y, pos.z));

		int innerRad = ValueOr<int>(innerRadius, 0);
		int outerRad = ValueOr<int>(outerRadius, 128);

		auto color = ValueOr<ScriptColor>(col, ScriptColor(255, 255, 255));
		int spd = ValueOr<int>(speed, 50);
		int ang = ValueOr<int>(angle, 0);
 
		float life = ValueOr<float>(lifetime, 1.0f);
		life = std::clamp(life, 0.0f, LIFE_IN_SECONDS_MAX);

		// Normalize to range [0, 255].
		int lifeInFrames = (int)round(life / SECONDS_PER_FRAME);

		bool doDamage = ValueOr<bool>(hurtPlayer, false);

		TriggerShockwave(
			&pose, innerRad, outerRad, spd,
			color.GetR(), color.GetG(), color.GetB(),
			lifeInFrames, EulerAngles(ANGLE(ang), 0.0f, 0.0f),
			(short)doDamage, true, false, false, (int)ShockwaveStyle::Normal);
	}

	/// Emit dynamic light that lasts for a single frame.
	// If you want a light that sticks around, you must call this each frame.
	// @function EmitLight
	// @tparam Vec3 pos position of the light
	// @tparam[opt] Color color light color (default Color(255, 255, 255))
	// @tparam[opt] int radius measured in "clicks" or 256 world units (default 20)
	// @tparam[opt] bool shadows determines whether light should generate dynamic shadows for applicable moveables (default is false)
	// @tparam[opt] string name if provided, engine will interpolate this light for high framerate mode (be careful not to use same name for different lights)
	static void EmitLight(Vec3 pos, TypeOrNil<ScriptColor> col, TypeOrNil<int> radius, TypeOrNil<bool> castShadows, TypeOrNil<std::string> name)
	{
		auto color = ValueOr<ScriptColor>(col, ScriptColor(255, 255, 255));
		int rad = (float)(ValueOr<int>(radius, 20) * BLOCK(0.25f));
		SpawnDynamicPointLight(pos.ToVector3(), color, rad, ValueOr<bool>(castShadows, false), GetHash(ValueOr<std::string>(name, std::string())));
	}

	/// Emit dynamic directional spotlight that lasts for a single frame.
	// If you want a light that sticks around, you must call this each frame.
	// @function EmitSpotLight
	// @tparam Vec3 pos position of the light
	// @tparam Vec3 dir normal which indicates light direction
	// @tparam[opt] Color color (default Color(255, 255, 255))
	// @tparam[opt] int radius overall radius at the endpoint of a light cone, measured in "clicks" or 256 world units (default 10)
	// @tparam[opt] int falloff radius, at which light starts to fade out, measured in "clicks" (default 5)
	// @tparam[opt] int distance distance, at which light cone fades out, measured in "clicks" (default 20)
	// @tparam[opt] bool shadows determines whether light should generate dynamic shadows for applicable moveables (default is false)
	// @tparam[opt] string name if provided, engine will interpolate this light for high framerate mode (be careful not to use same name for different lights)
	static void EmitSpotLight(Vec3 pos, Vec3 dir, TypeOrNil<ScriptColor> col, TypeOrNil<int> radius, TypeOrNil<int> falloff, TypeOrNil<int> distance, TypeOrNil<bool> castShadows, TypeOrNil<std::string> name)
	{
		auto color = ValueOr<ScriptColor>(col, ScriptColor(255, 255, 255));
		int rad =	  (float)(ValueOr<int>(radius,   10) * BLOCK(0.25f));
		int fallOff = (float)(ValueOr<int>(falloff,   5) * BLOCK(0.25f));
		int dist =	  (float)(ValueOr<int>(distance, 20) * BLOCK(0.25f));
		SpawnDynamicSpotLight(pos.ToVector3(), dir.ToVector3(), color, rad, fallOff, dist, ValueOr<bool>(castShadows, false), GetHash(ValueOr<std::string>(name, std::string())));
	}

	/// Emit blood.
	// @function EmitBlood
	// @tparam Vec3 pos
	// @tparam int count Sprite count. __default: 1__
	static void EmitBlood(const Vec3& pos, TypeOrNil<int> count)
	{
		int roomNumber = FindRoomNumber(pos.ToVector3i());
		const auto& room = g_Level.Rooms[roomNumber];
		if (room.flags & ENV_FLAG_WATER)
			SpawnUnderwaterBloodCloud(pos, roomNumber, (GetRandomControl() & 7) + 8, ValueOr<int>(count, 1));
		else
			TriggerBlood(pos.x, pos.y, pos.z, -1, ValueOr<int>(count, 1));
	}

	/// Emit an air bubble in a water room.
	// @function EmitAirBubble
	// @tparam Vec3 pos World position where the effect will be spawned. Must be in a water room.
	// @tparam[opt] float size Sprite size. __Default: 32__
	// @tparam[opt] float amp Oscillation amplitude. __Default: 32__
	static void EmitAirBubble(const Vec3& pos, TypeOrNil<float> size, TypeOrNil<float> amp)
	{
		constexpr auto DEFAULT_SIZE = 128.0f;
		constexpr auto DEFAULT_AMP	= 32.0f;

		int roomNumber = FindRoomNumber(pos.ToVector3i());
		float convertedSize = ValueOr<float>(size, DEFAULT_SIZE);
		float convertedAmp = ValueOr<float>(amp, DEFAULT_AMP);
		SpawnBubble(pos.ToVector3(), roomNumber, convertedSize, convertedAmp);
	}

	/// Emit waterfall mist.
	// @function EmitWaterfallMist
	// @tparam Vec3 pos World position where the effect will be spawned.
	// @tparam[opt] float size Effect size. __Default: 64__
	// @tparam[opt] float width Width of the effect. __Default: 32__
	// @tparam[opt] float rot Rotation of effect in degrees. __Default: 0__
	// @tparam[opt] Color color Color of the effect.__Default: Color(255, 255, 255, 255))__
	static void EmitWaterfallMist(const Vec3& pos, TypeOrNil<int> size, TypeOrNil<int> width, TypeOrNil<float> angle, TypeOrNil<ScriptColor> color)
	{
		constexpr auto DEFAULT_SIZE = 64;
		constexpr auto DEFAULT_WIDTH = 32;

		auto convertedAngle = ANGLE(ValueOr<float>(angle, 0.0f));
		auto convertedSize = ValueOr<int>(size, DEFAULT_SIZE);
		auto convertedWidth = ValueOr<int>(width, DEFAULT_WIDTH);
		auto _color = ValueOr<ScriptColor>(color, ScriptColor(255, 255, 255));
		auto convertedColor = Vector4(_color.GetR(), _color.GetG(), _color.GetB(), _color.GetA()) / UCHAR_MAX;

		TriggerWaterfallMist(pos, convertedSize, convertedWidth, convertedAngle, convertedColor);
	}

	/// Emit fire for one frame. Will not hurt player. Call this each frame if you want a continuous fire.
	// @function EmitFire
	// @tparam Vec3 pos
	// @tparam float size Fire size. __default: 1__
	static void EmitFire(const Vec3& pos, TypeOrNil<float> size)
	{
		AddFire(pos.x, pos.y, pos.z, FindRoomNumber(pos.ToVector3i()), ValueOr<float>(size, 1));
	}

	/// Make an explosion. Does not hurt Lara
	// @function MakeExplosion 
	// @tparam Vec3 pos
	// @tparam float size (default 512.0) this will not be the size of the sprites, but rather the distance between the origin and any additional sprites
	// @tparam bool shockwave (default false) if true, create a very faint white shockwave which will not hurt Lara. For underwater rooms it creates a splash if the pos is near the surface.
	static void MakeExplosion(Vec3 pos, TypeOrNil<float> size, TypeOrNil<bool> shockwave)
	{
		int roomNumber = FindRoomNumber(pos.ToVector3i());
		const auto& room = g_Level.Rooms[roomNumber];

		if (room.flags & ENV_FLAG_WATER)
			TriggerUnderwaterExplosion(pos.ToVector3(), ValueOr<bool>(shockwave, false));
		else
			TriggerExplosion(Vector3(pos.x, pos.y, pos.z), ValueOr<float>(size, 512.0f), true, false, ValueOr<bool>(shockwave, false), FindRoomNumber(Vector3i(pos.x, pos.y, pos.z)));
	}

	/// Make an earthquake
	// @function MakeEarthquake 
	// @tparam int strength (default 100) How strong should the earthquake be? Increasing this value also increases the lifespan of the earthquake.
	static void Earthquake(TypeOrNil<int> strength)
	{
		int str = ValueOr<int>(strength, 100);
		Camera.bounce = -str;
	}

	/// Get the wind vector for the current game frame.
	// This represents the 3D displacement applied by the engine on things like particles affected by wind.
	// @function GetWind()
	// @treturn Vec3 Wind vector.
	static Vec3 GetWind()
	{
		return Vec3(Weather.Wind());
	}

	/// Emit an extending streamer effect.
	// @function EmitStreamer
	// @tparam Moveable mov Moveable object with which to associate the effect.
	// @tparam int[opt] tag Numeric tag with which to associate the effect on the moveable. __Default: 0__
	// @tparam Vec3 pos World position.
	// @tparam Vec3 dir Direction vector of movement velocity.
	// @tparam[opt] float rot Start rotation in degrees. __Default: 0__
	// @tparam[opt] Color startColor Color at the start of life. __Default: Color(255, 255, 255, 255))__
	// @tparam[opt] Color endColor Color at the end of life. __Default: Color(0, 0, 0, 0))__
	// @tparam[opt] float width Width in world units. __Default: 0__
	// @tparam[opt] float life Lifetime in seconds. __Default: 1__
	// @tparam[opt] float vel Movement velocity in world units per second. __Default: 0__
	// @tparam[opt] float expRate Width expansion rate in world units per second. __Default: 0__
	// @tparam[opt] float rotRate Rotation rate in degrees per second. __Default: 0__
	// @tparam[opt] Effects.StreamerFeatherMode edgeFeatherMode Edge feather mode. __Default: Effects.StreamerFeatherMode.NONE__
	// @tparam[opt] Effects.StreamerFeatherMode lengthFeatherMode Length feather mode. __UNIMPLEMENTED, currently will always leave a fading tail__
	// @tparam[opt] Effects.BlendID blendID Renderer blend ID. __Default: Effects.BlendID.ALPHA_BLEND__
	static void EmitStreamer(const Moveable& mov, TypeOrNil<int> tag, const Vec3& pos, const Vec3& dir, TypeOrNil<float> rot, TypeOrNil<ScriptColor> startColor, TypeOrNil<ScriptColor> endColor,
							 TypeOrNil<float> width, TypeOrNil<float> life, TypeOrNil<float> vel, TypeOrNil<float> expRate, TypeOrNil<float> rotRate,
							 TypeOrNil<StreamerFeatherMode> edgeFeatherMode, TypeOrNil<StreamerFeatherMode> lengthFeatherMode, TypeOrNil<BlendMode> blendID)
	{
		int movID = mov.GetIndex();
		int convertedTag = ValueOr<int>(tag, 0);
		auto convertedPos = pos.ToVector3();
		auto convertedDir = dir.ToVector3();
		auto convertedRot = ANGLE(ValueOr<float>(rot, 0));
		auto convertedStartColor = ValueOr<ScriptColor>(startColor, ScriptColor(255, 255, 255, 255));
		auto convertedEndColor = ValueOr<ScriptColor>(endColor, ScriptColor(0, 0, 0, 0));

		auto convertedWidth = ValueOr<float>(width, 0.0f);
		auto convertedLife = ValueOr<float>(life, 1.0f);
		auto convertedVel = ValueOr<float>(vel, 0.0f) / (float)FPS;
		auto convertedExpRate = ValueOr<float>(expRate, 0.0f) / (float)FPS;
		auto convertedRotRate = ANGLE(ValueOr<float>(rotRate, 0.0f) / (float)FPS);

		auto convertedEdgeFeatherID = ValueOr<StreamerFeatherMode>(edgeFeatherMode, StreamerFeatherMode::None);
		auto convertedLengthFeatherID = ValueOr<StreamerFeatherMode>(lengthFeatherMode, StreamerFeatherMode::Left);
		auto convertedBlendID = ValueOr<BlendMode>(blendID, BlendMode::AlphaBlend);

		StreamerEffect.Spawn(
			movID, convertedTag, convertedPos, convertedDir, convertedRot, convertedStartColor, convertedEndColor,
			convertedWidth, convertedLife, convertedVel, convertedExpRate, convertedRotRate,
			convertedEdgeFeatherID, /*convertedLengthFeatherID, */convertedBlendID);
	}

	/// Emit a visual sink effect.
	// @function EmitSink
	// @tparam Vec3 pos World position.
	// @tparam Vec3 dir Directional vector.
	// @tparam[opt] float radius Radius of emitter. The particles will be emitted inside the circle of provided radius measured from centre of world position. __default: 512__ 
	// @tparam[opt] float life Lifespan in seconds. __default: 1__
	// @tparam[opt] float vel Velocity of the particles in world units per second. __default: 512__
	// @tfield[opt] float friction Friction affecting velocity over time in world units per second. __default: 0__
	// @tparam[opt] float maxSize Max size of the particle. __default: 25__
	// @tparam[opt] Color startColor Color at start of life. __default: Color(128, 128, 128)__
	// @tparam[opt] Color endColor Color at end of life. __default: Color(0, 0, 0)__
	static void EmitSink(const Vec3& pos, const Vec3& dir, TypeOrNil<float> radius, TypeOrNil<float> life, TypeOrNil<float> vel, TypeOrNil<float> friction, TypeOrNil<float> maxSize, TypeOrNil<ScriptColor> startColor, TypeOrNil<ScriptColor> endColor)
	{
		constexpr auto DEFAULT_LIFE = 1.0f;
		constexpr auto SECS_PER_FRAME = 1.0f / (float)FPS;
		constexpr auto DUST_SIZE_MAX = 25.0f;

		auto convertedPos = pos.ToVector3();
		auto convertedDir = dir.ToVector3();
		auto convertedRad = ValueOr<float>(radius, BLOCK(0.5f));
		auto convertedLife = std::max(0.1f, ValueOr<float>(life, DEFAULT_LIFE));
		auto convertedVel = ValueOr<float>(vel, BLOCK(0.5f)) / (float)FPS;
		auto convertedFriction = ValueOr<float>(friction, 0) / (float)FPS;
		auto convertedMaxSize = std::max(0.1f, ValueOr<float>(maxSize, DUST_SIZE_MAX));
		auto convertedStartColor = ValueOr<ScriptColor>(startColor, ScriptColor(128, 128, 128, 255));
		auto convertedEndColor = ValueOr<ScriptColor>(endColor, ScriptColor(0, 0, 0, 255));

		auto& part = *GetFreeParticle();

		part.on = true;
		part.SpriteSeqID = ID_DEFAULT_SPRITES;
		part.SpriteID = SPRITE_TYPES::SPR_UNDERWATERDUST;
		part.blendMode = BlendMode::Additive;

		// Set particle colors
		part.sR = convertedStartColor.GetR();
		part.sG = convertedStartColor.GetG();
		part.sB = convertedStartColor.GetB();

		part.dR = convertedEndColor.GetR();
		part.dG = convertedEndColor.GetG();
		part.dB = convertedEndColor.GetB();

		part.life =
			part.sLife = (int)round(convertedLife / SECS_PER_FRAME);
		part.colFadeSpeed = part.life / 2;
		part.fadeToBlack = part.life / 3;

		// Randomize position within the given radius
		float angle = TO_DEGREES(Random::GenerateAngle());
		float randRadius = sqrt(Random::GenerateFloat()) * convertedRad;

		part.x = convertedPos.x + randRadius * cos(angle);
		part.y = convertedPos.y + (Random::GenerateFloat() * 2.0f - 1.0f) * convertedRad;
		part.z = convertedPos.z + randRadius * sin(angle);
		part.roomNumber = FindRoomNumber(Vector3i(part.x, part.y, part.z));

		// Normalize direction
		convertedDir.Normalize();

		part.xVel = convertedDir.x * convertedVel * 32;
		part.yVel = convertedDir.y * convertedVel * 32;
		part.zVel = convertedDir.z * convertedVel * 32;

		// Other properties
		part.friction = convertedFriction;
		part.maxYvel = 0;
		part.gravity = 0;
		part.flags = SP_DEF | SP_EXPDEF;
		part.sSize = part.size = Random::GenerateFloat(convertedMaxSize / 2, convertedMaxSize);
	}

	void Register(sol::state* state, sol::table& parent) 
	{
		auto tableEffects = sol::table(state->lua_state(), sol::create);
		parent.set(ScriptReserved_Effects, tableEffects);

		// Emitters
		tableEffects.set_function(ScriptReserved_EmitLightningArc, &EmitLightningArc);
		tableEffects.set_function(ScriptReserved_EmitParticle, &EmitParticle);
		tableEffects.set_function(ScriptReserved_EmitAdvancedParticle, &EmitAdvancedParticle);
		tableEffects.set_function(ScriptReserved_EmitShockwave, &EmitShockwave);
		tableEffects.set_function(ScriptReserved_EmitLight, &EmitLight);
		tableEffects.set_function(ScriptReserved_EmitSpotLight, &EmitSpotLight);
		tableEffects.set_function(ScriptReserved_EmitBlood, &EmitBlood);
		tableEffects.set_function(ScriptReserved_EmitAirBubble, &EmitAirBubble);
		tableEffects.set_function(ScriptReserved_EmitStreamer, &EmitStreamer);
		tableEffects.set_function(ScriptReserved_EmitFire, &EmitFire);
		tableEffects.set_function(ScriptReserved_EmitWaterfallMist, &EmitWaterfallMist);
		tableEffects.set_function(ScriptReserved_EmitSink, &EmitSink);
		tableEffects.set_function(ScriptReserved_MakeExplosion, &MakeExplosion);
		tableEffects.set_function(ScriptReserved_MakeEarthquake, &Earthquake);
		tableEffects.set_function(ScriptReserved_GetWind, &GetWind);

		auto handler = LuaHandler(state);
		handler.MakeReadOnlyTable(tableEffects, ScriptReserved_BlendID, BLEND_IDS);
		handler.MakeReadOnlyTable(tableEffects, ScriptReserved_EffectID, EFFECT_IDS);
		handler.MakeReadOnlyTable(tableEffects, ScriptReserved_FeatherMode, FEATHER_MODES);
		handler.MakeReadOnlyTable(tableEffects, ScriptReserved_ParticleAnimationType, PARTICLE_ANIM_TYPES);
	}
}


