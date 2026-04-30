#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/Animation/Animation.h"
#include "Game/effects/Hair.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_tests.h"
#include "Game/control/control.h"
#include "Game/spotcam.h"
#include "Game/camera.h"
#include "Game/collision/Sphere.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Effects::Hair;
using namespace TEN::Math;
using namespace TEN::Renderer;

extern ScriptInterfaceFlowHandler *g_GameFlow;

bool ShouldAnimateUpperBody(const LaraWeaponType& weapon)
{
	const auto& nativeItem = *LaraItem;
	auto& player = Lara;

	switch (weapon)
	{
	case LaraWeaponType::RocketLauncher:
	case LaraWeaponType::HarpoonGun:
	case LaraWeaponType::GrenadeLauncher:
	case LaraWeaponType::Crossbow:
	case LaraWeaponType::Shotgun:
		if (nativeItem.Animation.ActiveState == LS_IDLE ||
			nativeItem.Animation.ActiveState == LS_TURN_LEFT_FAST ||
			nativeItem.Animation.ActiveState == LS_TURN_RIGHT_FAST ||
			nativeItem.Animation.ActiveState == LS_TURN_LEFT_SLOW ||
			nativeItem.Animation.ActiveState == LS_TURN_RIGHT_SLOW)
		{
			return true;
		}

		return false;

	case LaraWeaponType::HK:
	{
		// Animate upper body if player is shooting from shoulder or standing still/turning.
		if (player.RightArm.AnimNumber == 0 ||
			player.RightArm.AnimNumber == 2 ||
			player.RightArm.AnimNumber == 4)
		{
			return true;
		}
		else
		{
			if (nativeItem.Animation.ActiveState == LS_IDLE ||
				nativeItem.Animation.ActiveState == LS_TURN_LEFT_FAST ||
				nativeItem.Animation.ActiveState == LS_TURN_RIGHT_FAST ||
				nativeItem.Animation.ActiveState == LS_TURN_LEFT_SLOW ||
				nativeItem.Animation.ActiveState == LS_TURN_RIGHT_SLOW)
			{
				return true;
			}

			return false;
		}
	}

	break;

	default:
		return false;
		break;
	}
}

// HACK: Arm frames for pistols, uzis, and revolver currently remain absolute.
// Until the weapon system is rewritten from scratch, this will ensure correct behaviour. -- Sezz 2023.11.13
static int GetNormalizedArmAnimFrame(GAME_OBJECT_ID animObjectID, int frameNumber)
{
	int frameCount = 0;
	int animCount = (int)Objects[animObjectID].Animations.size();

	for (int i = 0; i < animCount; i++)
	{
		const auto& anim = GetAnimData(animObjectID, i);
		
		int currentAnimFrameCount = (int)anim.Keyframes.size();
		int nextFrameCount = (frameCount + currentAnimFrameCount);

		if (frameNumber < nextFrameCount)
			return frameNumber - frameCount;

		if (i == (animCount - 1) && frameNumber >= nextFrameCount)
			return currentAnimFrameCount - 1;

		frameCount = nextFrameCount;
	}

	return 0;
}

void Renderer::UpdateLaraAnimations(bool force)
{
	auto& rItem = _items[LaraItem->Index];
	rItem.ItemNumber = LaraItem->Index;

	if (!force && rItem.DoneAnimations)
		return;

	if (_moveableObjects.empty())
		return;

	auto& playerObject = *_moveableObjects[ID_LARA];

	// Clear extra rotations.
	for (auto* bonePtr : playerObject.LinearizedBones)
		bonePtr->ExtraRotation = Quaternion::Identity;

	// Set player world matrix.
	_playerWorldMatrix = LaraItem->Pose.ToMatrix();
	rItem.World = _playerWorldMatrix;

	// Update extra head and torso rotations.
	playerObject.LinearizedBones[LM_TORSO]->ExtraRotation = Lara.ExtraTorsoRot.ToQuaternion();
	playerObject.LinearizedBones[LM_HEAD]->ExtraRotation = Lara.ExtraHeadRot.ToQuaternion();

	// First calculate matrices for legs, hips, head, and torso.
	int mask = MESH_BITS(LM_HIPS) | MESH_BITS(LM_LTHIGH) | MESH_BITS(LM_LSHIN) | MESH_BITS(LM_LFOOT) | MESH_BITS(LM_RTHIGH) |
			   MESH_BITS(LM_RSHIN) | MESH_BITS(LM_RFOOT) | MESH_BITS(LM_TORSO) | MESH_BITS(LM_HEAD);
	
	auto frameData = GetFrameInterpData(*LaraItem);
	UpdateAnimation(&rItem, playerObject, frameData, mask);

	auto gunType = Lara.Control.Weapon.GunType;
	auto handStatus = Lara.Control.HandStatus;

	// HACK: Treat binoculars as two-handed weapon.
	if (Lara.Control.Look.IsUsingBinoculars)
	{
		gunType = LaraWeaponType::Shotgun;
		handStatus = HandStatus::WeaponReady;
	}

	// Then the arms, based on current weapon status.
	if (gunType != LaraWeaponType::Flare && (handStatus == HandStatus::Free || handStatus == HandStatus::Busy) ||
		gunType == LaraWeaponType::Flare && !Lara.Flare.ControlLeft)
	{
		// Both arms
		mask = MESH_BITS(LM_LINARM) | MESH_BITS(LM_LOUTARM) | MESH_BITS(LM_LHAND) | MESH_BITS(LM_RINARM) | MESH_BITS(LM_ROUTARM) | MESH_BITS(LM_RHAND);
		auto frameData = GetFrameInterpData(*LaraItem);
		UpdateAnimation(&rItem, playerObject, frameData, mask);
	}
	else
	{
		// While handling weapon, extra rotation may be applied to arms.
		if (gunType == LaraWeaponType::Pistol || gunType == LaraWeaponType::Uzi)
		{
			playerObject.LinearizedBones[LM_LINARM]->ExtraRotation *= Lara.LeftArm.Orientation.ToQuaternion();
			playerObject.LinearizedBones[LM_RINARM]->ExtraRotation *= Lara.RightArm.Orientation.ToQuaternion();
		}
		else if (gunType == LaraWeaponType::Revolver)
		{
			playerObject.LinearizedBones[LM_LINARM]->ExtraRotation =
			playerObject.LinearizedBones[LM_RINARM]->ExtraRotation *= Lara.LeftArm.Orientation.ToQuaternion();
		}
		else
		{
			playerObject.LinearizedBones[LM_LINARM]->ExtraRotation =
			playerObject.LinearizedBones[LM_RINARM]->ExtraRotation *= Lara.RightArm.Orientation.ToQuaternion();
		}

		// HACK: Back guns are handled differently.
		switch (gunType)
		{
		case LaraWeaponType::Shotgun:
		case LaraWeaponType::HK:
		case LaraWeaponType::Crossbow:
		case LaraWeaponType::GrenadeLauncher:
		case LaraWeaponType::RocketLauncher:
		case LaraWeaponType::HarpoonGun:
		{
			// Left arm.
			mask = MESH_BITS(LM_LINARM) | MESH_BITS(LM_LOUTARM) | MESH_BITS(LM_LHAND);

			if (ShouldAnimateUpperBody(gunType))
				mask |= MESH_BITS(LM_TORSO) | MESH_BITS(LM_HEAD);

			const auto& leftArmAnim = GetAnimData(Lara.LeftArm.AnimObjectID, Lara.LeftArm.AnimNumber);
			const auto& frameLeft = leftArmAnim.GetKeyframeInterpolationData(Lara.LeftArm.FrameNumber).Keyframe0;
			auto interpDataLeft = KeyframeInterpolationData(frameLeft, frameLeft, 0.0f);
			UpdateAnimation(&rItem, playerObject, interpDataLeft, mask);

			// Right arm.
			mask = MESH_BITS(LM_RINARM) | MESH_BITS(LM_ROUTARM) | MESH_BITS(LM_RHAND);
			if (ShouldAnimateUpperBody(Lara.Control.Weapon.GunType))
				mask |= MESH_BITS(LM_TORSO) | MESH_BITS(LM_HEAD);

			const auto& rightArmAnim = GetAnimData(Lara.RightArm.AnimObjectID, Lara.RightArm.AnimNumber);
			const auto& frameRight = rightArmAnim.GetKeyframeInterpolationData(Lara.RightArm.FrameNumber).Keyframe0;
			auto interpDataRight = KeyframeInterpolationData(frameRight, frameRight, 0.0f);
			UpdateAnimation(&rItem, playerObject, interpDataRight, mask);
		}
		break;

		case LaraWeaponType::Pistol:
		case LaraWeaponType::Uzi:
		case LaraWeaponType::Revolver:
		default:
		{
			// Left arm.
			bool movingModifier = !(gunType == LaraWeaponType::Revolver && LaraItem->Animation.Velocity.Length() < EPSILON) && Lara.LeftArm.FrameNumber;
			bool sideJumpModifier = !(gunType == LaraWeaponType::Revolver && IsSideJumpState(LaraItem->Animation.ActiveState));

			// HACK: Revolver is a special case because its right/left arm orientations aren't symmetrical and get messed up while moving.
			bool transformLeftUpperArm = (IsCrouching(LaraItem) || Lara.LeftArm.Locked || movingModifier) && sideJumpModifier;

			auto leftFrameNumber = GetNormalizedArmAnimFrame(Lara.LeftArm.AnimObjectID, Lara.LeftArm.FrameNumber);
			const auto& leftAnim = GetAnimData(Lara.LeftArm.AnimObjectID, Lara.LeftArm.AnimNumber);
			auto leftFrame = leftAnim.GetKeyframeInterpolationData(leftFrameNumber).Keyframe0;

			int upperArmMask = MESH_BITS(LM_LINARM);
			mask = MESH_BITS(LM_LOUTARM) | MESH_BITS(LM_LHAND);
			auto interpDataLeft = KeyframeInterpolationData(leftFrame, leftFrame, 0.0f);

			if (transformLeftUpperArm)
			{
				UpdateAnimation(&rItem, playerObject, interpDataLeft, upperArmMask, true);
			}
			else
			{
				mask |= MESH_BITS(LM_LINARM);
			}

			UpdateAnimation(&rItem, playerObject, interpDataLeft, mask);

			// Right arm.
			movingModifier = !(gunType == LaraWeaponType::Revolver && LaraItem->Animation.Velocity.Length() < EPSILON) && Lara.RightArm.FrameNumber;

			// HACK: Same as above, but for right arm.
			bool transformRightUpperArm = IsCrouching(LaraItem) || Lara.RightArm.Locked || movingModifier;

			auto rightFrameNumber = GetNormalizedArmAnimFrame(Lara.RightArm.AnimObjectID, Lara.RightArm.FrameNumber);
			const auto& rightAnim = GetAnimData(Lara.RightArm.AnimObjectID, Lara.RightArm.AnimNumber);
			auto rightFrame = rightAnim.GetKeyframeInterpolationData(rightFrameNumber).Keyframe0;

			upperArmMask = MESH_BITS(LM_RINARM);
			mask = MESH_BITS(LM_ROUTARM) | MESH_BITS(LM_RHAND);
			auto interpDataRight = KeyframeInterpolationData(rightFrame, rightFrame, 0.0f);

			if (transformRightUpperArm)
			{
				UpdateAnimation(&rItem, playerObject, interpDataRight, upperArmMask, true);
			}
			else
			{
				mask |= MESH_BITS(LM_RINARM);
			}

			UpdateAnimation(&rItem, playerObject, interpDataRight, mask);
		}
		break;

		case LaraWeaponType::Flare:
		case LaraWeaponType::Torch:
		{
			// Left arm.
			auto leftFrameNumber = GetNormalizedArmAnimFrame(Lara.LeftArm.AnimObjectID, Lara.LeftArm.FrameNumber);
			const auto& leftAnim = GetAnimData(Lara.LeftArm.AnimObjectID, Lara.LeftArm.AnimNumber);
			auto leftFrame = leftAnim.GetKeyframeInterpolationData(leftFrameNumber).Keyframe0;

			mask = MESH_BITS(LM_LINARM) | MESH_BITS(LM_LOUTARM) | MESH_BITS(LM_LHAND);

			// HACK: Mask head and torso when taking out flare.
			if (!Lara.Control.IsLow &&
				Lara.LeftArm.AnimNumber > 1 &&
				Lara.LeftArm.AnimNumber < 4)
			{
				mask |= MESH_BITS(LM_TORSO) | MESH_BITS(LM_HEAD);
			}

			auto interpDataLeft = KeyframeInterpolationData(leftFrame, leftFrame, 0.0f);
			UpdateAnimation(&rItem, playerObject, interpDataLeft, mask);

			// Right arm.
			mask = MESH_BITS(LM_RINARM) | MESH_BITS(LM_ROUTARM) | MESH_BITS(LM_RHAND);
			auto frameDataRight = GetFrameInterpData(*LaraItem);
			UpdateAnimation(&rItem, playerObject, frameDataRight, mask);

		}
		break;
		}
	}

	// Copy matrices in player object.
	for (int m = 0; m < NUM_LARA_MESHES; m++)
		playerObject.AnimationTransforms[m] = rItem.AnimTransforms[m];

	// Copy meshswap indices.
	rItem.SkinIndex = LaraItem->Model.SkinIndex;
	rItem.MeshIndex = LaraItem->Model.MeshIndex;
	rItem.DoneAnimations = true;
}

void Renderer::DrawLara(RenderView& view, RendererPass rendererPass)
{
	// TODO: Avoid Lara global.
	// Don't draw player if using optics (but still draw reflections).
	if (Lara.Control.Look.OpticRange != 0 && _currentMirror == nullptr)
		return;

	auto* item = &_items[LaraItem->Index];
	auto* nativeItem = &g_Level.Items[item->ItemNumber];

	if (nativeItem->Flags & IFLAG_INVISIBLE)
		return;

	_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
	_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

	auto& laraObj = *_moveableObjects[ID_LARA];
	auto skinMode = GetSkinningMode(laraObj, item->SkinIndex);

	RendererRoom* room = &_rooms[LaraItem->RoomNumber];

	_stItem.World = item->InterpolatedWorld;
	ReflectMatrixOptionally(_stItem.World);

	_stItem.Color = item->Color;
	_stItem.AmbientLight = item->AmbientLight;
	_stItem.Skinned = (int)skinMode;

	for (int k = 0; k < item->MeshIndex.size(); k++)
		_stItem.BoneLightModes[k] = (int)GetMesh(item->MeshIndex[k])->LightMode;

	bool acceptsShadows = laraObj.ShadowType == ShadowMode::None;
	BindMoveableLights(item->LightsToDraw, item->RoomNumber, item->PrevRoomNumber, item->LightFade, acceptsShadows);

	if (skinMode == SkinningMode::Full)
	{
		for (int m = 0; m < laraObj.AnimationTransforms.size(); m++)
			_stItem.BonesMatrices[m] =  laraObj.BindPoseTransforms[m] * item->InterpolatedAnimTransforms[m];
		UpdateConstantBuffer(&_stItem, _cbItem.get());

		DrawMesh(item, GetMesh(item->SkinIndex), RendererObjectType::Moveable, 0, true, view, rendererPass);
	}

	memcpy(_stItem.BonesMatrices, item->InterpolatedAnimTransforms, laraObj.AnimationTransforms.size() * sizeof(Matrix));
	UpdateConstantBuffer(&_stItem, _cbItem.get());

	for (int k = 0; k < item->MeshIndex.size(); k++)
	{
		if (!nativeItem->MeshBits.Test(k))
			continue;

		if (skinMode == SkinningMode::Full && g_Level.Meshes[nativeItem->Model.MeshIndex[k]].hidden)
			continue;

		DrawMesh(item, GetMesh(item->MeshIndex[k]), RendererObjectType::Moveable, k, false, view, rendererPass);
	}

	if (skinMode == SkinningMode::Classic)
		DrawLaraJoints(item, room, view, rendererPass);

	DrawLaraHolsters(item, room, view, rendererPass);
	DrawLaraHair(item, room, view, rendererPass);
}

void Renderer::DrawLaraHair(RendererItem* itemToDraw, RendererRoom* room, RenderView& view, RendererPass rendererPass)
{
	bool forceValue = g_GameFlow->CurrentFreezeMode == FreezeMode::Player;

	for (int i = 0; i < HairEffect.Units.size(); i++)
	{
		auto& unit = HairEffect.Units[i];
		if (!unit.IsEnabled)
			continue;

		const auto& object = Objects[unit.ObjectID];
		if (!object.loaded)
			continue;

		bool skinned = object.skinIndex != NO_VALUE && g_GameFlow->GetSettings()->Graphics.Skinning;
		bool flipped = skinned || (GetJointOffset(unit.ObjectID, 1, false).z < 0);
		auto objectType = i ? RendererObjectType::HairSecondary : RendererObjectType::HairPrimary;

		const auto& rendererObject = *_moveableObjects[unit.ObjectID];

		_stItem.World = Matrix::Identity;
		_stItem.BonesMatrices[0] = itemToDraw->InterpolatedAnimTransforms[HairUnit::GetRootMeshID(i)] * itemToDraw->InterpolatedWorld;
		_stItem.Skinned = (int)skinned;

		ReflectMatrixOptionally(_stItem.BonesMatrices[0]);

		for (int j = 0; j < unit.Segments.size(); j++)
		{
			auto& segment = unit.Segments[j];

			if ((segment.Position - segment.PrevPosition).Length() > CLICK(2))
				segment.StoreInterpolationData();

			auto worldMatrix = 
				Matrix::CreateFromQuaternion(
					Quaternion::Lerp(segment.PrevOrientation, segment.Orientation, GetInterpolationFactor(forceValue))) *
				Matrix::CreateTranslation(
					Vector3::Lerp(segment.PrevPosition, segment.Position, GetInterpolationFactor(forceValue)));

			if (flipped)
				worldMatrix = Matrix::CreateRotationY(PI) * worldMatrix;

			ReflectMatrixOptionally(worldMatrix);
			_stItem.BonesMatrices[j + 1] = segment.GlobalTransform = worldMatrix;

			_stItem.BoneLightModes[j] = (int)LightMode::Dynamic;
		}

		UpdateConstantBuffer(&_stItem, _cbItem.get());

		if (skinned)
		{
			DrawMesh(itemToDraw, GetMesh(object.skinIndex), objectType, 0, true, view, rendererPass);
		}
		else
		{
			for (int j = 0; j < rendererObject.ObjectMeshes.size(); j++)
			{
				auto& rendererMesh = *rendererObject.ObjectMeshes[j];
				DrawMesh(itemToDraw, &rendererMesh, objectType, j, false, view, rendererPass);
			}
		}
	}
}

void Renderer::DrawLaraJoints(RendererItem* itemToDraw, RendererRoom* room, RenderView& view, RendererPass rendererPass)
{
	if (!_moveableObjects[ID_LARA_SKIN_JOINTS].has_value())
		return;

	RendererObject& laraSkinJoints = *_moveableObjects[ID_LARA_SKIN_JOINTS];

	for (int k = 1; k < laraSkinJoints.ObjectMeshes.size(); k++)
	{
		RendererMesh* mesh = laraSkinJoints.ObjectMeshes[k];
		DrawMesh(itemToDraw, mesh, RendererObjectType::Moveable, k, false, view, rendererPass);
	}
}

void Renderer::DrawLaraHolsters(RendererItem* itemToDraw, RendererRoom* room, RenderView& view, RendererPass rendererPass)
{
	auto leftHolsterID  = Lara.Control.Weapon.HolsterInfo.LeftHolster;
	auto rightHolsterID = Lara.Control.Weapon.HolsterInfo.RightHolster;
	auto backHolsterID  = Lara.Control.Weapon.HolsterInfo.BackHolster;

	if (_moveableObjects[static_cast<int>(leftHolsterID)])
	{
		RendererObject& holsterSkin = *_moveableObjects[static_cast<int>(leftHolsterID)];
		RendererMesh* mesh = holsterSkin.ObjectMeshes[LM_LTHIGH];
		DrawMesh(itemToDraw, mesh, RendererObjectType::Moveable, LM_LTHIGH, false, view, rendererPass);
	}

	if (_moveableObjects[static_cast<int>(rightHolsterID)])
	{
		RendererObject& holsterSkin = *_moveableObjects[static_cast<int>(rightHolsterID)];
		RendererMesh* mesh = holsterSkin.ObjectMeshes[LM_RTHIGH];
		DrawMesh(itemToDraw, mesh, RendererObjectType::Moveable, LM_RTHIGH, false, view, rendererPass);
	}

	if (backHolsterID != HolsterSlot::Empty && _moveableObjects[static_cast<int>(backHolsterID)])
	{
		RendererObject& holsterSkin = *_moveableObjects[static_cast<int>(backHolsterID)];
		RendererMesh* mesh = holsterSkin.ObjectMeshes[LM_TORSO];
		DrawMesh(itemToDraw, mesh, RendererObjectType::Moveable, LM_TORSO, false, view, rendererPass);
	}
}