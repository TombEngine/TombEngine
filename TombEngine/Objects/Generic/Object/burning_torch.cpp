#include "framework.h"
#include "Objects/Generic/Object/burning_torch.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/collide_item.h"
#include "Game/control/los.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_flare.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Objects/Effects/flame_emitters.h"
#include "Renderer/RendererEnums.h"
#include "Sound/sound.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Entities::Effects;
using namespace TEN::Input;

namespace TEN::Entities::Generic
{
	void TriggerTorchFlame(int fxObject, unsigned char node)
	{
		auto* spark = GetFreeParticle();

		spark->on = true;

		spark->sR = 1.0f * UCHAR_MAX;
		spark->sB = 0.2f * UCHAR_MAX;
		spark->sG = Random::GenerateFloat(0.2f, 0.3f) * UCHAR_MAX;
		spark->dR = Random::GenerateFloat(-0.25f, 0.0f) * UCHAR_MAX;
		spark->dB = 0.1f * UCHAR_MAX;
		spark->dG =
		spark->dG = Random::GenerateFloat(-0.5f, -0.25f) * UCHAR_MAX;

		spark->fadeToBlack = 8;
		spark->colFadeSpeed = Random::GenerateInt(12, 15);
		spark->blendMode = BlendMode::Additive;
		spark->life =
		spark->sLife = Random::GenerateInt(24, 31);

		spark->x = Random::GenerateInt(-8, 8);
		spark->y = 0;
		spark->z = Random::GenerateInt(-8, 8);

		spark->xVel = Random::GenerateInt(-128, 128);
		spark->yVel = Random::GenerateInt(-31, -16);
		spark->zVel = Random::GenerateInt(-128, 128);

		spark->friction = 5;

		spark->flags = SP_NODEATTACH | SP_EXPDEF | SP_ITEM | SP_ROTATE | SP_DEF | SP_SCALE;

		spark->blendMode = BlendMode::Additive;

		if (Random::TestProbability(1 / 2.0f))
			spark->rotAdd = Random::GenerateFloat(-0.16f / 127.0f, 0.0f) * SCHAR_MAX;
		else
			spark->rotAdd = Random::GenerateFloat(0.0f, 0.16f) * SCHAR_MAX;

		spark->gravity = Random::GenerateInt (-31, -16);
		spark->nodeNumber = node;
		spark->maxYvel = Random::GenerateFloat(-0.16f, 0.0f) * SCHAR_MAX;
		spark->fxObj = fxObject;
		spark->scalar = 1;
		spark->sSize =
		spark->size = Random::GenerateFloat(64, 150);
		spark->dSize = spark->size / 8;

		int spriteOffset = GlobalCounter % Objects[ID_FIRE_SPRITES].nmeshes;
		spark->SpriteSeqID = ID_FIRE_SPRITES;
		spark->SpriteID = spriteOffset;
	}

	void DoFlameTorch()
	{
		constexpr auto HOLD_ANIM_NUMBER	 = 0;
		constexpr auto THROW_ANIM_NUMBER = 1;
		constexpr auto DROP_ANIM_NUMBER	 = 2;

		auto* laraItem = LaraItem;
		auto* lara = GetLaraInfo(laraItem);
		auto& animObject = Objects[ID_LARA_TORCH_ANIM];

		if (lara->Torch.State == TorchState::Holding)
		{
			if (lara->Control.Weapon.RequestGunType != lara->Control.Weapon.GunType)
			{
				lara->LeftArm.Locked = true;
				lara->LeftArm.FrameNumber = 31;
				lara->LeftArm.AnimNumber = DROP_ANIM_NUMBER;
				lara->Torch.State = TorchState::Dropping;
			}
			else if (IsHeld(In::Draw) &&
				!laraItem->Animation.IsAirborne &&
				laraItem->Animation.Velocity.y == 0.0f &&
				laraItem->Animation.ActiveState != LS_JUMP_PREPARE &&
				laraItem->Animation.ActiveState != LS_JUMP_UP &&
				laraItem->Animation.ActiveState != LS_JUMP_FORWARD &&
				laraItem->Animation.ActiveState != LS_JUMP_BACK &&
				laraItem->Animation.ActiveState != LS_JUMP_LEFT &&
				laraItem->Animation.ActiveState != LS_JUMP_RIGHT ||
				lara->Control.WaterStatus == WaterStatus::TreadWater ||
				lara->Control.WaterStatus == WaterStatus::Underwater)
			{
				lara->LeftArm.Locked = true;
				lara->LeftArm.FrameNumber = 1;
				lara->LeftArm.AnimNumber = THROW_ANIM_NUMBER;
				lara->Torch.State = TorchState::Throwing;

				if (lara->Control.WaterStatus == WaterStatus::Underwater)
					lara->Torch.IsLit = false;
			}
		}
		else if (lara->Torch.State == TorchState::Throwing)
		{
			if (lara->LeftArm.FrameNumber < 12 && laraItem->Animation.IsAirborne)
			{
				lara->LeftArm.Locked = false;
				lara->LeftArm.FrameNumber = 0;
				lara->LeftArm.AnimNumber = HOLD_ANIM_NUMBER;
				lara->Torch.State = TorchState::Holding;
			}
			else
			{
				lara->LeftArm.FrameNumber++;
				if (lara->LeftArm.FrameNumber == 27)
				{
					lara->Flare.ControlLeft = false;
					lara->LeftArm.Locked = false;
					lara->Torch.State = TorchState::Holding;
					lara->Control.Weapon.GunType = lara->Control.Weapon.LastGunType;
					lara->Control.Weapon.RequestGunType = LaraWeaponType::None;
					lara->Control.HandStatus = HandStatus::Free;
				}
				else if (lara->LeftArm.FrameNumber == 12)
				{
					laraItem->Model.MeshIndex[LM_LHAND] = laraItem->Model.BaseMesh + LM_LHAND;
					CreateFlare(*laraItem, ID_BURNING_TORCH_ITEM, true);
					lara->Torch.IsLit = false;
				}
			}
		}
		else if (lara->Torch.State == TorchState::Dropping)
		{
			lara->LeftArm.FrameNumber++;
			if (lara->LeftArm.FrameNumber == 41)
			{
				lara->Flare.ControlLeft = false;
				lara->LeftArm.Locked = false;
				lara->Torch.State = TorchState::Holding;
				lara->Control.Weapon.LastGunType = LaraWeaponType::None;
				lara->Control.Weapon.GunType = LaraWeaponType::None;
				lara->Control.HandStatus = HandStatus::Free;
			}
			else if (lara->LeftArm.FrameNumber == 36)
			{
				laraItem->Model.MeshIndex[LM_LHAND] = laraItem->Model.BaseMesh + LM_LHAND;
				CreateFlare(*laraItem, ID_BURNING_TORCH_ITEM, false);
				lara->Torch.IsLit = false;
			}
		}
		else if (lara->Torch.State == TorchState::JustLit)
		{
			if (laraItem->Animation.ActiveState != LS_MISC_CONTROL)
			{
				lara->LeftArm.Locked = false;
				lara->Torch.State = TorchState::Holding;
				lara->LeftArm.FrameNumber = 0;
				lara->Flare.ControlLeft = true;
				lara->Torch.IsLit = laraItem->ItemFlags[3] & 1;
				lara->LeftArm.AnimNumber = HOLD_ANIM_NUMBER;
			}
		}

		if (lara->Flare.ControlLeft)
			lara->Control.HandStatus = HandStatus::WeaponReady;

		lara->LeftArm.AnimObjectID = ID_LARA_TORCH_ANIM;

		if (lara->Torch.IsLit)
		{
			auto pos = GetJointPosition(laraItem, LM_LHAND, Vector3i(-32, 64, 256));
			auto lightColor = Color(
				Random::GenerateFloat(0.75f, 1.0f),
				Random::GenerateFloat(0.4f, 0.5f),
				0.0f);
			float lightFalloff = Random::GenerateFloat(0.04f, 0.045f);
			SpawnDynamicLight(pos.x, pos.y, pos.z, lightFalloff * UCHAR_MAX, lightColor.R() * UCHAR_MAX, lightColor.G() * UCHAR_MAX, lightColor.B() * UCHAR_MAX);

			if (!(Wibble & 3))
				TriggerTorchFlame(laraItem->Index, 0);

			SoundEffect(SFX_TR4_LOOP_FOR_SMALL_FIRES, &Pose(pos));
		}
	}

	void GetFlameTorch()
	{
		auto* laraItem = LaraItem;
		auto* lara = GetLaraInfo(laraItem);
		auto& animObject = Objects[ID_LARA_TORCH_ANIM];

		if (lara->Control.Weapon.GunType == LaraWeaponType::Flare)
			CreateFlare(*laraItem, ID_FLARE_ITEM, false);

		lara->Control.HandStatus = HandStatus::WeaponReady;
		lara->Control.Weapon.RequestGunType = LaraWeaponType::Torch;
		lara->Control.Weapon.GunType = LaraWeaponType::Torch;
		lara->Flare.ControlLeft = true;
		lara->LeftArm.Locked = false;
		lara->LeftArm.AnimObjectID = ID_LARA_TORCH_ANIM;
		lara->LeftArm.AnimNumber = 0;
		lara->LeftArm.FrameNumber = 0;
		laraItem->Model.MeshIndex[LM_LHAND] = animObject.meshIndex + LM_LHAND;
	}

	void TorchControl(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (item->Animation.Velocity.y)
		{
			item->Pose.Orientation.x -= ANGLE(5.0f);
			item->Pose.Orientation.z += ANGLE(5.0f);
		}
		else if (!item->Animation.Velocity.z)
		{
			item->Pose.Orientation.x = 0;
			item->Pose.Orientation.z = 0;
		}

		auto vel = Vector3i(
			item->Animation.Velocity.z * phd_sin(item->Pose.Orientation.y),
			item->Animation.Velocity.y,
			item->Animation.Velocity.z * phd_cos(item->Pose.Orientation.y));

		auto prevPos = item->Pose.Position;
		item->Pose.Position += Vector3i(vel.x, 0, vel.z);

		if (TestEnvironment(ENV_FLAG_WATER, item) ||
			TestEnvironment(ENV_FLAG_SWAMP, item))
		{
			item->Animation.Velocity.y += (5 - item->Animation.Velocity.y) / 2;
			item->Animation.Velocity.z += (5 - item->Animation.Velocity.z) / 2;

			if (item->ItemFlags[3] != 0)
				item->ItemFlags[3] = 0;
		}
		else
		{
			item->Animation.Velocity.y += g_GameFlow->GetSettings()->Physics.Gravity;
		}

		item->Pose.Position.y += item->Animation.Velocity.y;
		DoProjectileDynamics(itemNumber, prevPos.x, prevPos.y, prevPos.z, vel.x, vel.y, vel.z);

		// Collide with entities.
		auto collObjects = GetCollidedObjects(*item, true, true);
		if (!collObjects.IsEmpty())
		{
			LaraCollision.Setup.EnableObjectPush = true;
			if (!collObjects.Items.empty())
			{
				const auto& object = Objects[collObjects.Items.front()->ObjectNumber];

				if (!object.intelligent &&
					!collObjects.Items.front()->IsLara())
				{
					ObjectCollision(collObjects.Items.front()->Index, item, &LaraCollision);
				}
			}
			else if (!collObjects.Statics.empty())
			{
				ItemPushStatic(item, *collObjects.Statics.front(), &LaraCollision);
			}
			
			item->Animation.Velocity.z = -int(item->Animation.Velocity.z / 1.5f);
		}

		if (item->ItemFlags[3])
		{
			auto lightColor = Color(
				Random::GenerateFloat(0.75f, 1.0f),
				Random::GenerateFloat(0.4f, 0.5f),
				0.0f);
			float lightFalloff = Random::GenerateFloat(0.04f, 0.045f);
			SpawnDynamicLight(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, lightFalloff * UCHAR_MAX, lightColor.R() * UCHAR_MAX, lightColor.G() * UCHAR_MAX, lightColor.B() * UCHAR_MAX);
			
			if (!(Wibble & 7))
				TriggerTorchFlame(itemNumber, 1);

			SoundEffect(SFX_TR4_LOOP_FOR_SMALL_FIRES, &item->Pose);
		}
	}

	void FireCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* torchItem = &g_Level.Items[itemNumber];
		auto* lara = GetLaraInfo(laraItem);

		if (!IsHeld(In::Action) ||
			laraItem->Animation.ActiveState != LS_IDLE ||
			laraItem->Animation.AnimNumber != LA_STAND_IDLE ||
			laraItem->Animation.IsAirborne ||
			lara->Control.Weapon.GunType != LaraWeaponType::Torch ||
			lara->Control.HandStatus != HandStatus::WeaponReady ||
			lara->LeftArm.Locked ||
			lara->Torch.IsLit == (torchItem->Status == ITEM_ACTIVE) ||
			torchItem->Timer == -1)
		{
			if (torchItem->ObjectNumber == ID_BURNING_ROOTS)
				ObjectCollision(itemNumber, laraItem, coll);
		}
		else
		{
			short yOrient = torchItem->Pose.Orientation.y;

			switch (torchItem->ObjectNumber)
			{
			case ID_FLAME_EMITTER:
				FireBounds.BoundingBox.X1 = -256;
				FireBounds.BoundingBox.X2 = 256;
				FireBounds.BoundingBox.Y1 = 0;
				FireBounds.BoundingBox.Y2 = 1024;
				FireBounds.BoundingBox.Z1 = -800;
				FireBounds.BoundingBox.Z2 = 800;
				break;

			case ID_FLAME_EMITTER2:
				FireBounds.BoundingBox.X1 = -256;
				FireBounds.BoundingBox.X2 = 256;
				FireBounds.BoundingBox.Y1 = 0;
				FireBounds.BoundingBox.Y2 = 1024;
				FireBounds.BoundingBox.Z1 = -600;
				FireBounds.BoundingBox.Z2 = 600;
				break;

			case ID_BURNING_ROOTS:
				FireBounds.BoundingBox.X1 = -384;
				FireBounds.BoundingBox.X2 = 384;
				FireBounds.BoundingBox.Y1 = 0;
				FireBounds.BoundingBox.Y2 = 2048;
				FireBounds.BoundingBox.Z1 = -384;
				FireBounds.BoundingBox.Z2 = 384;
				break;
			}

			torchItem->Pose.Orientation.y = laraItem->Pose.Orientation.y;

			if (TestLaraPosition(FireBounds, torchItem, laraItem))
			{
				if (torchItem->ObjectNumber == ID_BURNING_ROOTS)
					laraItem->Animation.AnimNumber = LA_TORCH_LIGHT_5;
				else
				{
					int dy = abs(laraItem->Pose.Position.y - torchItem->Pose.Position.y);
					laraItem->ItemFlags[3] = 1;
					laraItem->Animation.AnimNumber = (dy >> 8) + LA_TORCH_LIGHT_1;
				}

				laraItem->Animation.ActiveState = LS_MISC_CONTROL;
				laraItem->Animation.FrameNumber = 0;
				lara->Flare.ControlLeft = false;
				lara->LeftArm.Locked = true;
				lara->Context.InteractedItem = itemNumber;
			}

			torchItem->Pose.Orientation.y = yOrient;
		}
		if (laraItem->Animation.ActiveState == LS_MISC_CONTROL &&
			lara->Context.InteractedItem == itemNumber &&
			torchItem->Status != ITEM_ACTIVE)
		{
			if (laraItem->Animation.AnimNumber >= LA_TORCH_LIGHT_1 &&
				laraItem->Animation.AnimNumber <= LA_TORCH_LIGHT_5)
			{
				if (laraItem->Animation.FrameNumber == 40)
				{
					TestTriggers(torchItem, true, torchItem->Flags & IFLAG_ACTIVATION_MASK);
					torchItem->Flags |= CODE_BITS;
					torchItem->ItemFlags[3] = 0;
					torchItem->Status = ITEM_ACTIVE;
					AddActiveItem(itemNumber);
				}
			}
		}
	}
}
