#include "framework.h"
#include "Objects/TR4/tr4_objects.h"

#include "Game/collision/collide_item.h"
#include "Game/control/box.h"
#include "Game/itemdata/creature_info.h"
#include "Game/pickup/pickup.h"
#include "Game/Setup.h"
#include "Objects/Utils/object_helper.h"
#include "Objects/Generic/Object/objects.h"
#include "Specific/level.h"

// Creatures
#include "Objects/TR4/Entity/Wraith.h" // OFF
#include "Objects/TR4/Entity/tr4_enemy_jeep.h"
#include "Objects/TR4/Entity/tr4_ahmet.h" // OK
#include "Objects/TR4/Entity/tr4_baddy.h" // OK
#include "Objects/TR4/Entity/tr4_bat.h" // OK
#include "Objects/TR4/Entity/tr4_big_scorpion.h" // OK
#include "Objects/TR4/Entity/tr4_crocodile.h" // OK
#include "Objects/TR4/Entity/tr4_demigod.h" // OK
#include "Objects/TR4/Entity/tr4_guide.h" // OK
#include "Objects/TR4/Entity/tr4_harpy.h" // OK
#include "Objects/TR4/Entity/tr4_horseman.h" // OFF
#include "Objects/TR4/Entity/tr4_jean_yves.h" // OK
#include "Objects/TR4/Entity/tr4_knight_templar.h" // OK
#include "Objects/TR4/Entity/tr4_lara_double.h"
#include "Objects/TR4/Entity/tr4_beetle_swarm.h"
#include "Objects/TR4/Entity/tr4_mummy.h" // OK
#include "Objects/TR4/Entity/tr4_sas.h" // OK
#include "Objects/TR4/Entity/tr4_sentry_gun.h" // OK
#include "Objects/TR4/Entity/tr4_skeleton.h" // OK
#include "Objects/TR4/Entity/tr4_small_scorpion.h" // OK
#include "Objects/TR4/Entity/tr4_sphinx.h" // OK
#include "Objects/TR4/Entity/tr4_troops.h" // OK
#include "Objects/TR4/Entity/tr4_wild_boar.h" // OK
#include "Objects/TR4/Entity/tr4_baboon.h" // OK
#include "Objects/TR4/Entity/tr4_mutant.h" // OK
#include "Objects/TR4/Entity/tr4_big_beetle.h" // OFF
#include "Objects/TR4/Entity/tr4_von_croy.h"
#include "Objects/TR4/Entity/tr4_hammerhead.h"
#include "Objects/TR4/Entity/tr4_dog.h"
#include "Objects/TR4/Entity/tr4_setha.h"

// Objects
#include "Objects/TR4/Object/StatuePlinth.h"
#include "Objects/TR4/Object/WraithTrap.h"
#include "Objects/TR4/Object/tr4_element_puzzle.h"
#include "Objects/TR4/Object/tr4_mapper.h"
#include "Objects/TR4/Object/tr4_sarcophagus.h"
#include "Objects/TR4/Object/tr4_senet.h"
#include "Objects/TR4/Object/tr4_clockwork_beetle.h"
#include "Objects/TR4/Object/tr4_obelisk.h"
#include "Objects/TR4/Object/tr4_scales.h"

// Switches

// Traps
#include "Objects/TR4/Trap/SpikyCeiling.h"
#include "Objects/TR4/Trap/SpikyWall.h"
#include "Objects/TR4/Trap/tr4_birdblade.h"
#include "Objects/TR4/Trap/tr4_blade.h"
#include "Objects/TR4/Trap/tr4_catwalkblade.h"
#include "Objects/TR4/Trap/tr4_chain.h"
#include "Objects/TR4/Trap/tr4_fourblades.h"
#include "Objects/TR4/Trap/tr4_hammer.h"
#include "Objects/TR4/Trap/tr4_joby_spikes.h"
#include "Objects/TR4/Trap/tr4_mine.h"
#include "Objects/TR4/Trap/tr4_moving_blade.h"
#include "Objects/TR4/Trap/tr4_plinthblade.h"
#include "Objects/TR4/Trap/tr4_plough.h"
#include "Objects/TR4/Trap/tr4_sethblade.h"
#include "Objects/TR4/Trap/tr4_slicerdicer.h"
#include "Objects/TR4/Trap/SquishyBlock.h"
#include "Objects/TR4/Trap/tr4_spikeball.h"
#include "Objects/TR4/Trap/tr4_stargate.h"
#include "Objects/TR4/Trap/tr4_cog.h"
#include "Objects/TR4/Trap/tr4_teethspike.h"

// Vehicles
#include "Objects/TR4/Vehicles/jeep.h"
#include "Objects/TR4/Vehicles/motorbike.h"

// Effects
#include "Objects/Effects/tr4_locusts.h" // OK

using namespace TEN::Entities::TR4;
using namespace TEN::Entities::Traps;

namespace TEN::Entities
{
	static void StartEntity(ObjectInfo* obj)
	{
		obj = &Objects[ID_SMALL_SCORPION];
		if (obj->loaded)
		{
			obj->Initialize = InitializeSmallScorpion;
			obj->control = SmallScorpionControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 8;
			obj->pivotLength = 20;
			obj->radius = 128;
			obj->intelligent = true;
			obj->SetHitEffect();
		}

		obj = &Objects[ID_BIG_SCORPION];
		if (obj->loaded)
		{
			obj->Initialize = InitializeScorpion;
			obj->control = ScorpionControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 80;
			obj->pivotLength = 50;
			obj->radius = 512;
			obj->intelligent = true;
			obj->SetHitEffect();
		}

		obj = &Objects[ID_HAMMERHEAD];
		if (obj->loaded)
		{
			obj->Initialize = InitializeHammerhead;
			obj->control = HammerheadControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 8;
			obj->pivotLength = 20;
			obj->radius = 128;
			obj->intelligent = true;
			obj->waterCreature = true;
			obj->LotType = LotType::Water;
			obj->SetHitEffect();
		}

		obj = &Objects[ID_WILD_BOAR];
		if (obj->loaded)
		{
			obj->Initialize = InitializeWildBoar;
			obj->control = WildBoarControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 40;
			obj->pivotLength = 50;
			obj->radius = 102;
			obj->intelligent = true;
			obj->SetBoneRotationFlags(12, ROT_Y | ROT_Z);
			obj->SetBoneRotationFlags(13, ROT_Y | ROT_Z);
			obj->SetHitEffect();
		}

		obj = &Objects[ID_DOG];
		if (obj->loaded)
		{
			obj->Initialize = InitializeTr4Dog;
			obj->collision = CreatureCollision;
			obj->control = Tr4DogControl;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 18;
			obj->pivotLength = 50;
			obj->radius = 256;
			obj->intelligent = true;
			obj->SetBoneRotationFlags(0, ROT_Y);
			obj->SetBoneRotationFlags(2, ROT_X | ROT_Y);
			obj->SetHitEffect();
		}

		obj = &Objects[ID_BAT];
		if (obj->loaded)
		{
			obj->Initialize = InitializeBat;
			obj->control = BatControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 5;
			obj->pivotLength = 10;
			obj->radius = 102;
			obj->intelligent = true;
			obj->LotType = LotType::Flyer;
			obj->SetHitEffect();
		}

		obj = &Objects[ID_AHMET];
		if (obj->loaded)
		{
			obj->Initialize = InitializeAhmet;
			obj->control = AhmetControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 80;
			obj->pivotLength = 300;
			obj->radius = 341;
			obj->intelligent = true;
			obj->SetBoneRotationFlags(9, ROT_Y);
			obj->SetHitEffect();
		}

		obj = &Objects[ID_BADDY1];
		if (obj->loaded)
		{
			AssignObjectMeshSwap(*obj, ID_MESHSWAP_BADDY1, "ID_BADDY1", "ID_MESHSWAP_BADDY1");
			obj->Initialize = InitializeBaddy;
			obj->control = BaddyControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 25;
			obj->pivotLength = 50;
			obj->radius = 102;
			obj->intelligent = true;
			obj->meshSwapSlot = ID_MESHSWAP_BADDY1;
			obj->LotType = LotType::HumanPlusJumpAndMonkey;
			obj->SetBoneRotationFlags(7, ROT_X | ROT_Y);
			obj->SetBoneRotationFlags(22, ROT_X | ROT_Y);
			obj->SetHitEffect();
		}

		obj = &Objects[ID_BADDY2];
		if (obj->loaded)
		{
			AssignObjectMeshSwap(*obj, ID_MESHSWAP_BADDY2, "ID_BADDY2", "ID_MESHSWAP_BADDY2");
			obj->Initialize = InitializeBaddy;
			obj->control = BaddyControl;
			obj->collision = CreatureCollision;
			obj->HitRoutine = Baddy2Hit;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 25;
			obj->pivotLength = 50;
			obj->radius = 102;
			obj->intelligent = true;
			obj->meshSwapSlot = ID_MESHSWAP_BADDY2;
			obj->LotType = LotType::HumanPlusJumpAndMonkey;
			obj->SetBoneRotationFlags(7, ROT_X | ROT_Y);
			obj->SetBoneRotationFlags(22, ROT_X | ROT_Y);
			obj->SetHitEffect();
		}

		obj = &Objects[ID_SAS_CAIRO];
		if (obj->loaded)
		{
			obj->Initialize = InitializeSas;
			obj->control = SasControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 40;
			obj->pivotLength = 50;
			obj->radius = 102;
			obj->intelligent = true;
			obj->SetBoneRotationFlags(0, ROT_X | ROT_Y);
			obj->SetBoneRotationFlags(7, ROT_X | ROT_Y);
			obj->SetHitEffect();
		}

		obj = &Objects[ID_MUMMY];
		if (obj->loaded)
		{
			obj->Initialize = InitializeMummy;
			obj->control = MummyControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 15;
			obj->radius = 170;
			obj->intelligent = true;
			obj->damageType = DamageMode::Explosion;
			obj->SetBoneRotationFlags(7, ROT_X | ROT_Y);
			obj->SetHitEffect();
		}

		obj = &Objects[ID_SKELETON];
		if (obj->loaded)
		{
			obj->Initialize = InitializeSkeleton;
			obj->control = SkeletonControl;
			obj->collision = CreatureCollision;
			obj->HitPoints = 15;
			obj->shadowType = ShadowMode::All;
			obj->pivotLength = 50;
			obj->radius = 128;
			obj->explodableMeshbits = 0xA00;
			obj->intelligent = true;
			obj->damageType = DamageMode::Explosion;
			obj->LotType = LotType::Skeleton;
			obj->SetHitEffect();
		}

		obj = &Objects[ID_KNIGHT_TEMPLAR];
		if (obj->loaded)
		{
			obj->Initialize = InitializeKnightTemplar;
			obj->control = KnightTemplarControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 15;
			obj->pivotLength = 50;
			obj->radius = 128;
			obj->intelligent = true;
			obj->damageType = DamageMode::None;
			obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
			obj->SetBoneRotationFlags(7, ROT_Y);
			obj->SetHitEffect();
		}

		obj = &Objects[ID_BIG_BEETLE];
		if (obj->loaded)
		{
			obj->Initialize = InitializeBigBeetle;
			obj->control = BigBeetleControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 30;
			obj->pivotLength = 50;
			obj->radius = 204;
			obj->intelligent = true;
			obj->LotType = LotType::Flyer;
			obj->SetHitEffect();
		}

		obj = &Objects[ID_SETHA];
		if (obj->loaded)
		{
			obj->Initialize = InitializeSeth;
			obj->control = SethControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 500;
			obj->pivotLength = 50;
			obj->radius = 341;
			obj->intelligent = true;
			obj->damageType = DamageMode::None;
			obj->LotType = LotType::Skeleton;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_DEMIGOD1];
		if (obj->loaded)
		{
			obj->Initialize = InitializeDemigod;
			obj->control = DemigodControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 200;
			obj->pivotLength = 50;
			obj->radius = 341;
			obj->intelligent = true;
			obj->damageType = DamageMode::None;
			obj->SetBoneRotationFlags(8, ROT_X | ROT_Y | ROT_Z);
			obj->SetBoneRotationFlags(20, ROT_Y);
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_DEMIGOD2];
		if (obj->loaded)
		{
			obj->Initialize = InitializeDemigod;
			obj->control = DemigodControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 200;
			obj->pivotLength = 50;
			obj->radius = 341;
			obj->intelligent = true;
			obj->SetBoneRotationFlags(8, ROT_X | ROT_Y | ROT_Z);
			obj->SetBoneRotationFlags(20, ROT_Y);
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_DEMIGOD3];
		if (obj->loaded)
		{
			obj->Initialize = InitializeDemigod;
			obj->control = DemigodControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 200;
			obj->pivotLength = 50;
			obj->radius = 341;
			obj->intelligent = true;
			obj->SetBoneRotationFlags(8, ROT_X | ROT_Y | ROT_Z);
			obj->SetBoneRotationFlags(20, ROT_Y);
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_JEAN_YVES];
		if (obj->loaded)
		{
			obj->Initialize = InitializeJeanYves;
			obj->control = JeanYvesControl;
			obj->collision = ObjectCollision;
			obj->nonLot = true;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_TROOPS];
		if (obj->loaded)
		{
			obj->Initialize = InitializeTroops;
			obj->control = TroopsControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 40;
			obj->pivotLength = 50;
			obj->radius = 102;
			obj->intelligent = true;
			obj->SetBoneRotationFlags(0, ROT_X | ROT_Y);
			obj->SetBoneRotationFlags(7, ROT_X | ROT_Y);
			obj->SetHitEffect();
		}

		obj = &Objects[ID_SENTRY_GUN];
		if (obj->loaded)
		{
			obj->Initialize = InitializeSentryGun;
			obj->control = ControlSentryGun;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->damageType = DamageMode::None;
			obj->HitPoints = 30;
			obj->pivotLength = 50;
			obj->radius = 204;
			obj->intelligent = true;
			obj->explodableMeshbits = 0x40;
			obj->SetBoneRotationFlags(0, ROT_X | ROT_Y);
			obj->SetBoneRotationFlags(2, ROT_Z);
			obj->SetBoneRotationFlags(3, ROT_Z);
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_HARPY];
		if (obj->loaded)
		{
			obj->Initialize = InitializeHarpy;
			obj->control = HarpyControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 60;
			obj->pivotLength = 50;
			obj->radius = 409;
			obj->intelligent = true;
			obj->LotType = LotType::Flyer;
			obj->SetHitEffect();
		}

		obj = &Objects[ID_GUIDE];
		if (obj->loaded)
		{
			obj->Initialize = InitializeGuide;
			obj->control = GuideControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = NOT_TARGETABLE;
			obj->pivotLength = 0;
			obj->radius = 128;
			obj->intelligent = true;
			obj->meshSwapSlot = ID_MESHSWAP2;
			obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
			obj->SetBoneRotationFlags(20, ROT_X | ROT_Y);
			obj->SetHitEffect();
		}

		obj = &Objects[ID_CROCODILE];
		if (obj->loaded)
		{
			obj->Initialize = InitializeCrocodile;
			obj->control = CrocodileControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 36;
			obj->pivotLength = 300;
			obj->radius = 409;
			obj->intelligent = true;
			obj->waterCreature = true;
			obj->LotType = LotType::Water; // TODO: later, change it to WaterAndLand.
			obj->SetBoneRotationFlags(0, ROT_Y);
			obj->SetBoneRotationFlags(7, ROT_Y);
			obj->SetBoneRotationFlags(9, ROT_Y);
			obj->SetBoneRotationFlags(10, ROT_Y);
			obj->SetHitEffect();
		}

		obj = &Objects[ID_SPHINX];
		if (obj->loaded)
		{
			obj->Initialize = InitializeSphinx;
			obj->control = SphinxControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 1000;
			obj->pivotLength = 500;
			obj->radius = 512;
			obj->intelligent = true;
			obj->damageType = DamageMode::None;
			obj->SetHitEffect();
		}

		obj = &Objects[ID_HORSE];
		if (obj->loaded)
		{
			obj->Initialize = InitializeHorse;
			obj->collision = ObjectCollision;
			obj->control = nullptr;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_HORSEMAN];
		if (obj->loaded)
		{
			obj->Initialize = InitializeHorseman;
			obj->control = HorsemanControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 25;
			obj->pivotLength = 500;
			obj->radius = 409;
			obj->intelligent = true;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_BABOON_NORMAL];
		if (obj->loaded)
		{
			obj->Initialize = InitializeBaboon;
			obj->control = BaboonControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 30;
			obj->pivotLength = 200;
			obj->radius = 256;
			obj->intelligent = true;
			obj->SetHitEffect();
		}

		obj = &Objects[ID_BABOON_INV];
		if (obj->loaded)
		{
			AssignObjectAnimations(*obj, ID_BABOON_NORMAL, "ID_BABOON_INV", "ID_BABOON_NORMAL");
			obj->Initialize = InitializeBaboon;
			obj->control = BaboonControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 30;
			obj->pivotLength = 200;
			obj->radius = 256;
			obj->intelligent = true;
			obj->SetHitEffect();
		}

		obj = &Objects[ID_BABOON_SILENT];
		if (obj->loaded)
		{
			AssignObjectAnimations(*obj, ID_BABOON_NORMAL, "ID_BABOON_SILENT", "ID_BABOON_NORMAL");
			obj->Initialize = InitializeBaboon;
			obj->control = BaboonControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 30;
			obj->pivotLength = 200;
			obj->radius = 256;
			obj->intelligent = true;
			obj->SetHitEffect();
		}

		obj = &Objects[ID_CROCODILE_GOD];
		if (obj->loaded)
		{
			obj->Initialize = InitializeCrocgod;
			obj->control = CrocgodControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = NOT_TARGETABLE;
			obj->pivotLength = 50;
			obj->radius = 128;
			obj->intelligent = true;
			obj->damageType = DamageMode::None;
			obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
			obj->SetBoneRotationFlags(7, ROT_X | ROT_Y);
			obj->SetHitEffect();
		}

		obj = &Objects[ID_LOCUSTS_EMITTER];
		if (obj->loaded)
		{
			obj->Initialize = InitializeLocust;
			obj->control = LocustControl;
			obj->drawRoutine = NULL;
		}

		obj = &Objects[ID_WRAITH1];
		if (obj->loaded)
		{
			obj->Initialize = InitializeWraith;
			obj->control = WraithControl;
		}

		obj = &Objects[ID_WRAITH2];
		if (obj->loaded)
		{
			obj->Initialize = InitializeWraith;
			obj->control = WraithControl;
		}

		obj = &Objects[ID_WRAITH3];
		if (obj->loaded)
		{
			obj->Initialize = InitializeWraith;
			obj->control = WraithControl;
		}	

		obj = &Objects[ID_LITTLE_BEETLE];
		if (obj->loaded)
		{
			obj->Initialize = InitializeBeetleSwarm;
			obj->control = BeetleSwarmControl;
			obj->drawRoutine = NULL;
		}

		obj = &Objects[ID_SAS_DYING];
		if (obj->loaded)
		{
			obj->Initialize = InitializeInjuredSas;
			obj->control = InjuredSasControl;
			obj->collision = ObjectCollision;
			obj->SetHitEffect(false, true);
		}

		obj = &Objects[ID_ENEMY_JEEP];
		if (obj->loaded)
		{
			obj->Initialize = InitializeEnemyJeep;
			obj->control = EnemyJeepControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 40;
			obj->pivotLength = 500;
			obj->radius = 512;
			obj->intelligent = true;
			obj->damageType = DamageMode::None; // NOTE: Prevents enemy jeep from being killed with skidoo gun or something like that.
			obj->LotType = LotType::HumanPlusJumpAndMonkey;
			obj->SetBoneRotationFlags(8, ROT_X);
			obj->SetBoneRotationFlags(9, ROT_X);
			obj->SetBoneRotationFlags(11, ROT_X);
			obj->SetBoneRotationFlags(12, ROT_X);
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_VON_CROY];
		if (obj->loaded)
		{
			obj->Initialize = InitializeVonCroy;
			obj->control = VonCroyControl;
			obj->collision = CreatureCollision;
			obj->pivotLength = 0;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 15;
			obj->explodableMeshbits = 0x200000;
			obj->intelligent = true;
			obj->LotType = LotType::HumanPlusJumpAndMonkey;
			obj->SetBoneRotationFlags(6, ROT_X | ROT_Y);
			obj->SetBoneRotationFlags(20, ROT_X | ROT_Y);
		}
	}

	static void StartObject(ObjectInfo* obj)
	{
		obj = &Objects[ID_SAS_DRAG_BLOKE];
		if (obj->loaded)
		{
			obj->control = AnimatingControl;
			obj->collision = SasDragBlokeCollision;
			obj->SetHitEffect(false, true);
		}

		obj = &Objects[ID_SARCOPHAGUS];
		if (obj->loaded)
		{
			obj->control = AnimatingControl;
			obj->collision = SarcophagusCollision;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_MAPPER];
		if (obj->loaded)
		{
			obj->Initialize = InitializeMapper;
			obj->control = MapperControl;
			obj->SetHitEffect(true);
			obj->shadowType = ShadowMode::All;
		}

		obj = &Objects[ID_ELEMENT_PUZZLE];
		if (obj->loaded)
		{
			obj->Initialize = InitializeElementPuzzle;
			obj->control = ElementPuzzleControl;
			obj->collision = ElementPuzzleCollision;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_STATUE_PLINTH];
		if (obj->loaded)
		{
			obj->Initialize = InitializeStatuePlinth;
			obj->collision = CollideStatuePlinth;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_WHEEL_OF_FORTUNE];
		if (obj->loaded)
		{
			obj->Initialize = InitializeGameSticks;
			obj->control = GameSticksControl;
			obj->collision = GameSticksCollision;
			obj->HitPoints = 1;
			obj->SetBoneRotationFlags(0, ROT_Z);
			obj->SetBoneRotationFlags(1, ROT_Z);
			obj->SetBoneRotationFlags(2, ROT_Z);
			obj->SetBoneRotationFlags(3, ROT_Z);
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_ENEMY_PIECE];
		if (obj->loaded)
		{
			obj->collision = ObjectCollision;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_GOD_HEAD];
		if (obj->loaded)
			obj->control = ControlGodHead;

		for (int i = 0; i < 3; i++)
		{
			obj = &Objects[ID_GAME_PIECE1 + i];
			if (obj->loaded)
			{
				obj->Initialize = InitializeGamePiece;
				obj->control = SenetControl;
				obj->collision = ObjectCollision;
				obj->SetHitEffect(true);
			}
		}

		InitPickup(obj, ID_CLOCKWORK_BEETLE, ClockworkBeetleControl);
		InitPickup(obj, ID_CLOCKWORK_BEETLE_COMBO1);
		InitPickup(obj, ID_CLOCKWORK_BEETLE_COMBO2);

		obj = &Objects[ID_OBELISK];
		if (obj->loaded)
		{
			obj->Initialize = InitializeObelisk;
			obj->control = ObeliskControl;
			obj->collision = ObjectCollision;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_WRAITH_TRAP];
		if (obj->loaded)
		{
			obj->Initialize = InitializeWraithTrap;
			obj->control = WraithTrapControl;
			obj->collision = ObjectCollision;
			obj->SetHitEffect(true);
		}

		for (int objectID = ID_WATERFALL1; objectID <= ID_WATERFALL6; objectID++)
		{
			obj = &Objects[objectID];
			if (obj->loaded)
				obj->control = nullptr;
		}
	}

	static void StartTrap(ObjectInfo* obj)
	{
		obj = &Objects[ID_CHAIN];
		if (obj->loaded)
		{
			obj->control = ControlChain;
			obj->collision = GenericSphereBoxCollision;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_PLOUGH];
		if (obj->loaded)
		{
			obj->Initialize = InitializePlough;
			obj->control = ControlPlough;
			obj->collision = GenericSphereBoxCollision;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_CATWALK_BLADE];
		if (obj->loaded)
		{
			obj->control = ControlCatwalkBlade;
			obj->collision = CollideBlade;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_SETH_BLADE];
		if (obj->loaded)
		{
			obj->Initialize = InitializeSethBlade;
			obj->control = ControlSethBlade;
			obj->collision = GenericSphereBoxCollision;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_PLINTH_BLADE];
		if (obj->loaded)
		{
			obj->control = ControlPlinthBlade;
			obj->collision = CollideBlade;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_BIRD_BLADE];
		if (obj->loaded)
		{
			obj->Initialize = InitializeBirdBlade;
			obj->control = ControlBirdBlade;
			obj->collision = GenericSphereBoxCollision;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_JOBY_SPIKES];
		if (obj->loaded)
		{
			obj->Initialize = InitializeJobySpikes;
			obj->control = ControlJobySpikes;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_MOVING_BLADE];
		if (obj->loaded)
		{
			obj->control = ControlMovingBlade;
			obj->collision = CollideBlade;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_SPIKEBALL];
		if (obj->loaded)
		{
			obj->control = ControlSpikeball;
			obj->collision = GenericSphereBoxCollision;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_FLOOR_4BLADES];
		if (obj->loaded)
		{
			obj->Initialize = InitializeFourBlades;
			obj->control = ControlFourBlades;
			obj->collision = GenericSphereBoxCollision;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_CEILING_4BLADES];
		if (obj->loaded)
		{
			obj->Initialize = InitializeFourBlades;
			obj->control = ControlFourBlades;
			obj->collision = GenericSphereBoxCollision;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_STARGATE];
		if (obj->loaded)
		{
			obj->control = StargateControl;
			obj->collision = StargateCollision;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_SLICER_DICER];
		if (obj->loaded)
		{
			obj->Initialize = InitializeSlicerDicer;
			obj->control = ControlSlicerDicer;
			obj->collision = CollideBlade;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_MINE];
		if (obj->loaded)
		{
			obj->Initialize = InitializeMine;
			obj->control = ControlMine;
			obj->collision = CollideMine;
		}

		obj = &Objects[ID_SPIKY_WALL];
		if (obj->loaded)
		{
			obj->Initialize = InitializeSpikyWall;
			obj->control = ControlSpikyWall;
			obj->collision = CollideSpikyWall;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_SPIKY_CEILING];
		if (obj->loaded)
		{
			obj->Initialize = InitializeSpikyCeiling;
			obj->control = ControlSpikyCeiling;
			obj->collision = CollideSpikyCeiling;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_COG];
		if (obj->loaded)
		{
			obj->control = ControlCog;
			obj->collision = CollideCog;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_LARA_DOUBLE];
		if (obj->loaded)
		{
			obj->Initialize = InitializeCreature;
			obj->control = LaraDoubleControl;
			obj->collision = CreatureCollision;
			obj->shadowType = ShadowMode::All;
			obj->HitPoints = 1000;
			obj->pivotLength = 50;
			obj->radius = 128;
			obj->intelligent = true;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_TEETH_SPIKES];
		if (obj->loaded)
		{
			obj->Initialize = InitializeTeethSpikes;
			obj->control = ControlTeethSpikes;
		}

		obj = &Objects[ID_HAMMER];
		if (obj->loaded)
		{
			obj->control = ControlHammer;
			obj->collision = GenericSphereBoxCollision;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_SQUISHY_BLOCK_HORIZONTAL];
		if (obj->loaded)
		{
			obj->Initialize = InitializeSquishyBlock;
			obj->control = ControlSquishyBlock;
			obj->collision = CollideSquishyBlock;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_SQUISHY_BLOCK_VERTICAL];
		if (obj->loaded)
		{
			obj->control = ControlFallingSquishyBlock;
			obj->collision = CollideFallingSquishyBlock;
			obj->SetHitEffect(true);
		}
	}

	static void StartVehicles(ObjectInfo* obj)
	{
		obj = &Objects[ID_JEEP];
		if (obj->loaded)
		{
			obj->Initialize = InitializeJeep;
			obj->collision = JeepPlayerCollision;
			obj->shadowType = ShadowMode::Player;
			obj->SetHitEffect(true);
		}

		obj = &Objects[ID_MOTORBIKE];
		if (obj->loaded)
		{
			obj->Initialize = InitializeMotorbike;
			obj->collision = MotorbikePlayerCollision;
			obj->shadowType = ShadowMode::Player;
			obj->SetHitEffect(true);
		}
	}

	static void StartSwitch(ObjectInfo* obj)
	{
	
	}

	void InitializeTR4Objects()
	{
		ObjectInfo* objectPtr = nullptr;
		StartEntity(objectPtr);
		StartObject(objectPtr);
		StartSwitch(objectPtr);
		StartTrap(objectPtr);
		StartVehicles(objectPtr);
	}

	void AllocTR4Objects()
	{
		ZeroMemory(BeetleSwarm, NUM_BEETLES * sizeof(BeetleData));
	}
}
