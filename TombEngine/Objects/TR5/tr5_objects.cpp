#include "framework.h"
#include "Objects/TR5/tr5_objects.h"

#include "Game/collision/collide_item.h"
#include "Game/control/box.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara_initialise.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/pickup/pickup.h"
#include "Game/Setup.h"
#include "Objects/Generic/Object/objects.h"
#include "Objects/Generic/Switches/switch.h"
#include "Objects/Utils/object_helper.h"
#include "Specific/level.h"

// Creatures
#include "Objects/TR5/Entity/AutoGun.h"			 // OK
#include "Objects/TR5/Entity/HeavyGuard.h"		 // OK
#include "Objects/TR5/Entity/tr5_brownbeast.h"	 // OK
#include "Objects/TR5/Entity/tr5_chef.h"		 // OK
#include "Objects/TR5/Entity/tr5_cyborg.h"		 // OK
#include "Objects/TR5/Entity/tr5_doberman.h"	 // OK
#include "Objects/TR5/Entity/tr5_dog.h"			 // OK
#include "Objects/TR5/Entity/tr5_ghost.h"		 // OK
#include "Objects/TR5/Entity/tr5_gladiator.h"	 // OK
#include "Objects/TR5/Entity/tr5_guard.h"		 // OK
#include "Objects/TR5/Entity/tr5_gunship.h"		 // OK
#include "Objects/TR5/Entity/tr5_hydra.h"		 // OK
#include "Objects/TR5/Entity/tr5_imp.h"			 // OK
#include "Objects/TR5/Entity/tr5_lagoon_witch.h" // OK
#include "Objects/TR5/Entity/tr5_larson.h"		 // OK
#include "Objects/TR5/Entity/tr5_laser_head.h"	 // OK
#include "Objects/TR5/Entity/tr5_lion.h"		 // OK
#include "Objects/TR5/Entity/tr5_reaper.h"		 // OK
#include "Objects/TR5/Entity/tr5_roman_statue.h" // OK
#include "Objects/TR5/Entity/tr5_submarine.h"	 // OK
#include "Objects/TR5/Entity/tr5_willowwisp.h"	 // OK

// Emitters
#include "Objects/TR5/Emitter/tr5_rats_emitter.h"
#include "Objects/TR5/Emitter/tr5_bats_emitter.h"
#include "Objects/TR5/Emitter/tr5_spider_emitter.h"
#include "Objects/TR5/Emitter/tr5_smoke_emitter.h"
#include "Objects/TR5/Emitter/Waterfall.h"
#include "Objects/Effects/Fireflies.h"

// Objects
#include "Objects/TR5/Light/tr5_light.h"
#include "Objects/TR5/Object/tr5_bodypart.h"
#include "Objects/TR5/Object/tr5_expandingplatform.h"
#include "Objects/TR5/Object/tr5_genslot.h"
#include "Objects/TR5/Object/tr5_highobject.h"
#include "Objects/TR5/Object/tr5_missile.h"
#include "Objects/TR5/Object/tr5_raisingblock.h"
#include "Objects/TR5/Switch/tr5_raisingcog.h"
#include "Objects/TR5/Object/tr5_teleporter.h"
#include "Objects/TR5/Object/tr5_twoblockplatform.h"

// Traps
#include "Objects/Effects/EmberEmitter.h"
#include "Objects/TR5/Trap/LaserBarrier.h"
#include "Objects/TR5/Trap/LaserBeam.h"
#include "Objects/TR5/Trap/MovingLaser.h"
#include "Objects/TR5/Trap/ZipLine.h"
#include "Objects/Effects/tr5_electricity.h"
#include "Objects/TR5/Object/tr5_rollingball.h"
#include "Objects/TR5/Trap/tr5_ventilator.h"
#include "Objects/TR5/Trap/tr5_romehammer.h"
#include "Objects/TR5/Trap/tr5_fallingceiling.h"
#include "Objects/TR5/Trap/tr5_explosion.h"
#include "Objects/TR5/Trap/tr5_wreckingball.h"

// Switches
#include "Objects/TR5/Switch/tr5_crowdove_switch.h"

// Shatters
#include "Objects/TR5/Shatter/tr5_smashobject.h"

using namespace TEN::Effects::EmberEmitter;
using namespace TEN::Effects::SmokeEmitter;
using namespace TEN::Effects::WaterfallEmitter;
using namespace TEN::Entities::Creatures::TR5;
using namespace TEN::Entities::Switches;
using namespace TEN::Entities::Traps;
using namespace TEN::Effects::Fireflies;

static void StartEntity(ObjectInfo *obj)
{
	obj = &Objects[ID_LARA];
	if (obj->loaded)
	{
		obj->Initialize = InitializeLaraLoad;
		obj->shadowType = ShadowMode::Player;
		obj->HitPoints = 1000;
		obj->usingDrawAnimatingItem = false;
	}

	// TODO: Will be moved to TR4 in ObjectCleaning branch.
	obj = &Objects[ID_SAS];
	if (obj->loaded)
	{
		obj->Initialize = InitializeGuard;
		obj->control = GuardControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 40;
		obj->radius = 102;
		obj->pivotLength = 50;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_SWAT];
	if (obj->loaded)
	{
		obj->Initialize = InitializeGuard;
		obj->collision = CreatureCollision;
		obj->control = GuardControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 24;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->intelligent = true;
		obj->HitRoutine = GuardHit;
		obj->LotType = LotType::HumanPlusJump;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_GUARD1];
	if (obj->loaded)
	{
		AssignObjectAnimations(*obj, ID_SWAT, "ID_GUARD1", "ID_SWAT");
		obj->Initialize = InitializeGuard;
		obj->collision = CreatureCollision;
		obj->control = GuardControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 24;
		obj->radius = 102;
		obj->pivotLength = 50;
		obj->HitRoutine = GuardHit;
		obj->intelligent = true;
		obj->LotType = LotType::HumanPlusJump;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_SWAT_PLUS];
	if (obj->loaded)
	{
		if (!AssignObjectAnimations(*obj, ID_SWAT, "ID_SWAT_PLUS", "ID_SWAT"))
			AssignObjectAnimations(*obj, ID_GUARD1, "ID_SWAT_PLUS", "ID_GUARD1");

		obj->Initialize = InitializeGuard;
		obj->collision = CreatureCollision;
		obj->control = GuardControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 24;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->intelligent = true;
		obj->LotType = LotType::HumanPlusJump;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_MAFIA];
	if (obj->loaded)
	{
		if (!AssignObjectAnimations(*obj, ID_SWAT, "ID_MAFIA", "ID_SWAT"))
			AssignObjectAnimations(*obj, ID_GUARD1, "ID_MAFIA", "ID_GUARD1");

		obj->Initialize = InitializeGuard;
		obj->collision = CreatureCollision;
		obj->control = GuardControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 24;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->HitRoutine = GuardHit;
		obj->intelligent = true;
		obj->LotType = LotType::HumanPlusJump;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_SCIENTIST];
	if (obj->loaded)
	{
		if (!AssignObjectAnimations(*obj, ID_SWAT, "ID_SCIENTIST", "ID_SWAT"))
			AssignObjectAnimations(*obj, ID_GUARD1, "ID_SCIENTIST", "ID_GUARD1");

		obj->Initialize = InitializeGuard;
		obj->control = GuardControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 24;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->intelligent = true;
		obj->HitRoutine = GuardHit;
		obj->LotType = LotType::HumanPlusJump;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_GUARD2];
	if (obj->loaded)
	{
		if (!AssignObjectAnimations(*obj, ID_SWAT, "ID_GUARD2", "ID_SWAT"))
			AssignObjectAnimations(*obj, ID_GUARD1, "ID_GUARD2", "ID_GUARD1");

		obj->Initialize = InitializeGuard;
		obj->control = GuardControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 24;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->HitRoutine = GuardHit;
		obj->intelligent = true;
		obj->LotType = LotType::Human;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_GUARD3];
	if (obj->loaded)
	{
		if (!AssignObjectAnimations(*obj, ID_SWAT, "ID_GUARD3", "ID_SWAT"))
			AssignObjectAnimations(*obj, ID_GUARD1, "ID_GUARD3", "ID_GUARD1");

		obj->Initialize = InitializeGuard;
		obj->control = GuardControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 24;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->HitRoutine = GuardHit;
		obj->intelligent = true;
		obj->LotType = LotType::Human;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_ATTACK_SUB];
	if (obj->loaded)
	{
		obj->Initialize = InitializeSubmarine;
		obj->collision = CreatureCollision;
		obj->control = SubmarineControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 100;
		obj->pivotLength = 200;
		obj->radius = 512;
		obj->intelligent = true;
		obj->waterCreature = true;
		obj->damageType = DamageMode::None;
		obj->LotType = LotType::Water;
		obj->SetBoneRotationFlags(0, ROT_X);
		obj->SetBoneRotationFlags(1, ROT_X);
	}

	obj = &Objects[ID_CHEF];
	if (obj->loaded)
	{
		obj->Initialize = InitializeChef;
		obj->control = ControlChef;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 35;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->intelligent = true;
		obj->LotType = LotType::Human;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_LION];
	if (obj->loaded)
	{
		obj->Initialize = InitializeLion;
		obj->collision = CreatureCollision;
		obj->control = LionControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 40;
		obj->pivotLength = 50;
		obj->radius = 341;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(6, ROT_Y);
		obj->SetBoneRotationFlags(19, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_DOBERMAN];
	if (obj->loaded)
	{
		obj->Initialize = InitializeDoberman;
		obj->collision = CreatureCollision;
		obj->control = DobermanControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 18;
		obj->pivotLength = 50;
		obj->radius = 256;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(19, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_HUSKIE];
	if (obj->loaded)
	{
		obj->Initialize = InitializeTr5Dog;
		obj->collision = CreatureCollision;
		obj->control = Tr5DogControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 24;
		obj->pivotLength = 50;
		obj->radius = 256;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(19, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_REAPER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeReaper;
		obj->collision = CreatureCollision;
		obj->control = ReaperControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 10;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->intelligent = true;
		obj->waterCreature = true;
		obj->LotType = LotType::Water;
		obj->SetHitEffect();
	}

	obj = &Objects[ID_MAFIA2];
	if (obj->loaded)
	{
		obj->Initialize = InitializeMafia2;
		obj->collision = CreatureCollision;
		obj->control = Mafia2Control;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 26;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->HitRoutine = GuardHit;
		obj->intelligent = true;
		obj->LotType = LotType::Human;
		obj->meshSwapSlot = ID_MESHSWAP_MAFIA2;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_PIERRE];
	if (obj->loaded)
	{
		obj->Initialize = InitializeLarson;
		obj->collision = CreatureCollision;
		obj->control = LarsonControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 60;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->intelligent = true;
		obj->LotType = LotType::Basic;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(7, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_LARSON];
	if (obj->loaded)
	{
		obj->Initialize = InitializeLarson;
		obj->collision = CreatureCollision;
		obj->control = LarsonControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 60;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->intelligent = true;
		obj->LotType = LotType::Basic;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(7, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_CYBORG];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCyborg;
		obj->collision = CreatureCollision;
		obj->control = CyborgControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 50;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->intelligent = true;
		obj->damageType = DamageMode::None;
		obj->LotType = LotType::Human;
		obj->meshSwapSlot = ID_MESHSWAP_HITMAN;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_SNIPER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeSniper;
		obj->collision = CreatureCollision;
		obj->control = SniperControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 35;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->HitRoutine = GuardHit;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_GUARD_LASER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeHeavyGuard;
		obj->collision = CreatureCollision;
		obj->control = HeavyGuardControl;
		obj->HitRoutine = HeavyGuardHit;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 24;
		obj->pivotLength = 50;
		obj->radius = 128;
		obj->intelligent = true;
		obj->damageType = DamageMode::None;
		obj->SetBoneRotationFlags(0, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(1, ROT_X | ROT_Y);
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_HYDRA];
	if (obj->loaded)
	{
		obj->Initialize = InitializeHydra;
		obj->collision = CreatureCollision;
		obj->control = HydraControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 30;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->intelligent = true;
		obj->damageType = DamageMode::None;
		obj->SetBoneRotationFlags(0, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(8, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_IMP];
	if (obj->loaded)
	{
		obj->Initialize = InitializeImp;
		obj->collision = CreatureCollision;
		obj->control = ImpControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 12;
		obj->pivotLength = 20;
		obj->radius = 102;
		obj->intelligent = true;
		obj->meshSwapSlot = ID_MESHSWAP_IMP;
		obj->SetBoneRotationFlags(4, ROT_X | ROT_Z);
		obj->SetBoneRotationFlags(9, ROT_X | ROT_Z);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_WILLOWISP];
	if (obj->loaded)
	{
		obj->Initialize = InitializeLightingGuide;
		//obj->control = ControlLightingGuide;
		obj->drawRoutine = nullptr;
		obj->shadowType = ShadowMode::All;
		obj->radius = 256;
		obj->HitPoints = NOT_TARGETABLE;
		obj->pivotLength = 20;
		obj->intelligent = true;
		obj->LotType = LotType::Flyer;
		obj->SetBoneRotationFlags(4, ROT_X | ROT_Z);
		obj->SetBoneRotationFlags(9, ROT_X | ROT_Z);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_BROWN_BEAST];
	if (obj->loaded)
	{
		obj->Initialize = InitializeBrownBeast;
		obj->collision = CreatureCollision;
		obj->control = ControlBrowsBeast;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 100;
		obj->pivotLength = 20;
		obj->radius = 341;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(4, ROT_X | ROT_Z);
		obj->SetBoneRotationFlags(9, ROT_X | ROT_Z);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_LAGOON_WITCH];
	if (obj->loaded)
	{
		obj->Initialize = InitializeLagoonWitch;
		obj->collision = CreatureCollision;
		obj->control = LagoonWitchControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 100;
		obj->pivotLength = 20;
		obj->radius = 256;
		obj->intelligent = true;
		obj->waterCreature = true;
		obj->LotType = LotType::Water;
		obj->SetBoneRotationFlags(4, ROT_X | ROT_Z);
		obj->SetBoneRotationFlags(9, ROT_X | ROT_Z);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_INVISIBLE_GHOST];
	if (obj->loaded)
	{
		obj->Initialize = InitializeInvisibleGhost;
		obj->collision = CreatureCollision;
		obj->control = InvisibleGhostControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = NOT_TARGETABLE;
		obj->pivotLength = 20;
		obj->radius = 256;
		obj->intelligent = true;
		obj->LotType = LotType::Human;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(8, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_RATS_EMITTER];
	if (obj->loaded)
	{
		obj->drawRoutine = nullptr;
		obj->Initialize = InitializeLittleRats;
		obj->control = LittleRatsControl;
		obj->usingDrawAnimatingItem = false;
	}

	obj = &Objects[ID_BATS_EMITTER];
	if (obj->loaded)
	{
		obj->drawRoutine = nullptr;
		obj->Initialize = InitializeLittleBats;
		obj->control = LittleBatsControl;
		obj->usingDrawAnimatingItem = false;
	}

	obj = &Objects[ID_SPIDERS_EMITTER];
	if (obj->loaded)
	{
		obj->drawRoutine = nullptr;
		obj->Initialize = InitializeSpiders;
		obj->control = SpidersEmitterControl;
		obj->usingDrawAnimatingItem = false;
	}

	obj = &Objects[ID_GLADIATOR];
	if (obj->loaded)
	{
		obj->Initialize = InitializeGladiator;
		obj->control = ControlGladiator;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 20;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	for (int i = 0; i < 2; i++)
	{
		obj = &Objects[ID_ROMAN_GOD1 + i];
		if (obj->loaded)
		{
			obj->Initialize = InitializeRomanStatue;
			obj->collision = CreatureCollision;
			obj->control = RomanStatueControl;
			obj->HitRoutine = RomanStatueHit;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 300;
			obj->pivotLength = 50;
			obj->radius = 256;
			obj->intelligent = true;
			obj->meshSwapSlot = ID_MESHSWAP_ROMAN_GOD1 + i;
			obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
			obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
			obj->SetHitEffect(true);
		}
	}

	obj = &Objects[ID_LASERHEAD];
	if (obj->loaded)
	{
		obj->Initialize = InitializeGuardian;
		obj->collision = CreatureCollision;
		obj->control = ControlGuardian;
		obj->explodableMeshbits = 6;
		obj->usingDrawAnimatingItem = false;
		obj->damageType = DamageMode::None;
		obj->nonLot = true;
		obj->SetHitEffect(true);
	}

	InitAnimating(obj, ID_LASERHEAD_BASE);
	InitAnimating(obj, ID_LASERHEAD_TENTACLE);

	obj = &Objects[ID_AUTO_GUN_VCI];
	if (obj->loaded)
	{
		obj->Initialize = InitializeAutoGun;
		obj->control = ControlAutoGun;
		obj->intelligent = true;
		obj->damageType = DamageMode::None;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(8, ROT_Y);
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_GUNSHIP];
	if (obj->loaded)
	{
		obj->control = ControlGunShip;
		obj->SetBoneRotationFlags(0, ROT_Y);
		obj->SetBoneRotationFlags(1, ROT_X);
		obj->SetHitEffect(true);
	}
}

static void StartObject(ObjectInfo *obj)
{
	InitFlare(obj, ID_FLARE_ITEM);

	for (int objectNumber = ID_SEARCH_OBJECT1; objectNumber <= ID_SEARCH_OBJECT4; objectNumber++)
		InitSearchObject(obj, objectNumber);

	obj = &Objects[ID_TWOBLOCK_PLATFORM];
	if (obj->loaded)
	{
		obj->Initialize = InitializeTwoBlockPlatform;
		obj->control = TwoBlockPlatformControl;
		obj->SetHitEffect(true);
	}

	for (int objectNumber = ID_RAISING_BLOCK1; objectNumber <= ID_RAISING_BLOCK4; objectNumber++)
	{
		obj = &Objects[objectNumber];
		if (obj->loaded)
		{
			obj->Initialize = InitializeRaisingBlock;
			obj->control = ControlRaisingBlock;
			obj->SetHitEffect(true);
		}
	}

	obj = &Objects[ID_EXPANDING_PLATFORM];
	if (obj->loaded)
	{
		obj->Initialize = InitializeExpandingPlatform;
		obj->control = ControlExpandingPlatform;
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_ELECTRICAL_LIGHT];
	if (obj->loaded)
	{
		obj->control = ElectricalLightControl;
		obj->Initialize = InitializeElectricalLight;
		obj->meshSwapSlot = ID_ELECTRICAL_LIGHT;
	}

	obj = &Objects[ID_PULSE_LIGHT];
	if (obj->loaded)
	{
		obj->control = PulseLightControl;
		obj->drawRoutine = nullptr;
		obj->usingDrawAnimatingItem = false;
	}

	obj = &Objects[ID_STROBE_LIGHT];
	if (obj->loaded)
		obj->control = StrobeLightControl;

	obj = &Objects[ID_COLOR_LIGHT];
	if (obj->loaded)
	{
		obj->control = ColorLightControl;
		obj->drawRoutine = nullptr;
		obj->usingDrawAnimatingItem = false;
	}

	obj = &Objects[ID_BLINKING_LIGHT];
	if (obj->loaded)
	{
		obj->control = BlinkingLightControl;
		obj->drawRoutine = nullptr;
		
	}

	obj = &Objects[ID_BODY_PART];
	obj->loaded = true;
	obj->control = ControlBodyPart;

	obj = &Objects[ID_SMOKE_EMITTER_BLACK];
	if (obj->loaded)
	{
		obj->Initialize = InitializeSmokeEmitter;
		obj->control = ControlSmokeEmitter;
		obj->drawRoutine = nullptr;
		obj->usingDrawAnimatingItem = false;
	}

	obj = &Objects[ID_SMOKE_EMITTER_WHITE];
	if (obj->loaded)
	{
		obj->Initialize = InitializeSmokeEmitter;
		obj->control = ControlSmokeEmitter;
		obj->drawRoutine = nullptr;
		obj->usingDrawAnimatingItem = false;
	}

	obj = &Objects[ID_SMOKE_EMITTER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeSmokeEmitter;
		obj->control = ControlSmokeEmitter;
		obj->drawRoutine = nullptr;
		obj->usingDrawAnimatingItem = false;
	}

	obj = &Objects[ID_WATERFALL_EMITTER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeWaterfall;
		obj->control = ControlWaterfall;
		obj->drawRoutine = nullptr;
		obj->usingDrawAnimatingItem = false;
	}

	obj = &Objects[ID_TELEPORTER];
	if (obj->loaded)
	{
		obj->control = ControlTeleporter;
		obj->drawRoutine = nullptr;
	}

	obj = &Objects[ID_LARA_START_POS];
	if (obj->loaded)
		obj->drawRoutine = nullptr;

	obj = &Objects[ID_HIGH_OBJECT1];
	if (obj->loaded)
	{
		obj->Initialize = InitializeHighObject1;
		obj->control = ControlHighObject1;
		obj->collision = ObjectCollision;
	}

	obj = &Objects[ID_EMBER_EMITTER];
	if (obj->loaded)
	{
		obj->drawRoutine = nullptr;
		obj->control = ControlEmberEmitter;
	}

	obj = &Objects[ID_FIREFLY_EMITTER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeFireflySwarm;
		obj->control = ControlFireflySwarm;
		obj->drawRoutine = NULL;
	}

	obj = &Objects[ID_GEN_SLOT1];
	if (obj->loaded)
	{
		obj->control = GenSlot1Control;
	}

	obj = &Objects[ID_GEN_SLOT2];
	if (obj->loaded)
	{
		/*obj->Initialize = InitializeGenSlot2;
		obj->control = GenSlot2Control;
		obj->drawRoutine = DrawGenSlot2;*/
		obj->usingDrawAnimatingItem = false;
	}

	for (int objectNumber = ID_AI_GUARD; objectNumber <= ID_AI_X2; objectNumber++)
	{
		obj = &Objects[objectNumber];
		if (obj->loaded)
		{
			obj->drawRoutine = nullptr;
			obj->collision = AIPickupCollision;
		}
	}

	obj = &Objects[ID_PORTAL];
	if (obj->loaded)
	{
		//obj->Initialize = InitializePortal;
		//obj->control = PortalControl;        // TODO: found the control procedure !
		obj->drawRoutine = nullptr;             // go to nullsub_44() !

		obj->usingDrawAnimatingItem = false;
	}

	obj = &Objects[ID_WATERFALLSS1];
	if (obj->loaded)
	{
		obj->control = nullptr;
	}

	obj = &Objects[ID_WATERFALLSS2];
	if (obj->loaded)
	{
		obj->control = nullptr;
	}
}

static void StartTrap(ObjectInfo *obj)
{
	obj = &Objects[ID_ZIPLINE_HANDLE];
	if (obj->loaded)
	{
		obj->Initialize = InitializeZipLine;
		obj->collision = CollideZipLine;
		obj->control = ControlZipLine;
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_PROPELLER_H];
	if (obj->loaded)
	{
		obj->Initialize = InitializeVentilator;
		obj->control = ControlVentilator;
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_PROPELLER_V];
	if (obj->loaded)
	{
		obj->Initialize = InitializeVentilator;
		obj->control = ControlVentilator;
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_ELECTRICAL_CABLES];
	if (obj->loaded)
	{
		obj->control = ElectricityWiresControl;
	}

	obj = &Objects[ID_ROME_HAMMER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeRomeHammer;
		obj->collision = GenericSphereBoxCollision;
		obj->control = ControlRomeHammer;
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_FALLING_CEILING];
	if (obj->loaded)
	{
		obj->collision = TrapCollision;
		obj->control = ControlFallingCeiling;
	}

	obj = &Objects[ID_ROLLINGBALL];
	if (obj->loaded)
	{
		obj->collision = RollingBallCollision;
		obj->control = RollingBallControl;
		obj->shadowType = ShadowMode::All;
		obj->SetHitEffect(true);
	}
	
	obj = &Objects[ID_CLASSIC_ROLLING_BALL];
	if (obj->loaded)
	{
		obj->Initialize = InitializeClassicRollingBall;
		obj->control = ClassicRollingBallControl;
		obj->collision = ClassicRollingBallCollision;
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_BIG_ROLLING_BALL];
	if (obj->loaded)
	{
		obj->collision = ClassicRollingBallCollision;
		obj->control = ClassicRollingBallControl;
		obj->Initialize = InitializeClassicRollingBall;
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_GEN_SLOT3];
	if (obj->loaded)
	{
		obj->Initialize = InitializeGenSlot3;
		obj->collision = HybridCollision;
		obj->control = AnimatingControl;
	}

	// TODO: Seem not decompiled. -- TokyoSU, 2023.01.12
	obj = &Objects[ID_GEN_SLOT4];
	if (obj->loaded)
	{
		//obj->Initialize = InitializeGenSlot4;
		//obj->control = GenSlot4Control;
	}

	obj = &Objects[ID_EXPLOSION];
	if (obj->loaded)
	{
		obj->Initialize = InitializeExplosion;
		obj->control = ControlExplosion;
		obj->drawRoutine = nullptr;
		obj->usingDrawAnimatingItem = false;
	}

	obj = &Objects[ID_LASER_BARRIER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeLaserBarrier;
		obj->control = ControlLaserBarrier;
		obj->collision = CollideLaserBarrier;
		obj->drawRoutine = nullptr;
		obj->usingDrawAnimatingItem = false;
	}

	obj = &Objects[ID_LASER_BEAM];
	if (obj->loaded)
	{
		obj->Initialize = InitializeLaserBeam;
		obj->control = ControlLaserBeam;
		obj->collision = CollideLaserBeam;
		obj->drawRoutine = nullptr;
		obj->usingDrawAnimatingItem = false;
	}

	obj = &Objects[ID_MOVING_LASER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeMovingLaser;
		obj->control = ControlMovingLaser;
		obj->collision = CollideMovingLaser;
		obj->SetHitEffect(true);
	}
}

static void StartSwitch(ObjectInfo *obj)
{
	obj = &Objects[ID_RAISING_COG];
	if (obj->loaded)
	{
		obj->Initialize = InitializeRaisingCog;
		obj->control = RaisingCogControl;
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_CROWDOVE_SWITCH];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCrowDoveSwitch;
		obj->collision = CrowDoveSwitchCollision;
		obj->control = CrowDoveSwitchControl;
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_WRECKING_BALL];
	if (obj->loaded)
	{
		obj->Initialize = InitializeWreckingBall;
		obj->collision = CollideWreckingBall;
		obj->control = ControlWreckingBall;
		obj->SetHitEffect(true);
	}
}

static void StartShatter(ObjectInfo *obj)
{
	for (int i = ID_SMASH_OBJECT1; i <= ID_SMASH_OBJECT16; i++)
		InitSmashObject(obj, i);
}

static void StartProjectiles(ObjectInfo *obj)
{
	InitProjectile(obj, MissileControl, ID_BUBBLES, true);
	InitProjectile(obj, MissileControl, ID_IMP_ROCK, true);
	InitProjectile(obj, TorpedoControl, ID_TORPEDO);
	InitProjectile(obj, GrenadeControl, ID_GRENADE);
	InitProjectile(obj, RocketControl, ID_ROCKET);
	InitProjectile(obj, HarpoonBoltControl, ID_HARPOON);
	InitProjectile(obj, CrossbowBoltControl, ID_CROSSBOW_BOLT);
}

static void StartPickup(ObjectInfo *obj)
{
	for (int objNumber = ID_PUZZLE_ITEM1; objNumber <= ID_EXAMINE8_COMBO2; objNumber++)
	{
		InitPickup(obj, objNumber);
	}

	InitPickup(obj, ID_HAMMER_ITEM);
	InitPickup(obj, ID_CROWBAR_ITEM);
	InitPickup(obj, ID_PISTOLS_ITEM);
	InitPickup(obj, ID_PISTOLS_AMMO_ITEM);
	InitPickup(obj, ID_UZI_ITEM);
	InitPickup(obj, ID_UZI_AMMO_ITEM);
	InitPickup(obj, ID_SHOTGUN_ITEM);
	InitPickup(obj, ID_SHOTGUN_AMMO1_ITEM);
	InitPickup(obj, ID_SHOTGUN_AMMO2_ITEM);
	InitPickup(obj, ID_CROSSBOW_ITEM);
	InitPickup(obj, ID_CROSSBOW_AMMO1_ITEM);
	InitPickup(obj, ID_CROSSBOW_AMMO2_ITEM);
	InitPickup(obj, ID_CROSSBOW_AMMO3_ITEM);
	InitPickup(obj, ID_GRENADE_GUN_ITEM);
	InitPickup(obj, ID_GRENADE_AMMO1_ITEM);
	InitPickup(obj, ID_GRENADE_AMMO2_ITEM);
	InitPickup(obj, ID_GRENADE_AMMO3_ITEM);
	InitPickup(obj, ID_HARPOON_ITEM);
	InitPickup(obj, ID_HARPOON_AMMO_ITEM);
	InitPickup(obj, ID_ROCKET_LAUNCHER_ITEM);
	InitPickup(obj, ID_ROCKET_LAUNCHER_AMMO_ITEM);
	InitPickup(obj, ID_HK_ITEM);
	InitPickup(obj, ID_HK_AMMO_ITEM);
	InitPickup(obj, ID_REVOLVER_ITEM);
	InitPickup(obj, ID_REVOLVER_AMMO_ITEM);
	InitPickup(obj, ID_BIGMEDI_ITEM);
	InitPickup(obj, ID_SMALLMEDI_ITEM);
	InitPickup(obj, ID_LASERSIGHT_ITEM);
	InitPickup(obj, ID_BINOCULARS_ITEM);
	InitPickup(obj, ID_SILENCER_ITEM);
	InitPickup(obj, ID_FLARE_INV_ITEM);
	InitPickup(obj, ID_WATERSKIN1_EMPTY);
	InitPickup(obj, ID_WATERSKIN2_EMPTY);
	InitPickup(obj, ID_GOLDROSE_ITEM);
	InitPickup(obj, ID_DIARY_ITEM);
	InitPickup(obj, ID_PC_LOAD_INV_ITEM);
	InitPickup(obj, ID_PC_SAVE_INV_ITEM);
	InitPickup(obj, ID_STOPWATCH_ITEM);
	InitPickup(obj, ID_COMPASS_ITEM);
}

void InitializeTR5Objects()
{
	ObjectInfo* objectPtr = nullptr;
	StartEntity(objectPtr);
	StartObject(objectPtr);
	StartTrap(objectPtr);
	StartPickup(objectPtr);
	StartSwitch(objectPtr);
	StartShatter(objectPtr);
	StartProjectiles(objectPtr);
}

void AllocTR5Objects()
{
	ZeroMemory(Bats, NUM_BATS * sizeof(BatData));
	ZeroMemory(Spiders, NUM_SPIDERS * sizeof(SpiderData));
	ZeroMemory(Rats, NUM_RATS * sizeof(RatData));
}
