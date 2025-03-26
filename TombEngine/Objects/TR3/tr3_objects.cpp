#include "framework.h"
#include "Objects/TR3/tr3_objects.h"

#include "Game/collision/collide_item.h"
#include "Game/control/box.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Setup.h"
#include "Specific/level.h"

// Creatures
#include "Objects/TR3/Entity/Compsognathus.h"
#include "Objects/TR3/Entity/Lizard.h"
#include "Objects/TR3/Entity/PunaBoss.h"
#include "Objects/TR3/Entity/SealMutant.h"
#include "Objects/TR3/Entity/Shiva.h"
#include "Objects/TR3/Entity/SophiaLeigh.h"
#include "Objects/TR3/Entity/Raptor.h"
#include "Objects/TR3/Entity/TwinAutoGun.h"
#include "Objects/TR3/Entity/Willard.h"
#include "Objects/TR3/Entity/WaspMutant.h"
#include "Objects/TR3/Entity/Winston.h"
#include "Objects/TR3/Entity/tr3_tony.h"
#include "Objects/TR3/Entity/tr3_civvy.h"
#include "Objects/TR3/Entity/tr3_claw_mutant.h"
#include "Objects/TR3/Entity/tr3_cobra.h"
#include "Objects/TR3/Entity/FishSwarm.h"
#include "Objects/TR3/Entity/tr3_flamethrower.h"
#include "Objects/TR3/Entity/tr3_monkey.h"
#include "Objects/TR3/Entity/tr3_mp_gun.h"
#include "Objects/TR3/Entity/tr3_mp_stick.h"
#include "Objects/TR3/Entity/tr3_scuba_diver.h"
#include "Objects/TR3/Entity/tr3_tiger.h"
#include "Objects/TR3/Entity/tr3_trex.h"
#include "Objects/TR3/Entity/tr3_tribesman.h"

// Effects
#include "Objects/Effects/Boss.h"

// Objects
#include "Objects/TR3/Object/Corpse.h"

// Traps
#include "Objects/TR3/Trap/ElectricCleaner.h"
#include "Objects/TR3/Trap/train.h"
#include "Objects/TR3/Trap/WallMountedBlade.h"

// Vehicles
#include "Objects/TR3/Vehicles/big_gun.h"
#include "Objects/TR3/Vehicles/kayak.h"
#include "Objects/TR3/Vehicles/minecart.h"
#include "Objects/TR3/Vehicles/quad_bike.h"
#include "Objects/TR3/Vehicles/upv.h"
#include "Objects/TR3/Vehicles/rubber_boat.h"
#include "Objects/Utils/object_helper.h"

using namespace TEN::Entities::Creatures::TR3;
using namespace TEN::Entities::Traps;
using namespace TEN::Effects::Boss;
using namespace TEN::Entities::TR3;

static void StartEntity(ObjectInfo* obj)
{
	obj = &Objects[ID_TONY_BOSS];
	if (obj->loaded)
	{
		obj->Initialize = InitializeTony;
		obj->collision = CreatureCollision;
		obj->control = TonyControl;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 100;
		obj->pivotLength = 50;
		obj->radius = 102;
		obj->intelligent = true;
		obj->nonLot = true; // NOTE: Doesn't move to reach the player, only throws projectiles.
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_Y);
		obj->SetHitEffect(HitEffect::NonExplosive);
	}

	obj = &Objects[ID_TIGER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->control = TigerControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 24;
		obj->pivotLength = 200;
		obj->radius = 340;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(21, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_COBRA];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCobra;
		obj->control = CobraControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 8;
		obj->radius = 102;
		obj->intelligent = true;
		obj->nonLot = true;
		obj->SetBoneRotationFlags(0, ROT_Y);
		obj->SetBoneRotationFlags(6, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_RAPTOR];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->control = RaptorControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 100;
		obj->radius = 341;
		obj->pivotLength = 600;
		obj->intelligent = true;
		obj->LotType = LotType::HumanPlusJump;
		obj->SetBoneRotationFlags(20, ROT_Y);
		obj->SetBoneRotationFlags(21, ROT_Y);
		obj->SetBoneRotationFlags(23, ROT_Y);
		obj->SetBoneRotationFlags(25, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_TRIBESMAN_WITH_AX];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->control = TribemanAxeControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 28;
		obj->radius = 102;
		obj->intelligent = true;
		obj->pivotLength = 0;
		obj->SetBoneRotationFlags(6, ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_TRIBESMAN_WITH_DARTS];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->control = TribemanDartsControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 28;
		obj->radius = 102;
		obj->intelligent = true;
		obj->pivotLength = 0;
		obj->SetBoneRotationFlags(6, ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_TYRANNOSAUR];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->control = TRexControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 800;
		obj->pivotLength = 1800;
		obj->radius = 512;
		obj->intelligent = true;
		obj->LotType = LotType::Blockable;
		obj->SetBoneRotationFlags(10, ROT_Y);
		obj->SetBoneRotationFlags(11, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_SCUBA_DIVER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->control = ScubaControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 20;
		obj->radius = 340;
		obj->intelligent = true;
		obj->waterCreature = true;
		obj->pivotLength = 50;
		obj->LotType = LotType::Water;
		obj->SetBoneRotationFlags(10, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(14, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_SCUBA_HARPOON];
	if (obj->loaded)
	{
		obj->control = ScubaHarpoonControl;
		obj->collision = ObjectCollision;
	}

	obj = &Objects[ID_FLAMETHROWER_BADDY];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->control = FlameThrowerControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 36;
		obj->radius = 102;
		obj->intelligent = true;
		obj->pivotLength = 0;
		obj->SetBoneRotationFlags(0, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(7, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_MONKEY];
	if (obj->loaded)
	{
		CheckIfSlotExists(ID_MESHSWAP_MONKEY_KEY, GetObjectName(ID_MONKEY));
		CheckIfSlotExists(ID_MESHSWAP_MONKEY_MEDIPACK, GetObjectName(ID_MONKEY));
		obj->Initialize = InitializeMonkey;
		obj->control = MonkeyControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 8;
		obj->radius = 102;
		obj->intelligent = true;
		obj->pivotLength = 0;
		obj->SetBoneRotationFlags(0, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(7, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_MP_WITH_GUN];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->control = MPGunControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 28;
		obj->radius = 102;
		obj->intelligent = true;
		obj->pivotLength = 0;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_MP_WITH_STICK];
	if (obj->loaded)
	{
		obj->Initialize = InitializeMPStick;
		obj->control = MPStickControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 28;
		obj->radius = 102;
		obj->intelligent = true;
		obj->pivotLength = 0;
		obj->LotType = LotType::Human;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_SHIVA];
	if (obj->loaded)
	{
		CheckIfSlotExists(ID_SHIVA_STATUE, GetObjectName(ID_SHIVA));
		obj->Initialize = InitializeShiva;
		obj->collision = CreatureCollision;
		obj->control = ShivaControl;
		obj->HitRoutine = ShivaHit;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 100;
		obj->pivotLength = 0;
		obj->radius = 256;
		obj->intelligent = true;
		obj->LotType = LotType::Blockable;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(25, ROT_X | ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_SOPHIA_LEIGH_BOSS];
	if (obj->loaded)
	{
		obj->Initialize = InitializeSophiaLeigh;
		obj->control = SophiaLeighControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->pivotLength = 50;
		obj->HitPoints = 300;
		obj->radius = 102;
		obj->intelligent = true;
		obj->LotType = LotType::Human;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_Y);
		obj->SetHitEffect(HitEffect::NonExplosive);
	}

	obj = &Objects[ID_CIVVY];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCivvy;
		obj->control = CivvyControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 15;
		obj->radius = 102;
		obj->intelligent = true;
		obj->pivotLength = 0;
		obj->LotType = LotType::Human;
		obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
		obj->SetBoneRotationFlags(13, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_LIZARD];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->control = LizardControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 36;
		obj->intelligent = true;
		obj->pivotLength = 0;
		obj->radius = 204;
		obj->LotType = LotType::Human;
		obj->SetBoneRotationFlags(9, ROT_X | ROT_Z);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_TWIN_AUTO_GUN];
	if (obj->loaded)
	{
		obj->Initialize = InitializeTwinAutoGun;
		obj->control = ControlTwinAutoGun;
		obj->collision = CreatureCollision;
		obj->HitRoutine = HitTwinAutoGun;
		obj->HitPoints = 28;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(0, ROT_Y);
		obj->SetBoneRotationFlags(1, ROT_X);
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_PUNA_BOSS];
	if (obj->loaded)
	{
		obj->Initialize = InitializePuna;
		obj->control = PunaControl;
		obj->collision = CreatureCollision;
		obj->HitRoutine = PunaHit;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 200;
		obj->intelligent = true;
		obj->nonLot = true;
		obj->radius = 102;
		obj->pivotLength = 50;
		obj->SetBoneRotationFlags(4, ROT_Y);		 // Puna quest object.
		obj->SetBoneRotationFlags(7, ROT_X | ROT_Y); // Head.
		obj->SetHitEffect(HitEffect::NonExplosive);
	}
	
	obj = &Objects[ID_WASP_MUTANT];
	if (obj->loaded)
	{
		obj->Initialize = InitializeWaspMutant;
		obj->control = WaspMutantControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->intelligent = true;
		obj->HitPoints = 24;
		obj->radius = 102;
		obj->pivotLength = 0;
		obj->LotType = LotType::Flyer;
		obj->SetHitEffect();
	}
	
	obj = &Objects[ID_COMPSOGNATHUS];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCompsognathus;
		obj->control = CompsognathusControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 10;
		obj->intelligent = true;
		obj->radius = 204;
		obj->pivotLength = 0;
		obj->SetBoneRotationFlags(1, ROT_X | ROT_Z);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_CLAW_MUTANT];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCreature;
		obj->control = ClawMutantControl;
		obj->collision = CreatureCollision;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 130;
		obj->intelligent = true;
		obj->radius = 204;
		obj->pivotLength = 0;
		obj->SetBoneRotationFlags(0, ROT_X | ROT_Z);
		obj->SetBoneRotationFlags(7, ROT_Y);
		obj->SetHitEffect();
	}

	obj = &Objects[ID_SEAL_MUTANT];
	if (obj->loaded)
	{
		obj->Initialize = InitializeSealMutant;
		obj->collision = CreatureCollision;
		obj->control = ControlSealMutant;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 50;
		obj->radius = 204;
		obj->pivotLength = 0;
		obj->intelligent = true;
		obj->SetBoneRotationFlags(8, ROT_X | ROT_Z); // Torso X/Z
		obj->SetBoneRotationFlags(9, ROT_Y);		 // Head
		obj->SetHitEffect();
	}

	obj = &Objects[ID_WILLARD];
	if (obj->loaded)
	{
		obj->Initialize = InitializeWillard;
		obj->collision = CreatureCollision;
		obj->control = ControlWillard;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 200;
		obj->radius = 102;
		obj->pivotLength = 50;
		obj->intelligent = true;
		obj->LotType = LotType::Human;
		obj->SetHitEffect();
	}

	obj = &Objects[ID_WINSTON];
	if (obj->loaded)
	{
		obj->Initialize = InitializeWinston;
		obj->control = ControlWinston;
		obj->collision = ObjectCollision;
		obj->HitRoutine = HitWinston;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = 20;
		obj->radius = 102;
		obj->intelligent = true;
		obj->SetHitEffect();
	}

	obj = &Objects[ID_FISH_EMITTER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeFishSwarm;
		obj->control = ControlFishSwarm;
		obj->intelligent = true;
		obj->drawRoutine = NULL;
	}
}

static void StartObject(ObjectInfo* obj)
{
	obj = &Objects[ID_BOSS_SHIELD];
	if (obj->loaded)
	{
		obj->Initialize = nullptr;
		obj->collision = ObjectCollision;
		obj->control = ShieldControl;
		obj->shadowType = ShadowMode::None;
	}

	obj = &Objects[ID_BOSS_EXPLOSION_RING];
	if (obj->loaded)
	{
		obj->Initialize = nullptr;
		obj->collision = nullptr;
		obj->control = ShockwaveRingControl;
		obj->shadowType = ShadowMode::None;
	}

	obj = &Objects[ID_BOSS_EXPLOSION_SHOCKWAVE];
	if (obj->loaded)
	{
		obj->Initialize = nullptr;
		obj->collision = nullptr;
		obj->control = ShockwaveExplosionControl;
		obj->shadowType = ShadowMode::None;
	}

	obj = &Objects[ID_CORPSE];
	if (obj->loaded)
	{
		obj->Initialize = InitializeCorpse;
		obj->collision = CreatureCollision;
		obj->damageType = DamageMode::None;
		obj->control = ControlCorpse;
		obj->HitRoutine = HitCorpse;
		obj->HitPoints = NOT_TARGETABLE;
		obj->shadowType = ShadowMode::None;
		obj->SetHitEffect();
	}
}

static void StartTrap(ObjectInfo* obj)
{
	obj = &Objects[ID_TRAIN];
	if (obj->loaded)
	{
		obj->control = ControlTrain;
		obj->collision = CollideTrain;
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_ELECTRIC_CLEANER];
	if (obj->loaded)
	{
		obj->Initialize = InitializeElectricCleaner;
		obj->control = ControlElectricCleaner;
		obj->collision = CollideElectricCleaner;
		obj->shadowType = ShadowMode::All;
		obj->HitPoints = NOT_TARGETABLE;
		obj->nonLot = 1;
		obj->radius = 512;
	}

	obj = &Objects[ID_WALL_MOUNTED_BLADE];
	if (obj->loaded)
	{
		obj->Initialize = InitializeWallMountedBlade;
		obj->control = WallMountedBladeControl;
		obj->collision = GenericSphereBoxCollision;
	}
}

static void StartVehicles(ObjectInfo* obj)
{
	obj = &Objects[ID_QUAD];
	if (obj->loaded)
	{
		obj->Initialize = InitializeQuadBike;
		obj->collision = QuadBikePlayerCollision;
		obj->shadowType = ShadowMode::Player;
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_RUBBER_BOAT];
	if (obj->loaded)
	{
		obj->Initialize = InitializeRubberBoat;
		obj->control = RubberBoatControl;
		obj->collision = RubberBoatPlayerCollision;
		obj->drawRoutine = DrawRubberBoat;
		obj->shadowType = ShadowMode::Player;
		obj->SetHitEffect(true);

	}

	obj = &Objects[ID_KAYAK];
	if (obj->loaded)
	{
		obj->Initialize = InitializeKayak;
		obj->collision = KayakPlayerCollision;
		obj->shadowType = ShadowMode::Player;
		obj->SetHitEffect(true);

	}

	obj = &Objects[ID_MINECART];
	if (obj->loaded)
	{
		obj->Initialize = InitializeMinecart;
		obj->collision = MinecartPlayerCollision;
		obj->shadowType = ShadowMode::Player;
		obj->SetHitEffect(true);

	}

	obj = &Objects[ID_BIGGUN];
	if (obj->loaded)
	{
		obj->Initialize = BigGunInitialize;
		obj->collision = BigGunCollision;
		obj->shadowType = ShadowMode::Player;
		obj->SetHitEffect(true);
	}

	obj = &Objects[ID_UPV];
	if (obj->loaded)
	{
		obj->Initialize = InitializeUPV;
		obj->control = UPVEffects;
		obj->collision = UPVPlayerCollision;
		obj->shadowType = ShadowMode::Player;
		obj->SetHitEffect(true);
	}
}

static void StartProjectiles(ObjectInfo* obj)
{
	obj = &Objects[ID_TONY_BOSS_FLAME];
	obj->control = ControlTonyFireBall;
	obj->drawRoutine = nullptr;
}

void InitializeTR3Objects()
{
	ObjectInfo* objectPtr = nullptr;
	StartEntity(objectPtr);
	StartObject(objectPtr);
	StartTrap(objectPtr);
	StartVehicles(objectPtr);
	StartProjectiles(objectPtr);
}
