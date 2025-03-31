#pragma once

#include "Game/Items.h"
#include "Game/effects/Light.h"
#include "Math/Math.h"
#include "Renderer/RendererEnums.h"

using namespace TEN::Effects::Light;

enum class LaraWeaponType;
enum GAME_OBJECT_ID : short;
struct CollisionInfo;
struct ItemInfo;

constexpr auto SD_EXPLOSION	  = 1;
constexpr auto SD_UWEXPLOSION = 2;

constexpr auto MAX_NODE		= 23;
constexpr auto MAX_DYNAMICS = 64;

constexpr auto MAX_PARTICLES		 = 8192;
constexpr auto MAX_PARTICLE_DYNAMICS = 8;

extern int Wibble;

enum SpriteEnumFlag
{
	SP_NONE		  = 0,
	SP_FIRE		  = (1 << 0),
	SP_SCALE	  = (1 << 1),
	SP_BLOOD	  = (1 << 2),
	SP_DEF		  = (1 << 3),
	SP_ROTATE	  = (1 << 4),
	SP_EXPLOSION  = (1 << 5),
	SP_FX		  = (1 << 6),
	SP_ITEM		  = (1 << 7),
	SP_WIND		  = (1 << 8),
	SP_EXPDEF	  = (1 << 9),
	SP_DAMAGE	  = (1 << 10),
	SP_UNDERWEXP  = (1 << 11),
	SP_NODEATTACH = (1 << 12),
	SP_PLASMAEXP  = (1 << 13),
	SP_POISON	  = (1 << 14),
	SP_COLOR	  = (1 << 15),
	SP_ANIMATED	  = (1 << 16),
	SP_LIGHT	  = (1 << 17),
	SP_SOUND	  = (1 << 18),
	SP_CONSTRAINED = (1 << 19),
};

enum ParticleAnimType
{
	None,
	OneShot,
	Loop,
	BackAndForth,
	LifetimeSpread
};

// Used by Particle.nodeNumber.
enum ParticleNodeOffsetIDs
{
	NodeUnknown0, // TR5
	NodeUnknown1,
	NodeUnknown2,
	NodeUnknown3,
	NodeUnknown4,
	NodeUnknown5,
	NodeUnknown6,
	NodeUnknown7,
	NodeUnknown8,
	NodePilotFlame, // TR3-5
	NodeWasp,
	NodePunkFlame,
	NodePendulumFlame,
	NodeTonyHandLeftFlame,
	NodeTonyHandRightFlame,
	NodeClawMutantPlasma,
	NodeWillardBossLeftPlasma,
	NodeWillardBossRightPlasma,
	NodeEmpty, // Empty node (mesh 0, position 0)
	NodeMax
};

enum class FlameType
{
	Big,
	Medium,
	Small,
	Static,
	Pulse,
	SmallFast,
	Trail
};

struct FX_INFO
{
	Pose pos;
	short roomNumber;
	short objectNumber;
	short nextFx;
	short nextActive;
	short speed;
	short fallspeed;
	int frameNumber;
	short counter;
	Vector4 color;
	short flag1;
	short flag2;

	bool DisableInterpolation;
};

struct NODEOFFSET_INFO
{
	short x;
	short y;
	short z;
	char meshNum;
	unsigned char gotIt;
};

// TODO: Refactor this entire struct.
struct Particle
{
	bool on;

	GAME_OBJECT_ID SpriteSeqID = GAME_OBJECT_ID::ID_DEFAULT_SPRITES;
	int	SpriteID = 0;
	int	fxObj;

	int x;
	int y;
	int z;
	int roomNumber;
	Vector3 targetPos;

	short xVel;
	short yVel;
	short zVel;

	short rotAng; // TODO: Due to legacy conventions, assigned values must be shifted >> 4.
	short rotAdd; // TODO: Due to legacy conventions, assigned values must be shifted >> 4.

	short gravity;
	unsigned int flags; // SP_enum
  
	float sSize;
	float dSize;
	float size;

	unsigned int friction;
	unsigned int scalar;
	int maxYvel;

	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char sR;
	unsigned char sG;
	unsigned char sB;
	unsigned char dR;
	unsigned char dG;
	unsigned char dB;

	unsigned char colFadeSpeed;
	unsigned char fadeToBlack;

	int sLife;
	int life;

	BlendMode blendMode;
	unsigned char extras;
	signed char dynamic;
	unsigned char nodeNumber; // ParticleNodeOffsetIDs enum.
  
	int damage;
	float framerate;
	ParticleAnimType animationType;

	int lightRadius;
	int lightFlicker;
	int lightFlickerS;

	int sound;

	Vector3 constraint;

	int PrevX;
	int PrevY;
	int PrevZ;
	short PrevRotAng;
	byte PrevR;
	byte PrevG; 
	byte PrevB;
	byte PrevScalar;

	void StoreInterpolationData()
	{
		PrevX = x;
		PrevY = y;
		PrevZ = z;
		PrevRotAng = rotAng;
		PrevR = r;
		PrevG = g;
		PrevB = b;
		PrevScalar = scalar;
	}
};

struct ParticleDynamic
{
	byte On;
	byte Falloff;
	byte R;
	byte G;
	byte B;
	byte Flags;
	byte Pad[2];
};

extern GameBoundingBox DeadlyBounds;

// New particle class
extern Particle Particles[MAX_PARTICLES];
extern ParticleDynamic ParticleDynamics[MAX_PARTICLE_DYNAMICS];

extern Vector3i NodeVectors[ParticleNodeOffsetIDs::NodeMax];
extern NODEOFFSET_INFO NodeOffsets[ParticleNodeOffsetIDs::NodeMax];

extern FX_INFO EffectList[MAX_SPAWNED_ITEM_COUNT];

template <typename TEffect>
TEffect& GetNewEffect(std::vector<TEffect>& effects, unsigned int countMax)
{
	TENAssert(effects.size() <= countMax, "Too many particle effects.");

	// Add and return new effect.
	if (effects.size() < countMax)
		return effects.emplace_back();

	TEffect* effectPtr = nullptr;
	float shortestLife = INFINITY;

	// Find effect with shortest remaining life.
	for (auto& effect : effects)
	{
		if (effect.Life < shortestLife)
		{
			effectPtr = &effect;
			shortestLife = effect.Life;
		}
	}

	// Clear and return existing effect.
	*effectPtr = {};
	return *effectPtr;
}

template <typename TEffect>
void ClearInactiveEffects(std::vector<TEffect>& effects)
{
	effects.erase(
		std::remove_if(
			effects.begin(), effects.end(),
			[](const auto& effect) { return (effect.Life <= 0.0f); }),
		effects.end());
}

Particle* GetFreeParticle();

void SetSpriteSequence(Particle& particle, GAME_OBJECT_ID objectID);
void SetAdvancedSpriteSequence(Particle& particle, GAME_OBJECT_ID objectID, ParticleAnimType animationType, float frameRate);

void DetatchSpark(int num, SpriteEnumFlag type);
void UpdateSparks();
void TriggerRicochetSpark(const GameVector& pos, short angle, bool sound = true);
void TriggerCyborgSpark(int x, int y, int z, short xv, short yv, short zv);
void TriggerExplosionSparks(int x, int y, int z, int extraTrig, int dynamic, int uw, int roomNumber, const Vector3& mainColor = Vector3::Zero, const Vector3& secondColor = Vector3::Zero);
void TriggerExplosionSmokeEnd(int x, int y, int z, int uw);
void TriggerExplosionSmoke(int x, int y, int z, int uw);
void TriggerFireFlame(int x, int y, int z, FlameType type, const Vector3& color1 = Vector3::Zero, const Vector3& color2 = Vector3::Zero);
void TriggerSuperJetFlame(ItemInfo* item, int yvel, int deadly);
void TriggerLaraBlood();
short DoBloodSplat(int x, int y, int z, short speed, short yRot, short roomNumber);
void DoLotsOfBlood(int x, int y, int z, int speed, short direction, short roomNumber, int count);
void ControlWaterfallMist(short itemNumber);
void TriggerWaterfallMist(const ItemInfo& item);
void KillAllCurrentItems(short itemNumber);
void TriggerRocketFlame(int x, int y, int z, int xv, int yv, int zv, int itemNumber);
void TriggerRocketSmoke(int x, int y, int z);
void TriggerFlashSmoke(int x, int y, int z, short roomNumber);
void TriggerMetalSparks(int x, int y, int z, int xv, int yv, int zv, const Vector3& color, int additional);
void SpawnCorpseEffect(const Vector3& pos);
void TriggerAttackFlame(const Vector3i& pos, const Vector3& color, int scale);
void TriggerRocketFire(int x, int y, int z);
void TriggerExplosionBubbles(int x, int y, int z, short roomNumber);
void Ricochet(Pose& pos);
void ProcessEffects(ItemInfo* item);
void UpdateWibble();

void SpawnPlayerWaterSurfaceEffects(const ItemInfo& item, int waterHeight, int waterDepth);
