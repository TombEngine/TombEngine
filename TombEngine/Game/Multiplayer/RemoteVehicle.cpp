#include "framework.h"
#include "Game/Multiplayer/RemoteVehicle.h"

#include <algorithm>

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/debug/debug.h"
#include "Game/Multiplayer/RemoteItemPool.h"
#include "Game/room.h"
#include "Game/Setup.h"
#include "Objects/Utils/VehicleHelpers.h"

using namespace TEN::Animation;
using namespace TEN::Entities::Vehicles;

namespace TEN::Multiplayer
{
	// Pose seen at the previous update of a pooled vehicle, and the speed and turn rates measured from it.
	// The control function of a vehicle advances its wheel, propeller and rudder values and does not run for a pooled item,
	// so these values are rebuilt from the motion of the received pose.
	struct RemoteVehicleMotion
	{
		bool		IsValid			= false;
		int			LastTick		= 0;
		Vector3i	LastPosition	= Vector3i(0, 0, 0);
		EulerAngles LastOrientation = EulerAngles(0, 0, 0);

		float Speed		= 0.0f; // Forward world units per game tick.
		float YawRate	= 0.0f; // Angle units per game tick.
		float PitchRate = 0.0f; // Angle units per game tick.
	};

	constexpr auto MOTION_TICK_GAP_MAX = 15;
	constexpr auto MOTION_SMOOTHING	   = 0.5f;

	static RemoteVehicleMotion s_motions[MAX_REMOTE_VEHICLES];

	static RemoteVehicleMotion& GetMotion(const ItemInfo& item)
	{
		int slot = item.Index - g_RemoteItemRendererOffset - MAX_REMOTE_PLAYERS;
		return s_motions[std::clamp(slot, 0, MAX_REMOTE_VEHICLES - 1)];
	}

	// Returns the number of game ticks that the measurement covers, 0 when there is nothing to advance.
	static int MeasureMotion(RemoteVehicleMotion& motion, const Vector3i& pos, const EulerAngles& orient)
	{
		int ticks = GlobalCounter - motion.LastTick;

		// Several packets within one game tick: keep measuring from the older pose.
		if (motion.IsValid && ticks <= 0)
			return 0;

		bool isUsable = (motion.IsValid && ticks <= MOTION_TICK_GAP_MAX);
		if (isUsable)
		{
			float yawRad = TO_RAD(orient.y);
			float deltaX = (float)(pos.x - motion.LastPosition.x);
			float deltaZ = (float)(pos.z - motion.LastPosition.z);

			float speed = ((deltaX * sin(yawRad)) + (deltaZ * cos(yawRad))) / (float)ticks;
			float yawRate = (float)(short)(orient.y - motion.LastOrientation.y) / (float)ticks;
			float pitchRate = (float)(short)(orient.x - motion.LastOrientation.x) / (float)ticks;

			motion.Speed += (speed - motion.Speed) * MOTION_SMOOTHING;
			motion.YawRate += (yawRate - motion.YawRate) * MOTION_SMOOTHING;
			motion.PitchRate += (pitchRate - motion.PitchRate) * MOTION_SMOOTHING;
		}
		else
		{
			// First update or a long gap (teleport, level load): restart instead of spinning the wheels.
			motion.Speed = 0.0f;
			motion.YawRate = 0.0f;
			motion.PitchRate = 0.0f;
		}

		motion.IsValid = true;
		motion.LastTick = GlobalCounter;
		motion.LastPosition = pos;
		motion.LastOrientation = orient;

		return (isUsable ? ticks : 0);
	}

	// Advances the values that the renderer reads in the Data.apply block of Renderer::UpdateItemAnimations().
	// The kayak, skidoo, speedboat and big gun are not advanced.
	static void AdvanceVehicleBones(ItemInfo& item, const RemoteVehicleMotion& motion, int ticks)
	{
		int velocity = (int)(motion.Speed * VEHICLE_VELOCITY_SCALE);
		short wheelAdd = (short)((velocity / 4) * ticks);
		short turnRate = (short)motion.YawRate;

		item.Data.apply(
			[&](QuadBikeInfo& quad)
			{
				quad.TurnRate = turnRate;
				quad.FrontRot -= wheelAdd;
				quad.RearRot -= wheelAdd;
			},
			[&](JeepInfo& jeep)
			{
				jeep.TurnRate = turnRate;
				jeep.FrontRightWheelRotation -= wheelAdd;
				jeep.FrontLeftWheelRotation -= wheelAdd;
				jeep.BackRightWheelRotation -= wheelAdd;
				jeep.BackLeftWheelRotation -= wheelAdd;
			},
			[&](MotorbikeInfo& bike)
			{
				bike.TurnRate = turnRate;
				bike.LeftWheelRotation -= wheelAdd;
				bike.RightWheelsRotation -= wheelAdd;
			},
			[&](MinecartInfo& cart)
			{
				cart.Velocity = velocity;
			},
			[&](RubberBoatInfo& boat)
			{
				int propellerAdd = (motion.Speed >= 0.0f) ? (int)((motion.Speed * ANGLE(3.0f)) + ANGLE(2.0f)) : (int)ANGLE(33.0f);
				boat.PropellerRotation += (short)(propellerAdd * ticks);
			},
			[&](UPVInfo& upv)
			{
				int turbineAdd = (velocity != 0) ? (velocity / 8) : (int)ANGLE(2.0f);
				upv.Velocity = velocity;
				upv.TurbineRotation += (short)(turbineAdd * ticks);
				upv.LeftRudderRotation = (short)((motion.PitchRate + motion.YawRate) * 8.0f);
				upv.RightRudderRotation = (short)((motion.PitchRate - motion.YawRate) * 8.0f);
			});
	}

	// The renderer applies the wheel, steering and propeller bones by testing the type held in ItemInfo::Data,
	// so a pooled vehicle needs the info struct that the Initialize function of its object gives a placed vehicle.
	static bool AssignVehicleData(ItemInfo& item)
	{
		constexpr auto JEEP_HEADLIGHT_JOINT		 = 17;
		constexpr auto JEEP_SECOND_HIDDEN_JOINT	 = 18;
		constexpr auto MOTORBIKE_HEADLIGHT_JOINT = 3;

		switch (item.ObjectNumber)
		{
		case ID_JEEP:
			item.Data = JeepInfo();
			item.MeshBits.Clear(JEEP_HEADLIGHT_JOINT);
			item.MeshBits.Clear(JEEP_SECOND_HIDDEN_JOINT);
			return true;

		case ID_MOTORBIKE:
			item.Data = MotorbikeInfo();
			item.MeshBits.Clear(MOTORBIKE_HEADLIGHT_JOINT);
			return true;

		case ID_QUAD:
			item.Data = QuadBikeInfo();
			return true;

		case ID_KAYAK:
			item.Data = KayakInfo();
			return true;

		case ID_UPV:
			item.Data = UPVInfo();
			return true;

		case ID_RUBBER_BOAT:
			item.Data = RubberBoatInfo();
			return true;

		case ID_SPEEDBOAT:
			item.Data = SpeedboatInfo();
			return true;

		case ID_SNOWMOBILE:
			item.Data = SkidooInfo();
			return true;

		case ID_MINECART:
			item.Data = MinecartInfo();
			return true;

		case ID_BIGGUN:
			item.Data = BigGunInfo();
			return true;

		default:
			return false;
		}
	}

	bool IsRemoteVehicleObject(int objectNumber)
	{
		switch (objectNumber)
		{
		case ID_JEEP:
		case ID_MOTORBIKE:
		case ID_QUAD:
		case ID_KAYAK:
		case ID_UPV:
		case ID_RUBBER_BOAT:
		case ID_SPEEDBOAT:
		case ID_SNOWMOBILE:
		case ID_MINECART:
		case ID_BIGGUN:
			return true;

		default:
			return false;
		}
	}

	short CreateRemoteVehicle(GAME_OBJECT_ID objectID, const std::string& name, const Vector3i& pos, const EulerAngles& orient, int roomNumber)
	{
		if (!IsRemoteVehicleObject(objectID) || !Objects[objectID].loaded)
		{
			TENLog("[REMVEH] Can't create remote vehicle " + GetObjectName(objectID) + ": not a vehicle or object not loaded.", LogLevel::Error);
			return NO_VALUE;
		}

		short slotIndex = AllocRemoteVehicleItem();
		if (slotIndex == NO_VALUE)
			return NO_VALUE;

		auto& item = g_RemoteItems[slotIndex];
		item.ObjectNumber = objectID;
		item.Name = name;
		item.Pose.Position = pos;
		item.Pose.Orientation = orient;
		item.RoomNumber = roomNumber;
		item.Active = true;
		item.Status = ITEM_ACTIVE;
		item.Collidable = false;
		item.Flags &= ~IFLAG_INVISIBLE;
		item.MeshBits = ALL_JOINT_BITS;
		item.CarriedItem = NO_VALUE;
		item.Model.Color = NEUTRAL_COLOR;
		item.ResetModelToDefault();

		item.Animation.AnimObjectID = objectID;
		item.Animation.RequiredState = NO_VALUE;
		if (!Objects[objectID].Animations.empty())
			SetAnimation(item, 0);

		if (!AssignVehicleData(item))
		{
			FreeRemoteItem(slotIndex);
			return NO_VALUE;
		}

		GetMotion(item) = RemoteVehicleMotion();

		return slotIndex;
	}

	void KillRemoteVehicle(short slotIndex)
	{
		if (!IsRemoteVehicleSlot(slotIndex))
			return;

		auto& item = g_RemoteItems[slotIndex];

		item.Data = nullptr;
		item.Active = false;

		if (g_RemoteItemsOccupied[slotIndex])
			ReleaseRemoteItemScriptName(item);

		FreeRemoteItem(slotIndex);
	}

	void UpdateRemoteVehiclePose(ItemInfo& item, const Vector3i& pos, const EulerAngles& orient)
	{
		if (Vector3i::Distance(item.Pose.Position, pos) > BLOCK(1))
			item.DisableInterpolation = true;

		item.Pose.Position = pos;
		item.Pose.Orientation = orient;

		int roomNumber = FindRoomNumber(pos, item.RoomNumber);
		if (roomNumber != NO_VALUE)
			item.RoomNumber = roomNumber;

		auto& motion = GetMotion(item);
		int ticks = MeasureMotion(motion, pos, orient);
		if (ticks > 0)
			AdvanceVehicleBones(item, motion, ticks);
	}
}
