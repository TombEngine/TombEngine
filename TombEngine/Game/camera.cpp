#include "framework.h"
#include "Game/camera.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/los.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/Optics.h"
#include "Game/room.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Game/spotcam.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/burning_torch.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"
#include "Specific/winmain.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Effects::Environment;
using namespace TEN::Entities::Generic;
using namespace TEN::Input;
using namespace TEN::Math;
using TEN::Renderer::g_Renderer;

constexpr auto PARTICLE_FADE_THRESHOLD = BLOCK(14);
constexpr auto COLL_CHECK_THRESHOLD    = BLOCK(4);
constexpr auto COLL_CANCEL_THRESHOLD   = BLOCK(2);
constexpr auto COLL_DISCARD_THRESHOLD  = CLICK(0.5f);
constexpr auto CAMERA_RADIUS           = CLICK(1);

struct OLD_CAMERA
{
	short ActiveState;
	short TargetState;
	int targetDistance;
	short actualElevation;
	short targetElevation;
	short actualAngle;
	Pose pos;
	Pose pos2;
	Vector3i target;
};

bool ItemCameraOn;
GameVector LastPosition;
GameVector LastTarget;
GameVector LastIdeal;
GameVector Ideals[5];
OLD_CAMERA OldCam;
int CameraSnaps = 0;
int TargetSnaps = 0;
GameVector LookCamPosition;
GameVector LookCamTarget;
CAMERA_INFO Camera;
GameVector ForcedFixedCamera;
int UseForcedFixedCamera;

CameraType BinocularOldCamera;

short CurrentFOV;
short LastFOV;

int RumbleTimer = 0;
int RumbleCounter = 0;

bool  ScreenFadedOut = false;
bool  ScreenFading = false;
float ScreenFadeSpeed = 0;
float ScreenFadeStart = 0;
float ScreenFadeEnd = 0;
float ScreenFadeCurrent = 0;

float CinematicBarsHeight = 0;
float CinematicBarsDestinationHeight = 0;
float CinematicBarsSpeed = 0;

void DoThumbstickCamera()
{
	constexpr auto VERTICAL_CONSTRAINT_ANGLE   = ANGLE(120.0f);
	constexpr auto HORIZONTAL_CONSTRAINT_ANGLE = ANGLE(80.0f);

	if (!g_Configuration.EnableThumbstickCamera)
		return;

	if (Camera.laraNode == -1 && Camera.target.ToVector3i() == OldCam.target)
	{
		const auto& axisCoeff = AxisMap[InputAxisID::Camera];

		if (abs(axisCoeff.x) > EPSILON && abs(Camera.targetAngle) == 0)
			Camera.targetAngle = ANGLE(VERTICAL_CONSTRAINT_ANGLE * axisCoeff.x);

		if (abs(axisCoeff.y) > EPSILON)
			Camera.targetElevation = ANGLE(-10.0f + (HORIZONTAL_CONSTRAINT_ANGLE * axisCoeff.y));
	}
}

static int GetLookCameraVerticalOffset(const ItemInfo& item, const CollisionInfo& coll)
{
	constexpr auto VERTICAL_OFFSET_DEFAULT		  = -BLOCK(1 / 16.0f);
	constexpr auto VERTICAL_OFFSET_SWAMP		  = BLOCK(0.4f);
	constexpr auto VERTICAL_OFFSET_MONKEY_SWING	  = BLOCK(0.25f);
	constexpr auto VERTICAL_OFFSET_TREADING_WATER = BLOCK(0.5f);

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

	// Return appropriate vertical offset.
	return -((verticalOffset < floorToCeilHeight) ? verticalOffset : floorToCeilHeight);
}

void LookCamera(ItemInfo& item, const CollisionInfo& coll)
{
	constexpr auto POS_LERP_ALPHA	 = 0.25f;
	constexpr auto COLL_PUSH		 = BLOCK(0.25f) - BLOCK(1 / 16.0f);
	constexpr auto LOOK_AT_DIST		 = BLOCK(0.5f);
	constexpr auto CAMERA_DIST_COEFF = 0.7f;
	constexpr auto CAMERA_DIST_MAX	 = BLOCK(0.75f);

	const auto& player = GetLaraInfo(item);

	int verticalOffset = GetLookCameraVerticalOffset(item, coll);
	auto pivotOffset = Vector3i(0, verticalOffset, 0);

	float idealDist = -std::max(Camera.targetDistance * CAMERA_DIST_COEFF, CAMERA_DIST_MAX);

	// Define absolute camera orientation.
	auto orient = player.Control.Look.Orientation +
		EulerAngles(item.Pose.Orientation.x, item.Pose.Orientation.y, 0) +
		EulerAngles(0, Camera.targetAngle, 0);
	orient.x = std::clamp(orient.x, LOOKCAM_ORIENT_CONSTRAINT.first.x, LOOKCAM_ORIENT_CONSTRAINT.second.x);

	// Determine base position.
	bool isInSwamp = TestEnvironment(ENV_FLAG_SWAMP, item.RoomNumber);
	auto basePos = Vector3i(
		item.Pose.Position.x,
		isInSwamp ? g_Level.Rooms[item.RoomNumber].TopHeight : item.Pose.Position.y,
		item.Pose.Position.z);

	// Define landmarks.
	auto pivotPos = Geometry::TranslatePoint(basePos, item.Pose.Orientation.y, pivotOffset);
	auto idealPos = Geometry::TranslatePoint(pivotPos, orient, idealDist);
	auto lookAtPos = Geometry::TranslatePoint(pivotPos, orient, LOOK_AT_DIST);

	// Determine best position.
	auto origin = GameVector(pivotPos, GetPointCollision(item, item.Pose.Orientation.y, pivotOffset.z, pivotOffset.y).GetRoomNumber());
	auto target = GameVector(idealPos, GetPointCollision(origin.ToVector3i(), origin.RoomNumber, orient.ToDirection(), idealDist).GetRoomNumber());

	// Handle room and object collisions.
	LOSAndReturnTarget(&origin, &target, 0);
	CameraCollisionBounds(&target, COLL_PUSH, true);
	ItemsCollideCamera();

	// Smoothly update camera position.
	MoveCamera(&target, Camera.speed);
	Camera.target = GameVector(Camera.target.ToVector3i() + (lookAtPos - Camera.target.ToVector3i()) * POS_LERP_ALPHA, item.RoomNumber);

	LookAt(&Camera, 0);
	UpdateMikePos(item);
	Camera.oldType = Camera.type;
}

void LookAt(CAMERA_INFO* cam, short roll)
{
	cam->Fov = TO_RAD(CurrentFOV / 1.333333f);
	cam->Roll = TO_RAD(roll);
}

void AlterFOV(short value, bool store)
{
	if (store)
		LastFOV = value;

	CurrentFOV = value;
}

short GetCurrentFOV()
{
	return CurrentFOV;
}

inline void RumbleFromBounce()
{
	Rumble(std::clamp((float)abs(Camera.bounce) / 70.0f, 0.0f, 0.8f), 0.2f);
}

void CalculateBounce(bool binocularMode)
{
	if (Camera.bounce == 0)
		return;

	if (Camera.bounce <= 0)
	{
		if (binocularMode)
		{
			Camera.target.x += (CLICK(0.25f) / 4) * (GetRandomControl() % (-Camera.bounce) - (-Camera.bounce / 2));
			Camera.target.y += (CLICK(0.25f) / 4) * (GetRandomControl() % (-Camera.bounce) - (-Camera.bounce / 2));
			Camera.target.z += (CLICK(0.25f) / 4) * (GetRandomControl() % (-Camera.bounce) - (-Camera.bounce / 2));
		}
		else
		{
			int bounce = -Camera.bounce;
			int bounce2 = bounce / 2;
			Camera.target.x += GetRandomControl() % bounce - bounce2;
			Camera.target.y += GetRandomControl() % bounce - bounce2;
			Camera.target.z += GetRandomControl() % bounce - bounce2;
		}

		Camera.bounce += 5;
		RumbleFromBounce();
	}
	else
	{
		Camera.pos.y += Camera.bounce;
		Camera.target.y += Camera.bounce;
		Camera.bounce = 0;
	}
}

void InitializeCamera()
{
	Camera.shift = LaraItem->Pose.Position.y - BLOCK(1);

	LastTarget = GameVector(
		LaraItem->Pose.Position.x,
		Camera.shift,
		LaraItem->Pose.Position.z,
		LaraItem->RoomNumber);

	Camera.target = GameVector(
		LastTarget.x,
		Camera.shift,
		LastTarget.z,
		LaraItem->RoomNumber);

	Camera.pos = GameVector(
		LastTarget.x,
		Camera.shift,
		LastTarget.z - 100,
		LaraItem->RoomNumber);

	Camera.targetDistance = BLOCK(1.5f);
	Camera.item = nullptr;
	Camera.type = CameraType::Chase;
	Camera.speed = 1;
	Camera.flags = CF_NONE;
	Camera.bounce = 0;
	Camera.number = -1;
	Camera.fixedCamera = false;
	Camera.DisableInterpolation = true;

	AlterFOV(ANGLE(DEFAULT_FOV));

	UseForcedFixedCamera = 0;
	CalculateCamera(LaraCollision);

	// Fade in screen.
	SetScreenFadeIn(FADE_SCREEN_SPEED);
}

void MoveCamera(GameVector* ideal, int speed)
{
	if (Lara.Control.Look.IsUsingBinoculars)
		speed = 1;

	if (OldCam.pos.Orientation != LaraItem->Pose.Orientation ||
		OldCam.pos2.Orientation.x != Lara.ExtraHeadRot.x ||
		OldCam.pos2.Orientation.y != Lara.ExtraHeadRot.y ||
		OldCam.pos2.Position.x != Lara.ExtraTorsoRot.x ||
		OldCam.pos2.Position.y != Lara.ExtraTorsoRot.y ||
		OldCam.pos.Position != LaraItem->Pose.Position ||
		OldCam.ActiveState != LaraItem->Animation.ActiveState ||
		OldCam.TargetState != LaraItem->Animation.TargetState ||
		OldCam.targetDistance != Camera.targetDistance ||
		OldCam.targetElevation != Camera.targetElevation ||
		OldCam.actualElevation != Camera.actualElevation ||
		OldCam.actualAngle != Camera.actualAngle ||
		OldCam.target.x != Camera.target.x ||
		OldCam.target.y != Camera.target.y ||
		OldCam.target.z != Camera.target.z ||
		Camera.oldType != Camera.type ||
		Lara.Control.Look.IsUsingBinoculars)
	{
		OldCam.pos.Orientation = LaraItem->Pose.Orientation;
		OldCam.pos2.Orientation.x = Lara.ExtraHeadRot.x;
		OldCam.pos2.Orientation.y = Lara.ExtraHeadRot.y;
		OldCam.pos2.Position.x = Lara.ExtraTorsoRot.x;
		OldCam.pos2.Position.y = Lara.ExtraTorsoRot.y;
		OldCam.pos.Position = LaraItem->Pose.Position;
		OldCam.ActiveState = LaraItem->Animation.ActiveState;
		OldCam.TargetState = LaraItem->Animation.TargetState;
		OldCam.targetDistance = Camera.targetDistance;
		OldCam.targetElevation = Camera.targetElevation;
		OldCam.actualElevation = Camera.actualElevation;
		OldCam.actualAngle = Camera.actualAngle;
		OldCam.target.x = Camera.target.x;
		OldCam.target.y = Camera.target.y;
		OldCam.target.z = Camera.target.z;
		LastIdeal.x = ideal->x;
		LastIdeal.y = ideal->y;
		LastIdeal.z = ideal->z;
		LastIdeal.RoomNumber = ideal->RoomNumber;
	}
	else
	{
		ideal->x = LastIdeal.x;
		ideal->y = LastIdeal.y;
		ideal->z = LastIdeal.z;
		ideal->RoomNumber = LastIdeal.RoomNumber;
	}

	Camera.pos.x += (ideal->x - Camera.pos.x) / speed;
	Camera.pos.y += (ideal->y - Camera.pos.y) / speed;
	Camera.pos.z += (ideal->z - Camera.pos.z) / speed;
	Camera.pos.RoomNumber = ideal->RoomNumber;

	CalculateBounce(false);

	int y = Camera.pos.y;
	if (TestEnvironment(ENV_FLAG_SWAMP, Camera.pos.RoomNumber))
		y = g_Level.Rooms[Camera.pos.RoomNumber].Position.y - CLICK(1);

	auto pointColl = GetPointCollision(Vector3i(Camera.pos.x, y, Camera.pos.z), Camera.pos.RoomNumber);
	if (y < pointColl.GetCeilingHeight() ||
		y > pointColl.GetFloorHeight())
	{
		LOSAndReturnTarget(&Camera.target, &Camera.pos, 0);

		if (abs(Camera.pos.x - ideal->x) < BLOCK(0.5f) &&
			abs(Camera.pos.y - ideal->y) < BLOCK(0.5f) &&
			abs(Camera.pos.z - ideal->z) < BLOCK(0.5f))
		{
			auto origin = *ideal;
			auto target = Camera.pos;
			if (!LOSAndReturnTarget(&origin, &target, 0) &&
				++CameraSnaps >= 8)
			{
				Camera.pos = *ideal;
				CameraSnaps = 0;
			}
		}
	}

	pointColl = GetPointCollision(Camera.pos.ToVector3i(), Camera.pos.RoomNumber);

	int buffer = CLICK(1) - 1;
	if ((Camera.pos.y - buffer) < pointColl.GetCeilingHeight() &&
		(Camera.pos.y + buffer) > pointColl.GetFloorHeight() &&
		pointColl.GetCeilingHeight() < pointColl.GetFloorHeight() &&
		pointColl.GetCeilingHeight() != NO_HEIGHT &&
		pointColl.GetFloorHeight() != NO_HEIGHT)
	{
		Camera.pos.y = (pointColl.GetFloorHeight() + pointColl.GetCeilingHeight()) / 2;
	}
	else if ((Camera.pos.y + buffer) > pointColl.GetFloorHeight() &&
		pointColl.GetCeilingHeight() < pointColl.GetFloorHeight() &&
		pointColl.GetCeilingHeight() != NO_HEIGHT &&
		pointColl.GetFloorHeight() != NO_HEIGHT)
	{
		Camera.pos.y = pointColl.GetFloorHeight() - buffer;
	}
	else if ((Camera.pos.y - buffer) < pointColl.GetCeilingHeight() &&
		pointColl.GetCeilingHeight() < pointColl.GetFloorHeight() &&
		pointColl.GetCeilingHeight() != NO_HEIGHT &&
		pointColl.GetFloorHeight() != NO_HEIGHT)
	{
		Camera.pos.y = pointColl.GetCeilingHeight() + buffer;
	}
	else if (pointColl.GetCeilingHeight() >= pointColl.GetFloorHeight() ||
		pointColl.GetFloorHeight() == NO_HEIGHT ||
		pointColl.GetCeilingHeight() == NO_HEIGHT)
	{
		Camera.pos = *ideal;
	}

	ItemsCollideCamera();

	Camera.pos.RoomNumber = GetPointCollision(Camera.pos.ToVector3i(), Camera.pos.RoomNumber).GetRoomNumber();
	LookAt(&Camera, 0);
	UpdateMikePos(*LaraItem);
	Camera.oldType = Camera.type;
}

void ObjCamera(ItemInfo* camSlotId, int camMeshId, ItemInfo* targetItem, int targetMeshId, bool cond)
{
	//camSlotId and targetItem stay the same object until I know how to expand targetItem to another object.
	//activates code below ->  void CalculateCamera().
	ItemCameraOn = cond;

	UpdateCameraElevation();

	//get mesh 0 coordinates.	
	auto pos = GetJointPosition(camSlotId, 0, Vector3i::Zero);
	auto dest = Vector3(pos.x, pos.y, pos.z);

	GameVector from = GameVector(dest, camSlotId->RoomNumber);
	Camera.fixedCamera = true;

	MoveObjCamera(&from, camSlotId, camMeshId, targetItem, targetMeshId);
	Camera.timer = -1;
}

void ClearObjCamera()
{
	ItemCameraOn = false;
}

void MoveObjCamera(GameVector* ideal, ItemInfo* camSlotId, int camMeshId, ItemInfo* targetItem, int targetMeshId)
{
	int	speed = 1;
	//Get mesh1 to attach camera to
	//Vector3i pos = Vector3i::Zero;
	auto pos = GetJointPosition(camSlotId, camMeshId, Vector3i::Zero);
	//Get mesh2 to attach target to
	//Vector3i pos2 = Vector3i::Zero;
	auto pos2 = GetJointPosition(targetItem, targetMeshId, Vector3i::Zero);

	if (OldCam.pos.Position != pos ||
		OldCam.targetDistance  != Camera.targetDistance  ||
		OldCam.targetElevation != Camera.targetElevation ||
		OldCam.actualElevation != Camera.actualElevation ||
		OldCam.actualAngle != Camera.actualAngle ||
		OldCam.target != Camera.target.ToVector3i() ||
		Camera.oldType != Camera.type ||
		Lara.Control.Look.IsUsingBinoculars)
	{
		OldCam.pos.Position = pos;
		OldCam.targetDistance = Camera.targetDistance;
		OldCam.targetElevation = Camera.targetElevation;
		OldCam.actualElevation = Camera.actualElevation;
		OldCam.actualAngle = Camera.actualAngle;
		OldCam.target = Camera.target.ToVector3i();
		LastIdeal = pos;
		LastIdeal.RoomNumber = ideal->RoomNumber;
		LastTarget = pos2;
	}
	else
	{
		pos  = LastIdeal.ToVector3i();
		pos2 = LastTarget.ToVector3i();
		ideal->RoomNumber = LastIdeal.RoomNumber;
	}

	Camera.pos += (ideal->ToVector3i() - Camera.pos.ToVector3i()) / speed;
	Camera.pos.RoomNumber = GetPointCollision(Camera.pos.ToVector3i(), Camera.pos.RoomNumber).GetRoomNumber();
	LookAt(&Camera, 0);

	auto angle = Camera.target.ToVector3i() - Camera.pos.ToVector3i();
	auto position = Vector3i(Camera.target.ToVector3i() - Camera.pos.ToVector3i());

	// write last frame camera angle to LastAngle to compare if next frame camera angle has a bigger step than 100.
	// To make camera movement smoother a speed of 2 is used.
	// While for big camera angle steps (cuts) -
	// the speed is set to 1 to make the cut immediatelly.
	constexpr int angleThresholdDegrees = 100;

	if (LastTarget.x - Camera.target.x > angleThresholdDegrees ||
		LastTarget.y - Camera.target.y > angleThresholdDegrees ||
		LastTarget.z - Camera.target.z > angleThresholdDegrees)
	{
		speed = 1;
	}
	else
	{
		speed = 2;
	}

	// Actual movement of the target.
	Camera.target.x += (pos2.x - Camera.target.x) / speed;
	Camera.target.y += (pos2.y - Camera.target.y) / speed;
	Camera.target.z += (pos2.z - Camera.target.z) / speed;
}

void RefreshFixedCamera(short camNumber)
{
	auto& camera = g_Level.Cameras[camNumber];

	auto origin = GameVector(camera.Position, camera.RoomNumber);
	int moveSpeed = camera.Speed * 8 + 1;

	MoveCamera(&origin, moveSpeed);
}

void ChaseCamera(ItemInfo* item)
{
	static const unsigned int maxSwivelSteps = 5;

	if (!Camera.targetElevation)
		Camera.targetElevation = -ANGLE(10.0f);

	Camera.targetElevation += item->Pose.Orientation.x;
	UpdateCameraElevation();

	// Clamp X orientation.
	if (Camera.actualElevation > ANGLE(85.0f))
		Camera.actualElevation = ANGLE(85.0f);
	else if (Camera.actualElevation < ANGLE(-85.0f))
		Camera.actualElevation = ANGLE(-85.0f);

	// Force item position after exiting look mode to avoid weird movements near walls.
	if (Camera.oldType == CameraType::Look)
	{
		Camera.target.x = item->Pose.Position.x;
		Camera.target.z = item->Pose.Position.z;
	}

	int distance = Camera.targetDistance * phd_cos(Camera.actualElevation);

	auto pointColl = GetPointCollision(Vector3i(Camera.target.x, Camera.target.y + CLICK(1), Camera.target.z), Camera.target.RoomNumber);

	if (TestEnvironment(ENV_FLAG_SWAMP, pointColl.GetRoomNumber()))
		Camera.target.y = g_Level.Rooms[pointColl.GetRoomNumber()].TopHeight - CLICK(1);

	int y = Camera.target.y;
	pointColl = GetPointCollision(Vector3i(Camera.target.x, y, Camera.target.z), Camera.target.RoomNumber);
	if (((y < pointColl.GetCeilingHeight() || pointColl.GetFloorHeight() < y) || pointColl.GetFloorHeight() <= pointColl.GetCeilingHeight()) ||
		(pointColl.GetFloorHeight() == NO_HEIGHT || pointColl.GetCeilingHeight() == NO_HEIGHT))
	{
		TargetSnaps++;
		Camera.target = LastTarget;
	}
	else
	{
		TargetSnaps = 0;
	}

	for (int i = 0; i < maxSwivelSteps; i++)
		Ideals[i].y = Camera.target.y + (Camera.targetDistance * phd_sin(Camera.actualElevation));

	// Determine best player viewing angle.
	float farthestDist = FLT_MAX;
	int indexOfFarthestIdeal = 0;
	GameVector temp[2];
	for (int i = 0; i < maxSwivelSteps; i++)
	{
		// Incrementally swivel camera position.
		short angle = (i == 0) ? Camera.actualAngle : ((i - 1) * ANGLE(90.0f));

		// Record ideal position at default distance for given swivel.
		Ideals[i].x = Camera.target.x - (distance * phd_sin(angle));
		Ideals[i].z = Camera.target.z - (distance * phd_cos(angle));
		Ideals[i].RoomNumber = Camera.target.RoomNumber;

		// Assess LOS.
		if (LOSAndReturnTarget(&Camera.target, &Ideals[i], 200))
		{
			temp[0] = Ideals[i];
			temp[1] = Camera.pos;

			if (i == 0 || LOSAndReturnTarget(&temp[0], &temp[1], 0))
			{
				if (i == 0)
				{
					indexOfFarthestIdeal = 0;
					break;
				}

				float dx = (Camera.pos.x - Ideals[i].x) * (Camera.pos.x - Ideals[i].x);
				dx += (Camera.pos.z - Ideals[i].z) * (Camera.pos.z - Ideals[i].z);
				if (dx < farthestDist)
				{
					farthestDist = dx;
					indexOfFarthestIdeal = i;
				}
			}
		}
		else if (i == 0)
		{
			temp[0] = Ideals[i];
			temp[1] = Camera.pos;

			if (i == 0 || LOSAndReturnTarget(&temp[0], &temp[1], 0))
			{
				float dx = (Camera.target.x - Ideals[i].x) * (Camera.target.x - Ideals[i].x);
				float dz = (Camera.target.z - Ideals[i].z) * (Camera.target.z - Ideals[i].z);

				if ((dx + dz) > SQUARE(BLOCK(0.75f)))
				{
					indexOfFarthestIdeal = 0;
					break;
				}
			}
		}
	}

	auto ideal = Ideals[indexOfFarthestIdeal];
	CameraCollisionBounds(&ideal, CLICK(1.5f), 1);
	MoveCamera(&ideal, Camera.speed);
}

void UpdateCameraElevation()
{
	DoThumbstickCamera();

	if (Camera.laraNode != -1)
	{
		auto pos = GetJointPosition(LaraItem, Camera.laraNode, Vector3i::Zero);
		auto pos1 = GetJointPosition(LaraItem, Camera.laraNode, Vector3i(0, -CLICK(1), BLOCK(2)));
		pos = pos1 - pos;
		Camera.actualAngle = Camera.targetAngle + phd_atan(pos.z, pos.x);
	}
	else
		Camera.actualAngle = LaraItem->Pose.Orientation.y + Camera.targetAngle;

	Camera.actualElevation += (Camera.targetElevation - Camera.actualElevation) / 8;
}

void CombatCamera(ItemInfo* item)
{
	static const unsigned int maxSwivelSteps = 5;

	auto& player = GetLaraInfo(*item);

	Camera.target.x = item->Pose.Position.x;
	Camera.target.z = item->Pose.Position.z;

	if (player.TargetEntity)
	{
		Camera.targetAngle = player.TargetArmOrient.y;
		Camera.targetElevation = player.TargetArmOrient.x + item->Pose.Orientation.x;
	}
	else
	{
		Camera.targetAngle = player.ExtraHeadRot.y + player.ExtraTorsoRot.y;
		Camera.targetElevation = player.ExtraHeadRot.x + player.ExtraTorsoRot.x + item->Pose.Orientation.x - ANGLE(15.0f);
	}

	auto pointColl = GetPointCollision(Vector3i(Camera.target.x, Camera.target.y + CLICK(1), Camera.target.z), Camera.target.RoomNumber);
	if (TestEnvironment(ENV_FLAG_SWAMP, pointColl.GetRoomNumber()))
		Camera.target.y = g_Level.Rooms[pointColl.GetRoomNumber()].TopHeight - CLICK(1);

	pointColl = GetPointCollision(Camera.target.ToVector3i(), Camera.target.RoomNumber);
	Camera.target.RoomNumber = pointColl.GetRoomNumber();

	int buffer = CLICK(0.25f);
	if ((pointColl.GetCeilingHeight() + buffer) > (pointColl.GetFloorHeight() - buffer) &&
		pointColl.GetFloorHeight() != NO_HEIGHT &&
		pointColl.GetCeilingHeight() != NO_HEIGHT)
	{
		Camera.target.y = (pointColl.GetCeilingHeight() + pointColl.GetFloorHeight()) / 2;
		Camera.targetElevation = 0;
	}
	else if (Camera.target.y > (pointColl.GetFloorHeight() - buffer) &&
		pointColl.GetFloorHeight() != NO_HEIGHT)
	{
		Camera.target.y = pointColl.GetFloorHeight() - buffer;
		Camera.targetElevation = 0;
	}
	else if (Camera.target.y < (pointColl.GetCeilingHeight() + buffer) &&
		pointColl.GetCeilingHeight() != NO_HEIGHT)
	{
		Camera.target.y = pointColl.GetCeilingHeight() + buffer;
		Camera.targetElevation = 0;
	}

	int y = Camera.target.y;
	pointColl = GetPointCollision(Vector3i(Camera.target.x, y, Camera.target.z), Camera.target.RoomNumber);
	Camera.target.RoomNumber = pointColl.GetRoomNumber();

	if (y < pointColl.GetCeilingHeight() ||
		y > pointColl.GetFloorHeight() ||
		pointColl.GetCeilingHeight() >= pointColl.GetFloorHeight() ||
		pointColl.GetFloorHeight() == NO_HEIGHT ||
		pointColl.GetCeilingHeight() == NO_HEIGHT)
	{
		TargetSnaps++;
		Camera.target = LastTarget;
	}
	else
		TargetSnaps = 0;

	UpdateCameraElevation();

	Camera.targetDistance = BLOCK(1.5f);
	int distance = Camera.targetDistance * phd_cos(Camera.actualElevation);

	for (int i = 0; i < maxSwivelSteps; i++)
		Ideals[i].y = Camera.target.y + (Camera.targetDistance * phd_sin(Camera.actualElevation));

	// Determine best player viewing angle.
	float farthestDist = FLT_MAX;
	int indexOfFarthestIdeal = 0;
	GameVector temp[2];
	for (int i = 0; i < maxSwivelSteps; i++)
	{
		// Incrementally swivel camera position.
		short angle = (i == 0) ? Camera.actualAngle : ((i - 1) * ANGLE(90.0f));

		// Record ideal position at default distance for given swivel.
		Ideals[i].x = Camera.target.x - (distance * phd_sin(angle));
		Ideals[i].z = Camera.target.z - (distance * phd_cos(angle));
		Ideals[i].RoomNumber = Camera.target.RoomNumber;

		// Assess LOS.
		if (LOSAndReturnTarget(&Camera.target, &Ideals[i], 200))
		{
			temp[0] = Ideals[i];
			temp[1] = Camera.pos;
			if (i == 0 || LOSAndReturnTarget(&temp[0], &temp[1], 0))
			{
				if (i == 0)
				{
					indexOfFarthestIdeal = 0;
					break;
				}

				float dx = (Camera.pos.x - Ideals[i].x) * (Camera.pos.x - Ideals[i].x);
				dx += (Camera.pos.z - Ideals[i].z) * (Camera.pos.z - Ideals[i].z);
				if (dx < farthestDist)
				{
					farthestDist = dx;
					indexOfFarthestIdeal = i;
				}
			}
		}
		else if (i == 0)
		{
			temp[0] = Ideals[i];
			temp[1] = Camera.pos;
			if (i == 0 || LOSAndReturnTarget(&temp[0], &temp[1], 0))
			{
				float dx = (Camera.target.x - Ideals[i].x) * (Camera.target.x - Ideals[i].x);
				float dz = (Camera.target.z - Ideals[i].z) * (Camera.target.z - Ideals[i].z);
				if ((dx + dz) > SQUARE(BLOCK(0.75f)))
				{
					indexOfFarthestIdeal = 0;
					break;
				}
			}
		}
	}

	// Handle room collision.
	auto ideal = Ideals[indexOfFarthestIdeal];
	CameraCollisionBounds(&ideal, CLICK(1.5f), 1);

	// Snap position of fixed camera type.
	if (Camera.oldType == CameraType::Fixed)
		Camera.speed = 1;

	MoveCamera(&ideal, Camera.speed);
}

bool CameraCollisionBounds(GameVector* ideal, int push, bool yFirst)
{
	int x = ideal->x;
	int y = ideal->y;
	int z = ideal->z;

	auto pointColl = GetPointCollision(Vector3i(x, y, z), ideal->RoomNumber);
	if (yFirst)
	{
		int buffer = CLICK(1) - 1;
		if ((y - buffer) < pointColl.GetCeilingHeight() &&
			(y + buffer) > pointColl.GetFloorHeight() &&
			pointColl.GetCeilingHeight() < pointColl.GetFloorHeight() &&
			pointColl.GetCeilingHeight() != NO_HEIGHT &&
			pointColl.GetFloorHeight() != NO_HEIGHT)
		{
			y = (pointColl.GetFloorHeight() + pointColl.GetCeilingHeight()) / 2;
		}
		else if ((y + buffer) > pointColl.GetFloorHeight() &&
			pointColl.GetCeilingHeight() < pointColl.GetFloorHeight() &&
			pointColl.GetCeilingHeight() != NO_HEIGHT &&
			pointColl.GetFloorHeight() != NO_HEIGHT)
		{
			y = pointColl.GetFloorHeight() - buffer;
		}
		else if ((y - buffer) < pointColl.GetCeilingHeight() &&
			pointColl.GetCeilingHeight() < pointColl.GetFloorHeight() &&
			pointColl.GetCeilingHeight() != NO_HEIGHT &&
			pointColl.GetFloorHeight() != NO_HEIGHT)
		{
			y = pointColl.GetCeilingHeight() + buffer;
		}
	}

	pointColl = GetPointCollision(Vector3i(x - push, y, z), ideal->RoomNumber);
	if (y > pointColl.GetFloorHeight() ||
		pointColl.GetFloorHeight() == NO_HEIGHT ||
		pointColl.GetCeilingHeight() == NO_HEIGHT ||
		pointColl.GetCeilingHeight() >= pointColl.GetFloorHeight() ||
		y < pointColl.GetCeilingHeight())
	{
		x = (x & (~1023)) + push;
	}

	pointColl = GetPointCollision(Vector3i(x, y, z - push), ideal->RoomNumber);
	if (y > pointColl.GetFloorHeight() ||
		pointColl.GetFloorHeight() == NO_HEIGHT ||
		pointColl.GetCeilingHeight() == NO_HEIGHT ||
		pointColl.GetCeilingHeight() >= pointColl.GetFloorHeight() ||
		y < pointColl.GetCeilingHeight())
	{
		z = (z & (~1023)) + push;
	}

	pointColl = GetPointCollision(Vector3i(x + push, y, z), ideal->RoomNumber);
	if (y > pointColl.GetFloorHeight() ||
		pointColl.GetFloorHeight() == NO_HEIGHT ||
		pointColl.GetCeilingHeight() == NO_HEIGHT ||
		pointColl.GetCeilingHeight() >= pointColl.GetFloorHeight() ||
		y < pointColl.GetCeilingHeight())
	{
		x = (x | 1023) - push;
	}

	pointColl = GetPointCollision(Vector3i(x, y, z + push), ideal->RoomNumber);
	if (y > pointColl.GetFloorHeight() ||
		pointColl.GetFloorHeight() == NO_HEIGHT ||
		pointColl.GetCeilingHeight() == NO_HEIGHT ||
		pointColl.GetCeilingHeight() >= pointColl.GetFloorHeight() ||
		y < pointColl.GetCeilingHeight())
	{
		z = (z | 1023) - push;
	}

	if (!yFirst)
	{
		pointColl = GetPointCollision(Vector3i(x, y, z), ideal->RoomNumber);

		int buffer = CLICK(1) - 1;
		if ((y - buffer) < pointColl.GetCeilingHeight() &&
			(y + buffer) > pointColl.GetFloorHeight() &&
			pointColl.GetCeilingHeight() < pointColl.GetFloorHeight() &&
			pointColl.GetCeilingHeight() != NO_HEIGHT &&
			pointColl.GetFloorHeight() != NO_HEIGHT)
		{
			y = (pointColl.GetFloorHeight() + pointColl.GetCeilingHeight()) / 2;
		}
		else if ((y + buffer) > pointColl.GetFloorHeight() &&
			pointColl.GetCeilingHeight() < pointColl.GetFloorHeight() &&
			pointColl.GetCeilingHeight() != NO_HEIGHT &&
			pointColl.GetFloorHeight() != NO_HEIGHT)
		{
			y = pointColl.GetFloorHeight() - buffer;
		}
		else if ((y - buffer) < pointColl.GetCeilingHeight() &&
			pointColl.GetCeilingHeight() < pointColl.GetFloorHeight() &&
			pointColl.GetCeilingHeight() != NO_HEIGHT &&
			pointColl.GetFloorHeight() != NO_HEIGHT)
		{
			y = pointColl.GetCeilingHeight() + buffer;
		}
	}

	pointColl = GetPointCollision(Vector3i(x, y, z), ideal->RoomNumber);
	if (y > pointColl.GetFloorHeight() ||
		y < pointColl.GetCeilingHeight() ||
		pointColl.GetFloorHeight() == NO_HEIGHT ||
		pointColl.GetCeilingHeight() == NO_HEIGHT ||
		pointColl.GetCeilingHeight() >= pointColl.GetFloorHeight())
	{
		return true;
	}

	ideal->RoomNumber = pointColl.GetRoomNumber();
	ideal->x = x;
	ideal->y = y;
	ideal->z = z;

	return false;
}

void FixedCamera(ItemInfo* item)
{
	// Fixed cameras before TR3 had optional "movement" effect. 
	// Later for some reason it was forced to always be 1, and actual speed value
	// from camera trigger was ignored. In TEN, we move speed value out of legacy
	// floordata trigger to camera itself and make use of it again. Still, by default,
	// value is 1 for UseForcedFixedCamera hack.

	int moveSpeed = 1;

	GameVector origin, target;
	if (UseForcedFixedCamera)
		origin = ForcedFixedCamera;
	else
	{
		auto* camera = &g_Level.Cameras[Camera.number];

		origin = GameVector(camera->Position, camera->RoomNumber);

		// Multiply original speed by 8 to comply with original bitshifted speed from TR1-2.
		moveSpeed = camera->Speed * 8 + 1;
	}

	Camera.fixedCamera = true;

	MoveCamera(&origin, moveSpeed);

	if (Camera.timer)
	{
		if (!--Camera.timer)
			Camera.timer = -1;
	}
}

void BounceCamera(ItemInfo* item, short bounce, short maxDistance)
{
	float distance = Vector3i::Distance(item->Pose.Position, Camera.pos.ToVector3i());
	if (distance < maxDistance)
	{
		if (maxDistance == -1)
			Camera.bounce = bounce;
		else
			Camera.bounce = -(bounce * (maxDistance - distance) / maxDistance);
	}
	else if (maxDistance == -1)
		Camera.bounce = bounce;
}

void BinocularCamera(ItemInfo* item)
{
	auto& player = GetLaraInfo(*item);

	AlterFOV(7 * (ANGLE(11.5f) - player.Control.Look.OpticRange), false);

	int x = item->Pose.Position.x;
	int y = item->Pose.Position.y + GameBoundingBox(item).Y1;
	int z = item->Pose.Position.z;

	auto pointColl = GetPointCollision(Vector3i(x, y, z), item->RoomNumber);

	Camera.pos.x = x;
	Camera.pos.y = y;
	Camera.pos.z = z;
	Camera.pos.RoomNumber = pointColl.GetRoomNumber();

	float l = BLOCK(20.25f) * phd_cos(player.Control.Look.Orientation.x);
	float tx = x + l * phd_sin(item->Pose.Orientation.y + player.Control.Look.Orientation.y);
	float ty = y - BLOCK(20.25f) * phd_sin(player.Control.Look.Orientation.x);
	float tz = z + l * phd_cos(item->Pose.Orientation.y + player.Control.Look.Orientation.y);

	if (Camera.oldType == CameraType::Fixed)
	{
		Camera.target.x = tx;
		Camera.target.y = ty;
		Camera.target.z = tz;
		Camera.target.RoomNumber = item->RoomNumber;
	}
	else
	{
		Camera.target.x += (tx - Camera.target.x) / 4;
		Camera.target.y += (ty - Camera.target.y) / 4;
		Camera.target.z += (tz - Camera.target.z) / 4;
		Camera.target.RoomNumber = item->RoomNumber;
	}

	if (Camera.type == Camera.oldType)
		CalculateBounce(true);

	Camera.target.RoomNumber = GetPointCollision(Camera.pos.ToVector3i(), Camera.target.RoomNumber).GetRoomNumber();
	LookAt(&Camera, 0);
	UpdateMikePos(*item);
	Camera.oldType = Camera.type;

	GetTargetOnLOS(&Camera.pos, &Camera.target, false, false);
}

void ConfirmCameraTargetPos()
{
	auto pos = Vector3i(
		LaraItem->Pose.Position.x,
		LaraItem->Pose.Position.y - (LaraCollision.Setup.Height / 2),
		LaraItem->Pose.Position.z);

	if (Camera.laraNode != -1)
	{
		Camera.target.x = pos.x;
		Camera.target.y = pos.y;
		Camera.target.z = pos.z;
	}
	else
	{
		Camera.target.x = LaraItem->Pose.Position.x;
		Camera.target.y = (Camera.target.y + pos.y) / 2;
		Camera.target.z = LaraItem->Pose.Position.z;
	}

	int y = Camera.target.y;
	auto pointColl = GetPointCollision(Vector3i(Camera.target.x, y, Camera.target.z), Camera.target.RoomNumber);
	if (y < pointColl.GetCeilingHeight() ||
		pointColl.GetFloorHeight() < y ||
		pointColl.GetFloorHeight() <= pointColl.GetCeilingHeight() ||
		pointColl.GetFloorHeight() == NO_HEIGHT ||
		pointColl.GetCeilingHeight() == NO_HEIGHT)
	{
		Camera.target.x = pos.x;
		Camera.target.y = pos.y;
		Camera.target.z = pos.z;
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

void CalculateCamera(const CollisionInfo& coll)
{
	if (ItemCameraOn)
		return;

	if (!HandlePlayerOptics(*LaraItem))
	{
		Camera.pos = LastPosition;
		Camera.target = LastTarget;
	}

	if (Lara.Control.Look.IsUsingBinoculars)
	{
		BinocularCamera(LaraItem);
		return;
	}
	else
	{
		LastPosition = Camera.pos;
	}

	if (UseForcedFixedCamera != 0)
	{
		Camera.type = CameraType::Fixed;
		if (Camera.oldType != CameraType::Fixed)
			Camera.speed = 1;
	}

	// Camera is in a water room, play water sound effect.
	if (TestEnvironment(ENV_FLAG_WATER, Camera.pos.RoomNumber))
	{
		SoundEffect(SFX_TR4_UNDERWATER, nullptr, SoundEnvironment::Always);
		if (Camera.underwater == false)
			Camera.underwater = true;
	}
	else
	{
		if (Camera.underwater == true)
			Camera.underwater = false;
	}

	ItemInfo* item = nullptr;
	bool isFixedCamera = false;
	if (Camera.item != nullptr &&
		(Camera.type == CameraType::Fixed || Camera.type == CameraType::Heavy))
	{
		item = Camera.item;
		isFixedCamera = true;
	}
	else
	{
		item = LaraItem;
		isFixedCamera = false;
	}

	auto bounds = GameBoundingBox(item);

	int x;
	int y = item->Pose.Position.y + bounds.Y2 + (3 * (bounds.Y1 - bounds.Y2) / 4);
	int z;

	if (Camera.item)
	{
		if (!isFixedCamera)
		{
			auto deltaPos = Camera.item->Pose.Position - item->Pose.Position;
			float dist = Vector3i::Distance(Camera.item->Pose.Position, item->Pose.Position);

			auto lookOrient = EulerAngles(
				phd_atan(dist, y - (bounds.Y1 + bounds.Y2) / 2 - Camera.item->Pose.Position.y),
				phd_atan(deltaPos.z, deltaPos.x) - item->Pose.Orientation.y,
				0) / 2;

			if (lookOrient.y > ANGLE(-50.0f) &&	lookOrient.y < ANGLE(50.0f) &&
				lookOrient.z > ANGLE(-85.0f) && lookOrient.z < ANGLE(85.0f))
			{
				short angleDelta = lookOrient.y - Lara.ExtraHeadRot.y;
				if (angleDelta > ANGLE(4.0f))
				{
					Lara.ExtraHeadRot.y += ANGLE(4.0f);
				}
				else if (angleDelta < ANGLE(-4.0f))
				{
					Lara.ExtraHeadRot.y -= ANGLE(4.0f);
				}
				else
				{
					Lara.ExtraHeadRot.y += angleDelta;
				}
				Lara.ExtraTorsoRot.y = Lara.ExtraHeadRot.y;

				angleDelta = lookOrient.z - Lara.ExtraHeadRot.x;
				if (angleDelta > ANGLE(4.0f))
				{
					Lara.ExtraHeadRot.x += ANGLE(4.0f);
				}
				else if (angleDelta < ANGLE(-4.0f))
				{
					Lara.ExtraHeadRot.x -= ANGLE(4.0f);
				}
				else
				{
					Lara.ExtraHeadRot.x += angleDelta;
				}
				Lara.ExtraTorsoRot.x = Lara.ExtraHeadRot.x;

				Lara.Control.Look.Orientation = lookOrient;
				Camera.type = CameraType::Look;
				Camera.item->LookedAt = true;
			}
		}
	}

	if (Camera.type == CameraType::Look ||
		Camera.type == CameraType::Combat)
	{
		if (Camera.type == CameraType::Combat)
		{
			LastTarget.x = Camera.target.x;
			LastTarget.y = Camera.target.y;
			LastTarget.z = Camera.target.z;
			LastTarget.RoomNumber = Camera.target.RoomNumber;

			y -= CLICK(1);
		}

		Camera.target.RoomNumber = item->RoomNumber;

		if (Camera.fixedCamera || Lara.Control.Look.IsUsingBinoculars)
		{
			Camera.target.y = y;
			Camera.speed = 1;
		}
		else
		{
			Camera.target.y += (y - Camera.target.y) / 4;
			Camera.speed = (Camera.type != CameraType::Look) ? 8 : 4;
		}

		Camera.fixedCamera = false;
		if (Camera.type == CameraType::Look)
		{
			LookCamera(*item, coll);
		}
		else
		{
			CombatCamera(item);
		}
	}
	else
	{
		LastTarget.x = Camera.target.x;
		LastTarget.y = Camera.target.y;
		LastTarget.z = Camera.target.z;
		LastTarget.RoomNumber = Camera.target.RoomNumber;

		Camera.target.RoomNumber = item->RoomNumber;
		Camera.target.y = y;

		x = item->Pose.Position.x;
		z = item->Pose.Position.z;

		if (Camera.flags == CF_FOLLOW_CENTER)	//Troye Aug. 7th 2022
		{
			auto shift = (bounds.Z1 + bounds.Z2) / 2;
			x += shift * phd_sin(item->Pose.Orientation.y);
			z += shift * phd_cos(item->Pose.Orientation.y);
		}

		Camera.target.x = x;
		Camera.target.z = z;

		// CF_FOLLOW_CENTER sets target on the item, ConfirmCameraTargetPos overrides this target, 
		// hence the flag check. Troye Aug. 7th 2022
		
		if (item->IsLara() && Camera.flags != CF_FOLLOW_CENTER)
			ConfirmCameraTargetPos();

		if (isFixedCamera == Camera.fixedCamera)
		{
			Camera.fixedCamera = false;

			if (Camera.speed != 1 && Camera.oldType != CameraType::Look &&
				!Lara.Control.Look.IsUsingBinoculars)
			{
				if (TargetSnaps <= 8)
				{
					x = LastTarget.x + ((x - LastTarget.x) / 4);
					y = LastTarget.y + ((y - LastTarget.y) / 4);
					z = LastTarget.z + ((z - LastTarget.z) / 4);

					Camera.target.x = x;
					Camera.target.y = y;
					Camera.target.z = z;
				}
				else
				{
					TargetSnaps = 0;
				}
			}
		}
		else
		{
			Camera.fixedCamera = true;
			Camera.speed = 1;
		}

		Camera.target.RoomNumber = GetPointCollision(Vector3i(x, y, z), Camera.target.RoomNumber).GetRoomNumber();

		if (abs(LastTarget.x - Camera.target.x) < 4 &&
			abs(LastTarget.y - Camera.target.y) < 4 &&
			abs(LastTarget.z - Camera.target.z) < 4)
		{
			Camera.target.x = LastTarget.x;
			Camera.target.y = LastTarget.y;
			Camera.target.z = LastTarget.z;
		}

		if (Camera.type != CameraType::Chase && Camera.flags != CF_CHASE_OBJECT)
		{
			FixedCamera(item);
		}
		else
		{
			ChaseCamera(item);
		}
	}

	Camera.fixedCamera = isFixedCamera;
	Camera.last = Camera.number;
	Camera.DisableInterpolation = (Camera.DisableInterpolation || Camera.lastType != Camera.type);
	Camera.lastType = Camera.type;

	if (CalculateDeathCamera(*LaraItem))
		return;

	if (Camera.type != CameraType::Heavy || Camera.timer == -1)
	{
		Camera.type = CameraType::Chase;
		Camera.speed = 10;
		Camera.number = -1;
		Camera.lastItem = Camera.item;
		Camera.item = nullptr;
		Camera.targetElevation = 0;
		Camera.targetAngle = 0;
		Camera.targetDistance = BLOCK(1.5f);
		Camera.flags = 0;
		Camera.laraNode = -1;
	}
}

bool TestBoundsCollideCamera(const GameBoundingBox& bounds, const Pose& pose, short radius)
{
	auto camSphere = BoundingSphere(Camera.pos.ToVector3(), radius);
	auto boundsSphere = BoundingSphere(pose.Position.ToVector3(), bounds.GetExtents().Length());

	if (!camSphere.Intersects(boundsSphere))
		return false;

	return camSphere.Intersects(bounds.ToBoundingOrientedBox(pose));
}

float GetParticleDistanceFade(const Vector3i& pos)
{
	float dist = Vector3::Distance(Camera.pos.ToVector3(), pos.ToVector3());
	if (dist <= PARTICLE_FADE_THRESHOLD)
		return 1.0f;

	return std::clamp(1.0f - ((dist - PARTICLE_FADE_THRESHOLD) / COLL_CHECK_THRESHOLD), 0.0f, 1.0f);
}

void ItemPushCamera(GameBoundingBox* bounds, Pose* pos, short radius)
{
	int dx = Camera.pos.x - pos->Position.x;
	int dz = Camera.pos.z - pos->Position.z;
	auto sinY = phd_sin(pos->Orientation.y);
	auto cosY = phd_cos(pos->Orientation.y);
	auto x = (dx * cosY) - (dz * sinY);
	auto z = (dx * sinY) + (dz * cosY);

	int xMin = bounds->X1 - radius;
	int xMax = bounds->X2 + radius;
	int zMin = bounds->Z1 - radius;
	int zMax = bounds->Z2 + radius;

	if (x <= xMin || x >= xMax || z <= zMin || z >= zMax)
		return;

	auto left = x - xMin;
	auto right = xMax - x;
	auto top = zMax - z;
	auto bottom = z - zMin;

	// Left is closest.
	if (left <= right && left <= top && left <= bottom)
		x -= left;
	// Right is closest.
	else if (right <= left && right <= top && right <= bottom)
		x += right;
	// Top is closest.
	else if (top <= left && top <= right && top <= bottom)
		z += top;
	// Bottom is closest.
	else
		z -= bottom;

	Camera.pos.x = pos->Position.x + ((x * cosY) + (z * sinY));
	Camera.pos.z = pos->Position.z + ((z * cosY) - (x * sinY));

	auto pointColl = GetPointCollision(Camera.pos.ToVector3i(), Camera.pos.RoomNumber);
	if (pointColl.GetFloorHeight() == NO_HEIGHT || Camera.pos.y > pointColl.GetFloorHeight() || Camera.pos.y < pointColl.GetCeilingHeight())
		Camera.pos = GameVector(LastPosition.ToVector3i(), pointColl.GetRoomNumber());
}

bool CheckItemCollideCamera(ItemInfo* item)
{
	bool isCloseEnough = Vector3i::Distance(item->Pose.Position, Camera.pos.ToVector3i()) <= COLL_CHECK_THRESHOLD;

	if (!isCloseEnough || !item->Collidable || !Objects[item->ObjectNumber].usingDrawAnimatingItem)
		return false;

	// TODO: Find a better way to define objects which are collidable with camera.
	auto* object = &Objects[item->ObjectNumber];
	if (object->intelligent || object->isPickup || object->isPuzzleHole || object->collision == nullptr)
		return false;

	// Check extents; if any 2 bounds are smaller than threshold, discard.
	auto extents = GameBoundingBox(item).ToBoundingOrientedBox(item->Pose).Extents;
	if ((abs(extents.x) < COLL_DISCARD_THRESHOLD && abs(extents.y) < COLL_DISCARD_THRESHOLD) ||
		(abs(extents.x) < COLL_DISCARD_THRESHOLD && abs(extents.z) < COLL_DISCARD_THRESHOLD) ||
		(abs(extents.y) < COLL_DISCARD_THRESHOLD && abs(extents.z) < COLL_DISCARD_THRESHOLD))
	{
		return false;
	}

	return true;
}

static std::vector<int> FillCollideableItemList()
{
	auto itemList = std::vector<int>{};
	auto& roomList = g_Level.Rooms[Camera.pos.RoomNumber].NeighborRoomNumbers;

	for (short i = 0; i < g_Level.NumItems; i++)
	{
		const auto& item = g_Level.Items[i];

		if (std::find(roomList.begin(), roomList.end(), item.RoomNumber) == roomList.end())
			continue;

		if (!g_Level.Rooms[item.RoomNumber].Active())
			continue;

		if (!CheckItemCollideCamera(&g_Level.Items[i]))
			continue;

		itemList.push_back(i);
	}

	return itemList;
}

bool CheckStaticCollideCamera(MESH_INFO* mesh)
{
	bool isCloseEnough = Vector3i::Distance(mesh->pos.Position, Camera.pos.ToVector3i()) <= COLL_CHECK_THRESHOLD;
	if (!isCloseEnough)
		return false;

	if (!(mesh->flags & StaticMeshFlags::SM_VISIBLE))
		return false;

	const auto& bounds = GetBoundsAccurate(*mesh, false);
	auto extents = Vector3(
		abs(bounds.X1 - bounds.X2),
		abs(bounds.Y1 - bounds.Y2),
		abs(bounds.Z1 - bounds.Z2));

	// Check extents; if any two bounds are smaller than threshold, discard.
	if ((abs(extents.x) < COLL_DISCARD_THRESHOLD && abs(extents.y) < COLL_DISCARD_THRESHOLD) ||
		(abs(extents.x) < COLL_DISCARD_THRESHOLD && abs(extents.z) < COLL_DISCARD_THRESHOLD) ||
		(abs(extents.y) < COLL_DISCARD_THRESHOLD && abs(extents.z) < COLL_DISCARD_THRESHOLD))
	{
		return false;
	}

	return true;
}

std::vector<MESH_INFO*> FillCollideableStaticsList()
{
	std::vector<MESH_INFO*> staticList;
	auto& roomList = g_Level.Rooms[Camera.pos.RoomNumber].NeighborRoomNumbers;

	for (int i : roomList)
	{
		auto* room = &g_Level.Rooms[i];

		if (!room->Active())
			continue;

		for (short j = 0; j < room->mesh.size(); j++)
		{
			if (!CheckStaticCollideCamera(&room->mesh[j]))
				continue;

			staticList.push_back(&room->mesh[j]);
		}
	}

	return staticList;
}

void ItemsCollideCamera()
{
	if (!g_GameFlow->GetSettings()->Camera.ObjectCollision)
		return;

	constexpr auto RADIUS = CLICK(0.5f);

	auto itemList = FillCollideableItemList();

	// Collide with items in the items list.
	for (int i = 0; i < itemList.size(); i++)
	{
		auto* item = &g_Level.Items[itemList[i]];
		if (!item)
			return;

		// Break off if camera is stuck behind an object and the player runs off.
		auto distance = Vector3i::Distance(item->Pose.Position, LaraItem->Pose.Position);
		if (distance > COLL_CANCEL_THRESHOLD)
			continue;

		auto bounds = GameBoundingBox(item);
		if (TestBoundsCollideCamera(bounds, item->Pose, CAMERA_RADIUS))
			ItemPushCamera(&bounds, &item->Pose, RADIUS);

		if (DebugMode)
		{
			DrawDebugBox(
				bounds.ToBoundingOrientedBox(item->Pose),
				Vector4(1.0f, 0.0f, 0.0f, 1.0f), RendererDebugPage::CollisionStats);
		}
	}

	// Done.
	itemList.clear();

	// Collide with static meshes.
	auto staticList = FillCollideableStaticsList();
	for (auto* mesh : staticList)
	{
		if (!mesh)
			return;

		auto distance = Vector3i::Distance(mesh->pos.Position, LaraItem->Pose.Position);
		if (distance > COLL_CANCEL_THRESHOLD)
			continue;

		auto bounds = GetBoundsAccurate(*mesh, false);
		if (TestBoundsCollideCamera(bounds, mesh->pos, CAMERA_RADIUS))
			ItemPushCamera(&bounds, &mesh->pos, RADIUS);

		if (DebugMode)
		{
			DrawDebugBox(
				bounds.ToBoundingOrientedBox(mesh->pos),
				Vector4(1.0f, 0.0f, 0.0f, 1.0f), RendererDebugPage::CollisionStats);
		}
	}

	// Done.
	staticList.clear();
}

void PrepareCamera()
{
	if (TrackCameraInit)
	{
		UseSpotCam = false;
		AlterFOV(LastFOV);
	}
}

static void DrawPortals()
{
	constexpr auto EXT_COLOR = Color(1.0f, 1.0f, 0.0f, 0.15f);
	constexpr auto INT_COLOR = Color(1.0f, 0.0f, 0.0f, 0.15f);

	if (!DebugMode)
		return;

	auto neighborRoomNumbers = GetNeighborRoomNumbers(Camera.pos.RoomNumber, 1);
	for (auto& roomNumber : neighborRoomNumbers)
	{
		const auto& room = g_Level.Rooms[roomNumber];

		auto pos = room.Position.ToVector3();
		auto color = (roomNumber == Camera.pos.RoomNumber) ? INT_COLOR : EXT_COLOR;

		for (const auto& door : room.doors)
		{
			DrawDebugTriangle(door.vertices[0] + pos, door.vertices[1] + pos, door.vertices[2] + pos, color, RendererDebugPage::PortalDebug);
			DrawDebugTriangle(door.vertices[2] + pos, door.vertices[3] + pos, door.vertices[0] + pos, color, RendererDebugPage::PortalDebug);

			DrawDebugLine(door.vertices[0] + pos, door.vertices[2] + pos, color, RendererDebugPage::PortalDebug);
			DrawDebugLine(door.vertices[1] + pos, door.vertices[3] + pos, color, RendererDebugPage::PortalDebug);

			auto center = pos + ((door.vertices[0] + door.vertices[1] + door.vertices[2] + door.vertices[3]) / 4);
			auto target = Geometry::TranslatePoint(center, door.normal, CLICK(1));

			DrawDebugLine(center, target, color, RendererDebugPage::PortalDebug);
		}
	}
}

void UpdateCamera()
{
	// HACK: Disable interpolation when switching to/from flyby camera.
	// When camera structs are converted to a class, this should go to getter/setter. -- Lwmte, 29.10.2024
	if (UseSpotCam != SpotcamSwitched)
	{
		Camera.DisableInterpolation = true;
		SpotcamSwitched = UseSpotCam;
	}

	if (UseSpotCam)
	{
		// Draw flyby cameras.
		CalculateSpotCameras();
	}
	else
	{
		// Do the standard camera.
		TrackCameraInit = false;
		CalculateCamera(LaraCollision);
	}

	// Update cameras matrices there, after having done all the possible camera logic.
	g_Renderer.UpdateCameraMatrices(&Camera, BLOCK(g_GameFlow->GetLevel(CurrentLevel)->GetFarView()));

	DrawPortals();
}

void UpdateMikePos(const ItemInfo& item)
{
	if (Camera.mikeAtLara)
	{
		Camera.mikePos = item.Pose.Position;
		Camera.actualAngle = item.Pose.Orientation.y;

		if (item.IsLara())
		{
			const auto& player = GetLaraInfo(item);
			Camera.actualAngle += player.ExtraHeadRot.y + player.ExtraTorsoRot.y;
		}
	}
	else
	{
		int phdPerspective = g_Configuration.ScreenWidth / 2 * phd_cos(CurrentFOV / 2) / phd_sin(CurrentFOV / 2);

		Camera.actualAngle = phd_atan(Camera.target.z - Camera.pos.z, Camera.target.x - Camera.pos.x);
		Camera.mikePos.x = Camera.pos.x + phdPerspective * phd_sin(Camera.actualAngle);
		Camera.mikePos.z = Camera.pos.z + phdPerspective * phd_cos(Camera.actualAngle);
		Camera.mikePos.y = Camera.pos.y;
	}
}

void RumbleScreen()
{
	if (!(GlobalCounter & 0x1FF))
		// SFX Enum Changed from TR5 and pitch shift removed. User can set this in their sound XML. Stranger1992 31st August 2024
		SoundEffect(SFX_TR4_ENVIORONMENT_RUMBLE, nullptr, SoundEnvironment::Land);

	if (RumbleTimer >= 0)
		RumbleTimer++;

	if (RumbleTimer > 450)
	{
		if (!(GetRandomControl() & 0x1FF))
		{
			RumbleCounter = 0;
			RumbleTimer = -32 - (GetRandomControl() & 0x1F);
			return;
		}
	}

	if (RumbleTimer < 0)
	{
		if (RumbleCounter >= abs(RumbleTimer))
		{
			Camera.bounce = -(GetRandomControl() % abs(RumbleTimer));
			RumbleTimer++;
		}
		else
		{
			RumbleCounter++;
			Camera.bounce = -(GetRandomControl() % RumbleCounter);
		}
	}
}

void SetScreenFadeOut(float speed, bool force)
{
	if (ScreenFading && !force)
		return;

	ScreenFading = true;
	ScreenFadeStart = 1.0f;
	ScreenFadeEnd = 0;
	ScreenFadeSpeed = speed;
	ScreenFadeCurrent = ScreenFadeStart;
}

void SetScreenFadeIn(float speed, bool force)
{
	if (ScreenFading && !force)
		return;

	ScreenFading = true;
	ScreenFadeStart = 0;
	ScreenFadeEnd = 1.0f;
	ScreenFadeSpeed = speed;
	ScreenFadeCurrent = ScreenFadeStart;
}

void SetCinematicBars(float height, float speed)
{
	CinematicBarsDestinationHeight = height;
	CinematicBarsSpeed = speed;
}

void ClearCinematicBars()
{
	CinematicBarsHeight = 0;
	CinematicBarsDestinationHeight = 0;
	CinematicBarsSpeed = 0;
}

void UpdateFadeScreenAndCinematicBars()
{
	if (CinematicBarsDestinationHeight < CinematicBarsHeight)
	{
		CinematicBarsHeight -= CinematicBarsSpeed;
		if (CinematicBarsDestinationHeight > CinematicBarsHeight)
			CinematicBarsHeight = CinematicBarsDestinationHeight;
	}
	else if (CinematicBarsDestinationHeight > CinematicBarsHeight)
	{
		CinematicBarsHeight += CinematicBarsSpeed;
		if (CinematicBarsDestinationHeight < CinematicBarsHeight)
			CinematicBarsHeight = CinematicBarsDestinationHeight;
	}

	int prevScreenFadeCurrent = ScreenFadeCurrent;

	if (ScreenFadeEnd != 0 && ScreenFadeEnd >= ScreenFadeCurrent)
	{
		ScreenFadeCurrent += ScreenFadeSpeed;
		if (ScreenFadeCurrent > ScreenFadeEnd)
		{
			ScreenFadeCurrent = ScreenFadeEnd;
			if (prevScreenFadeCurrent >= ScreenFadeCurrent)
			{
				ScreenFadedOut = true;
				ScreenFading = false;
			}

		}
	}
	else if (ScreenFadeEnd < ScreenFadeCurrent)
	{
		ScreenFadeCurrent -= ScreenFadeSpeed;
		if (ScreenFadeCurrent < ScreenFadeEnd)
		{
			ScreenFadeCurrent = ScreenFadeEnd;
			ScreenFading = false;
		}
	}
}
