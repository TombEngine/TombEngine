#include "framework.h"
#include "Game/Setup.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/control/flipeffect.h"
#include "Game/effects/effects.h"
#include "Game/effects/Hair.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/pickup/pickup.h"
#include "Game/room.h"
#include "Objects/Effects/effect_objects.h"
#include "Objects/Generic/generic_objects.h"
#include "Objects/Generic/Object/objects.h"
#include "Objects/Generic/Object/rope.h"
#include "Objects/Generic/Switches/fullblock_switch.h"
#include "Objects/Generic/Switches/switch.h"
#include "Objects/Generic/Traps/falling_block.h"
#include "Objects/TR1/tr1_objects.h"
#include "Objects/TR2/tr2_objects.h"
#include "Objects/TR3/Entity/FishSwarm.h"
#include "Objects/TR3/tr3_objects.h"
#include "Objects/TR4/tr4_objects.h"
#include "Objects/TR5/tr5_objects.h"
#include "Objects/TR4/Entity/tr4_beetle_swarm.h"
#include "Objects/Utils/object_helper.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Effects::Hair;
using namespace TEN::Entities;
using namespace TEN::Entities::Switches;

ObjectHandler Objects;
StaticHandler Statics;

void ObjectHandler::Initialize() 
{ 
	std::memset(_objects, 0, sizeof(ObjectInfo) * GAME_OBJECT_ID::ID_NUMBER_OBJECTS);
}

bool ObjectHandler::CheckID(GAME_OBJECT_ID objectID, bool isSilent)
{
	if (objectID == GAME_OBJECT_ID::ID_NO_OBJECT || objectID >= GAME_OBJECT_ID::ID_NUMBER_OBJECTS)
	{
		if (!isSilent)
		{
			TENLog(
				"Attempted to access unavailable slot ID (" + std::to_string(objectID) + "). " +
				"Check if last accessed item exists in level.", LogLevel::Warning, LogConfig::Debug);
		}

		return false;
	}

	return true;
}

ObjectInfo& ObjectHandler::operator [](int objectID) 
{
	if (CheckID((GAME_OBJECT_ID)objectID))
		return _objects[objectID];

	return GetFirstAvailableObject();
}

ObjectInfo& ObjectHandler::GetFirstAvailableObject()
{
	for (int i = 0; i < ID_NUMBER_OBJECTS; i++)
	{
		if (_objects[i].loaded)
			return _objects[i];
	}

	return _objects[0];
}

void StaticHandler::Initialize()
{
	_lut.resize(0);
	_lut.reserve(LUT_SIZE);
	_statics.resize(0);
}

int StaticHandler::GetIndex(int staticID)
{
	if (staticID < 0 || staticID >= _lut.size())
	{
		TENLog("Attempted to get index of missing static object " + std::to_string(staticID) + ".", LogLevel::Warning);
		return _lut.front();
	}

	return _lut[staticID];
}

StaticInfo& StaticHandler::operator [](int staticID)
{
	if (staticID < 0)
	{
		TENLog("Attempted to access missing static object " + std::to_string(staticID) + ".", LogLevel::Warning);
		return _statics.front();
	}

	if (staticID >= _lut.size())
		_lut.resize(staticID + 1, NO_VALUE);

	if (_lut[staticID] != NO_VALUE)
		return _statics[_lut[staticID]];

	_statics.emplace_back();
	_lut[staticID] = (int)_statics.size() - 1;

	return _statics.back();
}

// NOTE: JointRotationFlags allows bones to be rotated with CreatureJoint().
void ObjectInfo::SetBoneRotationFlags(int boneID, int flags)
{
	int index = boneIndex + (boneID * 4);
	if (index < 0 || index >= g_Level.Bones.size())
	{
		TENLog("Failed to set rotation flag for bone ID " + std::to_string(boneID), LogLevel::Warning);
		return;
	}

	g_Level.Bones[index] |= flags;
}

void ObjectInfo::SetHitEffect(HitEffect hitEffect)
{
	this->hitEffect = hitEffect;
}

// NOTE: Use if object is alive, but not intelligent, to set up blood effects.
void ObjectInfo::SetHitEffect(bool isSolid, bool isAlive)
{
	// Avoid some objects such as ID_SAS_DYING having None.
	if (isAlive)
	{
		hitEffect = HitEffect::Blood;
		return;
	}

	if (intelligent)
	{
		if (isSolid && HitPoints > 0)
		{
			hitEffect = HitEffect::Richochet;
		}
		else if ((damageType != DamageMode::Any && HitPoints > 0) || HitPoints == NOT_TARGETABLE)
		{
			hitEffect = HitEffect::Smoke;
		}
		else if (damageType == DamageMode::Any && HitPoints > 0)
		{
			hitEffect = HitEffect::Blood;
		}
	}
	else if (isSolid && HitPoints <= 0)
	{
		hitEffect = HitEffect::Richochet;
	}
	else
	{
		hitEffect = HitEffect::None;
	}
}

void InitializeGameFlags()
{
	ZeroMemory(FlipMap, MAX_FLIPMAP * sizeof(int));
	ZeroMemory(FlipStats, MAX_FLIPMAP * sizeof(bool));

	FlipEffect = NO_VALUE;
	FlipStatus = false;
	NumRPickups = 0;
	Camera.underwater = false;
}

void InitializeSpecialEffects()
{
	memset(&FireSparks, 0, MAX_SPARKS_FIRE * sizeof(FIRE_SPARKS));
	memset(&SmokeSparks, 0, MAX_SPARKS_SMOKE * sizeof(SMOKE_SPARKS));
	memset(&Gunshells, 0, MAX_GUNSHELL * sizeof(GUNSHELL_STRUCT));
	memset(&Blood, 0, MAX_SPARKS_BLOOD * sizeof(BLOOD_STRUCT));
	memset(&ShockWaves, 0, MAX_SHOCKWAVE * sizeof(SHOCKWAVE_STRUCT));
	memset(&Particles, 0, MAX_PARTICLES * sizeof(Particle));

	for (int i = 0; i < MAX_PARTICLES; i++)
	{
		Particles[i].on = false;
		Particles[i].dynamic = -1;
	}

	NextFireSpark = 1;
	NextSmokeSpark = 0;
	NextGunShell = 0;
	NextBlood = 0;

	TEN::Entities::TR4::ClearBeetleSwarm();
	TEN::Entities::Creatures::TR3::ClearFishSwarm();
}

void CustomObjects()
{
	
}

void InitializeObjects()
{
	TENLog("Initializing objects...", LogLevel::Info);

	AllocTR4Objects();
	AllocTR5Objects();

	ObjectInfo* obj;

	for (int i = 0; i < ID_NUMBER_OBJECTS; i++)
	{
		obj = &Objects[i];
		obj->Initialize = nullptr;
		obj->collision = nullptr;
		obj->control = nullptr;
		obj->drawRoutine = DrawAnimatingItem;
		obj->HitRoutine = DefaultItemHit;
		obj->pivotLength = 0;
		obj->radius = DEFAULT_RADIUS;
		obj->shadowType = ShadowMode::None;
		obj->HitPoints = NOT_TARGETABLE;
		obj->hitEffect = HitEffect::None;
		obj->explodableMeshbits = 0;
		obj->intelligent = false;
		obj->AlwaysActive = false;
		obj->waterCreature = false;
		obj->nonLot = false;
		obj->usingDrawAnimatingItem = true;
		obj->damageType = DamageMode::Any;
		obj->LotType = LotType::Basic;
		obj->meshSwapSlot = NO_VALUE;
		obj->isPickup = false;
		obj->isPuzzleHole = false;
	}

	InitializeEffectsObjects();
	InitializeGenericObjects(); // Generic objects
	InitializeTR5Objects(); // Standard TR5 objects (NOTE: lara need to be loaded first, so entity like doppelganger can use this animIndex !)
	InitializeTR1Objects(); // Standard TR1 objects
	InitializeTR2Objects(); // Standard TR2 objects
	InitializeTR3Objects(); // Standard TR3 objects
	InitializeTR4Objects(); // Standard TR4 objects

	// User defined objects
	CustomObjects();

	CurrentSequence = 0;
	SequenceResults[0][1][2] = 0;
	SequenceResults[0][2][1] = 1;
	SequenceResults[1][0][2] = 2;
	SequenceResults[1][2][0] = 3;
	SequenceResults[2][0][1] = 4;
	SequenceResults[2][1][0] = 5;
	SequenceUsed[0] = 0;
	SequenceUsed[1] = 0;
	SequenceUsed[2] = 0;
	SequenceUsed[3] = 0;
	SequenceUsed[4] = 0;
	SequenceUsed[5] = 0;
}
