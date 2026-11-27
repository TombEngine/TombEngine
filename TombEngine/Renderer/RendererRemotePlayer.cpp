#include "framework.h"
#include "Renderer/Renderer.h"

#include <array>
#include <exception>
#include <functional>
#include <vector>

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/debug/debug.h"
#include "Game/items.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara_tests.h"
#include "Game/Multiplayer/Multiplayer.h"
#include "Game/Multiplayer/RemoteHair.h"
#include "Game/Multiplayer/RemoteItemPool.h"
#include "Game/Multiplayer/RemotePlayer.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Math;
using namespace TEN::Multiplayer;

int GetNormalizedArmAnimFrame(GAME_OBJECT_ID animObjectID, int frameNumber);

namespace TEN::Renderer
{
	struct RemotePlayerFrameData
	{
		int CurrentReceivedFrameNumber = 0;
	};

	// Last frame number received for each pool slot, to step the hair once per received frame.
	static std::array<RemotePlayerFrameData, MAX_REMOTE_PLAYERS> s_remotePlayerFrames;

	// Renderer indices of the remote players collected for the current frame.
	static std::vector<int> s_remotePlayersToDraw;

	static bool ShouldAnimateRemoteUpperBody(const LaraWeaponType& weapon, const ItemInfo& item, const LaraInfo& remoteInfo)
	{
		switch (weapon)
		{
		case LaraWeaponType::RocketLauncher:
		case LaraWeaponType::HarpoonGun:
		case LaraWeaponType::GrenadeLauncher:
		case LaraWeaponType::Crossbow:
		case LaraWeaponType::Shotgun:
			return (item.Animation.ActiveState == LS_IDLE ||
					item.Animation.ActiveState == LS_TURN_LEFT_FAST ||
					item.Animation.ActiveState == LS_TURN_RIGHT_FAST ||
					item.Animation.ActiveState == LS_TURN_LEFT_SLOW ||
					item.Animation.ActiveState == LS_TURN_RIGHT_SLOW);

		case LaraWeaponType::HK:
			if (remoteInfo.RightArm.AnimNumber == 0 ||
				remoteInfo.RightArm.AnimNumber == 2 ||
				remoteInfo.RightArm.AnimNumber == 4)
			{
				return true;
			}

			return (item.Animation.ActiveState == LS_IDLE ||
					item.Animation.ActiveState == LS_TURN_LEFT_FAST ||
					item.Animation.ActiveState == LS_TURN_RIGHT_FAST ||
					item.Animation.ActiveState == LS_TURN_LEFT_SLOW ||
					item.Animation.ActiveState == LS_TURN_RIGHT_SLOW);

		default:
			return false;
		}
	}

	void Renderer::CollectRemotePlayers(RenderView& renderView)
	{
		s_remotePlayersToDraw.clear();

		if (!IsRemotePlayerEnabled())
			return;

		if (!_moveableObjects[ID_LARA].has_value())
			return;

		for (int i = 0; i < MAX_REMOTE_PLAYERS; i++)
		{
			if (!g_RemoteItemsOccupied[i])
				continue;

			auto* item = &g_RemoteItems[i];
			if (!item->Active || item->Status == ITEM_INVISIBLE)
				continue;

			if (item->Model.Color.w < EPSILON)
				continue;

			int rendererIndex = item->Index;
			if (rendererIndex < 0 || rendererIndex >= (int)_items.size())
				continue;

			if (item->RoomNumber == NO_VALUE || item->RoomNumber >= (short)_rooms.size())
				continue;

			// Resets the hair step when the slot gets a new allocation.
			static int lastDrawnSerial[MAX_REMOTE_PLAYERS] = {};
			static int lastHairTick[MAX_REMOTE_PLAYERS] = {};
			if (lastDrawnSerial[i] != g_RemoteItemSerial[i])
			{
				lastDrawnSerial[i] = g_RemoteItemSerial[i];
				lastHairTick[i] = NO_VALUE;
			}

			auto& newItem = _items[rendererIndex];

			newItem.ItemNumber = rendererIndex;
			newItem.ObjectID = item->ObjectNumber;
			newItem.Color = item->Model.Color;

			auto newPosition = item->Pose.Position.ToVector3();
			auto newRotation = item->Pose.Orientation.ToRotationMatrix();
			auto newScale = Matrix::CreateScale(item->Pose.Scale);
			auto newTranslation = Matrix::CreateTranslation(newPosition);
			auto newWorld = newScale * newRotation * newTranslation;

			bool forceSnap = item->DisableInterpolation || newItem.DisableInterpolation;
			bool posJumped = Vector3::Distance(newItem.PrevPosition, newPosition) > BLOCK(1);
			bool posChanged = (newPosition != newItem.Position);

			auto& frameData = s_remotePlayerFrames[i];

			if (forceSnap || posJumped)
			{
				newItem.PrevPosition = newPosition;
				newItem.PrevTranslation = newTranslation;
				newItem.PrevRotation = newRotation;
				newItem.PrevScale = newScale;
				newItem.PrevWorld = newWorld;

				// A pool item is not reset by the control loop, so the renderer clears both flags. -- divezzi, 20.11.26
				newItem.DisableInterpolation = false;
				item->DisableInterpolation = false;

				frameData.CurrentReceivedFrameNumber = item->Animation.FrameNumber;
			}
			else if (posChanged)
			{
				// A new network position arrived: the previous one becomes the interpolation origin.
				newItem.PrevPosition = newItem.Position;
				newItem.PrevTranslation = newItem.Translation;
				newItem.PrevRotation = newItem.Rotation;
				newItem.PrevScale = newItem.Scale;
				newItem.PrevWorld = newItem.World;
			}

			int currentFrame = item->Animation.FrameNumber;
			bool isFrameAdvanced = (currentFrame != frameData.CurrentReceivedFrameNumber);
			if (isFrameAdvanced)
				frameData.CurrentReceivedFrameNumber = currentFrame;

			// The hair steps once per game tick, as the hair of the local player does, so its previous and current positions stay one tick apart
			// while the frames in between are drawn. A step tied to network updates made it move in jumps while the camera was smooth.
			bool isHairStep = (lastHairTick[i] != GlobalCounter);
			lastHairTick[i] = GlobalCounter;

			newItem.Position = newPosition;
			newItem.Translation = newTranslation;
			newItem.Rotation = newRotation;
			newItem.Scale = newScale;
			newItem.World = newWorld;

			float interpFactor = GetInterpolationFactor(false);

			try
			{
				auto& remoteInfo = GetRemotePlayerInfo((short)rendererIndex);

				newItem.DoneAnimations = false;
				UpdateRemotePlayerAnimations(item, remoteInfo);

				// The hair reads the head bone, so it steps right after the animations.
				if (isHairStep)
					GetRemoteHairController(*item).Update(*item, remoteInfo);
			}
			catch (const std::exception& ex)
			{
				TENLog(std::string("[REMOTE] Skipping remote player in slot ") + std::to_string(i) + ": " + ex.what(), LogLevel::Error);
				continue;
			}

			// The first frame after a spawn or a teleport has no previous bones to blend from.
			if (forceSnap || posJumped)
			{
				for (int j = 0; j < BONE_COUNT_MAX; j++)
					newItem.PrevAnimationTransforms[j] = newItem.AnimationTransforms[j];
			}

			newItem.InterpolatedPosition = Vector3::Lerp(newItem.PrevPosition, newItem.Position, interpFactor);
			newItem.InterpolatedTranslation = Matrix::Lerp(newItem.PrevTranslation, newItem.Translation, interpFactor);
			newItem.InterpolatedRotation = Matrix::Lerp(newItem.PrevRotation, newItem.Rotation, interpFactor);
			newItem.InterpolatedScale = Matrix::Lerp(newItem.PrevScale, newItem.Scale, interpFactor);
			newItem.InterpolatedWorld = Matrix::Lerp(newItem.PrevWorld, newItem.World, interpFactor);

			// The bones blend over the same tick as the pose (SaveOldState() stores the previous ones), so the two never run on different clocks.
			for (int j = 0; j < BONE_COUNT_MAX; j++)
				newItem.InterpolatedAnimationTransforms[j] = Matrix::Lerp(newItem.PrevAnimationTransforms[j], newItem.AnimationTransforms[j], interpFactor);

			CalculateLightFades(&newItem);
			CollectLightsForItem(&newItem);

			s_remotePlayersToDraw.push_back(rendererIndex);
		}
	}

	// The vehicles of remote players are drawn by the generic item path (DrawAnimatingItem()), so they only need the collection step that room items get.
	void Renderer::CollectRemoteVehicles(RenderView& renderView)
	{
		if (!IsRemotePlayerEnabled())
			return;

		for (int i = MAX_REMOTE_PLAYERS; i < MAX_REMOTE_ITEMS; i++)
		{
			if (!g_RemoteItemsOccupied[i])
				continue;

			auto* item = &g_RemoteItems[i];
			if (!item->Active || item->Status == ITEM_INVISIBLE || item->Model.Color.w < EPSILON)
				continue;

			if (!_moveableObjects[item->ObjectNumber].has_value())
				continue;

			int rendererIndex = item->Index;
			if (rendererIndex < 0 || rendererIndex >= (int)_items.size())
				continue;

			// Rooms outside the view are skipped, as the draw loops only walk the rooms in view.
			short roomNumber = item->RoomNumber;
			if (roomNumber == NO_VALUE || roomNumber >= (short)_rooms.size() || !_rooms[roomNumber].Visited)
				continue;

			auto& newItem = _items[rendererIndex];

			newItem.ItemNumber = rendererIndex;
			newItem.ObjectID = item->ObjectNumber;
			newItem.Color = item->Model.Color;

			auto newPosition = item->Pose.Position.ToVector3();
			auto newRotation = item->Pose.Orientation.ToRotationMatrix();
			auto newScale = Matrix::CreateScale(item->Pose.Scale);
			auto newTranslation = Matrix::CreateTranslation(newPosition);
			auto newWorld = newScale * newRotation * newTranslation;

			bool forceSnap = item->DisableInterpolation || newItem.DisableInterpolation;
			bool posJumped = Vector3::Distance(newItem.PrevPosition, newPosition) > BLOCK(1);
			bool poseChanged = (newPosition != newItem.Position) || (newRotation != newItem.Rotation);

			if (forceSnap || posJumped)
			{
				newItem.PrevPosition = newPosition;
				newItem.PrevTranslation = newTranslation;
				newItem.PrevRotation = newRotation;
				newItem.PrevScale = newScale;
				newItem.PrevWorld = newWorld;

				// A pool item is not reset by the control loop, so the renderer clears both flags.
				newItem.DisableInterpolation = false;
				item->DisableInterpolation = false;
			}
			else if (poseChanged)
			{
				// A new network pose arrived: the previous one becomes the interpolation origin.
				newItem.PrevPosition = newItem.Position;
				newItem.PrevTranslation = newItem.Translation;
				newItem.PrevRotation = newItem.Rotation;
				newItem.PrevScale = newItem.Scale;
				newItem.PrevWorld = newItem.World;
			}

			newItem.Position = newPosition;
			newItem.Translation = newTranslation;
			newItem.Rotation = newRotation;
			newItem.Scale = newScale;
			newItem.World = newWorld;

			float interpFactor = GetInterpolationFactor(false);

			newItem.InterpolatedPosition = Vector3::Lerp(newItem.PrevPosition, newItem.Position, interpFactor);
			newItem.InterpolatedTranslation = Matrix::Lerp(newItem.PrevTranslation, newItem.Translation, interpFactor);
			newItem.InterpolatedRotation = Matrix::Lerp(newItem.PrevRotation, newItem.Rotation, interpFactor);
			newItem.InterpolatedScale = Matrix::Lerp(newItem.PrevScale, newItem.Scale, interpFactor);
			newItem.InterpolatedWorld = Matrix::Lerp(newItem.PrevWorld, newItem.World, interpFactor);

			// The bones (wheel, steering and propeller rotations) follow the latest received values and are not blended.
			UpdateItemAnimations(rendererIndex, true);
			for (int j = 0; j < BONE_COUNT_MAX; j++)
				newItem.InterpolatedAnimationTransforms[j] = newItem.AnimationTransforms[j];

			CalculateLightFades(&newItem);
			CollectLightsForItem(&newItem);

			_rooms[roomNumber].ItemsToDraw.push_back(&newItem);
		}
	}

	void Renderer::DrawRemotePlayers(RenderView& view, RendererPass rendererPass)
	{
		if (s_remotePlayersToDraw.empty())
			return;

		for (auto* room : view.RoomsToDraw)
		{
			if (IgnoreReflectionPassForRoom(room->RoomNumber))
				continue;

			for (int rendererIndex : s_remotePlayersToDraw)
			{
				const auto* nativeItem = GetItemByRendererIndex(rendererIndex);
				if (nativeItem == nullptr || nativeItem->RoomNumber != room->RoomNumber)
					continue;

				try
				{
					DrawRemotePlayer(view, &_items[rendererIndex], GetRemotePlayerInfo((short)rendererIndex), rendererPass);
				}
				catch (const std::exception& ex)
				{
					TENLog(std::string("[REMOTE] Cannot draw remote player at renderer index ") + std::to_string(rendererIndex) + ": " + ex.what(), LogLevel::Error);
				}
			}
		}
	}

	void Renderer::UpdateRemotePlayerAnimations(ItemInfo* item, LaraInfo& remoteInfo)
	{
		auto& rItem = _items[item->Index];
		rItem.ItemNumber = item->Index;

		if (rItem.DoneAnimations)
			return;

		if (_moveableObjects.empty())
			return;

		GAME_OBJECT_ID animSlot = item->Animation.AnimObjectID;
		if (animSlot < 0 || animSlot >= ID_NUMBER_OBJECTS || !_moveableObjects[animSlot].has_value())
			animSlot = ID_LARA;

		auto& playerObject = *_moveableObjects[animSlot];

		// NOTE: The bone rotations of this object are shared with the local player. They are saved here and restored
		// at the end, and nothing reads them in between.
		auto& sharedBones = playerObject.LinearizedBones;
		auto savedExtraRotations = std::vector<Quaternion>(sharedBones.size());
		for (int i = 0; i < (int)sharedBones.size(); i++)
			savedExtraRotations[i] = sharedBones[i]->ExtraRotation;

		for (auto& bone : sharedBones)
			bone->ExtraRotation = Quaternion::Identity;

		// The world matrix of the local player must not be overwritten, so the remote player uses its own.
		rItem.World = item->Pose.ToMatrix();

		playerObject.LinearizedBones[LM_TORSO]->ExtraRotation = remoteInfo.ExtraTorsoRot.ToQuaternion();
		playerObject.LinearizedBones[LM_HEAD]->ExtraRotation = remoteInfo.ExtraHeadRot.ToQuaternion();

		int mask = MESH_BITS(LM_HIPS) | MESH_BITS(LM_LTHIGH) | MESH_BITS(LM_LSHIN) | MESH_BITS(LM_LFOOT) | MESH_BITS(LM_RTHIGH) | MESH_BITS(LM_RSHIN) | MESH_BITS(LM_RFOOT) | MESH_BITS(LM_TORSO) | MESH_BITS(LM_HEAD);

		const auto& frameData = GetFrame(*item);
		UpdateAnimation(&rItem, playerObject, frameData, mask);

		auto gunType = remoteInfo.Control.Weapon.GunType;
		auto handStatus = remoteInfo.Control.HandStatus;

		if (remoteInfo.Control.Look.IsUsingBinoculars)
		{
			gunType = LaraWeaponType::Shotgun;
			handStatus = HandStatus::WeaponReady;
		}

		if (gunType != LaraWeaponType::Flare && (handStatus == HandStatus::Free || handStatus == HandStatus::Busy) ||
			gunType == LaraWeaponType::Flare && !remoteInfo.Flare.ControlLeft)
		{
			mask = MESH_BITS(LM_LINARM) | MESH_BITS(LM_LOUTARM) | MESH_BITS(LM_LHAND) | MESH_BITS(LM_RINARM) | MESH_BITS(LM_ROUTARM) | MESH_BITS(LM_RHAND);
			UpdateAnimation(&rItem, playerObject, frameData, mask);
		}
		else
		{
			// The arm animation object is not sent over the network, so it is taken from the weapon type.
			auto armAnimObjectID = GetWeaponObjectID(gunType);

			if (gunType == LaraWeaponType::Pistol || gunType == LaraWeaponType::Uzi)
			{
				playerObject.LinearizedBones[LM_LINARM]->ExtraRotation *= remoteInfo.LeftArm.Orientation.ToQuaternion();
				playerObject.LinearizedBones[LM_RINARM]->ExtraRotation *= remoteInfo.RightArm.Orientation.ToQuaternion();
			}
			else
			{
				playerObject.LinearizedBones[LM_LINARM]->ExtraRotation =
				playerObject.LinearizedBones[LM_RINARM]->ExtraRotation *= remoteInfo.RightArm.Orientation.ToQuaternion();
			}

			if (IsCrouching(item) && gunType == LaraWeaponType::Revolver)
				gunType = LaraWeaponType::Pistol;

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
				if (ShouldAnimateRemoteUpperBody(gunType, *item, remoteInfo))
					mask |= MESH_BITS(LM_TORSO) | MESH_BITS(LM_HEAD);

				const auto& leftArmAnim = GetAnimData(armAnimObjectID, remoteInfo.LeftArm.AnimNumber);
				const auto& leftArmFrame = leftArmAnim.Frames[remoteInfo.LeftArm.FrameNumber];
				UpdateAnimation(&rItem, playerObject, leftArmFrame, mask);

				// Right arm.
				mask = MESH_BITS(LM_RINARM) | MESH_BITS(LM_ROUTARM) | MESH_BITS(LM_RHAND);
				if (ShouldAnimateRemoteUpperBody(remoteInfo.Control.Weapon.GunType, *item, remoteInfo))
					mask |= MESH_BITS(LM_TORSO) | MESH_BITS(LM_HEAD);

				const auto& rightArmAnim = GetAnimData(armAnimObjectID, remoteInfo.RightArm.AnimNumber);
				const auto& rightArmFrame = rightArmAnim.Frames[remoteInfo.RightArm.FrameNumber];
				UpdateAnimation(&rItem, playerObject, rightArmFrame, mask);
			}
			break;

			case LaraWeaponType::Revolver:
			{
				mask = MESH_BITS(LM_LINARM) | MESH_BITS(LM_LOUTARM) | MESH_BITS(LM_LHAND);
				int leftFrameNumber = GetNormalizedArmAnimFrame(armAnimObjectID, remoteInfo.LeftArm.FrameNumber);
				const auto& leftArmAnim = GetAnimData(armAnimObjectID, remoteInfo.LeftArm.AnimNumber);
				UpdateAnimation(&rItem, playerObject, leftArmAnim.Frames[leftFrameNumber], mask);

				mask = MESH_BITS(LM_RINARM) | MESH_BITS(LM_ROUTARM) | MESH_BITS(LM_RHAND);
				int rightFrameNumber = GetNormalizedArmAnimFrame(armAnimObjectID, remoteInfo.RightArm.FrameNumber);
				const auto& rightArmAnim = GetAnimData(armAnimObjectID, remoteInfo.RightArm.AnimNumber);
				UpdateAnimation(&rItem, playerObject, rightArmAnim.Frames[rightFrameNumber], mask);
			}
			break;

			case LaraWeaponType::Pistol:
			case LaraWeaponType::Uzi:
			default:
			{
				// Left arm.
				int leftFrameNumber = GetNormalizedArmAnimFrame(armAnimObjectID, remoteInfo.LeftArm.FrameNumber);
				const auto& leftArmAnim = GetAnimData(armAnimObjectID, remoteInfo.LeftArm.AnimNumber);
				const auto& leftFrame = leftArmAnim.Frames[leftFrameNumber];

				int upperArmMask = MESH_BITS(LM_LINARM);
				mask = MESH_BITS(LM_LOUTARM) | MESH_BITS(LM_LHAND);
				UpdateAnimation(&rItem, playerObject, leftFrame, upperArmMask, true);
				UpdateAnimation(&rItem, playerObject, leftFrame, mask);

				// Right arm.
				int rightFrameNumber = GetNormalizedArmAnimFrame(armAnimObjectID, remoteInfo.RightArm.FrameNumber);
				const auto& rightArmAnim = GetAnimData(armAnimObjectID, remoteInfo.RightArm.AnimNumber);
				const auto& rightFrame = rightArmAnim.Frames[rightFrameNumber];

				upperArmMask = MESH_BITS(LM_RINARM);
				mask = MESH_BITS(LM_ROUTARM) | MESH_BITS(LM_RHAND);
				UpdateAnimation(&rItem, playerObject, rightFrame, upperArmMask, true);
				UpdateAnimation(&rItem, playerObject, rightFrame, mask);
			}
			break;

			case LaraWeaponType::Flare:
			case LaraWeaponType::Torch:
			{
				// Left arm.
				int leftFrameNumber = GetNormalizedArmAnimFrame(armAnimObjectID, remoteInfo.LeftArm.FrameNumber);
				const auto& leftArmAnim = GetAnimData(armAnimObjectID, remoteInfo.LeftArm.AnimNumber);
				const auto& leftFrame = leftArmAnim.Frames[leftFrameNumber];

				mask = MESH_BITS(LM_LINARM) | MESH_BITS(LM_LOUTARM) | MESH_BITS(LM_LHAND);

				// The head and torso are masked while the flare is taken out.
				if (!remoteInfo.Control.IsLow &&
					remoteInfo.LeftArm.AnimNumber > 1 &&
					remoteInfo.LeftArm.AnimNumber < 4)
				{
					mask |= MESH_BITS(LM_TORSO) | MESH_BITS(LM_HEAD);
				}

				UpdateAnimation(&rItem, playerObject, leftFrame, mask);

				// The right arm follows the body animation.
				mask = MESH_BITS(LM_RINARM) | MESH_BITS(LM_ROUTARM) | MESH_BITS(LM_RHAND);
				UpdateAnimation(&rItem, playerObject, frameData, mask);
			}
			break;
			}
		}

		rItem.SkinIndex = item->Model.GetSkinGlobalIndex();
		rItem.MeshIndex = item->Model.MeshIndex;

		for (int i = 0; i < (int)sharedBones.size(); i++)
			sharedBones[i]->ExtraRotation = savedExtraRotations[i];

		rItem.DoneAnimations = true;
	}

	void Renderer::DrawRemotePlayer(RenderView& view, RendererItem* itemToDraw, LaraInfo& remoteInfo, RendererPass rendererPass)
	{
		if (remoteInfo.Control.Look.OpticRange != 0 && _currentMirror == nullptr)
			return;

		const auto* nativeItem = GetItemByRendererIndex(itemToDraw->ItemNumber);
		if (nativeItem == nullptr)
			return;

		if (nativeItem->Flags & IFLAG_INVISIBLE)
			return;

		_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
		_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

		auto& playerObject = *_moveableObjects[ID_LARA];
		auto skinMode = GetSkinningMode(playerObject, itemToDraw->SkinIndex);

		_stObjects.Objects[0].World = itemToDraw->InterpolatedWorld;
		ReflectMatrixOptionally(_stObjects.Objects[0].World);

		_stObjects.Objects[0].Color = itemToDraw->Color;
		_stObjects.Objects[0].AmbientLight = itemToDraw->AmbientLight;
		_stObjects.Skinned = (int)skinMode;

		for (int k = 0; k < itemToDraw->MeshIndex.size(); k++)
			_stObjects.BoneLightModes[k] = (int)GetMesh(itemToDraw->MeshIndex[k])->LightMode;

		bool acceptsShadows = playerObject.ShadowType == ShadowMode::None;
		BindMoveableLights(itemToDraw->LightsToDraw, itemToDraw->RoomNumber, itemToDraw->PrevRoomNumber, itemToDraw->LightFade, acceptsShadows);

		if (skinMode == SkinningMode::Full)
		{
			for (int m = 0; m < playerObject.AnimationTransforms.size(); m++)
				_stObjects.Bones[m] = playerObject.BindPoseTransforms[m] * itemToDraw->InterpolatedAnimationTransforms[m];

			UpdateConstantBuffer(&_stObjects, _cbObjects.get());
			DrawMesh(itemToDraw, GetMesh(itemToDraw->SkinIndex), RendererObjectType::Moveable, 0, true, view, rendererPass);
		}

		memcpy(_stObjects.Bones, itemToDraw->InterpolatedAnimationTransforms, playerObject.AnimationTransforms.size() * sizeof(Matrix));
		UpdateConstantBuffer(&_stObjects, _cbObjects.get());

		for (int k = 0; k < itemToDraw->MeshIndex.size(); k++)
		{
			if (!nativeItem->MeshBits.Test(k))
				continue;

			if (skinMode == SkinningMode::Full && g_Level.Meshes[nativeItem->Model.MeshIndex[k]].hidden)
				continue;

			DrawMesh(itemToDraw, GetMesh(itemToDraw->MeshIndex[k]), RendererObjectType::Moveable, k, false, view, rendererPass);
		}

		if (skinMode == SkinningMode::Classic)
			DrawRemotePlayerJoints(itemToDraw, view, rendererPass);

		DrawRemotePlayerHolsters(itemToDraw, view, remoteInfo, rendererPass);
		DrawRemotePlayerHair(itemToDraw, view, rendererPass);
	}

	bool Renderer::GetRemotePlayerBoneMatrix(int rendererIndex, int jointIndex, Matrix* outMatrix)
	{
		if (rendererIndex < 0 || rendererIndex >= (int)_items.size())
			return false;

		const auto& rendererItem = _items[rendererIndex];
		if (!rendererItem.DoneAnimations)
			return false;

		if (jointIndex >= BONE_COUNT_MAX)
			jointIndex = 0;

		*outMatrix = rendererItem.AnimationTransforms[jointIndex] * rendererItem.World;
		return true;
	}

	void Renderer::DrawRemotePlayerHolsters(RendererItem* itemToDraw, RenderView& view, LaraInfo& remoteInfo, RendererPass rendererPass)
	{
		auto leftHolsterID = remoteInfo.Control.Weapon.HolsterInfo.LeftHolster;
		auto rightHolsterID = remoteInfo.Control.Weapon.HolsterInfo.RightHolster;
		auto backHolsterID = remoteInfo.Control.Weapon.HolsterInfo.BackHolster;

		if (_moveableObjects[(int)leftHolsterID])
		{
			auto& holsterSkin = *_moveableObjects[(int)leftHolsterID];
			DrawMesh(itemToDraw, holsterSkin.ObjectMeshes[LM_LTHIGH], RendererObjectType::Moveable, LM_LTHIGH, false, view, rendererPass);
		}

		if (_moveableObjects[(int)rightHolsterID])
		{
			auto& holsterSkin = *_moveableObjects[(int)rightHolsterID];
			DrawMesh(itemToDraw, holsterSkin.ObjectMeshes[LM_RTHIGH], RendererObjectType::Moveable, LM_RTHIGH, false, view, rendererPass);
		}

		if (backHolsterID != HolsterSlot::Empty && _moveableObjects[(int)backHolsterID])
		{
			auto& holsterSkin = *_moveableObjects[(int)backHolsterID];
			DrawMesh(itemToDraw, holsterSkin.ObjectMeshes[LM_TORSO], RendererObjectType::Moveable, LM_TORSO, false, view, rendererPass);
		}
	}

	void Renderer::DrawRemotePlayerJoints(RendererItem* itemToDraw, RenderView& view, RendererPass rendererPass)
	{
		if (!_moveableObjects[ID_LARA_SKIN_JOINTS].has_value())
			return;

		auto& skinJoints = *_moveableObjects[ID_LARA_SKIN_JOINTS];
		for (int k = 1; k < skinJoints.ObjectMeshes.size(); k++)
			DrawMesh(itemToDraw, skinJoints.ObjectMeshes[k], RendererObjectType::Moveable, k, false, view, rendererPass);
	}

	// Computes the bones of a remote player outside the render loop, if they are not done yet. Returns false if that is not possible.
	bool Renderer::EnsureRemotePlayerAnimations(int rendererIndex, RendererItem& rendererItem)
	{
		if (rendererItem.DoneAnimations)
			return true;

		auto* item = GetItemByRendererIndex(rendererIndex);
		if (item == nullptr || !item->IsRemotePlayer())
			return false;

		try
		{
			UpdateRemotePlayerAnimations(item, GetRemotePlayerInfo((short)rendererIndex));
		}
		catch (const std::exception& ex)
		{
			TENLog(std::string("[REMOTE] Cannot update the bones of renderer index ") + std::to_string(rendererIndex) + ": " + ex.what(), LogLevel::Error);
			return false;
		}

		return rendererItem.DoneAnimations;
	}

	Vector3 Renderer::GetRemotePlayerBonePosition(int rendererIndex, int boneID, const Vector3& relOffset)
	{
		const auto* item = GetItemByRendererIndex(rendererIndex);
		if (item == nullptr || rendererIndex < 0 || rendererIndex >= (int)_items.size())
			return Vector3::Zero;

		auto& rendererItem = _items[rendererIndex];
		rendererItem.ItemNumber = rendererIndex;

		if (boneID < 0 || boneID >= BONE_COUNT_MAX || !EnsureRemotePlayerAnimations(rendererIndex, rendererItem))
			return Vector3::Transform(relOffset, item->Pose.ToMatrix());

		return Vector3::Transform(relOffset, rendererItem.AnimationTransforms[boneID] * rendererItem.World);
	}

	std::vector<BoundingSphere> Renderer::GetRemotePlayerSpheres(int rendererIndex)
	{
		auto spheres = std::vector<BoundingSphere>{};

		if (rendererIndex < 0 || rendererIndex >= (int)_items.size() || GetItemByRendererIndex(rendererIndex) == nullptr)
			return spheres;

		auto& rendererItem = _items[rendererIndex];
		rendererItem.ItemNumber = rendererIndex;

		if (!EnsureRemotePlayerAnimations(rendererIndex, rendererItem))
			return spheres;

		// The meshes are the ones of the remote player's own mesh list, as the pool slot has no renderer object.
		for (int i = 0; i < (int)rendererItem.MeshIndex.size() && i < BONE_COUNT_MAX; i++)
		{
			const auto& mesh = *GetMesh(rendererItem.MeshIndex[i]);
			auto pos = Vector3::Transform(mesh.Sphere.Center, rendererItem.AnimationTransforms[i] * rendererItem.World);
			spheres.push_back(BoundingSphere(pos, mesh.Sphere.Radius));
		}

		return spheres;
	}

	bool Renderer::DrawRemotePlayerGunFlashes(RenderView& view)
	{
		bool isAnyFlashDrawn = false;
		bool isStateBound = false;

		for (int rendererIndex : s_remotePlayersToDraw)
		{
			const auto* nativeItem = GetItemByRendererIndex(rendererIndex);
			if (nativeItem == nullptr)
				continue;

			const LaraInfo* remoteInfo = nullptr;
			try
			{
				remoteInfo = &GetRemotePlayerInfo((short)rendererIndex);
			}
			catch (const std::exception& ex)
			{
				TENLog(std::string("[REMOTE] Cannot draw gun flash at renderer index ") + std::to_string(rendererIndex) + ": " + ex.what(), LogLevel::Error);
				continue;
			}

			bool hasLeftFlash = remoteInfo->LeftArm.GunFlash;
			bool hasRightFlash = remoteInfo->RightArm.GunFlash;
			if (!hasLeftFlash && !hasRightFlash)
				continue;

			// The flash is hidden while the remote player looks through optics, as the player is.
			if (remoteInfo->Control.Look.OpticRange != 0 && _currentMirror == nullptr)
				continue;

			auto gunType = remoteInfo->Control.Weapon.GunType;
			if (gunType == LaraWeaponType::None || gunType == LaraWeaponType::Flare || (int)gunType >= (int)LaraWeaponType::NumWeapons)
				continue;

			const auto& settings = g_GameFlow->GetSettings()->Weapons[(int)gunType - 1];
			if (!settings.MuzzleFlash)
				continue;

			auto gunflash = (gunType == LaraWeaponType::HK && Objects[GAME_OBJECT_ID::ID_GUN_FLASH2].loaded) ?
				GAME_OBJECT_ID::ID_GUN_FLASH2 : GAME_OBJECT_ID::ID_GUN_FLASH;
			if (!Objects[gunflash].loaded || !_moveableObjects[gunflash].has_value())
				continue;

			if (rendererIndex < 0 || rendererIndex >= (int)_items.size() ||
				nativeItem->RoomNumber == NO_VALUE || nativeItem->RoomNumber >= (short)_rooms.size())
			{
				continue;
			}

			auto& rendererItem = _items[rendererIndex];
			const auto& room = _rooms[nativeItem->RoomNumber];

			if (!isStateBound)
			{
				_stObjects.Skinned = (int)SkinningMode::Static;

				_shaders.Bind(Shader::InstancedStatics);

				_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
				_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
				_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());

				SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);
				SetBlendMode(BlendMode::Additive);

				isStateBound = true;
			}

			_stObjects.Objects[0].AmbientLight = room.AmbientLight;
			_stObjects.Objects[0].LightMode = (int)LightMode::Static;
			BindInstancedStaticLights(rendererItem.LightsToDraw, 0);

			const auto& flashMesh = *_moveableObjects[gunflash]->ObjectMeshes[0];
			auto zRot = (gunflash == GAME_OBJECT_ID::ID_GUN_FLASH2) ? 0.0f : TO_RAD((short)std::hash<int>()(GlobalCounter));

			_stObjects.Objects[0].Color = settings.ColorizeMuzzleFlash ? settings.FlashColor : NEUTRAL_COLOR;

			for (const auto& flashBucket : flashMesh.Buckets)
			{
				if (flashBucket.BlendMode == BlendMode::Opaque || flashBucket.Polygons.size() == 0)
					continue;

				BindBucketTextures(flashBucket, TextureSource::Moveables, false);
				BindMaterial(flashBucket.MaterialIndex, false);

				for (int arm = 0; arm < 2; arm++)
				{
					bool isLeft = (arm == 0);
					if (isLeft ? !hasLeftFlash : !hasRightFlash)
						continue;

					// Offsets are inverted because of the bone orientation.
					auto meshOffset = Objects[gunflash].Animations.front().Frames.front().RootPosition;
					auto offset = settings.MuzzleOffset + Vector3(meshOffset.x, meshOffset.z, meshOffset.y);
					offset.x = isLeft ? -offset.x : offset.x;

					auto tMatrix = Matrix::CreateTranslation(offset);
					auto rotMatrix = Matrix::CreateRotationX(TO_RAD(gunType == LaraWeaponType::Pistol ? ANGLE(268) : ANGLE(280)));

					auto worldMatrix = rendererItem.InterpolatedAnimationTransforms[isLeft ? LM_LHAND : LM_RHAND] * rendererItem.InterpolatedWorld;
					worldMatrix = tMatrix * worldMatrix;
					worldMatrix = rotMatrix * worldMatrix;
					worldMatrix = Matrix::CreateRotationZ(zRot) * worldMatrix;
					ReflectMatrixOptionally(worldMatrix);

					_stObjects.Objects[0].World = worldMatrix;
					UpdateConstantBuffer(&_stObjects, _cbObjects.get(), GetObjectsBufferPrefixSize(1));

					DrawIndexedInstancedTriangles(flashBucket.NumIndices, 1, flashBucket.StartIndex, 0);

					_numMoveablesDrawCalls++;
					isAnyFlashDrawn = true;
				}
			}
		}

		if (isStateBound)
			SetBlendMode(BlendMode::Opaque);

		return isAnyFlashDrawn;
	}

	void Renderer::DrawRemotePlayerHair(RendererItem* itemToDraw, RenderView& view, RendererPass rendererPass)
	{
		const auto* nativeItem = GetItemByRendererIndex(itemToDraw->ItemNumber);
		if (nativeItem == nullptr)
			return;

		bool forceValue = (g_GameFlow->CurrentFreezeMode == FreezeMode::Player);

		auto& hairController = GetRemoteHairController(*nativeItem);

		for (int i = 0; i < hairController.Units.size(); i++)
		{
			auto& unit = hairController.Units[i];
			if (!unit.IsEnabled)
				continue;

			// A unit that has not had a valid anchor yet would be drawn at the world origin.
			if (!unit.IsInitialized)
				continue;

			const auto& object = Objects[unit.ObjectID];
			if (!object.loaded)
				continue;

			bool skinned = (object.skinIndex != NO_VALUE && g_GameFlow->GetSettings()->Graphics.Skinning);
			bool flipped = (skinned || GetJointOffset(unit.ObjectID, 1, false).z < 0.0f);
			auto objectType = i ? RendererObjectType::RemoteHairSecondary : RendererObjectType::RemoteHairPrimary;

			const auto& rendererObject = *_moveableObjects[unit.ObjectID];

			_stObjects.Objects[0].World = Matrix::Identity;
			_stObjects.Bones[0] = itemToDraw->InterpolatedAnimationTransforms[RemoteHairUnit::GetRootMeshID(i)] * itemToDraw->InterpolatedWorld;
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

	void Renderer::DrawRemotePlayerHairSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view, int index, int baseIndex, int count)
	{
		const auto* nativeItem = GetItemByRendererIndex(objectInfo->Item->ItemNumber);
		if (nativeItem == nullptr)
			return;

		auto& hairController = GetRemoteHairController(*nativeItem);

		if (index >= (int)hairController.Units.size())
		{
			TENLog("[REMOTE] Attempt to draw a nonexistent hair unit.", LogLevel::Warning);
			return;
		}

		if (!hairController.Units[index].IsInitialized)
			return;

		if (lastObjectType != objectInfo->ObjectType)
		{
			_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

			SetDepthState(DepthState::Read);
			SetCullMode(CullMode::CounterClockwise);

			_shaders.Bind(Shader::Items);
		}

		// The objects buffer is rebuilt only when the hair unit changes. The object type tells the primary and secondary units of one item apart.
		if (_lastSortedObjectType != objectInfo->ObjectType ||
			_lastSortedObject != objectInfo->Item ||
			_lastSortedSkinned != objectInfo->Skinned)
		{
			_stObjects.Objects[0].World = Matrix::Identity;
			_stObjects.Objects[0].Color = objectInfo->Item->Color;
			_stObjects.Objects[0].AmbientLight = objectInfo->Item->AmbientLight;
			_stObjects.Skinned = (int)(objectInfo->Skinned ? SkinningMode::Full : SkinningMode::None);

			const auto& moveableObj = *_moveableObjects[(int)GAME_OBJECT_ID::ID_HAIR_PRIMARY + index];

			_stObjects.Bones[0] = objectInfo->Item->InterpolatedAnimationTransforms[RemoteHairUnit::GetRootMeshID(index)] * objectInfo->Item->InterpolatedWorld;
			ReflectMatrixOptionally(_stObjects.Bones[0]);

			for (int i = 0; i < hairController.Units[index].Segments.size(); i++)
			{
				const auto& segment = hairController.Units[index].Segments[i];
				auto worldMatrix = segment.GlobalTransform;

				ReflectMatrixOptionally(worldMatrix);

				_stObjects.Bones[i + 1] = worldMatrix;
				_stObjects.BoneLightModes[i] = (int)LightMode::Dynamic;
			}

			for (int k = 0; k < moveableObj.ObjectMeshes.size(); k++)
				_stObjects.BoneLightModes[k] = (int)moveableObj.ObjectMeshes[k]->LightMode;

			bool acceptsShadows = moveableObj.ShadowType == ShadowMode::None;
			BindMoveableLights(objectInfo->Item->LightsToDraw, objectInfo->Item->RoomNumber, objectInfo->Item->PrevRoomNumber, objectInfo->Item->LightFade, acceptsShadows);

			// Only Objects[0] is used, so upload just the prefix of the buffer.
			UpdateConstantBuffer(&_stObjects, _cbObjects.get(), GetObjectsBufferPrefixSize(1));

			_lastSortedObjectType = objectInfo->ObjectType;
			_lastSortedObject = objectInfo->Item;
			_lastSortedSkinned = objectInfo->Skinned;
		}

		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		BindBucketTextures(*objectInfo->Bucket, TextureSource::Moveables, objectInfo->Bucket->Animated);
		BindMaterial(objectInfo->Bucket->MaterialIndex, false);

		DrawIndexedTriangles(count, baseIndex, 0);

		_numSortedMoveablesDrawCalls++;
		_numSortedTriangles += count / 3;
	}
}
