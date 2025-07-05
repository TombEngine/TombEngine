#include "framework.h"
#include "Objects/TR2/tr2_objects.h"

#include "Game/collision/collide_item.h"
#include "Game/control/box.h"
#include "Game/itemdata/creature_info.h"
#include "Game/missile.h"
#include "Game/Setup.h"
#include "Objects/Utils/object_helper.h"
#include "Specific/level.h"

// Creatures
#include "Objects/TR2/Entity/Bartoli.h" // OK
#include "Objects/TR2/Entity/Dragon.h" // OK
#include "Objects/TR2/Entity/tr2_barracuda.h" // OK
#include "Objects/TR2/Entity/tr2_bird_monster.h" // OK
#include "Objects/TR2/Entity/tr2_eagle_or_crow.h" // OK
#include "Objects/TR2/Entity/tr2_knife_thrower.h" // OK
#include "Objects/TR2/Entity/tr2_mercenary.h" // OK
#include "Objects/TR2/Entity/tr2_monk.h" // OK
#include "Objects/TR2/Entity/tr2_rat.h" // OK
#include "Objects/TR2/Entity/tr2_shark.h" // OK
#include "Objects/TR2/Entity/tr2_silencer.h" // OK
#include "Objects/TR2/Entity/tr2_skidman.h" // OK
#include "Objects/TR2/Entity/tr2_spear_guardian.h" // OK
#include "Objects/TR2/Entity/tr2_spider.h" // OK
#include "Objects/TR2/Entity/tr2_sword_guardian.h" // OK
#include "Objects/TR2/Entity/tr2_worker_dualrevolver.h" // OK
#include "Objects/TR2/Entity/tr2_worker_flamethrower.h" // OK
#include "Objects/TR2/Entity/tr2_worker_machinegun.h" // OK
#include "Objects/TR2/Entity/tr2_worker_shotgun.h" // OK
#include "Objects/TR2/Entity/tr2_yeti.h" // OK

// Traps
#include "Objects/TR2/Trap/tr2_spinningblade.h"
#include "Objects/TR2/Trap/tr2_springboard.h"
#include "Objects/TR2/Trap/tr2_killerstatue.h"

// Vehicles
#include "Objects/TR2/Vehicles/speedboat.h"
#include "Objects/TR2/Vehicles/skidoo.h"

using namespace TEN::Entities::Creatures::TR2;
using namespace TEN::Entities::Traps;

static void StartEntity(ObjectInfo* obj)
{
	obj = &Objects[ID_SHARK];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->control = SharkControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 30;
		obj->pivotLength = 200;
		obj->radius = 340;
		obj->intelligent = true;
		obj->waterCreature = true;
		obj->LotType = LotType::Water;
		obj->SetBoneRotationFlags(9, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_BARRACUDA];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->control = BarracudaControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 12;
		obj->pivotLength = 200;
		obj->radius = 204;
		obj->intelligent = true;
		obj->waterCreature = true;
		obj->LotType = LotType::Water;
		obj->SetBoneRotationFlags(6, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_EAGLE];
	if (obj->loaded)
	{
		obj->Initialize = InitializeEagle;
		obj->control = EagleControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 20;
		obj->radius = 204;
		obj->intelligent = true;
		obj->pivotLength = 0;
		obj->LotType = LotType::Flyer;
		obj->SetHitEffect();
	}

	obj = &Objects[ID_CROW];
	if (obj->loaded)
	{
		obj->Initialize = InitializeEagle;
		obj->control = EagleControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 15;
		obj->radius = 204;
		obj->intelligent = true;
		obj->pivotLength = 0;
		obj->LotType = LotType::Flyer;
		obj->SetHitEffect();
	}

	obj = &Objects[ID_RAT];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->control = RatControl;
		obj->collision = CreatureCollision;
		obj->HitPoints = 5;
		obj->shadowType = ShadowMode::All;
		obj->pivotLength = 50;
		obj->radius = 204;
		obj->intelligent = true;
		obj->SetHitEffect();
	}

	obj = &Objects[ID_YETI];
	if (obj->loaded)
	{
		obj->Initialize = InitializeYeti;
		obj->collision = CreatureCollision;
		obj->control = YetiControl;
		obj->HitPoints = 30;
		obj->shadowType = ShadowMode::All;
		obj->radius = 128;
		obj->pivotLength = 100;
		obj->intelligent = true;
		obj->LotType = LotType::Human;
		obj->SetBoneRotationFlags(6, ROT_Y);
		obj->SetBoneRotationFlags(14, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_GOON_SILENCER1];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->collision = CreatureCollision;
		obj->control = SilencerControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 25;
		obj->radius = 102;
		obj->pivotLength = 50;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(0, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(1, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_GOON_SILENCER2];
	if (obj->loaded)
	{
		AssignObjectAnimations(*obj, ID_GOON_SILENCER1, "ID_GOON_SILENCER2", "ID_GOON_SILENCER1");
		obj->Initialize = InitializeCreature;
		obj->collision = CreatureCollision;
		obj->control = SilencerControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 25;
		obj->radius = 102;
		obj->pivotLength = 50;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(0, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(1, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_GOON_SILENCER3];
	if (obj->loaded)
	{
		AssignObjectAnimations(*obj, ID_GOON_SILENCER1, "ID_GOON_SILENCER3", "ID_GOON_SILENCER1");
		obj->Initialize = InitializeCreature;
		obj->collision = CreatureCollision;
		obj->control = SilencerControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 25;
		obj->radius = 102;
		obj->pivotLength = 50;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(0, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(1, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_WORKER_SHOTGUN];
	if (obj->loaded)
	{
		obj->Initialize = InitializeWorkerShotgun;
		obj->collision = CreatureCollision;
		obj->control = WorkerShotgunControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 25;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(4, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_WORKER_MACHINEGUN];
	if (obj->loaded)
	{
		obj->Initialize = InitializeWorkerMachineGun;
		obj->collision = CreatureCollision;
		obj->control = WorkerMachineGunControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 20;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(4, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_SMALL_SPIDER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->collision = CreatureCollision;
		obj->control = SmallSpiderControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 5;
		obj->pivotLength = 0;
		obj->radius = 102;
		obj->intelligent = true;
		obj->LotType = LotType::Spider;
		obj->SetHitEffect();
	}

	obj = &Objects[ID_BIG_SPIDER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->collision = CreatureCollision;
		obj->control = BigSpiderControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 40;
		obj->pivotLength = 0;
		obj->radius = 102;
		obj->intelligent = true;
		obj->SetHitEffect();
	}

	obj = &Objects[ID_WORKER_DUAL_REVOLVER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->collision = CreatureCollision;
		obj->control = WorkerDualGunControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 150;
		obj->pivotLength = 0;
		obj->radius = 102;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(0, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(11, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_BIRDMONSTER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->collision = CreatureCollision;
		obj->control = BirdMonsterControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 200;
		obj->pivotLength = 0;
		obj->radius = 341;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(14, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_WORKER_FLAMETHROWER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeWorkerFlamethrower;
		obj->collision = CreatureCollision;
		obj->control = WorkerFlamethrower;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 20;
		obj->pivotLength = 0;
		obj->radius = 102;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(4, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(14, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_KNIFETHROWER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->collision = CreatureCollision;
		obj->control = KnifeThrowerControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 60;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(8, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_MERCENARY_UZI];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->collision = CreatureCollision;
		obj->control = MercenaryUziControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 45;
		obj->pivotLength = 0;
		obj->radius = 102;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(8, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_MERCENARY_AUTOPISTOLS1];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->collision = CreatureCollision;
		obj->control = MercenaryAutoPistolControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 50;
		obj->pivotLength = 0;
		obj->radius = 102;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(8, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_MERCENARY_AUTOPISTOLS2];
	if (obj->loaded)
	{
		AssignObjectAnimations(*obj, ID_MERCENARY_AUTOPISTOLS1, "ID_MERCENARY_AUTOPISTOLS2", "ID_MERCENARY_AUTOPISTOLS1");
		obj->Initialize = InitializeCreature;
		obj->collision = CreatureCollision;
		obj->control = MercenaryAutoPistolControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 50;
		obj->pivotLength = 0;
		obj->radius = 102;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(8, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_MONK1];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->collision = CreatureCollision;
		obj->control = MonkControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 50;
		obj->pivotLength = 0;
		obj->radius = 204;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_MONK2];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->collision = CreatureCollision;
		obj->control = MonkControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 50;
		obj->pivotLength = 0;
		obj->radius = 204;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_SWORD_GUARDIAN];
	if (obj->loaded)
	{
		CheckIfSlotExists(ID_SWORD_GUARDIAN_STATUE, GetObjectName(ID_SWORD_GUARDIAN));
		obj->Initialize = InitializeSwordGuardian;
		obj->collision = CreatureCollision;
		obj->control = SwordGuardianControl;
		obj->HitRoutine = SwordGuardianHit;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 80;
		obj->pivotLength = 0;
		obj->radius = 204;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(17, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_SPEAR_GUARDIAN];
	if (obj->loaded)
	{
		CheckIfSlotExists(ID_SPEAR_GUARDIAN_STATUE, GetObjectName(ID_SPEAR_GUARDIAN));
		obj->Initialize = InitializeSpearGuardian;
		obj->collision = CreatureCollision;
		obj->control = SpearGuardianControl;
		obj->HitRoutine = SpearGuardianHit;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 100;
		obj->pivotLength = 0;
		obj->radius = 204;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(12, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_DRAGON_FRONT];
	if (obj->loaded)
	{
		CheckIfSlotExists(ID_DRAGON_BACK, "Dragon");
		obj->Initialize = InitializeDragon;
		obj->collision = CollideDragonFront;
		obj->control = ControlDragon;
		obj->HitPoints = 300;
		obj->pivotLength = 300;
		obj->radius = 256;
		obj->shadowType = ShadowMode::All;
		obj->intelligent = true;
		obj->LotType = LotType::Blockable;
		obj->SetBoneRotationFlags(10, ROT_Z);
		obj->SetHitEffect(HitEffect::NonExplosive);
	}

	obj = &Objects[ID_DRAGON_BACK];
	if (obj->loaded)
	{
		CheckIfSlotExists(ID_DRAGON_FRONT, "Dragon");
		CheckIfSlotExists(ID_DRAGON_BONE_FRONT, "Dragon");
		CheckIfSlotExists(ID_DRAGON_BONE_BACK, "Dragon");
		obj->collision = CollideDragonBack;
		obj->SetHitEffect(false, true);
		obj->shadowType = ShadowMode::All;
	}

	obj = &Objects[ID_DRAGON_BONE_FRONT];
	if (obj->loaded)
	{
		obj->shadowType = ShadowMode::All;
	}

	obj = &Objects[ID_DRAGON_BONE_BACK];
	if (obj->loaded)
	{
		obj->shadowType = ShadowMode::All;
	}

	obj = &Objects[ID_MARCO_BARTOLI];
	if (obj->loaded)
	{
		CheckIfSlotExists(ID_SPHERE_OF_DOOM, "Marco Bartoli");
		CheckIfSlotExists(ID_SPHERE_OF_DOOM2, "Marco Bartoli");
		CheckIfSlotExists(ID_SPHERE_OF_DOOM3, "Marco Bartoli");
		obj->Initialize = InitializeBartoli;
		obj->control = ControlBartoli;
	}

	// TODO: Recreate renderer for skidoo.
	obj = &Objects[ID_SNOWMOBILE_GUN];
	if (obj->loaded)
	{
		obj->collision = SkidooManCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 100;
		obj->pivotLength = 0;
		obj->radius = 256;
		obj->intelligent = true;
		obj->LotType = LotType::SnowmobileGun;
		obj->SetHitEffect();
	}

	obj = &Objects[ID_SNOWMOBILE_DRIVER];
	if (obj->loaded)
	{
		CheckIfSlotExists(ID_SNOWMOBILE_GUN, GetObjectName(ID_SNOWMOBILE_DRIVER));
		obj->Initialize = InitializeSkidooMan;
		obj->control = SkidooManControl;
		obj->SetHitEffect(true);
	}
}

static void StartObject(ObjectInfo* obj)
{
	InitProjectile(obj, ControlMissile, ID_KNIFETHROWER_KNIFE);

	obj = &Objects[ID_SPHERE_OF_DOOM];
	if (obj->loaded)
	{
		obj->control = ControlBartoliTransformEffect;
		obj->shadowType = ShadowMode::None;
	}

	obj = &Objects[ID_SPHERE_OF_DOOM2];
	if (obj->loaded)
	{
		obj->control = ControlBartoliTransformEffect;
		obj->shadowType = ShadowMode::None;
	}

	obj = &Objects[ID_SPHERE_OF_DOOM3];
	if (obj->loaded)
	{
		obj->control = ControlBartoliTransformEffect;
		obj->shadowType = ShadowMode::None;
	}
}

static void StartTrap(ObjectInfo* obj)
{
	obj = &Objects[ID_ROLLING_SPINDLE];
	if (obj->loaded)
	{
		obj->Initialize = InitializeSpinningBlade;
		obj->control = ControlSpinningBlade;
		obj->collision = ObjectCollision;
	}

	obj = &Objects[ID_SPRINGBOARD];
	if (obj->loaded)
	{
		obj->control = SpringBoardControl;
	}

	obj = &Objects[ID_STATUE_WITH_BLADE];
	if (obj->loaded)
	{
		obj->Initialize = InitializeKillerStatue;
		obj->control = ControlKillerStatue;
		obj->collision = ObjectCollision;
		obj->SetHitEffect(true);
	}
}

// boat, snowmobile, snowmobile gun
static void StartVehicles(ObjectInfo* obj)
{
	// TODO: Fix BoatControl() not using BoatControl().
	obj = &Objects[ID_SPEEDBOAT];
	if (obj->loaded)
	{
		obj->Initialize = InitializeSpeedboat;
		obj->collision = SpeedboatPlayerCollision;
		obj->control = SpeedboatControl;
		obj->shadowType = ShadowMode::Player;
		obj->SetHitEffect(true);
	}

	// TODO: Create a new renderer for the skidoo with animated track.
	obj = &Objects[ID_SNOWMOBILE];
	if (obj->loaded)
	{
		obj->Initialize = InitializeSkidoo;
		obj->collision = SkidooPlayerCollision;
		obj->shadowType = ShadowMode::Player;
		obj->SetHitEffect(true);
	}
}

void InitializeTR2Objects()
{
	ObjectInfo* objectPtr = nullptr;
	StartEntity(objectPtr);
	StartObject(objectPtr);
	StartTrap(objectPtr);
	StartVehicles(objectPtr);
}
