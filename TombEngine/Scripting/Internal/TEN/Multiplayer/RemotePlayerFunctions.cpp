#include "framework.h"
#include "Scripting/Internal/TEN/Multiplayer/RemotePlayerFunctions.h"

#include "Game/Lara/lara.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Multiplayer/RemotePlayer.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"

using namespace TEN::Math;
using namespace TEN::Multiplayer;
using namespace TEN::Scripting;

namespace TEN::Scripting::Multiplayer
{
	void SetRemotePlayerState(ItemInfo& item, const sol::table& stateTable)
	{
		if (stateTable.empty())
			return;

		auto& remoteInfo = GetRemotePlayerInfo(item.Index);

		sol::optional<int> animObjectIDOpt = stateTable["animObjectID"];
		sol::optional<int> animNumberOpt = stateTable["animNumber"];
		sol::optional<int> frameNumberOpt = stateTable["frameNumber"];

		sol::optional<sol::table> meshIndicesOpt = stateTable["meshIndices"];
		if (meshIndicesOpt.has_value())
		{
			sol::table meshTable = meshIndicesOpt.value();
			for (int i = 0; i < NUM_LARA_MESHES && i < (int)meshTable.size(); i++)
			{
				sol::optional<int> meshIndex = meshTable[i + 1];
				if (meshIndex.has_value())
					item.Model.MeshIndex[i] = meshIndex.value();
			}
		}

		// The animation object is only changed to a loaded one, because the animation lookup reads it. -- divezzi, 20.11.26
		if (animObjectIDOpt.has_value())
		{
			auto objectID = (GAME_OBJECT_ID)animObjectIDOpt.value();
			if (objectID >= 0 && objectID < ID_NUMBER_OBJECTS && Objects[objectID].loaded)
				item.Animation.AnimObjectID = objectID;
		}

		if (animNumberOpt.has_value())
		{
			int animNumber = animNumberOpt.value();
			auto resolvedObjectID = item.Animation.AnimObjectID;
			if (resolvedObjectID >= 0 && resolvedObjectID < ID_NUMBER_OBJECTS &&
				Objects[resolvedObjectID].loaded &&
				animNumber >= 0 && animNumber < (int)Objects[resolvedObjectID].Animations.size())
			{
				item.Animation.AnimNumber = animNumber;
			}
		}

		if (frameNumberOpt.has_value())
			item.Animation.FrameNumber = frameNumberOpt.value();

		// NOTE: An animation change or a frame jump does not disable interpolation. The renderer blends the bones between ticks,
		// and the pose is only snapped by a teleport or a large turn (SetPosition(), SetRotation()).

		sol::optional<int> activeStateOpt = stateTable["activeState"];
		sol::optional<int> targetStateOpt = stateTable["targetState"];
		if (activeStateOpt.has_value())
			item.Animation.ActiveState = activeStateOpt.value();
		if (targetStateOpt.has_value())
			item.Animation.TargetState = targetStateOpt.value();

		sol::optional<int> gunTypeOpt = stateTable["gunType"];
		sol::optional<int> handStatusOpt = stateTable["handStatus"];
		sol::optional<int> leftHolsterOpt = stateTable["leftHolster"];
		sol::optional<int> rightHolsterOpt = stateTable["rightHolster"];
		sol::optional<int> backHolsterOpt = stateTable["backHolster"];
		sol::optional<bool> isUsingBinocularsOpt = stateTable["isUsingBinoculars"];
		sol::optional<bool> isLowOpt = stateTable["isLow"];
		sol::optional<EulerAngles> extraTorsoRotOpt = stateTable["extraTorsoRotation"];
		sol::optional<EulerAngles> extraHeadRotOpt = stateTable["extraHeadRotation"];
		sol::optional<Rotation> leftArmOrientOpt = stateTable["leftArmOrientation"];
		sol::optional<Rotation> rightArmOrientOpt = stateTable["rightArmOrientation"];
		sol::optional<Rotation> targetArmOrientOpt = stateTable["targetArmOrientation"];
		sol::optional<bool> flareControlLeftOpt = stateTable["flareControlLeft"];
		sol::optional<int> flareLifeOpt = stateTable["flareLife"];
		sol::optional<int> leftArmAnimOpt = stateTable["leftArmAnim"];
		sol::optional<int> leftArmFrameOpt = stateTable["leftArmFrame"];
		sol::optional<int> leftArmGunSmokeOpt = stateTable["leftArmGunSmoke"];
		sol::optional<int> leftArmGunFlashOpt = stateTable["leftArmGunFlash"];
		sol::optional<int> rightArmAnimOpt = stateTable["rightArmAnim"];
		sol::optional<int> rightArmFrameOpt = stateTable["rightArmFrame"];
		sol::optional<int> rightArmGunSmokeOpt = stateTable["rightArmGunSmoke"];
		sol::optional<int> rightArmGunFlashOpt = stateTable["rightArmGunFlash"];

		remoteInfo.Control.Weapon.GunType = (LaraWeaponType)gunTypeOpt.value_or((int)remoteInfo.Control.Weapon.GunType);
		remoteInfo.Control.HandStatus = (HandStatus)handStatusOpt.value_or((int)remoteInfo.Control.HandStatus);
		remoteInfo.Control.Weapon.HolsterInfo.LeftHolster = (HolsterSlot)leftHolsterOpt.value_or((int)remoteInfo.Control.Weapon.HolsterInfo.LeftHolster);
		remoteInfo.Control.Weapon.HolsterInfo.RightHolster = (HolsterSlot)rightHolsterOpt.value_or((int)remoteInfo.Control.Weapon.HolsterInfo.RightHolster);
		remoteInfo.Control.Weapon.HolsterInfo.BackHolster = (HolsterSlot)backHolsterOpt.value_or((int)remoteInfo.Control.Weapon.HolsterInfo.BackHolster);
		remoteInfo.Control.Look.IsUsingBinoculars = isUsingBinocularsOpt.value_or(remoteInfo.Control.Look.IsUsingBinoculars);
		remoteInfo.Control.IsLow = isLowOpt.value_or(remoteInfo.Control.IsLow);
		remoteInfo.ExtraTorsoRot = extraTorsoRotOpt.value_or(remoteInfo.ExtraTorsoRot);
		remoteInfo.ExtraHeadRot = extraHeadRotOpt.value_or(remoteInfo.ExtraHeadRot);

		if (leftArmOrientOpt.has_value())
			remoteInfo.LeftArm.Orientation = leftArmOrientOpt.value().ToEulerAngles();

		if (rightArmOrientOpt.has_value())
			remoteInfo.RightArm.Orientation = rightArmOrientOpt.value().ToEulerAngles();

		if (targetArmOrientOpt.has_value())
			remoteInfo.TargetArmOrient = targetArmOrientOpt.value().ToEulerAngles();

		sol::optional<bool> leftArmLockedOpt = stateTable["leftArmLocked"];
		sol::optional<bool> rightArmLockedOpt = stateTable["rightArmLocked"];
		remoteInfo.LeftArm.Locked = leftArmLockedOpt.value_or(remoteInfo.LeftArm.Locked);
		remoteInfo.RightArm.Locked = rightArmLockedOpt.value_or(remoteInfo.RightArm.Locked);

		sol::optional<bool> lookActiveOpt = stateTable["lookActive"];
		sol::optional<float> lookOrientXOpt = stateTable["lookOrientX"];
		sol::optional<float> lookOrientYOpt = stateTable["lookOrientY"];

		if (lookActiveOpt.value_or(false))
		{
			float orientX = lookOrientXOpt.value_or(0.0f);
			float orientY = lookOrientYOpt.value_or(0.0f);
			remoteInfo.ExtraHeadRot = EulerAngles(ANGLE(orientX / 2.0f), ANGLE(orientY / 2.0f), 0);
			remoteInfo.ExtraTorsoRot = remoteInfo.ExtraHeadRot;
		}
		else
		{
			if (!extraHeadRotOpt.has_value())
				remoteInfo.ExtraHeadRot = EulerAngles(0, 0, 0);

			if (!extraTorsoRotOpt.has_value())
			{
				// For non-pistol weapons the torso follows the left arm while it is locked.
				auto weaponType = remoteInfo.Control.Weapon.GunType;
				bool isPistolType = (weaponType == LaraWeaponType::Pistol || weaponType == LaraWeaponType::Uzi);
				if (!isPistolType && remoteInfo.LeftArm.Locked)
				{
					remoteInfo.ExtraTorsoRot = remoteInfo.LeftArm.Orientation;
				}
				else
				{
					remoteInfo.ExtraTorsoRot = EulerAngles(0, 0, 0);
				}
			}
		}

		remoteInfo.Flare.ControlLeft = flareControlLeftOpt.value_or(false);
		remoteInfo.Flare.Life = (unsigned int)std::max(flareLifeOpt.value_or(0), 0);

		remoteInfo.LeftArm.AnimNumber = leftArmAnimOpt.value_or(remoteInfo.LeftArm.AnimNumber);
		remoteInfo.LeftArm.FrameNumber = leftArmFrameOpt.value_or(remoteInfo.LeftArm.FrameNumber);
		remoteInfo.LeftArm.GunSmoke = leftArmGunSmokeOpt.value_or(0);
		remoteInfo.LeftArm.GunFlash = leftArmGunFlashOpt.value_or(0);

		remoteInfo.RightArm.AnimNumber = rightArmAnimOpt.value_or(remoteInfo.RightArm.AnimNumber);
		remoteInfo.RightArm.FrameNumber = rightArmFrameOpt.value_or(remoteInfo.RightArm.FrameNumber);
		remoteInfo.RightArm.GunSmoke = rightArmGunSmokeOpt.value_or(0);
		remoteInfo.RightArm.GunFlash = rightArmGunFlashOpt.value_or(0);
	}

	sol::table GetRemotePlayerState(const ItemInfo& item, sol::this_state state)
	{
		auto lua = sol::state_view(state);
		auto stateTable = lua.create_table();

		stateTable["posX"] = (float)item.Pose.Position.x;
		stateTable["posY"] = (float)item.Pose.Position.y;
		stateTable["posZ"] = (float)item.Pose.Position.z;
		stateTable["rotY"] = TO_DEGREES(item.Pose.Orientation.y);

		stateTable["animNumber"] = item.Animation.AnimNumber;
		stateTable["frameNumber"] = item.Animation.FrameNumber;
		stateTable["frameBase"] = 0;

		auto& remoteInfo = GetRemotePlayerInfo(item.Index);
		stateTable["gunType"] = (int)remoteInfo.Control.Weapon.GunType;
		stateTable["handStatus"] = (int)remoteInfo.Control.HandStatus;
		stateTable["isUsingBinoculars"] = remoteInfo.Control.Look.IsUsingBinoculars;
		stateTable["isLow"] = remoteInfo.Control.IsLow;
		stateTable["extraTorsoRotation"] = sol::make_object(state, remoteInfo.ExtraTorsoRot);
		stateTable["extraHeadRotation"] = sol::make_object(state, remoteInfo.ExtraHeadRot);
		stateTable["leftArmOrientation"] = remoteInfo.LeftArm.Orientation;
		stateTable["rightArmOrientation"] = remoteInfo.RightArm.Orientation;
		stateTable["flareControlLeft"] = remoteInfo.Flare.ControlLeft;
		stateTable["flareLife"] = remoteInfo.Flare.Life;

		stateTable["leftArmAnim"] = remoteInfo.LeftArm.AnimNumber;
		stateTable["leftArmFrame"] = remoteInfo.LeftArm.FrameNumber;
		stateTable["leftArmFrameBase"] = 0;
		stateTable["rightArmAnim"] = remoteInfo.RightArm.AnimNumber;
		stateTable["rightArmFrame"] = remoteInfo.RightArm.FrameNumber;
		stateTable["rightArmFrameBase"] = 0;

		return stateTable;
	}

	sol::table GetLocalPlayerState(const ItemInfo& item, sol::this_state state)
	{
		auto lua = sol::state_view(state);
		auto stateTable = lua.create_table();

		if (!item.IsLara())
			return stateTable;

		stateTable["posX"] = (float)item.Pose.Position.x;
		stateTable["posY"] = (float)item.Pose.Position.y;
		stateTable["posZ"] = (float)item.Pose.Position.z;
		stateTable["rotY"] = TO_DEGREES(item.Pose.Orientation.y);

		stateTable["animObjectID"] = item.Animation.AnimObjectID;
		stateTable["animNumber"] = item.Animation.AnimNumber;
		stateTable["frameNumber"] = item.Animation.FrameNumber;
		stateTable["frameBase"] = 0;
		stateTable["activeState"] = item.Animation.ActiveState;
		stateTable["targetState"] = item.Animation.TargetState;

		stateTable["gunType"] = (int)Lara.Control.Weapon.GunType;
		stateTable["handStatus"] = (int)Lara.Control.HandStatus;
		stateTable["leftHolster"] = (int)Lara.Control.Weapon.HolsterInfo.LeftHolster;
		stateTable["rightHolster"] = (int)Lara.Control.Weapon.HolsterInfo.RightHolster;
		stateTable["backHolster"] = (int)Lara.Control.Weapon.HolsterInfo.BackHolster;
		stateTable["isUsingBinoculars"] = Lara.Control.Look.IsUsingBinoculars;
		stateTable["isLow"] = Lara.Control.IsLow;
		stateTable["extraTorsoRotation"] = Lara.ExtraTorsoRot;
		stateTable["extraHeadRotation"] = Lara.ExtraHeadRot;
		stateTable["leftArmOrientation"] = Lara.LeftArm.Orientation;
		stateTable["rightArmOrientation"] = Lara.RightArm.Orientation;
		stateTable["flareControlLeft"] = Lara.Flare.ControlLeft;
		stateTable["flareLife"] = Lara.Flare.Life;
		stateTable["leftArmAnim"] = Lara.LeftArm.AnimNumber;
		stateTable["leftArmFrame"] = Lara.LeftArm.FrameNumber;
		stateTable["leftArmFrameBase"] = 0;
		stateTable["leftArmGunSmoke"] = Lara.LeftArm.GunSmoke;
		stateTable["leftArmGunFlash"] = Lara.LeftArm.GunFlash;
		stateTable["rightArmAnim"] = Lara.RightArm.AnimNumber;
		stateTable["rightArmFrame"] = Lara.RightArm.FrameNumber;
		stateTable["rightArmFrameBase"] = 0;
		stateTable["rightArmGunSmoke"] = Lara.RightArm.GunSmoke;
		stateTable["rightArmGunFlash"] = Lara.RightArm.GunFlash;

		auto meshIndices = lua.create_table();
		for (int i = 0; i < NUM_LARA_MESHES; i++)
			meshIndices[i + 1] = item.Model.MeshIndex[i];

		stateTable["meshIndices"] = meshIndices;
		return stateTable;
	}
}
