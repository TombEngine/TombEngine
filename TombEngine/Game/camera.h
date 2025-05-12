#pragma once
#include "Game/items.h"
#include "Math/Math.h"
#include "Specific/Clock.h"

struct CollisionInfo;

constexpr auto LOOKCAM_ORIENT_CONSTRAINT = std::pair<EulerAngles, EulerAngles>(
	EulerAngles(ANGLE(-70.0f), ANGLE(-90.0f), 0),
	EulerAngles(ANGLE(60.0f), ANGLE(90.0f), 0));

enum class CameraType
{
	Chase,
	Fixed,
	Look,
	Combat,
	Heavy
};

struct CAMERA_INFO
{
	GameVector pos;
	GameVector target;
	CameraType type;
	CameraType oldType;
	CameraType lastType;
	int shift;
	int flags;
	bool fixedCamera;
	bool underwater;
	int bounce;
	int targetDistance;
	short targetAngle;
	short targetElevation;
	short actualElevation;
	short actualAngle;
	short laraNode;
	short box;
	short number;
	short last;
	short timer;
	short speed;
	short targetspeed;
	ItemInfo* item;
	ItemInfo* lastItem;
	int mikeAtLara;
	Vector3i mikePos;

	float Roll = 0.0f;
	float Fov  = 0.0f;

	bool DisableInterpolation = false;
};

enum CAMERA_FLAGS
{
	CF_NONE			 = 0,
	CF_FOLLOW_CENTER = 1,
	CF_NO_CHUNKY	 = 2,
	CF_CHASE_OBJECT	 = 3,
};

constexpr auto FADE_SCREEN_SPEED = 2.0f / FPS;
constexpr auto DEFAULT_FOV = 80.0f;

extern CAMERA_INFO Camera;
extern GameVector ForcedFixedCamera;
extern int UseForcedFixedCamera;
extern CameraType BinocularOldCamera;
extern short CurrentFOV;
extern short LastFOV;

extern bool  ScreenFadedOut;
extern bool  ScreenFading;
extern float ScreenFadeSpeed;
extern float ScreenFadeStart;
extern float ScreenFadeEnd;
extern float ScreenFadeCurrent;
extern float CinematicBarsDestinationHeight;
extern float CinematicBarsHeight;
extern float CinematicBarsSpeed;

void DoThumbstickCamera();
void LookCamera(ItemInfo& item, const CollisionInfo& coll);

void LookAt(CAMERA_INFO* cam, short roll);
void AlterFOV(short value, bool store = true);
short GetCurrentFOV();
void InitializeCamera();
void MoveCamera(GameVector* ideal, int speed);
void ChaseCamera(ItemInfo* item);
void UpdateCameraElevation();
void CombatCamera(ItemInfo* item);
bool CameraCollisionBounds(GameVector* ideal, int push, bool yFirst);
void FixedCamera(ItemInfo* item);
void BounceCamera(ItemInfo* item, short bounce, short maxDistance);
void BinocularCamera(ItemInfo* item);
void ConfirmCameraTargetPos();
void CalculateCamera(const CollisionInfo& coll);
void CalculateBounce(bool binocularMode);
void RumbleScreen();
bool TestBoundsCollideCamera(const GameBoundingBox& bounds, const Pose& pose, short radius);
void ItemPushCamera(GameBoundingBox* bounds, Pose* pos, short radius);
void ItemsCollideCamera();
void RefreshFixedCamera(short camNumber);

void ObjCamera(ItemInfo* camSlotId, int camMeshID, ItemInfo* targetItem, int targetMeshID, bool cond);
void MoveObjCamera(GameVector* ideal, ItemInfo* camSlotId, int camMeshID, ItemInfo* targetItem, int targetMeshID);
void ClearObjCamera();

void SetScreenFadeOut(float speed, bool force = false);
void SetScreenFadeIn(float speed, bool force = false);
void SetCinematicBars(float height, float speed);
void ClearCinematicBars();
void PrepareCamera();
void UpdateCamera();
void UpdateFadeScreenAndCinematicBars();
void UpdateMikePos(const ItemInfo& item);

float GetParticleDistanceFade(const Vector3i& pos);
