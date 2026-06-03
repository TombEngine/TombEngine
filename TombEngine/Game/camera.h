#pragma once
#include "Game/items.h"
#include "Math/Math.h"
#include "Specific/Clock.h"

struct CollisionInfo;

using namespace TEN::Math;

constexpr auto DEFAULT_FOV				 = 80.0f;
constexpr auto FADE_SCREEN_SPEED		 = 16.0f / 255.0f;
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

enum class CameraFlag
{
	None,
	FollowCenter,
	NoChunky,
	ChaseObject
};

// TODO: Prepared for later refactor.
/*struct CameraSphereData
{
	Vector3 Position		 = Vector3::Zero;
	int		RoomNumber		 = 0;
	Vector3 LookAt			 = Vector3::Zero;
	int		LookAtRoomNumber = 0;

	EulerAngles Orientation = EulerAngles::Identity;
	float		Distance	= 0.0f;
};

class CameraObject
{
private:
	float _speedAlpha = 0.0f;

public:
	CameraSphereData Sphere		  = {};
	CameraSphereData SphereTarget = {};

private:
	void UpdateSphere()
	{
		auto posDelta = Vector3::Lerp(Sphere.Position, SphereTarget.Position, _speedAlpha) - Sphere.Position;
		auto posDir = SphereTarget.Position - Sphere.Position;
		posDir.Normalize();

		// Update position.
		Sphere.Position += posDelta;
		Sphere.RoomNumber = GetCollision(Sphere.Position, Sphere.RoomNumber, posDir, posDelta.Length()).RoomNumber;

		auto lookAtDelta = Vector3::Lerp(Sphere.LookAt, SphereTarget.LookAt, _speedAlpha) - Sphere.LookAt;
		auto lookAtDir = SphereTarget.LookAt - Sphere.LookAt;
		lookAtDir.Normalize();

		// Update lookAt.
		Sphere.LookAt += posDelta;
		Sphere.LookAtRoomNumber = GetCollision(Sphere.LookAt, Sphere.LookAtRoomNumber, lookAtDir, lookAtDelta.Length()).RoomNumber;

		// Update orientation and distance.
		Sphere.Orientation.Lerp(SphereTarget.Orientation, _speedAlpha);
		Sphere.Distance = Lerp(Sphere.Distance, SphereTarget.Distance, _speedAlpha);
	}
};*/

struct ObjectCameraInfo
{
	GameVector LastAngle	= GameVector::Zero;
	bool	   ItemCameraOn = false;
};

struct PrevCameraData
{
	Pose	pos	   = Pose::Zero;
	Pose	pos2   = Pose::Zero;
	Vector3 target = Vector3::Zero; // LookAt

	// Camera sphere

	short actualAngle	  = 0;
	short actualElevation = 0;
	short targetElevation = 0;
	float targetDistance  = 0.0f;

	// Player anim state

	int ActiveState = 0;
	int TargetState = 0;
};

struct CameraLosCollisionData
{
	Vector3 Position   = Vector3::Zero;
	int		RoomNumber = 0;
	Vector3 Normal	   = Vector3::Zero;

	bool  IsIntersected = false;
	float Distance		= 0.0f;
};

struct CameraInfo
{
	// Camera sphere

	Vector3 Position		 = Vector3::Zero;
	Vector3 Offset			 = Vector3::Zero;
	int		RoomNumber		 = 0;
	Vector3 LookAt			 = Vector3::Zero;
	int		LookAtRoomNumber = 0;
	short	actualAngle		 = 0; // AzimuthAngle
	short	targetAngle		 = 0;
	short	actualElevation	 = 0; // AltitudeAngle
	short	targetElevation	 = 0;
	float	Distance		 = 0.0f;
	float	targetDistance	 = 0.0f;
	float	Radius			 = 0.0f;
	float	speed			 = 0.0f;
	float	targetspeed		 = 0.0f;
	float   Roll             = 0.0f;
	short	Fov				 = 0;
	short	PrevFov			 = 0;

	EulerAngles Rotation = EulerAngles::Identity;

	Vector3 RelShift = Vector3::Zero;

	CameraType type	   = CameraType::Chase;
	CameraType oldType = CameraType::Chase;
	CameraFlag flags   = CameraFlag::None;

	int bounce		 = 0;
	int shift		 = 0;
	int numberFrames = 0;
	int laraNode	 = 0;
	int box			 = 0;
	int number		 = 0;
	int last		 = 0;
	int timer		 = 0;

	ObjectCameraInfo ItemCamera = {};
	ItemInfo*		 item		= nullptr;
	ItemInfo*		 lastItem	= nullptr;

	Vector3 ListenerPosition = Vector3::Zero;

	bool fixedCamera			 = false;
	bool underwater				 = false;
	bool IsControllingTankCamera = false;

	int TargetSnaps	  = 0;
	int RumbleTimer	  = 0;
	int RumbleCounter = 0;

	GameVector ForcedFixedCamera	= GameVector::Zero;
	bool	   UseForcedFixedCamera = false;

	PrevCameraData PrevCamera			   = {};
	GameVector	   PrevTarget			   = GameVector::Zero;
	Vector3		   PrevIdeal			   = Vector3::Zero;
	int			   PrevIdealRoomNumber	   = 0;
	CameraType	   PrevBinocularCameraType = CameraType::Chase;

	bool DisableInterpolation = false;

	// Getters

	CameraLosCollisionData GetLos(const Vector3& origin, int roomNumber, const Vector3& dir, float dist) const;
	Vector3				   GetGeometryOffset() const; // TODO
	Vector3				   GetPlayerOffset(const ItemInfo& item, const CollisionInfo& coll) const;
	EulerAngles			   GetControlRotation() const;

	// Utilities

	void Prepare();

	void Update();
	void UpdateMovement(const ItemInfo& playerItem, Vector3 idealPos, int idealRoomNumber, float speed);
	void UpdateSphere(const ItemInfo& playerItem);
	void UpdateListenerPosition(const ItemInfo& item);

	void HandleFollow(const ItemInfo& playerItem, bool isCombatCamera);
	void RumbleFromBounce();

	// Camera types

	void LookCamera(const ItemInfo& playerItem, const CollisionInfo& coll);

	// Inquirers

	bool IsLocked() const;

private:
	// Helper inquirers

	bool CanControlTankCamera(const ItemInfo& playerItem) const;
	bool TestStrafeZoom(const ItemInfo& playerItem) const;
	bool TestCollidableMoveable(const ItemInfo& mov) const;
	bool TestCollidableStatic(const StaticMesh& staticObj) const;
	bool TestCollidableObb(const BoundingOrientedBox& obb) const;

	// Helper utilities

	void UpdateAzimuthAngle(const ItemInfo& item);
	void ClampAltitudeAngle(bool isUnderwater);
};

struct ScreenEffectData
{
	bool  ScreenFadedOut	= false;
	bool  ScreenFading		= false;
	float ScreenFadeSpeed	= 0.0f;
	float ScreenFadeStart	= 0.0f;
	float ScreenFadeEnd		= 0.0f;
	float ScreenFadeCurrent = 0.0f;

	float CinematicBarsHeight			 = 0.0f;
	float CinematicBarsDestinationHeight = 0.0f;
	float CinematicBarsSpeed			 = 0.0f;
};

extern CameraInfo		g_Camera;
extern ScreenEffectData g_ScreenEffect;

void UpdatePlayerRefCameraOrient(ItemInfo& item);

void HandleLookAt(const CameraInfo& camera, short roll);
void SetFov(short fov, bool store = true);
short GetCurrentFov();
void InitializeCamera();
void ChaseCamera(const ItemInfo& playerItem);
void CombatCamera(const ItemInfo& playerItem);
void FixedCamera();
void BinocularCamera(ItemInfo* item);

void BounceCamera(ItemInfo* item, int bounce, float distMax);
void ConfirmCameraTargetPos();
void CalculateCamera(ItemInfo& playerItem, const CollisionInfo& coll);
void RumbleScreen();
bool TestBoundsCollideCamera(const GameBoundingBox& bounds, const Pose& pose, float radius);
void ObjCamera(ItemInfo* item, int boneID, ItemInfo* targetItem, int targetBoneID, bool cond);
void ClearObjCamera();
void MoveObjCamera(GameVector* ideal, ItemInfo* item, int boneID, ItemInfo* targetItem, int targetBoneID);
void RefreshFixedCamera(int cameraID);

// Screen effects

void SetScreenFadeOut(float speed, bool force = false);
void SetScreenFadeIn(float speed, bool force = false);
void SetCinematicBars(float height, float speed);
void UpdateFadeScreenAndCinematicBars();
void ClearCinematicBars();

float GetParticleDistanceFade(const Vector3i& pos);
