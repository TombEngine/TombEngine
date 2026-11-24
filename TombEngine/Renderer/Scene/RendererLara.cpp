#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/Animation/Animation.h"
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

// Player rendering: collection of the sorted faces of the player, skin joints, holsters and hair.

// Collects the sorted faces of the player: skin, meshes, skin joints, holsters and hair.
void Renderer::CollectSortedPlayerFaces(RenderView& view)
{
	// TODO: Avoid Lara global.
	// Don't draw player if using optics.
	if (Lara.Control.Look.OpticRange != 0)
		return;

	// The renderer item must already be collected this frame (the player room is forced into the view by CollectRooms).
	auto* item = &_items[LaraItem->Index];
	auto* nativeItem = &g_Level.Items[item->ItemNumber];

	if (nativeItem->Flags & IFLAG_INVISIBLE)
		return;

	// Full skinning draws the single skin mesh plus the non-hidden meshes; classic mode draws the segmented meshes plus joint meshes.
	auto& laraObj = *_moveableObjects[ID_LARA];
	auto skinMode = GetSkinningMode(laraObj, item->SkinIndex);

	RendererRoom* room = &_rooms[LaraItem->RoomNumber];

	if (skinMode == SkinningMode::Full)
		CollectSortedMeshFaces(item, GetMesh(item->SkinIndex), RendererObjectType::Moveable, 0, true, view);

	for (int k = 0; k < item->MeshIndex.size(); k++)
	{
		if (!nativeItem->MeshBits.Test(k))
			continue;

		if (skinMode == SkinningMode::Full && g_Level.Meshes[nativeItem->Model.MeshIndex[k]].hidden)
			continue;

		CollectSortedMeshFaces(item, GetMesh(item->MeshIndex[k]), RendererObjectType::Moveable, k, false, view);
	}

	if (skinMode == SkinningMode::Classic)
		CollectSortedPlayerJointFaces(item, room, view);

	CollectSortedPlayerHolsterFaces(item, room, view);
	CollectSortedPlayerHairFaces(item, room, view);
}

// Updates the interpolated segment transforms of each hair unit and collects its sorted faces.
void Renderer::CollectSortedPlayerHairFaces(RendererItem* itemToDraw, RendererRoom* room, RenderView& view)
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

		// Hair units are separate objects posed by the hair simulation: the segment transforms are interpolated here, not taken from the pose cache.
		bool skinned = object.skinIndex != NO_VALUE && g_GameFlow->GetSettings()->Graphics.Skinning;
		bool flipped = skinned || GetJointOffset(unit.ObjectID, 1, false).z < 0.0f;
		auto objectType = i ? RendererObjectType::HairSecondary : RendererObjectType::HairPrimary;

		const auto& rendererObject = *_moveableObjects[unit.ObjectID];

		for (int j = 0; j < unit.Segments.size(); j++)
		{
			auto& segment = unit.Segments[j];

			// A large jump (teleport) disables interpolation for the segment.
			if ((segment.Position - segment.PrevPosition).Length() > CLICK(2))
				segment.StoreInterpolationData();

			auto worldMatrix = 
				Matrix::CreateFromQuaternion(
					Quaternion::Lerp(segment.PrevOrientation, segment.Orientation, GetInterpolationFactor(forceValue))) *
				Matrix::CreateTranslation(
					Vector3::Lerp(segment.PrevPosition, segment.Position, GetInterpolationFactor(forceValue)));

			if (flipped)
				worldMatrix = Matrix::CreateRotationY(PI) * worldMatrix;

			segment.GlobalTransform = worldMatrix;
		}

		if (skinned)
		{
			CollectSortedMeshFaces(itemToDraw, GetMesh(object.skinIndex), objectType, 0, true, view);
		}
		else
		{
			for (int j = 0; j < rendererObject.ObjectMeshes.size(); j++)
			{
				auto& rendererMesh = *rendererObject.ObjectMeshes[j];
				CollectSortedMeshFaces(itemToDraw, &rendererMesh, objectType, j, false, view);
			}
		}
	}
}

// Classic skinning only: the joint meshes (index 1 and up) fill the seams between the segment meshes of the player.
void Renderer::CollectSortedPlayerJointFaces(RendererItem* itemToDraw, RendererRoom* room, RenderView& view)
{
	if (!_moveableObjects[Lara.Skin.SkinJoints].has_value())
		return;

	RendererObject& laraSkinJoints = *_moveableObjects[Lara.Skin.SkinJoints];

	for (int k = 1; k < laraSkinJoints.ObjectMeshes.size(); k++)
	{
		RendererMesh* mesh = laraSkinJoints.ObjectMeshes[k];
		CollectSortedMeshFaces(itemToDraw, mesh, RendererObjectType::Moveable, k, false, view);
	}
}

// Holster meshes are taken from the holster objects at the thigh and torso bones.
void Renderer::CollectSortedPlayerHolsterFaces(RendererItem* itemToDraw, RendererRoom* room, RenderView& view)
{
	auto leftHolsterID  = Lara.Control.Weapon.HolsterInfo.LeftHolster;
	auto rightHolsterID = Lara.Control.Weapon.HolsterInfo.RightHolster;
	auto backHolsterID  = Lara.Control.Weapon.HolsterInfo.BackHolster;

	if (_moveableObjects[(int)leftHolsterID])
	{
		RendererObject& holsterSkin = *_moveableObjects[(int)leftHolsterID];
		RendererMesh* mesh = holsterSkin.ObjectMeshes[LM_LTHIGH];
		CollectSortedMeshFaces(itemToDraw, mesh, RendererObjectType::Moveable, LM_LTHIGH, false, view);
	}

	if (_moveableObjects[(int)rightHolsterID])
	{
		RendererObject& holsterSkin = *_moveableObjects[(int)rightHolsterID];
		RendererMesh* mesh = holsterSkin.ObjectMeshes[LM_RTHIGH];
		CollectSortedMeshFaces(itemToDraw, mesh, RendererObjectType::Moveable, LM_RTHIGH, false, view);
	}

	if (backHolsterID != HolsterSlot::Empty && _moveableObjects[(int)backHolsterID])
	{
		RendererObject& holsterSkin = *_moveableObjects[(int)backHolsterID];
		RendererMesh* mesh = holsterSkin.ObjectMeshes[LM_TORSO];
		CollectSortedMeshFaces(itemToDraw, mesh, RendererObjectType::Moveable, LM_TORSO, false, view);
	}
}
