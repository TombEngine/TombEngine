#pragma once
#include "Math/Math.h"
#include "Objects/game_object_ids.h"

using namespace TEN::Math;

enum GAME_OBJECT_ID : short;
class EulerAngles;
class Pose;
class Vector3i;
struct CreatureBiteInfo;
struct ItemInfo;
struct ObjectInfo;

// NOTES:
// animNumber: Relative animation number.
// animIndex:  Index of animation in giant g_Level.Anims vector.

enum class AnimCommandType
{
	None,
	MoveOrigin, // "Post-animation adjustment"
	JumpVelocity,
	AttackReady,
	Deactivate,
	SoundEffect,
	Flipeffect,
	DisableInterpolation
};

struct AnimFrame
{
	GameBoundingBox			BoundingBox		 = GameBoundingBox::Zero;
	Vector3					Offset			 = Vector3::Zero;
	std::vector<Quaternion> BoneOrientations = {};
};

struct StateDispatchData
{
	int TargetState	 = NO_VALUE;
	int NumberRanges = 0;
	int RangeIndex	 = 0;
};

struct StateDispatchRangeData
{
	int StartFrame	 = 0;		// g_Level.Frames base index.
	int EndFrame	 = 0;		// g_Level.Frames end index.
	int LinkAnimNum	 = NO_VALUE; // g_Level.Anims index.
	int LinkFrameNum = NO_VALUE; // g_Level.Frames index.
};

struct AnimData
{
	int FramePtr	  = 0; // g_Level.Frames base index.
	int Interpolation = 0;
	int ActiveState	  = 0;

	// CONVENTION: +X = Right, +Y = Down, +Z = Forward.
	Vector3 VelocityStart = Vector3::Zero;
	Vector3 VelocityEnd	  = Vector3::Zero;

	int frameBase = 0; // g_Level.Frames base index.
	int frameEnd  = 0; // g_Level.Frames end index.

	int JumpAnimNum		   = NO_VALUE; // g_Level.Anims index.
	int JumpFrameNum	   = 0;		  // g_Level.Frames index.
	int NumStateDispatches = 0;
	int StateDispatchIndex = 0;
	int NumCommands		   = 0;
	int CommandIndex	   = 0;
};

struct AnimFrameInterpData
{
	const AnimFrame* FramePtr0 = nullptr;
	const AnimFrame* FramePtr1 = nullptr;
	float Alpha = 0.0f;
};

struct BoneMutator
{
	Vector3		Offset	 = Vector3::Zero;
	EulerAngles Rotation = EulerAngles::Identity;
	Vector3		Scale	 = Vector3::One;

	bool IsEmpty() const
	{
		return (Offset == Vector3::Zero &&
				Rotation == EulerAngles::Identity &&
				Scale == Vector3::One);
	};
};

// Animation controller
void AnimateItem(ItemInfo* item);

// Inquirers
bool HasStateDispatch(const ItemInfo* item, int targetState = NO_VALUE);
bool TestAnimNumber(const ItemInfo& item, int animNumber);
bool TestLastFrame(ItemInfo* item, int animNumber = NO_VALUE);
bool TestAnimFrame(const ItemInfo& item, int frameStart);
bool TestAnimFrameRange(const ItemInfo& item, int frameStart, int frameEnd);

// Entity translation
void TranslateItem(ItemInfo* item, short headingAngle, float forward, float down = 0.0f, float right = 0.0f);
void TranslateItem(ItemInfo* item, const EulerAngles& orient, float distance);
void TranslateItem(ItemInfo* item, const Vector3& direction, float distance);

// Setters
void SetAnimation(ItemInfo& item, GAME_OBJECT_ID animObjectID, int animNumber, int frameNumber = 0);
void SetAnimation(ItemInfo& item, int animNumber, int frameNumber = 0);
void SetAnimation(ItemInfo* item, int animNumber, int frameNumber = 0); // Deprecated.

// Getters
const AnimData& GetAnimData(int animIndex); // Deprecated.
const AnimData& GetAnimData(GAME_OBJECT_ID objectID, int animNumber);
const AnimData& GetAnimData(const ObjectInfo& object, int animNumber);
const AnimData& GetAnimData(const ItemInfo& item, int animNumber = NO_VALUE);
const AnimData& GetAnimData(const ItemInfo* item, int animNumber = NO_VALUE); // Deprecated.

AnimFrameInterpData GetFrameInterpData(const ItemInfo& item);
const AnimFrame&	GetAnimFrame(const ItemInfo& item, int animNumber, int frameNumber);
const AnimFrame*	GetFrame(GAME_OBJECT_ID objectID, int animNumber, int frameNumber);
const AnimFrame*	GetFirstFrame(GAME_OBJECT_ID objectID, int animNumber);
const AnimFrame*	GetLastFrame(GAME_OBJECT_ID objectID, int animNumber);
const AnimFrame&	GetBestFrame(const ItemInfo& item);

float GetEffectiveGravity(float verticalVel);

int GetAnimNumber(const ItemInfo& item);
int GetAnimIndex(const ItemInfo& item, int animNumber);

int GetFrameNumber(const ItemInfo& item);
int GetFrameNumber(ItemInfo* item); // Deprecated.
int GetFrameIndex(ItemInfo* item, int frameNumber);
int GetFrameIndex(GAME_OBJECT_ID objectID, int animNumber, int frameNumber);

int GetFrameCount(int animIndex);

int	 GetNextAnimState(ItemInfo* item);
int	 GetNextAnimState(int objectID, int animNumber);
bool GetStateDispatch(ItemInfo* item, const AnimData& anim);

void ClampRotation(Pose& outPose, short angle, short rotation); 
void DrawAnimatingItem(ItemInfo* item);

Vector3i   GetJointPosition(const ItemInfo& item, int jointIndex, const Vector3i& relOffset = Vector3i::Zero);
Vector3i   GetJointPosition(ItemInfo* item, int jointIndex, const Vector3i& relOffset = Vector3i::Zero);
Vector3i   GetJointPosition(ItemInfo* item, const CreatureBiteInfo& bite);
Vector3i   GetJointPosition(const ItemInfo& item, const CreatureBiteInfo& bite);

Vector3	   GetJointOffset(GAME_OBJECT_ID objectID, int jointIndex, bool discardZSign = false);
Quaternion GetBoneOrientation(const ItemInfo& item, int boneIndex);
float	   GetBoneLength(GAME_OBJECT_ID objectID, int boneIndex);
