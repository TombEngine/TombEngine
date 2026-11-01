#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/Animation/Animation.h"
#include "Game/Animation/Skeleton.h"
#include "Game/effects/hair.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_tests.h"
#include "Game/control/control.h"
#include "Game/spotcam.h"
#include "Game/camera.h"
#include "Game/collision/sphere.h"
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

// Player pose from the game skeleton, with the world matrix the player meshes and attachments are drawn with.
void Renderer::UpdateLaraAnimations(bool force)
{
	auto& rItem = _items[LaraItem->Index];
	rItem.ItemNumber = LaraItem->Index;

	if (!force && rItem.DoneAnimations)
		return;

	if (_moveableObjects.empty())
		return;

	_playerWorldMatrix = GetItemWorldMatrix(*LaraItem);
	rItem.World = _playerWorldMatrix;

	auto pose = SkeletonPose{};
	GetItemPose(*LaraItem, pose);
	std::copy(pose.Transforms.begin(), pose.Transforms.end(), rItem.AnimationTransforms);
	std::copy(pose.Orientations.begin(), pose.Orientations.end(), rItem.BoneOrientations);

	// Copy matrices in player object.
	auto& playerObject = *_moveableObjects[ID_LARA];
	for (int m = 0; m < NUM_LARA_MESHES; m++)
		playerObject.AnimationTransforms[m] = rItem.AnimationTransforms[m];

	// Copy meshswap indices.
	rItem.SkinIndex = LaraItem->Model.GetSkinGlobalIndex();
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

	_stObjects.Objects[0].World = item->InterpolatedWorld;
	ReflectMatrixOptionally(_stObjects.Objects[0].World);

	_stObjects.Objects[0].Color = item->Color;
	_stObjects.Objects[0].AmbientLight = item->AmbientLight;
	_stObjects.Skinned = (int)skinMode;

	for (int k = 0; k < item->MeshIndex.size(); k++)
		_stObjects.BoneLightModes[k] = (int)GetMesh(item->MeshIndex[k])->LightMode;

	bool acceptsShadows = laraObj.ShadowType == ShadowMode::None;
	BindMoveableLights(item->LightsToDraw, item->RoomNumber, item->PrevRoomNumber, item->LightFade, acceptsShadows);

	if (skinMode == SkinningMode::Full)
	{
		for (int m = 0; m < laraObj.AnimationTransforms.size(); m++)
			_stObjects.Bones[m] =  laraObj.BindPoseTransforms[m] * item->InterpolatedAnimationTransforms[m];
        UpdateConstantBuffer(&_stObjects, _cbObjects.get(), GetObjectsBufferPrefixSize(1));

		DrawMesh(item, GetMesh(item->SkinIndex), RendererObjectType::Moveable, 0, true, view, rendererPass);
	}

	memcpy(_stObjects.Bones, item->InterpolatedAnimationTransforms, laraObj.AnimationTransforms.size() * sizeof(Matrix));
    UpdateConstantBuffer(&_stObjects, _cbObjects.get(), GetObjectsBufferPrefixSize(1));

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
		bool flipped = skinned || GetJointOffset(unit.ObjectID, 1, false).z < 0.0f;
		auto objectType = i ? RendererObjectType::HairSecondary : RendererObjectType::HairPrimary;

		const auto& rendererObject = *_moveableObjects[unit.ObjectID];

		_stObjects.Objects[0].World = Matrix::Identity;
		_stObjects.Bones[0] = itemToDraw->InterpolatedAnimationTransforms[HairUnit::GetRootMeshID(i)] * itemToDraw->InterpolatedWorld;
		_stObjects.Skinned = (int)(skinned ? SkinningMode::Full : SkinningMode::None);

		ReflectMatrixOptionally(_stObjects.Bones[0]);

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
			_stObjects.Bones[j + 1] = segment.GlobalTransform = worldMatrix;

			_stObjects.BoneLightModes[j] = (int)LightMode::Dynamic;
		}

        UpdateConstantBuffer(&_stObjects, _cbObjects.get(), GetObjectsBufferPrefixSize(1));

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
	if (!_moveableObjects[Lara.Skin.SkinJoints].has_value())
		return;

	RendererObject& laraSkinJoints = *_moveableObjects[Lara.Skin.SkinJoints];

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

	if (_moveableObjects[(int)leftHolsterID])
	{
		RendererObject& holsterSkin = *_moveableObjects[(int)leftHolsterID];
		RendererMesh* mesh = holsterSkin.ObjectMeshes[LM_LTHIGH];
		DrawMesh(itemToDraw, mesh, RendererObjectType::Moveable, LM_LTHIGH, false, view, rendererPass);
	}

	if (_moveableObjects[(int)rightHolsterID])
	{
		RendererObject& holsterSkin = *_moveableObjects[(int)rightHolsterID];
		RendererMesh* mesh = holsterSkin.ObjectMeshes[LM_RTHIGH];
		DrawMesh(itemToDraw, mesh, RendererObjectType::Moveable, LM_RTHIGH, false, view, rendererPass);
	}

	if (backHolsterID != HolsterSlot::Empty && _moveableObjects[(int)backHolsterID])
	{
		RendererObject& holsterSkin = *_moveableObjects[(int)backHolsterID];
		RendererMesh* mesh = holsterSkin.ObjectMeshes[LM_TORSO];
		DrawMesh(itemToDraw, mesh, RendererObjectType::Moveable, LM_TORSO, false, view, rendererPass);
	}
}
