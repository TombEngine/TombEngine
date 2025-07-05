#include "framework.h"
#include "Game/control/flipeffect.h"

#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/control/lot.h"
#include "Game/effects/Hair.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/effects/Footprint.h"
#include "Game/effects/debris.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/pickup/pickup.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/level.h"
#include "Objects/Effects/Fireflies.h"
#include "Objects/Generic/puzzles_keys.h"
#include "Objects/TR3/Entity/FishSwarm.h"
#include "Objects/TR4/Entity/tr4_beetle_swarm.h"
#include "Objects/TR5/Emitter/tr5_spider_emitter.h"
#include "Objects/TR5/Emitter/tr5_rats_emitter.h"
#include "Objects/Effects/tr4_locusts.h"


using namespace TEN::Effects::Environment;
using namespace TEN::Effects::Footprint;
using namespace TEN::Effects::Hair;
using namespace TEN::Entities::Creatures::TR3;
using namespace TEN::Effects::Fireflies;

int FlipEffect;

std::function<EffectFunction> effect_routines[NUM_FLIPEFFECTS] =
{
	Turn180,					//0
	FloorShake,					//1
	PoseidonSFX,				//2
	HandlePlayerAirBubbles,		//3
	FinishLevel,				//4
	ActivateCamera,				//5
	ActivateKey,				//6
	RubbleFX,					//7
	SwapCrowbar,				//8
	Pickup,						//9
	PlaySoundEffect,			//10
	ExplosionFX,				//11
	LaraHandsFree,				//12
	Puzzle,						//13
	DrawRightPistol,			//14
	DrawLeftPistol,				//15
	ShootRightGun,				//16
	ShootLeftGun,				//17
	VoidEffect,					//18
	VoidEffect,					//19
	FlashOrange,				//20
	InvisibilityOn,				//21
	InvisibilityOff,			//22
	VoidEffect,					//23
	VoidEffect,					//24
	VoidEffect,					//25
	ResetHair,					//26
	VoidEffect,					//27
	SetFog,						//28
	VoidEffect,					//29
	LaraLocation,				//30
	ClearSwarmEnemies,			//31
	AddLeftFootprint,			//32
	AddRightFootprint,			//33
	VoidEffect,					//34
	VoidEffect,					//35
	VoidEffect,					//36
	VoidEffect,					//37
	VoidEffect,					//38
	VoidEffect,					//39
	VoidEffect,					//40
	VoidEffect,					//41
	VoidEffect,					//42
	MeshSwapToPour,				//43
	MeshSwapFromPour,			//44
	LaraLocationPad,			//45
	KillActiveBaddys			//46
};

void ClearSwarmEnemies(ItemInfo* item)
{
	ClearSpiders();
	ClearRats();
	ClearBeetleSwarm();
	ClearLocusts();
	ClearFishSwarm();
	ClearFireflySwarm();
}

void FlashOrange(ItemInfo* item) 
{
	FlipEffect = NO_VALUE;
	Weather.Flash(255, 128, 0, 0.03f);
}

void MeshSwapToPour(ItemInfo* item)
{
	item->Model.MeshIndex[LM_LHAND] = Objects[item->ItemFlags[2]].meshIndex + LM_LHAND;
}

void MeshSwapFromPour(ItemInfo* item)
{
	item->Model.MeshIndex[LM_LHAND] = item->Model.BaseMesh + LM_LHAND;
}

void Pickup(ItemInfo* item)
{
	DoPickup(item);
}

void Puzzle(ItemInfo* item)
{
	DoPuzzle();
}

void AddLeftFootprint(ItemInfo* item)
{
	SpawnFootprint(*item, false);
}

void AddRightFootprint(ItemInfo* item)
{
	SpawnFootprint(*item, true);
}

void ResetHair(ItemInfo* item)
{
	HairEffect.Initialize();
}

void InvisibilityOff(ItemInfo* item)
{
	item->Status = ITEM_ACTIVE;
}

void InvisibilityOn(ItemInfo* item)
{
	item->Status = ITEM_INVISIBLE;
}

void SetFog(ItemInfo* item)
{
	FlipEffect = NO_VALUE;
}

void DrawLeftPistol(ItemInfo* item)
{
	auto& player = GetLaraInfo(*item);

	if (item->Model.MeshIndex[LM_LHAND] == item->Model.BaseMesh + LM_LHAND)
	{
		item->Model.MeshIndex[LM_LHAND] = Objects[GetWeaponObjectMeshID(*item, LaraWeaponType::Pistol)].meshIndex + LM_LHAND;
		player.Control.Weapon.HolsterInfo.LeftHolster = HolsterSlot::Empty;
	}
	else
	{
		item->Model.MeshIndex[LM_LHAND] = item->Model.BaseMesh + LM_LHAND;
		player.Control.Weapon.HolsterInfo.LeftHolster = GetWeaponHolsterSlot(LaraWeaponType::Pistol);
	}
}

void DrawRightPistol(ItemInfo* item)
{
	auto& player = GetLaraInfo(*item);

	if (item->Model.MeshIndex[LM_RHAND] == item->Model.BaseMesh + LM_RHAND)
	{
		item->Model.MeshIndex[LM_RHAND] = Objects[GetWeaponObjectMeshID(*item, LaraWeaponType::Pistol)].meshIndex + LM_RHAND;
		player.Control.Weapon.HolsterInfo.RightHolster = HolsterSlot::Empty;
	}
	else
	{
		item->Model.MeshIndex[LM_RHAND] = item->Model.BaseMesh + LM_RHAND;
		player.Control.Weapon.HolsterInfo.RightHolster = GetWeaponHolsterSlot(LaraWeaponType::Pistol);
	}
}

void ShootLeftGun(ItemInfo* item)
{
	auto& player = GetLaraInfo(*item);

	player.LeftArm.GunFlash = 3;
}

void ShootRightGun(ItemInfo* item)
{
	auto& player = GetLaraInfo(*item);

	player.RightArm.GunFlash = 3;
}

void LaraHandsFree(ItemInfo* item)
{
	auto& player = GetLaraInfo(*item);

	player.Control.HandStatus = HandStatus::Free;
}

void KillActiveBaddys(ItemInfo* item)
{
	if (NextItemActive != NO_VALUE)
	{
		short itemNumber = NextItemActive;

		do
		{
			auto* targetItem = &g_Level.Items[itemNumber];

			if (Objects[targetItem->ObjectNumber].intelligent)
			{
				targetItem->Status = ITEM_INVISIBLE;

				if (*(int*)&item != 0xABCDEF)
				{
					RemoveActiveItem(itemNumber);
					DisableEntityAI(itemNumber);
					targetItem->Flags |= IFLAG_INVISIBLE;
				}
			}

			itemNumber = targetItem->NextActive;
		} while (itemNumber != NO_VALUE);
	}

	FlipEffect = NO_VALUE;
}

void LaraLocationPad(ItemInfo* item)
{
	auto* lara = GetLaraInfo(item);

	FlipEffect = NO_VALUE;

	lara->Location = TriggerTimer;
	lara->LocationPad = TriggerTimer;
}

void LaraLocation(ItemInfo* item)
{
	auto* lara = GetLaraInfo(item);

	FlipEffect = NO_VALUE;

	lara->Location = TriggerTimer;
	if (lara->HighestLocation < TriggerTimer)
		lara->HighestLocation = TriggerTimer;
}

void ExplosionFX(ItemInfo* item)
{
	SoundEffect(SFX_TR4_EXPLOSION1, nullptr);
	Camera.bounce = -75;
	FlipEffect = NO_VALUE;
}

void SwapCrowbar(ItemInfo* item)
{
	if (item->Model.MeshIndex[LM_RHAND] == item->Model.BaseMesh + LM_RHAND)
	{
		item->Model.MeshIndex[LM_RHAND] = Objects[ID_LARA_CROWBAR_ANIM].meshIndex + LM_RHAND;
	}
	else 
	{
		item->Model.MeshIndex[LM_RHAND] = item->Model.BaseMesh + LM_RHAND;
	}
}

void ActivateKey(ItemInfo* item)
{
	KeyTriggerActive = 1;
}

void ActivateCamera(ItemInfo* item)
{
	KeyTriggerActive = 2;
}

void PoseidonSFX(ItemInfo* item)
{
	SoundEffect(SFX_TR4_WATER_FLUSHES, nullptr);
	FlipEffect = NO_VALUE;
}

void RubbleFX(ItemInfo* item)
{
	const auto itemList = FindAllItems(ID_EARTHQUAKE);

	if (itemList.size() > 0)
	{
		auto* eq = &g_Level.Items[itemList[0]];

		AddActiveItem(itemList[0]);
		eq->Status = ITEM_ACTIVE;
		eq->Flags |= IFLAG_ACTIVATION_MASK;
	}
	else
		Camera.bounce = -150;

	FlipEffect = NO_VALUE;
}

void PlaySoundEffect(ItemInfo* item)
{
	SoundEffect(TriggerTimer, nullptr);
	FlipEffect = NO_VALUE;
}

void FloorShake(ItemInfo* item)
{
	int x = abs(item->Pose.Position.x - Camera.pos.x);
	int y = abs(item->Pose.Position.y - Camera.pos.y);
	int z = abs(item->Pose.Position.z - Camera.pos.z);

	if (x < BLOCK(16) &&
		y < BLOCK(16) &&
		z < BLOCK(16))
	{
		Camera.bounce = 66 * ((pow(x, 2) + pow(y, 2) + pow(z, 2)) / CLICK(1) - pow(BLOCK(1), 2)) / pow(BLOCK(1), 2);
	}
}

void Turn180(ItemInfo* item)
{
	item->Pose.Orientation.x = -item->Pose.Orientation.x;
	item->Pose.Orientation.y += ANGLE(180.0f);
	item->Pose.Orientation.z = -item->Pose.Orientation.z;

	item->DisableInterpolation = true;
}

void FinishLevel(ItemInfo* item)
{
	NextLevel = CurrentLevel + 1;
	RequiredStartPos = TriggerTimer;
}

void VoidEffect(ItemInfo* item)
{

}

void DoFlipEffect(int number, ItemInfo* item)
{
	if (number != NO_VALUE && number < NUM_FLIPEFFECTS && effect_routines[number] != nullptr)
		effect_routines[number](item);
}
