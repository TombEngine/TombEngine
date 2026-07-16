#pragma once

#include "Game/Animation/Commands.h"
#include "Math/Math.h"

using namespace TEN::Math;

enum   GAME_OBJECT_ID : short;
class  EulerAngles;
class  Pose;
class  Vector3i;
struct CreatureBiteInfo;
struct ItemInfo;
struct ObjectInfo;

namespace TEN::Animation
{
	enum class AnimFlags
	{
		None                   = 0,
		RootMotionTranslationX = 1 << 0,
		RootMotionTranslationY = 1 << 1,
		RootMotionTranslationZ = 1 << 2,
		RootMotionRotationX    = 1 << 3,
		RootMotionRotationY    = 1 << 4,
		RootMotionRotationZ    = 1 << 5,
		RootMotionCycle        = 1 << 6
	};

	struct FrameData
	{
		Vector3                 RootPosition     = Vector3::Zero;
		std::vector<Quaternion> BoneOrientations = {};
		BoundingBox             LocalAabb        = DirectX::BoundingBox();

		// Deprecated.
		GameBoundingBox BoundingBox = GameBoundingBox::Zero; // Local AABB.
	};

	struct StateDispatchData
	{
		int          StateID             = 0;
		int          FrameNumberLow      = 0;
		int          FrameNumberHigh     = 0;
		int          NextAnimNumber      = 0;
		int          NextFrameNumberLow  = 0;
		int          NextFrameNumberHigh = 0;
		int          BlendFrameCount     = 0;
		BezierCurve2 BlendCurve          = BezierCurve2::Zero;
	};

	struct FixedMotionData
	{
		Vector3 Translation = Vector3::Zero;
		float   CurveAlpha  = 0.0f;
	};

	struct RootMotionData
	{
		Vector3     Translation = Vector3::Zero;
		EulerAngles Rotation    = EulerAngles::Identity;
	};

	struct AnimData
	{
		using AnimCommandPtr = std::shared_ptr<IAnimCommand>;

		int          StateID           = 0;
		int          EndFrameNumber    = 0;
		int          NextAnimNumber    = 0;
		int          NextFrameNumber   = 0;
		int          BlendFrameCount   = 0;
		BezierCurve2 BlendCurve        = BezierCurve2::Zero;
		BezierCurve2 FixedMotionCurveX = BezierCurve2::Zero;
		BezierCurve2 FixedMotionCurveY = BezierCurve2::Zero;
		BezierCurve2 FixedMotionCurveZ = BezierCurve2::Zero;

		std::vector<FrameData>         Frames     = {};
		std::vector<StateDispatchData> Dispatches = {};
		std::vector<AnimCommandPtr>    Commands   = {};
		int                            Flags      = (int)AnimFlags::None;

		FixedMotionData GetFixedMotion(int frameNumber) const;
		RootMotionData  GetRootMotion(int frameNumber) const;
		RootMotionData  GetRootMotionCounteraction(int frameNumber) const;

		bool HasRootTranslation() const;
		bool HasRootRotation() const;
	};

	struct BoneMutator
	{
		Vector3     Offset        = Vector3::Zero;
		EulerAngles Rotation      = EulerAngles::Identity;
		EulerAngles ExtraRotation = EulerAngles::Identity;
		Vector3     Scale         = Vector3::One;

		bool IsEmpty() const;
	};

	// Animation controller

	void AnimateItem(ItemInfo& item);
	void AnimateItem(ItemInfo* item); // TODO: Deprecated. References should be universal.

	// Inquirers

	bool TestStateDispatch(const ItemInfo& item, int targetStateID = NO_VALUE);
	bool TestLastFrame(const ItemInfo& item, int animNumber = NO_VALUE);
	bool TestAnimFrameRange(const ItemInfo& item, int frameNumberLow, int frameNumberHigh);
	bool IsSoundEffectCommandActive(const ItemInfo& item, int soundId);

	// Getters

	const AnimData& GetAnimData(const ObjectInfo& object, int animNumber);
	const AnimData& GetAnimData(GAME_OBJECT_ID objectID, int animNumber);
	const AnimData& GetAnimData(const ItemInfo& item, int animNumber = NO_VALUE);

	const FrameData& GetFrame(GAME_OBJECT_ID objectID, int animNumber, int frameNumber = 0);
	const FrameData& GetFrame(const ItemInfo& item, int animNumber, int frameNumber = 0);
	const FrameData& GetFrame(const ItemInfo& item);
	const FrameData& GetFirstFrame(GAME_OBJECT_ID objectID, int animNumber);
	const FrameData& GetLastFrame(GAME_OBJECT_ID objectID, int animNumber);

	const StateDispatchData* GetStateDispatch(const ItemInfo& item, int targetStateID = NO_VALUE);

	int   GetNextAnimState(const ItemInfo& item);
	int   GetNextAnimState(GAME_OBJECT_ID objectID, int animNumber);
	int   GetFrameCount(GAME_OBJECT_ID objectID, int animNumber); // TODO: Not needed? Not the "real" frame count anyway since 0 isn't counted.
	int   GetFrameCount(const ItemInfo& item);
	float GetEffectiveGravity(float verticalVel);
	int   GetInternalBlendDuration();

	Vector3i   GetJointPosition(const ItemInfo& item, int boneID, const Vector3i& relOffset = Vector3i::Zero);
	Vector3i   GetJointPosition(ItemInfo* item, int boneID, const Vector3i& relOffset = Vector3i::Zero);
	Vector3i   GetJointPosition(ItemInfo* item, const CreatureBiteInfo& bite);
	Vector3i   GetJointPosition(const ItemInfo& item, const CreatureBiteInfo& bite);
	Vector3    GetJointOffset(GAME_OBJECT_ID objectID, int boneID, bool discardZSign);
	Quaternion GetBoneOrientation(const ItemInfo& item, int boneID);

	// Setters

	void SetAnimationFromSlot(ItemInfo& item, GAME_OBJECT_ID animObjectID, int animNumber, int frameNumber = 0, int blendFrameCount = 0, const BezierCurve2& blendCurve = BezierCurve2::Linear);
	void SetAnimation(ItemInfo& item, int animNumber, int frameNumber = 0, int blendFrameCount = 0, const BezierCurve2& blendCurve = BezierCurve2::Linear);
	void SetAnimation(ItemInfo* item, int animNumber, int frameNumber = 0, int blendFrameCount = 0, const BezierCurve2& blendCurve = BezierCurve2::Linear); // TODO: Deprecated. Must switch to references.
	void SetExtendedAnimation(ItemInfo& item, int rawAnimNumber);
	void SetStateDispatch(ItemInfo& item, const StateDispatchData& dispatch);

	// Utilities

	void ClampRotation(Pose& outPose, short angle, short rot);
}
