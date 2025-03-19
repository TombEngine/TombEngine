#pragma once

#include "Game/control/box.h"
#include "Objects/objectslist.h"
#include "Renderer/RendererEnums.h"
#include "Specific/level.h"

namespace TEN::Animation { struct AnimData; }
class Vector3i;
struct CollisionInfo;
struct ItemInfo;

constexpr auto DEFAULT_RADIUS = 10;

enum JointRotationFlags
{
	ROT_X = 1 << 2,
	ROT_Y = 1 << 3,
	ROT_Z = 1 << 4
};

// Unused.
enum ShatterFlags
{
	NoCollision = 1 << 0,
	Shatterable = 1 << 1
};

// Custom LOT definition for Creature. Used in InitializeSlot() in lot.cpp.
enum class LotType
{
	Skeleton,
	Basic,
	Water,
	WaterAndLand,
	Human,
	HumanPlusJump,
	HumanPlusJumpAndMonkey,
	Flyer,
	Blockable,	  // For large creatures such as trex and shiva.
	Spider,		  // Only 2 block vault allowed.
	Ape,		  // Only 2 block vault allowed.
	SnowmobileGun // Only 1 block vault allowed and 4 block drop max.
};

enum class HitEffect
{
    None,
    Blood,
    Smoke,
    Richochet,
	NonExplosive,
	Special
};

enum class DamageMode
{
	None,
	Any,
	Explosion
};

enum class ShatterType
{
	None,
	Fragment,
	Explode
};

// TODO: All fields to PascalCase.
struct ObjectInfo
{
	bool loaded = false; // IsLoaded

	int nmeshes; // BoneCount
	int meshIndex; // Base index in g_Level.Meshes.
	int boneIndex; // Base index in g_Level.Bones.

	LotType LotType;
	HitEffect hitEffect;
	DamageMode damageType;
	ShadowMode shadowType;

	int meshSwapSlot;
	int pivotLength;
	int radius;

	int	 HitPoints				= 0;
	bool AlwaysActive			= false;
	bool intelligent			= false;
	bool waterCreature			= false;
	bool nonLot					= false;
	bool isPickup				= false;
	bool isPuzzleHole			= false;
	bool usingDrawAnimatingItem = false;

	DWORD explodableMeshbits;

	std::vector<AnimData> Animations = {};

	std::function<void(short itemNumber)> Initialize = nullptr;
	std::function<void(short itemNumber)> control = nullptr;
	std::function<void(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)> collision = nullptr;

	std::function<void(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex)> HitRoutine = nullptr;
	std::function<void(ItemInfo* item)> drawRoutine = nullptr;

	void SetBoneRotationFlags(int boneID, int flags);
	void SetHitEffect(HitEffect hitEffect);
	void SetHitEffect(bool isSolid = false, bool isAlive = false);
};

class ObjectHandler
{
private:
	ObjectInfo _objects[ID_NUMBER_OBJECTS];
	ObjectInfo& GetFirstAvailableObject();

public:
	void Initialize();
	bool CheckID(GAME_OBJECT_ID objectID, bool isSilent = false);

	ObjectInfo& operator [](int objectID);
};

struct StaticInfo
{
	int meshNumber;
	int flags;
	GameBoundingBox visibilityBox;
	GameBoundingBox collisionBox;
	ShatterType shatterType;
	int shatterSound;
	int ObjectNumber;
};

class StaticHandler
{
private:
	static constexpr auto LUT_SIZE = 256;

	std::vector<StaticInfo> _statics = {};
	std::vector<int>		_lut	 = {};

public:
	void Initialize();
	int  GetIndex(int staticID);

	StaticInfo& operator [](int staticID);

	// Iterators

	auto begin() { return _statics.begin(); }			// Non-const begin
	auto end() { return _statics.end(); }				// Non-const end
	auto begin() const { return _statics.cbegin(); }	// Const begin
	auto end() const { return _statics.cend(); }		// Const end
	auto cbegin() const { return _statics.cbegin(); }	// Explicit const begin
	auto cend() const { return _statics.cend(); }		// Explicit const end
};

extern ObjectHandler Objects;
extern StaticHandler Statics;

void InitializeGameFlags();
void InitializeSpecialEffects();
void InitializeObjects();
