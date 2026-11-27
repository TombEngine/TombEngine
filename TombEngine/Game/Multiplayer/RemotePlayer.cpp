#include "framework.h"
#include "Game/Multiplayer/RemotePlayer.h"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <vector>

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/control/trigger.h"
#include "Game/debug/debug.h"
#include "Game/effects/chaffFX.h"
#include "Game/effects/Light.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_flare.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Multiplayer/Multiplayer.h"
#include "Game/Multiplayer/RemoteHair.h"
#include "Game/Multiplayer/RemoteItemPool.h"
#include "Game/Multiplayer/RemoteTargeting.h"
#include "Game/Multiplayer/RemoteVehicle.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"

namespace TEN::Multiplayer
{
	constexpr auto REMOTE_PLAYER_NAME_PREFIX = "remote_player_";
	constexpr auto LEGACY_NAME_PREFIX		 = "lara_double_";

	// Player info keyed by player ID, which is encoded in the item name.
	static std::map<int, LaraInfo> s_remotePlayers;

	// Pool slot of each registered player ID.
	static std::map<int, short> s_playerIdToSlot;

	// Returns the player ID encoded in an item name, or NO_VALUE. The legacy prefix is kept for existing scripts.
	int GetRemotePlayerId(const std::string& name)
	{
		for (const std::string prefix : { REMOTE_PLAYER_NAME_PREFIX, LEGACY_NAME_PREFIX })
		{
			if (name.size() <= prefix.size() || name.compare(0, prefix.size(), prefix) != 0)
				continue;

			try
			{
				return std::stoi(name.substr(prefix.size()));
			}
			catch (...)
			{
				return NO_VALUE;
			}
		}

		return NO_VALUE;
	}

	void InitializeRemotePlayerObject()
	{
		// Only the declaration of a script decides here: the slot is set up at level load, and the user option can change afterwards. -- divezzi, 24.11.26
		if (!g_GameFlow->IsMultiplayerDeclared())
			return;

		auto& object = Objects[ID_REMOTE_PLAYER];
		object.HitPoints = 1000;
		object.pivotLength = 50;
		object.radius = 128;
		object.SetHitEffect(true, true);
	}

	void InitializeRemotePlayer(short slotIndex)
	{
		if (slotIndex < 0 || slotIndex >= MAX_REMOTE_PLAYERS)
		{
			TENLog("[REMOTE] Ignoring out-of-range remote player slot " + std::to_string(slotIndex) + ".", LogLevel::Warning);
			return;
		}

		auto& item = g_RemoteItems[slotIndex];
		int playerId = GetRemotePlayerId(item.Name);

		s_remotePlayers[playerId] = {};
		s_remotePlayers[playerId].Control.Weapon.WeaponItem = NO_VALUE;
		s_playerIdToSlot[playerId] = slotIndex;
		InitializeRemoteHair(playerId);

		item.Animation.AnimNumber = 0;
		item.Animation.FrameNumber = 0;
		item.Animation.ActiveState = LS_IDLE;
		item.Animation.TargetState = LS_IDLE;
		// Animations are read from the local player's object, which this slot does not copy.
		item.Animation.AnimObjectID = ID_LARA;

		item.Status = ITEM_ACTIVE;
		item.Active = true;
		item.Flags &= ~IFLAG_INVISIBLE;
		item.Flags |= CODE_BITS;

		item.HitPoints = Objects[ID_REMOTE_PLAYER].HitPoints;
		item.MeshBits = ALL_JOINT_BITS;

		// Same skin object as the local player.
		int skinSlot = (Objects[ID_LARA_SKIN].loaded ? ID_LARA_SKIN : ID_LARA);
		const auto& skinObject = Objects[skinSlot];
		item.Model.BaseMesh = skinObject.meshIndex;
		item.Model.SkinObjectID = skinSlot;
		item.Model.SkinSwapIndex = NO_VALUE;

		item.Model.MeshIndex.resize(NUM_LARA_MESHES);
		for (int i = 0; i < NUM_LARA_MESHES; i++)
			item.Model.MeshIndex[i] = item.Model.BaseMesh + i;

	}

	short CreateRemotePlayer(const std::string& name, const Vector3i& pos, const EulerAngles& orient, int roomNumber)
	{
		if (GetRemotePlayerId(name) == NO_VALUE)
		{
			TENLog("[REMOTE] Cannot create a remote player named '" + name + "': the name has no player ID.", LogLevel::Error);
			return NO_VALUE;
		}

		short slotIndex = AllocRemotePlayerItem();
		if (slotIndex == NO_VALUE)
			return NO_VALUE;

		auto& item = g_RemoteItems[slotIndex];
		item.ObjectNumber = ID_REMOTE_PLAYER;
		item.Name = name;
		item.Pose.Position = pos;
		item.Pose.Orientation = orient;
		item.RoomNumber = roomNumber;
		item.CarriedItem = NO_VALUE;
		item.ResetModelToDefault();

		// Same tint as the local player. The alpha stays 1.0 because the renderer skips items below EPSILON alpha (RendererFrame.cpp:440).
		const auto* playerItem = LaraItem.Get();
		item.Model.Color = (playerItem != nullptr) ?
			Vector4(playerItem->Model.Color.x, playerItem->Model.Color.y, playerItem->Model.Color.z, 1.0f) :
			NEUTRAL_COLOR;

		InitializeRemotePlayer(slotIndex);
		return slotIndex;
	}

	void KillRemotePlayer(short slotIndex)
	{
		if (slotIndex < 0 || slotIndex >= MAX_REMOTE_ITEMS)
			return;

		auto& item = g_RemoteItems[slotIndex];
		int playerId = GetRemotePlayerId(item.Name);

		item.Active = false;
		SetRemotePlayerTargetable(item, false);
		ReleaseRemotePlayerTarget(item);
		s_remotePlayers.erase(playerId);
		s_playerIdToSlot.erase(playerId);
		ClearRemoteHair(playerId);

		// A free slot keeps the name of its last item, which may belong to someone else now.
		if (g_RemoteItemsOccupied[slotIndex])
			ReleaseRemoteItemScriptName(item);

		FreeRemoteItem(slotIndex);
	}

	void ClearAllRemoteItems()
	{
		// Copy the slots first: KillRemotePlayer() erases from the map.
		auto slotIndices = std::vector<short>{};
		slotIndices.reserve(s_playerIdToSlot.size());
		for (const auto& [playerId, slotIndex] : s_playerIdToSlot)
			slotIndices.push_back(slotIndex);

		for (short slotIndex : slotIndices)
			KillRemotePlayer(slotIndex);

		// Slots without a registry entry go through the same kill, so their script names are freed too.
		for (short slotIndex = 0; slotIndex < MAX_REMOTE_ITEMS; slotIndex++)
		{
			if (!g_RemoteItemsOccupied[slotIndex])
				continue;

			if (IsRemoteVehicleSlot(slotIndex))
			{
				KillRemoteVehicle(slotIndex);
			}
			else
			{
				KillRemotePlayer(slotIndex);
			}
		}

		// The registries can hold entries of a previous level, where no slot is occupied.
		s_remotePlayers.clear();
		s_playerIdToSlot.clear();
	}

	LaraInfo& GetRemotePlayerInfo(short rendererIndex)
	{
		auto* item = GetItemByRendererIndex(rendererIndex);
		if (item == nullptr)
		{
			TENLog("[REMOTE] No item for renderer index " + std::to_string(rendererIndex) + ".", LogLevel::Error);
			throw std::runtime_error("No remote player item for renderer index " + std::to_string(rendererIndex) + ".");
		}

		auto it = s_remotePlayers.find(GetRemotePlayerId(item->Name));
		if (it == s_remotePlayers.end())
		{
			TENLog("[REMOTE] Item '" + item->Name + "' is not an initialized remote player.", LogLevel::Error);
			throw std::runtime_error("Item '" + item->Name + "' is not an initialized remote player.");
		}

		return it->second;
	}

	void UpdateRemotePlayerLights()
	{
		using namespace TEN::Animation;
		using namespace TEN::Effects::Light;

		if (!IsRemotePlayerEnabled())
			return;

		// The muzzle light is a detail of the near view: it fades out with the distance to the camera and is half as strong as the local one.
		constexpr auto LIGHT_DISTANCE_NEAR	 = BLOCK(4);
		constexpr auto LIGHT_DISTANCE_FAR	 = BLOCK(8);
		constexpr auto LIGHT_INTENSITY_SCALE = 0.5f;

		auto cameraPos = Camera.pos.ToVector3();

		for (int i = 0; i < MAX_REMOTE_PLAYERS; i++)
		{
			if (!g_RemoteItemsOccupied[i])
				continue;

			auto& item = g_RemoteItems[i];
			if (!item.Active || item.Status == ITEM_INVISIBLE)
				continue;

			auto it = s_remotePlayers.find(GetRemotePlayerId(item.Name));
			if (it == s_remotePlayers.end())
				continue;

			const auto& remoteInfo = it->second;

			// A flare lit in the hand burns in every pose, as DoFlareInHand() runs for the local player whatever the arm does. The light, sparks, smoke and burn sound come from the left hand of the remote player and the flare settings of the level. Flare.ControlLeft only tells the arm pose, so it is not tested here.
			if (remoteInfo.Control.Weapon.GunType == LaraWeaponType::Flare && remoteInfo.Flare.Life != 0)
			{
				const auto& flareSettings = g_GameFlow->GetSettings()->Flare;
				int flareLife = (int)remoteInfo.Flare.Life;

				auto flarePos = GetJointPosition(item, LM_LHAND, flareSettings.Offset.ToVector3i() + Vector3i(11, 32, 0));
				if (DoFlareLight(item, flarePos, flareLife))
				{
					auto effectPos = GetJointPosition(item, LM_LHAND, flareSettings.Offset.ToVector3i() + Vector3i(11, 32, -4));
					auto effectVel = GetJointPosition(item, LM_LHAND, Vector3i(11, 32, BLOCK(1) + TEN::Math::Random::GenerateInt(0, 256))) - effectPos;
					TriggerChaffEffects(item, effectPos, effectVel, item.Animation.Velocity.z, TestEnvironment(ENV_FLAG_WATER, &item), remoteInfo.Control.Look.IsUsingBinoculars ? 0 : flareLife);
				}
			}

			if (!remoteInfo.LeftArm.GunFlash && !remoteInfo.RightArm.GunFlash)
				continue;

			auto gunType = remoteInfo.Control.Weapon.GunType;
			if (gunType == LaraWeaponType::None ||
				gunType == LaraWeaponType::Flare ||
				gunType == LaraWeaponType::Torch ||
				gunType == LaraWeaponType::Snowmobile ||
				(int)gunType >= (int)LaraWeaponType::NumWeapons)
			{
				continue;
			}

			const auto& settings = g_GameFlow->GetSettings()->Weapons[(int)gunType - 1];
			if (!settings.MuzzleFlash)
				continue;

			for (int arm = 0; arm < 2; arm++)
			{
				bool isLeft = (arm == 0);
				if (isLeft ? !remoteInfo.LeftArm.GunFlash : !remoteInfo.RightArm.GunFlash)
					continue;

				auto pos = GetJointPosition(item, isLeft ? LM_LHAND : LM_RHAND).ToVector3();

				float distance = Vector3::Distance(cameraPos, pos);
				if (distance >= LIGHT_DISTANCE_FAR)
					continue;

				float fade = (distance <= LIGHT_DISTANCE_NEAR) ? 1.0f : (1.0f - ((distance - LIGHT_DISTANCE_NEAR) / (LIGHT_DISTANCE_FAR - LIGHT_DISTANCE_NEAR)));
				auto color = Color(settings.FlashColor) * (LIGHT_INTENSITY_SCALE * fade);

				SpawnDynamicPointLight(pos, color, CLICK(settings.FlashRange));
			}
		}
	}
}
