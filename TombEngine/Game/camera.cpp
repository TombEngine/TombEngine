#include "framework.h"
#include "Game/camera.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Los.h"
#include "Game/collision/Point.h"
#include "Game/control/los.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_tests.h"
#include "Game/Lara/Optics.h"
#include "Game/room.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Game/spotcam.h"
#include "Game/StaticMesh.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/burning_torch.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Collision::Los;
using namespace TEN::Collision::Point;
using namespace TEN::Effects::Environment;
using namespace TEN::Entities::Generic;
using namespace TEN::Input;
using namespace TEN::Math;
using namespace TEN::Utils;
using TEN::Renderer::g_Renderer;

constexpr float CAMERA_OBJECT_COLL_DIST_THRESHOLD   = BLOCK(4);
constexpr float CAMERA_OBJECT_COLL_EXTENT_THRESHOLD = CLICK(0.5f);

CameraInfo       g_Camera;
ScreenEffectData g_ScreenEffect;

CameraLosCollisionData CameraInfo::GetLos(const Vector3& origin, int roomNumber, const Vector3& dir, float dist) const
{
	auto cameraLos = CameraLosCollisionData{};

	// 1) Get raw LOS collision.
	auto los = GetLosCollision(origin, roomNumber, dir, dist, true, false, true);

	// 2) Clip room.
	cameraLos.Normal = los.Room.Triangle.has_value() ? los.Room.Triangle->Normal : -dir;
	cameraLos.Position = los.Room.Position;
	cameraLos.RoomNumber = los.Room.RoomNumber;
	cameraLos.IsIntersected = los.Room.IsIntersected;
	cameraLos.Distance = los.Room.Distance;

	// 3) Clip item.
	for (const auto& itemLos : los.Items)
	{
		if (!TestCollidableMoveable(*itemLos.Item))
			continue;

		if (itemLos.Distance < cameraLos.Distance)
		{
			auto normal = itemLos.Item->GetObb().Center - origin;
			normal.Normalize();

			cameraLos.Normal = normal;
			cameraLos.Position = itemLos.Position;
			cameraLos.RoomNumber = itemLos.RoomNumber;
			cameraLos.IsIntersected = true;
			cameraLos.Distance = itemLos.Distance;
			break;
		}
	}

	// 4) Clip static.
	for (const auto& staticLos : los.Statics)
	{
		if (!TestCollidableStatic(*staticLos.Static))
			continue;

		if (staticLos.Distance < cameraLos.Distance)
		{
			auto normal = staticLos.Static->GetObb().Center - origin;
			normal.Normalize();

			cameraLos.Normal = normal;
			cameraLos.Position = staticLos.Position;
			cameraLos.RoomNumber = staticLos.RoomNumber;
			cameraLos.IsIntersected = true;
			cameraLos.Distance = staticLos.Distance;
			break;
		}
	}

	// 5) Return camera LOS collision.
	return cameraLos;
}

Vector3 CameraInfo::GetGeometryOffset() const
{
	return {};
}

Vector3 CameraInfo::GetPlayerOffset(const ItemInfo& item, const CollisionInfo& coll) const
{
	constexpr float VERTICAL_OFFSET_DEFAULT        = -BLOCK(0.05f);
	constexpr float VERTICAL_OFFSET_SWAMP          = BLOCK(0.4f);
	constexpr float VERTICAL_OFFSET_MONKEY_SWING   = BLOCK(0.25f);
	constexpr float VERTICAL_OFFSET_TREADING_WATER = BLOCK(0.5f);

	const auto& player = GetLaraInfo(item);

	bool isInSwamp = TestEnvironment(ENV_FLAG_SWAMP, item.RoomNumber);

	// Determine contextual vertical offset.
	float verticalOffset = coll.Setup.Height;
	if (player.Control.IsMonkeySwinging)
	{
		verticalOffset -= VERTICAL_OFFSET_MONKEY_SWING;
	}
	else if (player.Control.WaterStatus == WaterStatus::TreadWater)
	{
		verticalOffset -= VERTICAL_OFFSET_TREADING_WATER;
	}
	else if (isInSwamp)
	{
		verticalOffset = VERTICAL_OFFSET_SWAMP;
	}
	else
	{
		verticalOffset -= VERTICAL_OFFSET_DEFAULT;
	}

	// Get floor-to-ceiling height.
	auto pointColl = GetPointCollision(item);
	int floorToCeilHeight = abs(pointColl.GetCeilingHeight() - pointColl.GetFloorHeight());

	// Return offset.
	return Vector3(
		0.0f,
		-((verticalOffset < floorToCeilHeight) ? verticalOffset : floorToCeilHeight),
		0.0f);
}

EulerAngles CameraInfo::GetControlRotation() const
{
	constexpr float SLOW_ROT_COEFF                = 0.4f;
	constexpr float MOUSE_AXIS_SENSITIVITY_COEFF  = 20.0f;
	constexpr float CAMERA_AXIS_SENSITIVITY_COEFF = 12.0f;
	constexpr float SMOOTHING_FACTOR              = 8.0f;

	bool isUsingMouse = (GetCameraAxis() == Vector2::Zero);
	auto axisSign = Vector2(g_Configuration.InvertCameraXAxis ? -1 : 1, g_Configuration.InvertCameraYAxis ? -1 : 1);

	// Compute axis.
	auto axis = (isUsingMouse ? GetMouseAxis() : GetCameraAxis()) * axisSign;
	float sensitivityCoeff = isUsingMouse ? MOUSE_AXIS_SENSITIVITY_COEFF : CAMERA_AXIS_SENSITIVITY_COEFF;
	float sensitivity = sensitivityCoeff / (1.0f + (abs(axis.x) + abs(axis.y)));
	axis *= sensitivity * (isUsingMouse ? SMOOTHING_FACTOR : 1.0f);

	// Compute and return rotation.
	auto rotCoeff = IsHeld(In::Walk) ? SLOW_ROT_COEFF : 1.0f;
	return EulerAngles(ANGLE(axis.x), ANGLE(axis.y), ANGLE(0.0f)) * rotCoeff;
}

void CameraInfo::Prepare()
{
	if (!TrackCameraInit)
		return;

	UseSpotCam = false;
	SetFov(PrevFov);
	g_Renderer.RestoreDOF();
}

void CameraInfo::Update()
{
	// HACK: Disable interpolation when switching to/from flyby camera.
	// When camera structs are converted to a class, this should go to getter/setter. -- Lwmte, 29.10.2024
	if (UseSpotCam != SpotcamSwitched)
	{
		DisableInterpolation = true;
		SpotcamSwitched = UseSpotCam;
	}

	// Handle flyby cameras.
	if (UseSpotCam)
	{
		CalculateSpotCam();
	}
	// Handle standard camera.
	else
	{
		TrackCameraInit = false;
		CalculateCamera(*LaraItem, LaraCollision);
	}

	// Update cameras matrices after having done all possible camera logic.
	g_Renderer.UpdateCameraMatrices(g_Camera, BLOCK(g_GameFlow->GetLevel(CurrentLevel)->GetFarView()));
}

void CameraInfo::UpdateMovement(const ItemInfo& playerItem, Vector3 idealPos, int idealRoomNumber, float speed)
{
	constexpr float BUFFER = BLOCK(0.2f);

	const auto& player = GetLaraInfo(playerItem);

	if (player.Control.Look.IsUsingBinoculars)
		speed = 1.0f;

	UpdateAzimuthAngle(playerItem);
	UpdateListenerPosition(playerItem);

	PrevCamera.pos.Orientation = playerItem.Pose.Orientation;
	PrevCamera.pos2.Orientation.x = player.ExtraHeadRot.x;
	PrevCamera.pos2.Orientation.y = player.ExtraHeadRot.y;
	PrevCamera.pos2.Position.x = player.ExtraTorsoRot.x;
	PrevCamera.pos2.Position.y = player.ExtraTorsoRot.y;
	PrevCamera.pos.Position = playerItem.Pose.Position;
	PrevCamera.ActiveState = playerItem.Animation.ActiveState;
	PrevCamera.TargetState = playerItem.Animation.TargetState;
	PrevCamera.targetDistance = targetDistance;
	PrevCamera.targetElevation = targetElevation;
	PrevCamera.actualElevation = actualElevation;
	PrevCamera.actualAngle = actualAngle;
	PrevCamera.target = LookAt;
	PrevIdeal = idealPos;
	PrevIdealRoomNumber = idealRoomNumber;

	// Translate.
	Position = Vector3::Lerp(Position, idealPos, 1.0f / speed);
	RoomNumber = idealRoomNumber;

	// Apply geometry offset.
	Position += GetGeometryOffset();
	Offset = Vector3::Zero;

	// Get LOS.
	auto dir = Position - LookAt;
	dir.Normalize();
	float dist = Vector3::Distance(LookAt, Position);
	auto cameraLos = GetLos(LookAt, LookAtRoomNumber, dir, dist);

	// Apply LOS.
	Position = cameraLos.Position;
	RoomNumber = cameraLos.RoomNumber;

	Distance = Vector3::Distance(LookAt, Position);

	// TODO: Proper solution.
	// FAILSAFE: Prevent Position being equal to LookAt.
	if (Position == LookAt)
		LookAt += Vector3::One;

	// Bounce.
	if (bounce != 0)
	{
		if (bounce <= 0)
		{
			int bounce0 = -bounce;
			int bounce1 = bounce / 2;

			LookAt.x += Random::GenerateInt() % bounce0 - bounce1;
			LookAt.y += Random::GenerateInt() % bounce0 - bounce1;
			LookAt.z += Random::GenerateInt() % bounce0 - bounce1;
			bounce += 5;
			RumbleFromBounce();
		}
		else
		{
			Position.y += bounce;
			LookAt.y += bounce;
			bounce = 0;
		}
	}

	// Avoid entering swamp rooms.
	if (TestEnvironment(ENV_FLAG_SWAMP, RoomNumber))
		Position.y = g_Level.Rooms[RoomNumber].Position.y - BUFFER;

	RoomNumber = GetPointCollision(Position, RoomNumber).GetRoomNumber();
	HandleLookAt(*this, 0);
	oldType = type;
}

void CameraInfo::UpdateSphere(const ItemInfo& playerItem)
{
	constexpr float CONTROLLED_CAMERA_ROT_LERP_ALPHA = 0.5f;
	constexpr float COMBAT_CAMERA_REBOUND_ALPHA      = 0.3f;
	constexpr float LOCKED_CAMERA_ALTITUDE_ROT_ALPHA = 1 / 8.0f;

	const auto& player = GetLaraInfo(playerItem);

	if (laraNode != NO_VALUE)
	{
		auto origin = GetJointPosition(playerItem, laraNode, Vector3i::Zero);
		auto target = GetJointPosition(playerItem, laraNode, Vector3i(0, -CLICK(1), BLOCK(2)));
		auto deltaPos = target - origin;

		actualAngle = targetAngle + FROM_RAD(atan2(deltaPos.x, deltaPos.z));
		actualElevation += (targetElevation - actualElevation) * LOCKED_CAMERA_ALTITUDE_ROT_ALPHA;
		Rotation = EulerAngles::Identity;
	}
	else
	{
		// Modern camera.
		if (g_Configuration.IsUsingModernControls() && !player.Control.IsLocked)
		{
			Rotation.Lerp(GetControlRotation(), CONTROLLED_CAMERA_ROT_LERP_ALPHA);

			if (IsPlayerInCombat(playerItem))
			{
				short azimuthRot = Geometry::GetShortestAngle(actualAngle, (playerItem.Pose.Orientation.y + targetAngle) + Rotation.x);
				short altitudeRot = Geometry::GetShortestAngle(actualElevation, targetElevation - Rotation.y);

				actualAngle += azimuthRot * COMBAT_CAMERA_REBOUND_ALPHA;
				actualElevation += altitudeRot * COMBAT_CAMERA_REBOUND_ALPHA;
			}
			else
			{
				actualAngle += Rotation.x;
				actualElevation -= Rotation.y;
			}
		}
		// Tank camera.
		else
		{
			if (CanControlTankCamera(playerItem))
			{
				Rotation.Lerp(GetControlRotation(), CONTROLLED_CAMERA_ROT_LERP_ALPHA);

				actualAngle += Rotation.x;
				actualElevation -= Rotation.y;
				IsControllingTankCamera = true;
			}
			else
			{
				actualAngle = playerItem.Pose.Orientation.y + targetAngle;
				actualElevation += (targetElevation - actualElevation) * LOCKED_CAMERA_ALTITUDE_ROT_ALPHA;
				Rotation.Lerp(EulerAngles::Identity, CONTROLLED_CAMERA_ROT_LERP_ALPHA);
				IsControllingTankCamera = false;
			}
		}
	}
}

void CameraInfo::UpdateListenerPosition(const ItemInfo& item)
{
	float persp = ((g_Configuration.ScreenWidth / 2) * phd_cos(Fov / 2)) / phd_sin(Fov / 2);
	ListenerPosition = Position + (persp * Vector3(phd_sin(actualAngle), 0.0f, phd_cos(actualAngle)));
}

void CameraInfo::HandleFollow(const ItemInfo& playerItem, bool isCombatCamera)
{
	constexpr float MODERN_CAMERA_DIST_LERP_ALPHA   = 0.4f;
	constexpr short STRAFE_CAMERA_FOV               = ANGLE(86.0f);
	constexpr float STRAFE_CAMERA_FOV_LERP_ALPHA    = 0.5f;
	constexpr float STRAFE_CAMERA_DIST_OFFSET_COEFF = 0.5f;
	constexpr float STRAFE_CAMERA_ZOOM_BUFFER       = BLOCK(0.1f);
	constexpr int   TANK_CAMERA_SWIVEL_STEP_COUNT   = 4;
	constexpr float TANK_CAMERA_CLOSE_DIST_MIN      = BLOCK(0.75f);

	const auto& player = GetLaraInfo(playerItem);

	ClampAltitudeAngle(player.Control.WaterStatus == WaterStatus::Underwater);

	// Move camera.
	if (g_Configuration.IsUsingModernControls() || IsControllingTankCamera)
	{
		// Get LOS.
		auto dir = -EulerAngles(actualElevation, actualAngle, 0).ToDirection();
		float dist = Lerp(Distance, targetDistance, MODERN_CAMERA_DIST_LERP_ALPHA);
		auto cameraLos = GetLos(LookAt, LookAtRoomNumber, dir, dist);

		// Define ideal position.
		auto idealPos = cameraLos.Position;
		int idealRoomNumber = cameraLos.RoomNumber;

		// Apply strafe camera effects.
		if (IsPlayerStrafing(playerItem))
		{
			SetFov((short)Lerp(Fov, STRAFE_CAMERA_FOV, STRAFE_CAMERA_FOV_LERP_ALPHA));

			// Apply zoom if using Look action to strafe.
			if (TestStrafeZoom(playerItem))
			{
				float dist = std::max(Vector3::Distance(LookAt, idealPos) * STRAFE_CAMERA_DIST_OFFSET_COEFF, STRAFE_CAMERA_ZOOM_BUFFER);
				idealPos = Geometry::TranslatePoint(LookAt, dir, dist);
				idealRoomNumber = GetPointCollision(LookAt, LookAtRoomNumber, dir, dist).GetRoomNumber();
			}
		}
		else
		{
			SetFov((short)Lerp(Fov, ANGLE(DEFAULT_FOV), STRAFE_CAMERA_FOV_LERP_ALPHA / 2));
		}

		float speedCoeff = (type != CameraType::Look) ? 0.2f : 1.0f;
		UpdateMovement(playerItem, idealPos, idealRoomNumber, speed * speedCoeff);
	}
	else
	{
		auto farthestIdealPos = Position;
		int farthestIdealRoomNumber = RoomNumber;
		short farthestIdealAzimuthAngle = actualAngle;
		float farthestDistSqr = FLT_MAX;

		// Determine ideal position around player.
		for (int i = 0; i < (TANK_CAMERA_SWIVEL_STEP_COUNT + 1); i++)
		{
			// Calcuate azimuth angle and direction of swivel step.
			short azimuthAngle = (i == 0) ? actualAngle : (ANGLE(360.0f / TANK_CAMERA_SWIVEL_STEP_COUNT) * (i - 1));
			auto dir = -EulerAngles(actualElevation, azimuthAngle, 0).ToDirection();

			// Get camera LOS.
			auto cameraLos = GetLos(LookAt, LookAtRoomNumber, dir, targetDistance);

			// Has no LOS intersection.
			if (!cameraLos.IsIntersected)
			{
				// Initial swivel is clear; set ideal.
				if (i == 0)
				{
					farthestIdealPos = cameraLos.Position;
					farthestIdealAzimuthAngle = azimuthAngle;
					break;
				}

				// Track closest ideal.
				float distSqr = Vector3::DistanceSquared(Position, cameraLos.Position);
				if (distSqr < farthestDistSqr)
				{
					farthestIdealPos = cameraLos.Position;
					farthestIdealAzimuthAngle = azimuthAngle;
					farthestDistSqr = distSqr;
				}
			}
			// Has LOS intersection and is initial swivel step.
			else if (i == 0)
			{
				float distSqr = Vector3::DistanceSquared(LookAt, cameraLos.Position);
				if (distSqr > SQUARE(TANK_CAMERA_CLOSE_DIST_MIN))
				{
					farthestIdealPos = cameraLos.Position;
					farthestIdealAzimuthAngle = azimuthAngle;
					farthestDistSqr = distSqr;
					break;
				}
			}
		}

		actualAngle = farthestIdealAzimuthAngle;

		// Snap position of fixed camera.
		if (isCombatCamera && oldType == CameraType::Fixed)
			speed = 1.0f;

		UpdateMovement(playerItem, farthestIdealPos, farthestIdealRoomNumber, speed);
	}
}

void CameraInfo::RumbleFromBounce()
{
	Rumble(std::clamp(abs(bounce) / 70.0f, 0.0f, 0.8f), 0.2f);
}

void CameraInfo::LookCamera(const ItemInfo& playerItem, const CollisionInfo& coll)
{
	constexpr float DIST_COEFF = 0.5f;

	const auto& player = GetLaraInfo(playerItem);

	// Determine base position.
	bool isInSwamp = TestEnvironment(ENV_FLAG_SWAMP, playerItem.RoomNumber);
	auto basePos = Vector3(
		playerItem.Pose.Position.x,
		isInSwamp ? g_Level.Rooms[playerItem.RoomNumber].TopHeight : playerItem.Pose.Position.y,
		playerItem.Pose.Position.z);

	// Get camera LOS.
	auto orient = player.Control.Look.Orientation + EulerAngles(playerItem.Pose.Orientation.x, playerItem.Pose.Orientation.y + targetAngle, 0);
	orient.x = std::clamp(orient.x, LOOKCAM_ORIENT_CONSTRAINT.first.x, LOOKCAM_ORIENT_CONSTRAINT.second.x);
	auto cameraLos = GetLos(LookAt, LookAtRoomNumber, -orient.ToDirection(), targetDistance * DIST_COEFF);

	// Update camera.
	auto lookAtTarget = basePos + GetPlayerOffset(playerItem, coll);
	LookAt += (lookAtTarget - LookAt) * (1.0f / speed);
	UpdateMovement(playerItem, cameraLos.Position, cameraLos.RoomNumber, speed);
}

bool CameraInfo::IsLocked() const
{
	// Check if break condition is met.
	if (type != CameraType::Look && type != CameraType::Combat)
		return true;

	// Check if there's an active fixed camera.
	if (number == NO_VALUE)
		return true;

	// Check if locked bit is set for a given fixed camera.
	if (!(g_Level.Cameras[number].Flags & (int)LevelCameraFlags::Locked))
		return false;

	return true;
}

bool CameraInfo::CanControlTankCamera(const ItemInfo& playerItem) const
{
	if (!g_Configuration.EnableTankCameraControl)
		return false;

	const auto& player = GetLaraInfo(playerItem);
	if (player.Control.IsLocked)
		return false;

	bool isUsingMouse = (GetCameraAxis() == Vector2::Zero);
	const auto& axis = isUsingMouse ? GetMouseAxis() : GetCameraAxis();

	// Look input action resets camera.
	if (IsReleased(In::Look))
		return false;

	// Test if player is stationary.
	if (!IsWakeActionHeld() && (axis != Vector2::Zero || g_Camera.IsControllingTankCamera))
		return true;

	// Test if player is moving and camera or mouse axis isn't zero.
	if (IsWakeActionHeld() && axis != Vector2::Zero)
		return true;

	return false;
}

bool CameraInfo::TestStrafeZoom(const ItemInfo& playerItem) const
{
	const auto& player = GetLaraInfo(playerItem);

	if (!g_Configuration.IsUsingModernControls())
		return false;

	if (player.Control.HandStatus == HandStatus::WeaponDraw ||
		player.Control.HandStatus == HandStatus::WeaponReady)
	{
		return false;
	}

	if (IsHeld(In::Look))
		return true;

	return false;
}

bool CameraInfo::TestCollidableMoveable(const ItemInfo& mov) const
{
	// 1) Check if item is player or bridge.
	if (mov.IsLara())
		return false;

	// 2) Test distance.
	float distSqr = Vector3i::DistanceSquared(mov.Pose.Position, Position);
	if (distSqr >= SQUARE(CAMERA_OBJECT_COLL_DIST_THRESHOLD))
		return false;

	// 3) Check object collidability.
	const auto& object = Objects[mov.ObjectNumber];
	if (!mov.Collidable/* || !object.usingDrawAnimatingItem*/)
		return false;

	// 4) Check object attributes.
	if (object.intelligent || object.isPickup || object.isPuzzleHole || object.collision == nullptr)
		return false;

	// 5) Test if OBB is collidable.
	if (!TestCollidableObb(mov.GetObb()))
		return false;

	return true;
}

bool CameraInfo::TestCollidableStatic(const StaticMesh& staticObj) const
{
	// 1) Test distance.
	float distSqr = Vector3i::DistanceSquared(Position, staticObj.Pose.Position);
	if (distSqr >= SQUARE(CAMERA_OBJECT_COLL_DIST_THRESHOLD))
		return false;

	// 2) Test if OBB is collidable.
	if (!TestCollidableObb(staticObj.GetObb()))
		return false;

	return true;
}

bool CameraInfo::TestCollidableObb(const BoundingOrientedBox& obb) const
{
	// Test if any 2 OBB extents are below threshold.
	if ((abs(obb.Extents.x) < CAMERA_OBJECT_COLL_EXTENT_THRESHOLD && abs(obb.Extents.y) < CAMERA_OBJECT_COLL_EXTENT_THRESHOLD) ||
		(abs(obb.Extents.x) < CAMERA_OBJECT_COLL_EXTENT_THRESHOLD && abs(obb.Extents.z) < CAMERA_OBJECT_COLL_EXTENT_THRESHOLD) ||
		(abs(obb.Extents.y) < CAMERA_OBJECT_COLL_EXTENT_THRESHOLD && abs(obb.Extents.z) < CAMERA_OBJECT_COLL_EXTENT_THRESHOLD))
	{
		return false;
	}

	return true;
}

void CameraInfo::UpdateAzimuthAngle(const ItemInfo& item)
{
	constexpr auto BASE_VEL					= BLOCK(0.1f);
	constexpr auto BASE_ANGLE				= ANGLE(90.0f);
	constexpr auto AUTO_ROT_DELTA_ANGLE_MAX = BASE_ANGLE * 1.5f;
	constexpr auto AZIMUTH_ANGLE_LERP_ALPHA = 0.075f;

	if (!g_Configuration.IsUsingModernControls() || IsPlayerStrafing(item))
		return;

	float vel = Vector2(item.Animation.Velocity.x, item.Animation.Velocity.z).Length();
	if (GetMoveAxis() == Vector2::Zero || vel == 0.0f)
		return;

	float alpha = std::clamp(vel / BASE_VEL, 0.0f, 1.0f);

	short deltaAngle = Geometry::GetShortestAngle(actualAngle, GetPlayerHeadingAngleY(item));
	if (abs(deltaAngle) <= BASE_ANGLE)
	{
		actualAngle += deltaAngle * (AZIMUTH_ANGLE_LERP_ALPHA * alpha);
	}
	else if (abs(deltaAngle) <= AUTO_ROT_DELTA_ANGLE_MAX)
	{
		int sign = std::copysign(1, deltaAngle);
		actualAngle += (BASE_ANGLE * (AZIMUTH_ANGLE_LERP_ALPHA * alpha)) * sign;
	}
}

void CameraInfo::ClampAltitudeAngle(bool isUnderwater)
{
	constexpr auto ANGLE_CONSTRAINT = std::pair<short, short>(ANGLE(-80.0f), ANGLE(70.0f));

	if (actualElevation > ANGLE_CONSTRAINT.second)
	{
		actualElevation = ANGLE_CONSTRAINT.second;
	}
	else if (actualElevation < ANGLE_CONSTRAINT.first)
	{
		actualElevation = ANGLE_CONSTRAINT.first;
	}
}

void UpdatePlayerRefCameraOrient(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);

	float vel = Vector2(item.Animation.Velocity.x, item.Animation.Velocity.z).Length();

	bool isSpotCameraSwitch = (UseSpotCam != PrevUseSpotCam); // TODO: Fix this.
	bool isStopped = (((GetMoveAxis() == Vector2::Zero && !IsHeld(In::StepLeft) && !IsHeld(In::StepRight)) || vel == 0.0f) &&
					  TestState(item.Animation.ActiveState, PLAYER_IDLE_STATE_IDS));

	// Set lock.
	if (isSpotCameraSwitch && !isStopped)
	{
		player.Control.LockRefCameraOrient = true;
	}
	else if (isStopped)
	{
		player.Control.LockRefCameraOrient = false;
	}

	// Update orientation.
	if (!player.Control.LockRefCameraOrient)
		player.Control.RefCameraOrient = EulerAngles(g_Camera.actualElevation, g_Camera.actualAngle, 0);
}

void HandleLookAt(const CameraInfo& camera, short roll)
{
	float farView = BLOCK(g_GameFlow->GetLevel(CurrentLevel)->GetFarView());
	g_Renderer.UpdateCameraMatrices(camera, farView);
}

void SetFov(short fov, bool store)
{
	if (store)
		g_Camera.PrevFov = fov;

	g_Camera.Fov = fov;
}

void InitializeCamera()
{
	g_Camera.shift = LaraItem->Pose.Position.y - BLOCK(1);

	g_Camera.PrevTarget = GameVector(
		LaraItem->Pose.Position.x,
		g_Camera.shift,
		LaraItem->Pose.Position.z,
		LaraItem->RoomNumber);

	g_Camera.LookAt = Vector3(g_Camera.PrevTarget.x, g_Camera.shift, g_Camera.PrevTarget.z);
	g_Camera.LookAtRoomNumber = LaraItem->RoomNumber;

	g_Camera.Position = Vector3(g_Camera.PrevTarget.x, g_Camera.shift, g_Camera.PrevTarget.z - 100);
	g_Camera.RoomNumber = LaraItem->RoomNumber;

	g_Camera.targetDistance = BLOCK(1.5f);
	g_Camera.Radius = BLOCK(0.05f);
	g_Camera.item = nullptr;
	g_Camera.numberFrames = 1;
	g_Camera.type = CameraType::Chase;
	g_Camera.speed = 1.0f;
	g_Camera.flags = CameraFlag::None;
	g_Camera.bounce = 0;
	g_Camera.number = NO_VALUE;
	g_Camera.fixedCamera = false;

	SetFov(ANGLE(DEFAULT_FOV));

	g_Camera.UseForcedFixedCamera = false;
	CalculateCamera(*LaraItem, LaraCollision);

	// Fade in screen.
	SetScreenFadeIn(FADE_SCREEN_SPEED);
}

void ObjCamera(ItemInfo* item, int boneID, ItemInfo* targetItem, int targetBoneID, bool cond)
{
	// item and targetItem remain same object until it becomes possible to extend targetItem to another object.
	// Activates code below -> void CalculateCamera().
	g_Camera.ItemCamera.ItemCameraOn = cond;

	g_Camera.UpdateSphere(*LaraItem);

	// Get position of bone 0.	
	auto pos = GetJointPosition(item, 0, Vector3i::Zero);
	auto target = GameVector(pos, item->RoomNumber);

	g_Camera.fixedCamera = true;

	MoveObjCamera(&target, item, boneID, targetItem, targetBoneID);
	g_Camera.timer = NO_VALUE;
}

void ClearObjCamera()
{
	g_Camera.ItemCamera.ItemCameraOn = false;
}

void MoveObjCamera(GameVector* ideal, ItemInfo* item, int boneID, ItemInfo* targetItem, int targetBoneID)
{
	constexpr float ANGLE_THRESHOLD_DEG = 100.0f;

	auto idealPos = GetJointPosition(item, boneID, Vector3i::Zero).ToVector3();
	auto lookAt = GetJointPosition(targetItem, targetBoneID, Vector3i::Zero).ToVector3();

	if (g_Camera.PrevCamera.pos.Position != idealPos ||
		g_Camera.PrevCamera.targetDistance != g_Camera.targetDistance  ||
		g_Camera.PrevCamera.targetElevation != g_Camera.targetElevation ||
		g_Camera.PrevCamera.actualElevation != g_Camera.actualElevation ||
		g_Camera.PrevCamera.actualAngle != g_Camera.actualAngle ||
		g_Camera.PrevCamera.target != g_Camera.LookAt ||
		g_Camera.oldType != g_Camera.type ||
		Lara.Control.Look.IsUsingBinoculars)
	{
		g_Camera.PrevCamera.pos.Position = idealPos;
		g_Camera.PrevCamera.targetDistance = g_Camera.targetDistance;
		g_Camera.PrevCamera.targetElevation = g_Camera.targetElevation;
		g_Camera.PrevCamera.actualElevation = g_Camera.actualElevation;
		g_Camera.PrevCamera.actualAngle = g_Camera.actualAngle;
		g_Camera.PrevCamera.target = g_Camera.LookAt;
		g_Camera.PrevIdeal = idealPos;
		g_Camera.PrevIdealRoomNumber = ideal->RoomNumber;
		g_Camera.PrevTarget = Vector3i(lookAt);
	}
	else
	{
		idealPos  = g_Camera.PrevIdeal;
		lookAt = g_Camera.PrevTarget.ToVector3();
		ideal->RoomNumber = g_Camera.PrevIdealRoomNumber;
	}

	float speedAlpha = 1.0f;

	g_Camera.Position = Vector3::Lerp(g_Camera.Position, ideal->ToVector3(), speedAlpha);
	g_Camera.RoomNumber = GetPointCollision(g_Camera.Position, g_Camera.RoomNumber).GetRoomNumber();
	HandleLookAt(g_Camera, 0);

	auto angle = g_Camera.LookAt - g_Camera.Position;
	auto position = Vector3i(g_Camera.LookAt - g_Camera.Position);

	// Store previous frame's camera angle to LastAngle to compare if next frame camera angle has bigger step than 100.
	// To move camera smoothely, speed alpha 0.5 is set.
	// To cut immediately, speed alpha 1 is set.

	if (g_Camera.PrevTarget.x - g_Camera.LookAt.x > ANGLE_THRESHOLD_DEG ||
		g_Camera.PrevTarget.y - g_Camera.LookAt.y > ANGLE_THRESHOLD_DEG ||
		g_Camera.PrevTarget.z - g_Camera.LookAt.z > ANGLE_THRESHOLD_DEG)
	{
		speedAlpha = 1.0f;
	}
	else
	{
		speedAlpha = 0.5f;
	}

	// Move lookAt.
	g_Camera.LookAt = Vector3::Lerp(g_Camera.LookAt, lookAt, speedAlpha);

	if (g_Camera.ItemCamera.LastAngle != position)
	{
		g_Camera.ItemCamera.LastAngle = Vector3i(
			g_Camera.ItemCamera.LastAngle.x = angle.x,
			g_Camera.ItemCamera.LastAngle.y = angle.y,
			g_Camera.ItemCamera.LastAngle.z = angle.z);
	}
}

void RefreshFixedCamera(int cameraID)
{
	const auto& camera = g_Level.Cameras[cameraID];

	int speed = (camera.Speed * 8) + 1.0f;
	g_Camera.UpdateMovement(*LaraItem, camera.Position.ToVector3(), camera.RoomNumber, speed);
}

void ChaseCamera(const ItemInfo& playerItem)
{
	const auto& player = GetLaraInfo(playerItem);

	if (g_Camera.targetElevation == 0)
		g_Camera.targetElevation = ANGLE(-10.0f);

	g_Camera.targetElevation += playerItem.Pose.Orientation.x;
	g_Camera.UpdateSphere(playerItem);

	auto pointColl = GetPointCollision(g_Camera.LookAt, g_Camera.LookAtRoomNumber, 0, 0, CLICK(1));

	if (TestEnvironment(ENV_FLAG_SWAMP, pointColl.GetRoomNumber()))
		g_Camera.LookAt.y = g_Level.Rooms[pointColl.GetRoomNumber()].TopHeight - CLICK(1);

	int vPos = g_Camera.LookAt.y;
	pointColl = GetPointCollision(Vector3i(g_Camera.LookAt.x, vPos, g_Camera.LookAt.z), g_Camera.LookAtRoomNumber);
	if (((vPos < pointColl.GetCeilingHeight() || pointColl.GetFloorHeight() < vPos) || pointColl.GetFloorHeight() <= pointColl.GetCeilingHeight()) ||
		(pointColl.GetFloorHeight() == NO_HEIGHT || pointColl.GetCeilingHeight() == NO_HEIGHT))
	{
		g_Camera.TargetSnaps++;
		g_Camera.LookAt = g_Camera.PrevTarget.ToVector3();
		g_Camera.LookAtRoomNumber = g_Camera.PrevTarget.RoomNumber;
	}
	else
	{
		g_Camera.TargetSnaps = 0;
	}

	g_Camera.HandleFollow(playerItem, false);
}

void CombatCamera(const ItemInfo& playerItem)
{
	constexpr float BUFFER = CLICK(0.25f);

	const auto& player = GetLaraInfo(playerItem);

	g_Camera.LookAt.x = playerItem.Pose.Position.x;
	g_Camera.LookAt.z = playerItem.Pose.Position.z;

	if (player.TargetEntity != nullptr)
	{
		g_Camera.targetAngle = player.TargetArmOrient.y;
		g_Camera.targetElevation = player.TargetArmOrient.x + playerItem.Pose.Orientation.x;
	}
	else
	{
		g_Camera.targetAngle = player.ExtraHeadRot.y + player.ExtraTorsoRot.y;
		g_Camera.targetElevation = player.ExtraHeadRot.x + player.ExtraTorsoRot.x + playerItem.Pose.Orientation.x - ANGLE(15.0f);
	}

	auto pointColl = GetPointCollision(g_Camera.LookAt, g_Camera.LookAtRoomNumber, 0, 0, CLICK(1));
	if (TestEnvironment(ENV_FLAG_SWAMP, pointColl.GetRoomNumber()))
		g_Camera.LookAt.y = g_Level.Rooms[pointColl.GetRoomNumber()].Position.y - CLICK(1);

	pointColl = GetPointCollision(g_Camera.LookAt, g_Camera.LookAtRoomNumber);
	g_Camera.LookAtRoomNumber = pointColl.GetRoomNumber();

	if ((pointColl.GetCeilingHeight() + BUFFER) > (pointColl.GetFloorHeight() - BUFFER) &&
		pointColl.GetFloorHeight() != NO_HEIGHT &&
		pointColl.GetCeilingHeight() != NO_HEIGHT)
	{
		g_Camera.LookAt.y = (pointColl.GetCeilingHeight() + pointColl.GetFloorHeight()) / 2;
		g_Camera.targetElevation = 0;
	}
	else if (g_Camera.LookAt.y > (pointColl.GetFloorHeight() - BUFFER) &&
		pointColl.GetFloorHeight() != NO_HEIGHT)
	{
		g_Camera.LookAt.y = pointColl.GetFloorHeight() - BUFFER;
		g_Camera.targetElevation = 0;
	}
	else if (g_Camera.LookAt.y < (pointColl.GetCeilingHeight() + BUFFER) &&
		pointColl.GetCeilingHeight() != NO_HEIGHT)
	{
		g_Camera.LookAt.y = pointColl.GetCeilingHeight() + BUFFER;
		g_Camera.targetElevation = 0;
	}

	int y = g_Camera.LookAt.y;
	pointColl = GetPointCollision(Vector3i(g_Camera.LookAt.x, y, g_Camera.LookAt.z), g_Camera.LookAtRoomNumber);
	g_Camera.LookAtRoomNumber = pointColl.GetRoomNumber();

	if (y < pointColl.GetCeilingHeight() ||
		y > pointColl.GetFloorHeight() ||
		pointColl.GetCeilingHeight() >= pointColl.GetFloorHeight() ||
		pointColl.GetFloorHeight() == NO_HEIGHT ||
		pointColl.GetCeilingHeight() == NO_HEIGHT)
	{
		g_Camera.TargetSnaps++;
		g_Camera.LookAt = g_Camera.PrevTarget.ToVector3();
		g_Camera.LookAtRoomNumber = g_Camera.PrevTarget.RoomNumber;
	}
	else
	{
		g_Camera.TargetSnaps = 0;
	}

	g_Camera.UpdateSphere(playerItem);

	g_Camera.targetDistance = BLOCK(1.5f);

	g_Camera.HandleFollow(playerItem, true);
}

void FixedCamera()
{
	// Fixed cameras before TR3 had optional "movement" effect. 
	// Later for some reason it was forced to always be 1, and actual speed value
	// from camera trigger was ignored. In TEN, speed value was moved out out of legacy
	// floordata trigger to camera itself to make use of it again. Still, by default,
	// value is 1 for UseForcedFixedCamera hack.

	float speed = 1.0f;

	auto origin = GameVector::Zero;
	if (g_Camera.UseForcedFixedCamera)
	{
		origin = g_Camera.ForcedFixedCamera;
	}
	else
	{
		const auto& camera = g_Level.Cameras[g_Camera.number];

		origin = GameVector(camera.Position, camera.RoomNumber);
		speed = (camera.Speed * 8) + 1.0f; // Multiply original speed by 8 to comply with original bitshifted speed from TR1-2.
	}

	g_Camera.fixedCamera = true;

	g_Camera.UpdateMovement(*LaraItem, origin.ToVector3(), origin.RoomNumber, speed);

	if (g_Camera.timer)
	{
		if (!--g_Camera.timer)
			g_Camera.timer = NO_VALUE;
	}
}

void BounceCamera(ItemInfo* item, int bounce, float distMax)
{
	float dist = Vector3i::Distance(item->Pose.Position.ToVector3(), g_Camera.Position);
	if (dist < distMax)
	{
		if (distMax == NO_VALUE)
		{
			g_Camera.bounce = bounce;
		}
		else
		{
			g_Camera.bounce = -(bounce * (distMax - dist) / distMax);
		}
	}
	else if (distMax == NO_VALUE)
	{
		g_Camera.bounce = bounce;
	}
}

void BinocularCamera(ItemInfo* item)
{
	auto& player = GetLaraInfo(*item);

	if (!player.Control.Look.IsUsingLasersight)
	{
		if (IsClicked(In::Deselect) ||
			IsClicked(In::Draw) ||
			IsClicked(In::Look) ||
			IsHeld(In::Flare))
		{
			ResetPlayerFlex(item);
			player.Control.Look.OpticRange = 0;
			player.Control.Look.IsUsingBinoculars = false;
			player.Inventory.IsBusy = false;

			g_Camera.type = g_Camera.PrevBinocularCameraType;
			SetFov(g_Camera.PrevFov);
			return;
		}
	}

	SetFov(7 * (ANGLE(11.5f) - player.Control.Look.OpticRange), false);

	int x = item->Pose.Position.x;
	int y = item->Pose.Position.y + GameBoundingBox(item).Y1;
	int z = item->Pose.Position.z;

	auto pointColl = GetPointCollision(Vector3i(x, y, z), item->RoomNumber);

	g_Camera.Position.x = x;
	g_Camera.Position.y = y;
	g_Camera.Position.z = z;
	g_Camera.RoomNumber = pointColl.GetRoomNumber();

	float l = BLOCK(20.25f) * phd_cos(player.Control.Look.Orientation.x);
	float tx = x + l * phd_sin(item->Pose.Orientation.y + player.Control.Look.Orientation.y);
	float ty = y - BLOCK(20.25f) * phd_sin(player.Control.Look.Orientation.x);
	float tz = z + l * phd_cos(item->Pose.Orientation.y + player.Control.Look.Orientation.y);

	if (g_Camera.oldType == CameraType::Fixed)
	{
		g_Camera.LookAt.x = tx;
		g_Camera.LookAt.y = ty;
		g_Camera.LookAt.z = tz;
		g_Camera.LookAtRoomNumber = item->RoomNumber;
	}
	else
	{
		g_Camera.LookAt.x += (tx - g_Camera.LookAt.x) / 4;
		g_Camera.LookAt.y += (ty - g_Camera.LookAt.y) / 4;
		g_Camera.LookAt.z += (tz - g_Camera.LookAt.z) / 4;
		g_Camera.LookAtRoomNumber = item->RoomNumber;
	}

	if (g_Camera.bounce &&
		g_Camera.type == g_Camera.oldType)
	{
		if (g_Camera.bounce <= 0)
		{
			g_Camera.LookAt.x += (CLICK(0.25f) / 4) * (Random::GenerateInt() % (-g_Camera.bounce) - (-g_Camera.bounce / 2));
			g_Camera.LookAt.y += (CLICK(0.25f) / 4) * (Random::GenerateInt() % (-g_Camera.bounce) - (-g_Camera.bounce / 2));
			g_Camera.LookAt.z += (CLICK(0.25f) / 4) * (Random::GenerateInt() % (-g_Camera.bounce) - (-g_Camera.bounce / 2));
			g_Camera.bounce += 5;
			g_Camera.RumbleFromBounce();
		}
		else
		{
			g_Camera.bounce = 0;
			g_Camera.LookAt.y += g_Camera.bounce;
		}
	}

	g_Camera.LookAtRoomNumber = GetPointCollision(g_Camera.Position, g_Camera.LookAtRoomNumber).GetRoomNumber();
	HandleLookAt(g_Camera, 0);
	g_Camera.UpdateListenerPosition(*item);
	g_Camera.oldType = g_Camera.type;

	auto origin0 = GameVector(g_Camera.Position, g_Camera.RoomNumber);
	auto target0 = GameVector(g_Camera.LookAt, g_Camera.LookAtRoomNumber);
	GetTargetOnLOS(&origin0, &target0);
	g_Camera.LookAt = target0.ToVector3();
	g_Camera.LookAtRoomNumber = target0.RoomNumber;

	if (IsHeld(In::Action))
	{
		auto origin = Vector3i(g_Camera.Position);
		auto target = Vector3i(g_Camera.LookAt);
		//LaraTorch(&origin, &target, player.ExtraHeadRot.y, 192); // TODO: Restore.
	}
}

void ConfirmCameraTargetPos()
{
	auto pos = Vector3(
		LaraItem->Pose.Position.x,
		LaraItem->Pose.Position.y - (LaraCollision.Setup.Height / 2),
		LaraItem->Pose.Position.z);

	if (g_Camera.laraNode != NO_VALUE)
	{
		g_Camera.LookAt = pos;
	}
	else
	{
		g_Camera.LookAt = Vector3(LaraItem->Pose.Position.x, (g_Camera.LookAt.y + pos.y) / 2, LaraItem->Pose.Position.z);
	}

	int y = g_Camera.LookAt.y;
	auto pointColl = GetPointCollision(Vector3i(g_Camera.LookAt.x, y, g_Camera.LookAt.z), g_Camera.LookAtRoomNumber);
	if (y < pointColl.GetCeilingHeight() ||
		pointColl.GetFloorHeight() < y ||
		pointColl.GetFloorHeight() <= pointColl.GetCeilingHeight() ||
		pointColl.GetFloorHeight() == NO_HEIGHT ||
		pointColl.GetCeilingHeight() == NO_HEIGHT)
	{
		g_Camera.LookAt = pos;
	}
}

// HACK: Temporary fix for camera bouncing on slopes during player death.
static bool CalculateDeathCamera(const ItemInfo& item)
{
	// If player is alive, it's not a death camera.
	if (item.HitPoints > 0)
		return false;

	// If player is in a special death animation (from EXTRA_ANIMS slot) triggered by enemies.
	if (item.Animation.AnimObjectID == ID_LARA_EXTRA_ANIMS)
		return true;

	// Special death animations.
	if (item.Animation.AnimNumber == LA_SPIKE_DEATH ||
		item.Animation.AnimNumber == LA_TRAIN_OVERBOARD_DEATH)
	{
		return true;
	}

	return false;
}

void CalculateCamera(ItemInfo& playerItem, const CollisionInfo& coll)
{
	auto& player = GetLaraInfo(playerItem);

	if (player.Control.Look.IsUsingBinoculars)
	{
		BinocularCamera(&playerItem);
		return;
	}

	if (g_Camera.ItemCamera.ItemCameraOn)
		return;

	if (g_Camera.UseForcedFixedCamera)
	{
		g_Camera.type = CameraType::Fixed;
		if (g_Camera.oldType != CameraType::Fixed)
			g_Camera.speed = 1.0f;
	}

	// Play water sound effect if camera is in water room.
	if (TestEnvironment(ENV_FLAG_WATER, g_Camera.RoomNumber))
	{
		SoundEffect(SFX_TR4_UNDERWATER, nullptr, SoundEnvironment::Always);
		if (g_Camera.underwater == false)
			g_Camera.underwater = true;
	}
	else
	{
		if (g_Camera.underwater == true)
			g_Camera.underwater = false;
	}

	const ItemInfo* item = nullptr;
	bool isFixedCamera = false;
	if (g_Camera.item != nullptr && (g_Camera.type == CameraType::Fixed || g_Camera.type == CameraType::Heavy))
	{
		item = g_Camera.item;
		isFixedCamera = true;
	}
	else
	{
		item = &playerItem;
		isFixedCamera = false;
	}

	// TODO: Use DX box.
	auto box = item->GetObb();
	auto bounds = GameBoundingBox(item);

	int x = 0;
	int y = item->Pose.Position.y + bounds.Y2 + ((bounds.Y1 - bounds.Y2) / 2 * 1.5f);
	int z = 0;
	if (item->IsLara())
	{
		float heightCoeff = g_Configuration.IsUsingModernControls() ? 0.9f : 0.75f;
		auto offset = g_Camera.GetPlayerOffset(*item, coll) * heightCoeff;
		y = item->Pose.Position.y + offset.y;
	}

	// Make player look toward target item.
	if (g_Camera.item != nullptr)
	{
		if (!isFixedCamera)
		{
			auto deltaPos = g_Camera.item->Pose.Position - item->Pose.Position;
			float dist = Vector3i::Distance(g_Camera.item->Pose.Position, item->Pose.Position);

			auto lookOrient = EulerAngles(
				phd_atan(dist, y - (bounds.Y1 + bounds.Y2) / 2 - g_Camera.item->Pose.Position.y),
				phd_atan(deltaPos.z, deltaPos.x) - item->Pose.Orientation.y,
				0) / 2;

			if (lookOrient.y > ANGLE(-50.0f) &&	lookOrient.y < ANGLE(50.0f) &&
				lookOrient.z > ANGLE(-85.0f) && lookOrient.z < ANGLE(85.0f))
			{
				short angleDelta = lookOrient.y - player.ExtraHeadRot.y;
				if (angleDelta > ANGLE(4.0f))
				{
					player.ExtraHeadRot.y += ANGLE(4.0f);
				}
				else if (angleDelta < ANGLE(-4.0f))
				{
					player.ExtraHeadRot.y -= ANGLE(4.0f);
				}
				else
				{
					player.ExtraHeadRot.y += angleDelta;
				}
				player.ExtraTorsoRot.y = player.ExtraHeadRot.y;

				angleDelta = lookOrient.z - player.ExtraHeadRot.x;
				if (angleDelta > ANGLE(4.0f))
				{
					player.ExtraHeadRot.x += ANGLE(4.0f);
				}
				else if (angleDelta < ANGLE(-4.0f))
				{
					player.ExtraHeadRot.x -= ANGLE(4.0f);
				}
				else
				{
					player.ExtraHeadRot.x += angleDelta;
				}
				player.ExtraTorsoRot.x = player.ExtraHeadRot.x;

				player.Control.Look.Orientation = lookOrient;

				g_Camera.type = CameraType::Look;
				g_Camera.item->LookedAt = true;
			}
		}
	}

	if (g_Camera.type == CameraType::Look ||
		g_Camera.type == CameraType::Combat)
	{
		if (g_Camera.type == CameraType::Combat)
		{
			g_Camera.PrevTarget = GameVector(g_Camera.LookAt, g_Camera.LookAtRoomNumber);

			if (!g_Configuration.IsUsingModernControls())
				y -= CLICK(1);
		}

		g_Camera.LookAtRoomNumber = item->RoomNumber;

		if (g_Camera.fixedCamera || player.Control.Look.IsUsingBinoculars)
		{
			g_Camera.LookAt.y = y;
			g_Camera.speed = 1.0f;
		}
		else
		{
			g_Camera.LookAt.y += (y - g_Camera.LookAt.y) / 4;
			g_Camera.speed = (g_Camera.type != CameraType::Look) ? 8.0f : 4.0f;
		}

		g_Camera.fixedCamera = false;
		if (g_Camera.type == CameraType::Look)
		{
			g_Camera.LookCamera(*item, coll);
		}
		else
		{
			CombatCamera(*item);
		}
	}
	else
	{
		g_Camera.PrevTarget = GameVector(g_Camera.LookAt, g_Camera.LookAtRoomNumber);

		g_Camera.LookAtRoomNumber = item->RoomNumber;
		g_Camera.LookAt.y = y;

		x = item->Pose.Position.x;
		z = item->Pose.Position.z;

		// -- Troye 2022.8.7
		if (g_Camera.flags == CameraFlag::FollowCenter)
		{
			int shift = (bounds.Z1 + bounds.Z2) / 2;
			x += shift * phd_sin(item->Pose.Orientation.y);
			z += shift * phd_cos(item->Pose.Orientation.y);
		}

		g_Camera.LookAt.x = x;
		g_Camera.LookAt.z = z;

		// CameraFlag::FollowCenter sets target on item and
		// ConfirmCameraTargetPos() overrides this target, hence flag check. -- Troye 2022.8.7
		if (item->IsLara() && g_Camera.flags != CameraFlag::FollowCenter)
			ConfirmCameraTargetPos();

		if (isFixedCamera == g_Camera.fixedCamera)
		{
			g_Camera.fixedCamera = false;
			if (g_Camera.speed != 1.0f &&
				!player.Control.Look.IsUsingBinoculars)
			{
				if (g_Camera.TargetSnaps <= 8)
				{
					x = g_Camera.PrevTarget.x + ((x - g_Camera.PrevTarget.x) / 4);
					y = g_Camera.PrevTarget.y + ((y - g_Camera.PrevTarget.y) / 4);
					z = g_Camera.PrevTarget.z + ((z - g_Camera.PrevTarget.z) / 4);

					g_Camera.LookAt.x = x;
					g_Camera.LookAt.y = y;
					g_Camera.LookAt.z = z;
				}
				else
				{
					g_Camera.TargetSnaps = 0;
				}
			}
		}
		else
		{
			g_Camera.fixedCamera = true;
			g_Camera.speed = 1.0f;
		}

		g_Camera.LookAtRoomNumber = GetPointCollision(Vector3i(x, y, z), g_Camera.LookAtRoomNumber).GetRoomNumber();

		if (abs(g_Camera.PrevTarget.x - g_Camera.LookAt.x) < 4 &&
			abs(g_Camera.PrevTarget.y - g_Camera.LookAt.y) < 4 &&
			abs(g_Camera.PrevTarget.z - g_Camera.LookAt.z) < 4)
		{
			g_Camera.LookAt.x = g_Camera.PrevTarget.x;
			g_Camera.LookAt.y = g_Camera.PrevTarget.y;
			g_Camera.LookAt.z = g_Camera.PrevTarget.z;
		}

		if (g_Camera.type != CameraType::Chase && g_Camera.flags != CameraFlag::ChaseObject)
		{
			FixedCamera();
		}
		else
		{
			ChaseCamera(*item);
		}
	}

	g_Camera.fixedCamera = isFixedCamera;
	g_Camera.last = g_Camera.number;

	if ((g_Camera.type != CameraType::Heavy || g_Camera.timer == -1) &&
		playerItem.HitPoints > 0)
	{
		g_Camera.type = CameraType::Chase;
		g_Camera.speed = 10.0f;
		g_Camera.number = NO_VALUE;
		g_Camera.lastItem = g_Camera.item;
		g_Camera.item = nullptr;
		g_Camera.targetElevation = 0;
		g_Camera.targetAngle = 0;
		g_Camera.targetDistance = BLOCK(1.5f);
		g_Camera.flags = CameraFlag::None;
		g_Camera.laraNode = NO_VALUE;
	}
}

bool TestBoundsCollideCamera(const GameBoundingBox& bounds, const Pose& pose, float radius)
{
	auto sphere = BoundingSphere(g_Camera.Position, radius);
	return sphere.Intersects(bounds.ToBoundingOrientedBox(pose));
}

void RumbleScreen()
{
	if (!(GlobalCounter & 0x1FF))
		// SFX Enum Changed from TR5 and pitch shift removed. User can set this in their sound XML. Stranger1992 31st August 2024
		SoundEffect(SFX_TR4_ENVIORONMENT_RUMBLE, nullptr, SoundEnvironment::Land);

	if (g_Camera.RumbleTimer >= 0)
		g_Camera.RumbleTimer++;

	if (g_Camera.RumbleTimer > 450)
	{
		if (!(Random::GenerateInt() & 0x1FF))
		{
			g_Camera.RumbleCounter = 0;
			g_Camera.RumbleTimer = -32 - (Random::GenerateInt() & 0x1F);
			return;
		}
	}

	if (g_Camera.RumbleTimer < 0)
	{
		if (g_Camera.RumbleCounter >= abs(g_Camera.RumbleTimer))
		{
			g_Camera.bounce = -(Random::GenerateInt() % abs(g_Camera.RumbleTimer));
			g_Camera.RumbleTimer++;
		}
		else
		{
			g_Camera.RumbleCounter++;
			g_Camera.bounce = -(Random::GenerateInt() % g_Camera.RumbleCounter);
		}
	}
}

void SetScreenFadeOut(float speed, bool force)
{
	if (g_ScreenEffect.ScreenFading && !force)
		return;

	g_ScreenEffect.ScreenFading = true;
	g_ScreenEffect.ScreenFadeStart = 1.0f;
	g_ScreenEffect.ScreenFadeEnd = 0;
	g_ScreenEffect.ScreenFadeSpeed = speed;
	g_ScreenEffect.ScreenFadeCurrent = g_ScreenEffect.ScreenFadeStart;
}

void SetScreenFadeIn(float speed, bool force)
{
	if (g_ScreenEffect.ScreenFading && !force)
		return;

	g_ScreenEffect.ScreenFading = true;
	g_ScreenEffect.ScreenFadeStart = 0.0f;
	g_ScreenEffect.ScreenFadeEnd = 1.0f;
	g_ScreenEffect.ScreenFadeSpeed = speed;
	g_ScreenEffect.ScreenFadeCurrent = g_ScreenEffect.ScreenFadeStart;
}

void SetCinematicBars(float height, float speed)
{
	g_ScreenEffect.CinematicBarsDestinationHeight = height;
	g_ScreenEffect.CinematicBarsSpeed = speed;
}

void UpdateFadeScreenAndCinematicBars()
{
	if (g_ScreenEffect.CinematicBarsDestinationHeight < g_ScreenEffect.CinematicBarsHeight)
	{
		g_ScreenEffect.CinematicBarsHeight -= g_ScreenEffect.CinematicBarsSpeed;
		if (g_ScreenEffect.CinematicBarsDestinationHeight > g_ScreenEffect.CinematicBarsHeight)
			g_ScreenEffect.CinematicBarsHeight = g_ScreenEffect.CinematicBarsDestinationHeight;
	}
	else if (g_ScreenEffect.CinematicBarsDestinationHeight > g_ScreenEffect.CinematicBarsHeight)
	{
		g_ScreenEffect.CinematicBarsHeight += g_ScreenEffect.CinematicBarsSpeed;
		if (g_ScreenEffect.CinematicBarsDestinationHeight < g_ScreenEffect.CinematicBarsHeight)
			g_ScreenEffect.CinematicBarsHeight = g_ScreenEffect.CinematicBarsDestinationHeight;
	}

	int prevScreenFadeCurrent = g_ScreenEffect.ScreenFadeCurrent;

	if (g_ScreenEffect.ScreenFadeEnd != 0 && g_ScreenEffect.ScreenFadeEnd >= g_ScreenEffect.ScreenFadeCurrent)
	{
		g_ScreenEffect.ScreenFadeCurrent += g_ScreenEffect.ScreenFadeSpeed;
		if (g_ScreenEffect.ScreenFadeCurrent > g_ScreenEffect.ScreenFadeEnd)
		{
			g_ScreenEffect.ScreenFadeCurrent = g_ScreenEffect.ScreenFadeEnd;
			if (prevScreenFadeCurrent >= g_ScreenEffect.ScreenFadeCurrent)
			{
				g_ScreenEffect.ScreenFadedOut = true;
				g_ScreenEffect.ScreenFading = false;
			}

		}
	}
	else if (g_ScreenEffect.ScreenFadeEnd < g_ScreenEffect.ScreenFadeCurrent)
	{
		g_ScreenEffect.ScreenFadeCurrent -= g_ScreenEffect.ScreenFadeSpeed;
		if (g_ScreenEffect.ScreenFadeCurrent < g_ScreenEffect.ScreenFadeEnd)
		{
			g_ScreenEffect.ScreenFadeCurrent = g_ScreenEffect.ScreenFadeEnd;
			g_ScreenEffect.ScreenFading = false;
		}
	}
}

void ClearCinematicBars()
{
	g_ScreenEffect.CinematicBarsHeight = 0.0f;
	g_ScreenEffect.CinematicBarsDestinationHeight = 0.0f;
	g_ScreenEffect.CinematicBarsSpeed = 0.0f;
}

float GetParticleDistanceFade(const Vector3i& pos)
{
	constexpr auto PARTICLE_FADE_THRESHOLD = BLOCK(14);

	float dist = Vector3::Distance(g_Camera.Position, pos.ToVector3());
	if (dist <= PARTICLE_FADE_THRESHOLD)
		return 1.0f;

	return std::clamp(1.0f - ((dist - PARTICLE_FADE_THRESHOLD) / CAMERA_OBJECT_COLL_DIST_THRESHOLD), 0.0f, 1.0f);
}
