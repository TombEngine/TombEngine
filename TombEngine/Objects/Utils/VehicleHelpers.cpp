#include "framework.h"
#include "Objects/Utils/VehicleHelpers.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/Point.h"
#include "Game/collision/Sphere.h"
#include "Game/effects/simple_particle.h"
#include "Game/effects/Splash.h"
#include "Game/effects/Streamer.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Hud/Hud.h"
#include "Game/Lara/lara_flare.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Math/Random.h"
#include "Game/room.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"

using namespace TEN::Collision::Point;
using namespace TEN::Collision::Sphere;
using namespace TEN::Effects::Splash;
using namespace TEN::Effects::Streamer;
using namespace TEN::Hud;
using namespace TEN::Input;
using namespace TEN::Math;

namespace TEN::Entities::Vehicles
{
	enum class VehicleWakeEffectTag
	{
		FrontLeft,
		FrontRight,
		BackLeft,
		BackRight
	};

	VehicleMountType GetVehicleMountType(ItemInfo* vehicleItem, ItemInfo* laraItem, CollisionInfo* coll, std::vector<VehicleMountType> allowedMountTypes, float maxDistance2D, float maxVerticalDistance)
	{
		auto* lara = GetLaraInfo(laraItem);

		// Assess vehicle usability.
		if (vehicleItem->Flags & IFLAG_INVISIBLE)
			return VehicleMountType::None;

		// Assess hand status.
		if (lara->Control.HandStatus != HandStatus::Free)
			return VehicleMountType::None;

		// Assess vertical distance to vehicle.
		if (abs(laraItem->Pose.Position.y - vehicleItem->Pose.Position.y) > maxVerticalDistance)
			return VehicleMountType::None;

		// Assess 2D distance to vehicle.
		float distance2D = Vector2::Distance(
			Vector2(laraItem->Pose.Position.x, laraItem->Pose.Position.z),
			Vector2(vehicleItem->Pose.Position.x, vehicleItem->Pose.Position.z));
		if (distance2D > maxDistance2D)
			return VehicleMountType::None;

		// Assess object collision.
		if (!TestBoundsCollide(vehicleItem, laraItem, coll->Setup.Radius) || !HandleItemSphereCollision(*vehicleItem, *laraItem))
			return VehicleMountType::None;

		bool hasInputAction = IsHeld(In::Action);

		short deltaHeadingAngle = vehicleItem->Pose.Orientation.y - laraItem->Pose.Orientation.y;
		short angleBetweenPositions = vehicleItem->Pose.Orientation.y - Geometry::GetOrientToPoint(laraItem->Pose.Position.ToVector3(), vehicleItem->Pose.Position.ToVector3()).y;
		bool onCorrectSide = abs(deltaHeadingAngle - angleBetweenPositions) < ANGLE(45.0f);

		// Assess mount types allowed for vehicle.
		for (auto mountType : allowedMountTypes)
		{
			switch (mountType)
			{
			case VehicleMountType::LevelStart:
				if (!laraItem->Animation.Velocity.z &&
					!laraItem->Animation.Velocity.y &&
					laraItem->Pose.Position == vehicleItem->Pose.Position)
				{
					break;
				}

				continue;

			case VehicleMountType::Front:
				if (hasInputAction &&
					deltaHeadingAngle > ANGLE(135.0f) && deltaHeadingAngle < -ANGLE(135.0f) &&
					onCorrectSide &&
					!laraItem->Animation.IsAirborne)
				{
					break;
				}

				continue;

			case VehicleMountType::Back:
				if (hasInputAction &&
					abs(deltaHeadingAngle) < ANGLE(45.0f) &&
					onCorrectSide &&
					!laraItem->Animation.IsAirborne)
				{
					break;
				}

				continue;

			case VehicleMountType::Left:
				if (hasInputAction &&
					deltaHeadingAngle > -ANGLE(135.0f) && deltaHeadingAngle < -ANGLE(45.0f) &&
					onCorrectSide &&
					!laraItem->Animation.IsAirborne)
				{
					break;
				}

				continue;

			case VehicleMountType::Right:
				if (hasInputAction &&
					deltaHeadingAngle > ANGLE(45.0f) && deltaHeadingAngle < ANGLE(135.0f) &&
					onCorrectSide &&
					!laraItem->Animation.IsAirborne)
				{
					break;
				}

				continue;

			case VehicleMountType::Jump:
				if (laraItem->Animation.IsAirborne &&
					laraItem->Animation.Velocity.y > 0 &&
					laraItem->Pose.Position.y < vehicleItem->Pose.Position.y)
				{
					break;
				}

				continue;

			default:
				return VehicleMountType::None;
			}

			return mountType;
		}

		return VehicleMountType::None;
	}

	int GetVehicleHeight(ItemInfo* vehicleItem, int forward, int right, bool clamp, Vector3i* pos)
	{
		float sinX = phd_sin(vehicleItem->Pose.Orientation.x);
		float sinY = phd_sin(vehicleItem->Pose.Orientation.y);
		float cosY = phd_cos(vehicleItem->Pose.Orientation.y);
		float sinZ = phd_sin(vehicleItem->Pose.Orientation.z);

		pos->x = vehicleItem->Pose.Position.x + (forward * sinY) + (right * cosY);
		pos->y = vehicleItem->Pose.Position.y - (forward * sinX) + (right * sinZ);
		pos->z = vehicleItem->Pose.Position.z + (forward * cosY) - (right * sinY);

		// Get collision a bit higher to be able to detect bridges.
		auto probe = GetPointCollision(Vector3i(pos->x, pos->y - CLICK(2), pos->z), vehicleItem->RoomNumber);

		if (pos->y < probe.GetCeilingHeight() || probe.GetCeilingHeight() == NO_HEIGHT)
			return NO_HEIGHT;

		if (pos->y > probe.GetFloorHeight() && clamp)
			pos->y = probe.GetFloorHeight();

		return probe.GetFloorHeight();
	}

	int GetVehicleWaterHeight(ItemInfo* vehicleItem, int forward, int right, bool clamp, Vector3i* pos)
	{
		auto rotMatrix = vehicleItem->Pose.Orientation.ToRotationMatrix();
		auto tMatrix = Matrix::CreateTranslation(vehicleItem->Pose.Position.ToVector3());
		auto world = rotMatrix * tMatrix;

		auto point = Vector3(right, 0, forward);
		point = Vector3::Transform(point, world);
		*pos = Vector3i(point);

		auto pointColl = GetPointCollision(*pos, vehicleItem->RoomNumber);
		int height = GetPointCollision(Vector3i(pos->x, pos->y, pos->z), pointColl.GetRoomNumber()).GetWaterTopHeight();

		if (height == NO_HEIGHT)
		{
			height = pointColl.GetFloorHeight();
			if (height == NO_HEIGHT)
				return height;
		}

		height -= 5;

		if (pos->y > height && clamp)
			pos->y = height;

		return height;
	}

	void SyncVehicleAnim(ItemInfo& vehicleItem, const ItemInfo& playerItem)
	{
		SetAnimation(vehicleItem, playerItem.Animation.AnimNumber, playerItem.Animation.FrameNumber);
	}

	void DoVehicleCollision(ItemInfo* vehicleItem, int radius)
	{
		CollisionInfo coll = {};
		coll.Setup.Radius = radius * 0.8f; // HACK: Most vehicles use radius larger than needed.
		coll.Setup.UpperCeilingBound = MAX_HEIGHT; // HACK: this needs to be set to prevent GCI result interference.
		coll.Setup.PrevPosition = vehicleItem->Pose.Position;
		coll.Setup.EnableObjectPush = true;

		DoObjectCollision(vehicleItem, &coll);
	}

	int DoVehicleWaterMovement(ItemInfo* vehicleItem, ItemInfo* laraItem, int currentVelocity, int radius, short* turnRate, const Vector3& wakeOffset)
	{
		if (TestEnvironment(ENV_FLAG_WATER, vehicleItem) ||
			TestEnvironment(ENV_FLAG_SWAMP, vehicleItem))
		{
			int waterDepth = GetPointCollision(*vehicleItem).GetWaterBottomHeight();

			// HACK: Sometimes quadbike test position may end up under non-portal ceiling block.
			// GetWaterDepth returns DEEP_WATER constant in that case, which is too large for our needs.
			if (waterDepth == DEEP_WATER)
				waterDepth = VEHICLE_WATER_HEIGHT_MAX;

			if (waterDepth <= VEHICLE_WATER_HEIGHT_MAX)
			{
				bool isWater = TestEnvironment(ENV_FLAG_WATER, vehicleItem);

				if (currentVelocity != 0)
				{
					auto coeff = isWater ? VEHICLE_WATER_VELOCITY_COEFF : VEHICLE_SWAMP_VELOCITY_COEFF;
					currentVelocity -= std::copysign(currentVelocity * ((waterDepth / VEHICLE_WATER_HEIGHT_MAX) / coeff), currentVelocity);

					if (TEN::Math::Random::GenerateInt(0, 32) > 28)
						SoundEffect(SFX_TR4_LARA_WADE, &Pose(vehicleItem->Pose.Position), SoundEnvironment::Land, isWater ? 0.8f : 0.7f);

					if (isWater)
					{
						int waterHeight = GetPointCollision(*vehicleItem).GetWaterTopHeight();
						SpawnVehicleWake(*vehicleItem, wakeOffset, waterHeight);
					}
				}

				if (*turnRate)
				{
					auto coeff = isWater ? VEHICLE_WATER_TURN_RATE_COEFF : VEHICLE_SWAMP_TURN_RATE_COEFF;
					*turnRate -= *turnRate * ((waterDepth / VEHICLE_WATER_HEIGHT_MAX) / coeff);
				}
			}
			else
			{
				int waterHeight = vehicleItem->Pose.Position.y - GetPointCollision(*vehicleItem).GetWaterTopHeight();

				if (waterDepth > VEHICLE_WATER_HEIGHT_MAX && waterHeight > VEHICLE_WATER_HEIGHT_MAX)
				{
					ExplodeVehicle(laraItem, vehicleItem);
				}
				else if (TEN::Math::Random::GenerateInt(0, 32) > 25)
				{
					Splash(vehicleItem);
				}
			}
		}

		return currentVelocity;
	}

	// TODO: Allow flares on every vehicle and see how that works. Boats already allowed them originally.
	void DoVehicleFlareDiscard(ItemInfo* laraItem)
	{
		auto* lara = GetLaraInfo(laraItem);

		if (lara->Control.Weapon.GunType == LaraWeaponType::Flare)
		{
			CreateFlare(*laraItem, ID_FLARE_ITEM, 0);
			UndrawFlareMeshes(*laraItem);
			lara->Control.Weapon.GunType = LaraWeaponType::None;
			lara->Control.Weapon.RequestGunType = LaraWeaponType::None;
			lara->Flare.ControlLeft = false;
		}
	}

	// TODO: This is a copy-pase of the player version, but I may need a different method for vehicles.
	short ModulateVehicleTurnRate(short turnRate, short accelRate, short minTurnRate, short maxTurnRate, float axisCoeff)
	{
		int sign = std::copysign(1, axisCoeff);
		short minTurnRateNormalized = minTurnRate * abs(axisCoeff);
		short maxTurnRateNormalized = maxTurnRate * abs(axisCoeff);

		short newTurnRate = (turnRate + (accelRate * sign)) * sign;
		newTurnRate = std::clamp(newTurnRate, minTurnRateNormalized, maxTurnRateNormalized);
		return newTurnRate * sign;
	}

	void ModulateVehicleTurnRateX(short* turnRate, short accelRate, short minTurnRate, short maxTurnRate)
	{
		*turnRate = ModulateVehicleTurnRate(*turnRate, accelRate, minTurnRate, maxTurnRate, -AxisMap[InputAxisID::Move].y);
	}

	void ModulateVehicleTurnRateY(short* turnRate, short accelRate, short minTurnRate, short maxTurnRate)
	{
		*turnRate = ModulateVehicleTurnRate(*turnRate, accelRate, minTurnRate, maxTurnRate, AxisMap[InputAxisID::Move].x);
	}
	
	void ModulateVehicleLean(ItemInfo* vehicleItem, short baseRate, short maxAngle)
	{
		float axisCoeff = AxisMap[InputAxisID::Move].x;
		int sign = copysign(1, axisCoeff);
		short maxAngleNormalized = maxAngle * axisCoeff;
		vehicleItem->Pose.Orientation.z += std::min<short>(baseRate, abs(maxAngleNormalized - vehicleItem->Pose.Orientation.z) / 3) * sign;
	}

	void ResetVehicleLean(ItemInfo* vehicleItem, float rate)
	{
		if (abs(vehicleItem->Pose.Orientation.z) > ANGLE(0.1f))
			vehicleItem->Pose.Orientation.z += vehicleItem->Pose.Orientation.z / -rate;
		else
			vehicleItem->Pose.Orientation.z = 0;
	}

	static std::pair<Vector3, Vector3> GetVehicleWakePositions(const ItemInfo& vehicleItem, const Vector3& relOffset, int waterHeight,
															   bool isUnderwater, bool isMovingForward)
	{
		constexpr auto HEIGHT_OFFSET_ON_WATER = (int)BLOCK(1 / 32.0f);
		
		waterHeight -= HEIGHT_OFFSET_ON_WATER;

		int vPos = isUnderwater ? vehicleItem.Pose.Position.y : waterHeight;
		auto posBase = Vector3(vehicleItem.Pose.Position.x, vPos, vehicleItem.Pose.Position.z);
		auto orient = isUnderwater ? vehicleItem.Pose.Orientation : EulerAngles(0, vehicleItem.Pose.Orientation.y, 0);
		auto rotMatrix = orient.ToRotationMatrix();

		// Calculate relative offsets.
		// NOTE: X and Z offsets are flipped accordingly.
		auto relOffsetLeft = Vector3(-relOffset.x, relOffset.y, isMovingForward ? relOffset.z : -relOffset.z);
		auto relOffsetRight = Vector3(relOffset.x, relOffset.y, isMovingForward ? relOffset.z : -relOffset.z);

		// Calculate positions.
		auto posLeft = posBase + Vector3::Transform(relOffsetLeft, rotMatrix);
		auto posRight = posBase + Vector3::Transform(relOffsetRight, rotMatrix);

		// Clamp vertical positions to water surface.
		posLeft.y = (posLeft.y < waterHeight) ? waterHeight : posLeft.y;
		posRight.y = (posRight.y < waterHeight) ? waterHeight : posRight.y;

		// Return left and right positions in pair.
		return std::pair(posLeft, posRight);
	}

	void SpawnVehicleWake(const ItemInfo& vehicleItem, const Vector3& relOffset, int waterHeight, bool isUnderwater)
	{
		constexpr auto COLOR_START		   = Color(0.75f, 0.75f, 0.75f, 0.75f);
		constexpr auto COLOR_END		   = Color(0.0f, 0.0f, 0.0f, 0.0f);
		constexpr auto LIFE_MAX			   = 2.5f;
		constexpr auto VEL_ABS			   = 4.0f;
		constexpr auto EXP_RATE_ON_WATER   = 6.0f;
		constexpr auto EXP_RATE_UNDERWATER = 1.5f;

		// Vehicle is out of water; return early.
		if (waterHeight == NO_HEIGHT)
			return;

		bool isMovingForward = (vehicleItem.Animation.Velocity.z >= 0.0f);

		// Determine tags.
		auto tagLeft = isMovingForward ? VehicleWakeEffectTag::FrontLeft : VehicleWakeEffectTag::BackLeft;
		auto tagRight = isMovingForward ? VehicleWakeEffectTag::FrontRight : VehicleWakeEffectTag::BackRight;

		// Determine key parameters.
		auto positions = GetVehicleWakePositions(vehicleItem, relOffset, waterHeight, isUnderwater, isMovingForward);
		auto dir = -vehicleItem.Pose.Orientation.ToDirection();
		short orient2D = isUnderwater ? vehicleItem.Pose.Orientation.z : 0;
		float life = isUnderwater ? (LIFE_MAX / 2) : LIFE_MAX;
		float vel = isMovingForward ? VEL_ABS : -VEL_ABS;
		float expRate = isUnderwater ? EXP_RATE_UNDERWATER : EXP_RATE_ON_WATER;

		// Spawn left wake.
		StreamerEffect.Spawn(
			vehicleItem.Index, (int)tagLeft,
			positions.first, dir, orient2D, COLOR_START, COLOR_END,
			0.0f, life, vel, expRate, 0,
			StreamerFeatherMode::Right, BlendMode::Additive);

		// Spawn right wake.
		StreamerEffect.Spawn(
			vehicleItem.Index, (int)tagRight,
			positions.second, dir, orient2D, COLOR_START, COLOR_END,
			0.0f, life, vel, expRate, 0,
			StreamerFeatherMode::Left, BlendMode::Additive);
	}

	void HandleVehicleSpeedometer(float vel, float velMax)
	{
		float value = abs(vel / velMax);
		g_Hud.Speedometer.UpdateValue(value);
	}
}
