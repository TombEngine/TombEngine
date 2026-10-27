#include "framework.h"
#include "Game/Animation/Skeleton.h"

#include "Game/Animation/Animation.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_tests.h"
#include "Game/Setup.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Animation
{
	constexpr auto ALL_BONES_MASK = ~0;

	static auto Skeletons = std::vector<std::vector<SkeletonBone>>{};

	// Reads the hierarchy of an object from the level bone data: per bone after the root an opcode and the offset from its
	// parent. Opcode bits 0 and 1 pop or push the parent stack, bits 2 to 4 are the axes creatures may rotate the bone around.
	static std::vector<SkeletonBone> ReadSkeleton(const ObjectInfo& object)
	{
		auto skeleton = std::vector<SkeletonBone>(object.nmeshes);
		if (object.nmeshes <= 1 || object.boneIndex < 0)
			return skeleton;

		const int* bone = &g_Level.Bones[object.boneIndex];
		auto stack = std::vector<int>{};
		int currentBone = 0;

		for (int i = 1; i < object.nmeshes; i++, bone += 4)
		{
			int opcode = bone[0];
			auto offset = Vector3((float)bone[1], (float)bone[2], (float)bone[3]);
			skeleton[i].RotationFlags = (unsigned char)(opcode & 0x1C);

			int parent = currentBone;
			switch (opcode & 0x03)
			{
			case 1:
				if (stack.empty())
					continue;

				parent = stack.back();
				stack.pop_back();
				break;

			case 2:
				stack.push_back(currentBone);
				break;

			case 3:
				if (stack.empty())
					continue;

				parent = stack.back();
				break;
			}

			skeleton[i].Parent = parent;
			skeleton[i].Offset = offset;
			currentBone = i;
		}

		return skeleton;
	}

	void InitializeSkeletons()
	{
		Skeletons.clear();
		Skeletons.resize(ID_NUMBER_OBJECTS);

		for (int i = 0; i < ID_NUMBER_OBJECTS; i++)
		{
			const auto& object = Objects[i];
			if (object.loaded && object.nmeshes > 0)
				Skeletons[i] = ReadSkeleton(object);
		}
	}

	const std::vector<SkeletonBone>& GetSkeleton(GAME_OBJECT_ID objectID)
	{
		static const auto emptySkeleton = std::vector<SkeletonBone>{};

		if (objectID < 0 || objectID >= (int)Skeletons.size())
			return emptySkeleton;

		return Skeletons[objectID];
	}

	// Poses the bones of the mask from an animation frame, blended with the frame the blend started from. Bones keep their
	// previous transform when masked out, so a pose can be built in several passes; extra rotations turn bones relative to
	// their animated orientation, or relative to the object when useObjectWorldRotation is set.
	void PoseBones(GAME_OBJECT_ID objectID, const FrameData& frame, int mask, const BoneRotations& extraRotations, SkeletonPose& pose,
				   bool useObjectWorldRotation, const MoveableAnimBlendData* blend, const RootMotionData* rootMotion, const ItemInfo* mutatedItem)
	{
		const auto& skeleton = GetSkeleton(objectID);
		int boneCount = std::min((int)skeleton.size(), BONE_COUNT_MAX);
		float blendAlpha = (blend != nullptr) ? blend->GetAlpha() : 0.0f;

		auto posedBones = std::array<int, BONE_COUNT_MAX>{};
		int posedBoneCount = 0;

		for (int i = 0; i < boneCount; i++)
		{
			const auto& bone = skeleton[i];
			bool isRoot = (i == 0);

			if (!isRoot && bone.Parent == NO_VALUE)
				continue;

			if (frame.BoneOrientations.size() <= i)
			{
				TENLog("Attempted to animate object with ID " + GetObjectName(objectID) + " using incorrect animation data. Bad animations set for slot?", LogLevel::Error);
				return;
			}

			posedBones[posedBoneCount++] = i;

			if (((mask >> i) & 1) == 0)
				continue;

			auto rootPos = frame.RootPosition;
			auto rotMatrix = Matrix::CreateFromQuaternion(frame.BoneOrientations[i]);

			if (blend != nullptr)
			{
				auto offset = (rootMotion != nullptr) ? rootMotion->Translation : Vector3::Zero;
				rootPos = Vector3::Lerp(blend->RootPosition - offset, rootPos, blendAlpha);

				auto orient = Quaternion::Slerp(blend->BoneOrientations[i], Quaternion::CreateFromRotationMatrix(rotMatrix), blendAlpha);
				rotMatrix = Matrix::CreateFromQuaternion(orient);
			}

			pose.Orientations[i] = Quaternion::CreateFromRotationMatrix(rotMatrix);

			auto extraRotMatrix = Matrix::CreateFromQuaternion(extraRotations[i]);
			if (useObjectWorldRotation && !isRoot)
			{
				auto scale = Vector3::Zero;
				auto inverseOrient = Quaternion::Identity;
				auto translation = Vector3::Zero;
				pose.Transforms[bone.Parent].Invert().Decompose(scale, inverseOrient, translation);

				rotMatrix = (rotMatrix * extraRotMatrix) * Matrix::CreateFromQuaternion(inverseOrient);
			}
			else
			{
				rotMatrix = extraRotMatrix * rotMatrix;
			}

			pose.Transforms[i] = rotMatrix * Matrix::CreateTranslation(isRoot ? rootPos : bone.Offset);
			if (!isRoot)
				pose.Transforms[i] *= pose.Transforms[bone.Parent];
		}

		// Mutators of the item scale, turn and move its bones on top of the animation.
		if (mutatedItem == nullptr || mutatedItem->Model.Mutators.size() != posedBoneCount)
			return;

		for (int i = 0; i < posedBoneCount; i++)
		{
			int boneID = posedBones[i];
			const auto& mutator = mutatedItem->Model.Mutators[boneID];
			if (mutator.IsEmpty())
				continue;

			auto rotMatrix = mutator.Rotation.ToRotationMatrix();
			auto scaleMatrix = Matrix::CreateScale(mutator.Scale);
			auto translationMatrix = Matrix::CreateTranslation(mutator.Offset);
			pose.Transforms[boneID] = ((rotMatrix * scaleMatrix) * translationMatrix) * pose.Transforms[boneID];
		}
	}

	// Joints that vehicles turn with their wheels, rudders and propellers, and creatures with their head and torso. Minecart
	// wheels keep spinning from their previous rotation.
	static void GetItemExtraRotations(const ItemInfo& item, BoneRotations& rotations)
	{
		const auto& skeleton = GetSkeleton(item.ObjectNumber);
		int boneCount = std::min((int)skeleton.size(), BONE_COUNT_MAX);
		int lastJoint = 0;

		for (int j = 0; j < boneCount; j++)
		{
			auto prevRotation = rotations[j];
			auto& rotation = rotations[j];
			rotation = Quaternion::Identity;

			item.Data.apply(
				[&](const QuadBikeInfo& quadBike)
				{
					if (j == 3 || j == 4)
					{
						rotation = EulerAngles(quadBike.RearRot, 0, 0).ToQuaternion();
					}
					else if (j == 6 || j == 7)
					{
						rotation = EulerAngles(quadBike.FrontRot, quadBike.TurnRate * 2, 0).ToQuaternion();
					}
				},
				[&](const JeepInfo& jeep)
				{
					switch (j)
					{
					case 9:
						rotation = EulerAngles(jeep.FrontRightWheelRotation, jeep.TurnRate * 4, 0).ToQuaternion();
						break;

					case 10:
						rotation = EulerAngles(jeep.FrontLeftWheelRotation, jeep.TurnRate * 4, 0).ToQuaternion();
						break;

					case 12:
						rotation = EulerAngles(jeep.BackRightWheelRotation, 0, 0).ToQuaternion();
						break;

					case 13:
						rotation = EulerAngles(jeep.BackLeftWheelRotation, 0, 0).ToQuaternion();
						break;
					}
				},
				[&](const MotorbikeInfo& bike)
				{
					switch (j)
					{
					case 2:
						rotation = EulerAngles(bike.RightWheelsRotation, bike.TurnRate * 8, 0).ToQuaternion();
						break;

					case 4:
						rotation = EulerAngles(bike.RightWheelsRotation, 0, 0).ToQuaternion();
						break;

					case 8:
						rotation = EulerAngles(bike.LeftWheelRotation, 0, 0).ToQuaternion();
						break;
					}
				},
				[&](const MinecartInfo& cart)
				{
					if (j >= 1 && j <= 4)
					{
						short zRot = (short)std::clamp(cart.Velocity, 0, (int)ANGLE(25.0f)) + EulerAngles(prevRotation).z;
						rotation = EulerAngles(0, 0, zRot).ToQuaternion();
					}
				},
				[&](const RubberBoatInfo& boat)
				{
					if (j == 2)
						rotation = EulerAngles(0, 0, boat.PropellerRotation).ToQuaternion();
				},
				[&](const UPVInfo& upv)
				{
					switch (j)
					{
					case 1:
						rotation = EulerAngles(upv.LeftRudderRotation, 0, 0).ToQuaternion();
						break;

					case 2:
						rotation = EulerAngles(upv.RightRudderRotation, 0, 0).ToQuaternion();
						break;

					case 3:
						rotation = EulerAngles(0, 0, upv.TurbineRotation).ToQuaternion();
						break;
					}
				},
				[&](const BigGunInfo& bigGun)
				{
					if (j == 2)
						rotation = EulerAngles(0, 0, FROM_RAD(bigGun.BarrelRotation)).ToQuaternion();
				},
				[&](const CreatureInfo& creature)
				{
					auto xRot = Quaternion::Identity;
					auto yRot = Quaternion::Identity;
					auto zRot = Quaternion::Identity;

					if (skeleton[j].RotationFlags & ROT_Y)
					{
						yRot = EulerAngles(0, creature.JointRotation[lastJoint], 0).ToQuaternion();
						lastJoint++;
					}

					if (skeleton[j].RotationFlags & ROT_X)
					{
						xRot = EulerAngles(creature.JointRotation[lastJoint], 0, 0).ToQuaternion();
						lastJoint++;
					}

					if (skeleton[j].RotationFlags & ROT_Z)
					{
						zRot = EulerAngles(0, 0, creature.JointRotation[lastJoint]).ToQuaternion();
						lastJoint++;
					}

					rotation = xRot * yRot * zRot;
				});
		}
	}

	// Back-mounted weapons turn the upper body while aiming from a standstill; the HK also while shooting from the shoulder.
	static bool ShouldAnimateUpperBody(const ItemInfo& item, const LaraInfo& player, LaraWeaponType weaponType)
	{
		bool isStandingOrTurning =
			item.Animation.ActiveState == LS_IDLE ||
			item.Animation.ActiveState == LS_TURN_LEFT_FAST ||
			item.Animation.ActiveState == LS_TURN_RIGHT_FAST ||
			item.Animation.ActiveState == LS_TURN_LEFT_SLOW ||
			item.Animation.ActiveState == LS_TURN_RIGHT_SLOW;

		switch (weaponType)
		{
		case LaraWeaponType::RocketLauncher:
		case LaraWeaponType::HarpoonGun:
		case LaraWeaponType::GrenadeLauncher:
		case LaraWeaponType::Crossbow:
		case LaraWeaponType::Shotgun:
			return isStandingOrTurning;

		case LaraWeaponType::HK:
			return (player.RightArm.AnimNumber == 0 || player.RightArm.AnimNumber == 2 || player.RightArm.AnimNumber == 4 || isStandingOrTurning);

		default:
			return false;
		}
	}

	// HACK: Arm frames for pistols, uzis, and revolver currently remain absolute.
	// Until the weapon system is rewritten from scratch, this will ensure correct behaviour. -- Sezz 2023.11.13
	static int GetNormalizedArmAnimFrame(GAME_OBJECT_ID animObjectID, int frameNumber)
	{
		int frameCount = 0;
		int animCount = (int)Objects[animObjectID].Animations.size();

		for (int i = 0; i < animCount; i++)
		{
			const auto& anim = GetAnimData(animObjectID, i);

			int currentAnimFrameCount = (int)anim.Frames.size();
			int nextFrameCount = frameCount + currentAnimFrameCount;

			if (frameNumber < nextFrameCount)
				return (frameNumber - frameCount);

			if (i == (animCount - 1) && frameNumber >= nextFrameCount)
				return (currentAnimFrameCount - 1);

			frameCount = nextFrameCount;
		}

		return 0;
	}

	// Player arms holding a weapon: the arm animations of the weapon, turned towards the target.
	static void PosePlayerWeaponArms(const ItemInfo& item, const LaraInfo& player, LaraWeaponType weaponType, BoneRotations& extraRotations, SkeletonPose& pose)
	{
		// While handling a weapon, extra rotation may be applied to the arms.
		if (weaponType == LaraWeaponType::Pistol || weaponType == LaraWeaponType::Uzi)
		{
			extraRotations[LM_LINARM] *= player.LeftArm.Orientation.ToQuaternion();
			extraRotations[LM_RINARM] *= player.RightArm.Orientation.ToQuaternion();
		}
		else
		{
			const auto& aimingArm = (weaponType == LaraWeaponType::Revolver) ? player.LeftArm : player.RightArm;
			extraRotations[LM_RINARM] *= aimingArm.Orientation.ToQuaternion();
			extraRotations[LM_LINARM] = extraRotations[LM_RINARM];
		}

		switch (weaponType)
		{
		// HACK: Back guns are handled differently.
		case LaraWeaponType::Shotgun:
		case LaraWeaponType::HK:
		case LaraWeaponType::Crossbow:
		case LaraWeaponType::GrenadeLauncher:
		case LaraWeaponType::RocketLauncher:
		case LaraWeaponType::HarpoonGun:
		{
			int upperBodyMask = MESH_BITS(LM_TORSO) | MESH_BITS(LM_HEAD);

			int mask = MESH_BITS(LM_LINARM) | MESH_BITS(LM_LOUTARM) | MESH_BITS(LM_LHAND);
			if (ShouldAnimateUpperBody(item, player, weaponType))
				mask |= upperBodyMask;

			const auto& leftArmFrame = GetAnimData(player.LeftArm.AnimObjectID, player.LeftArm.AnimNumber).Frames[player.LeftArm.FrameNumber];
			PoseBones(ID_LARA, leftArmFrame, mask, extraRotations, pose, false, nullptr, nullptr, &item);

			mask = MESH_BITS(LM_RINARM) | MESH_BITS(LM_ROUTARM) | MESH_BITS(LM_RHAND);
			if (ShouldAnimateUpperBody(item, player, player.Control.Weapon.GunType))
				mask |= upperBodyMask;

			const auto& rightArmFrame = GetAnimData(player.RightArm.AnimObjectID, player.RightArm.AnimNumber).Frames[player.RightArm.FrameNumber];
			PoseBones(ID_LARA, rightArmFrame, mask, extraRotations, pose, false, nullptr, nullptr, &item);
		}
		break;

		case LaraWeaponType::Flare:
		case LaraWeaponType::Torch:
		{
			int leftFrameNumber = GetNormalizedArmAnimFrame(player.LeftArm.AnimObjectID, player.LeftArm.FrameNumber);
			const auto& leftFrame = GetAnimData(player.LeftArm.AnimObjectID, player.LeftArm.AnimNumber).Frames[leftFrameNumber];

			// HACK: Mask head and torso when taking out flare.
			int mask = MESH_BITS(LM_LINARM) | MESH_BITS(LM_LOUTARM) | MESH_BITS(LM_LHAND);
			if (!player.Control.IsLow && player.LeftArm.AnimNumber > 1 && player.LeftArm.AnimNumber < 4)
				mask |= MESH_BITS(LM_TORSO) | MESH_BITS(LM_HEAD);

			PoseBones(ID_LARA, leftFrame, mask, extraRotations, pose, false, nullptr, nullptr, &item);

			mask = MESH_BITS(LM_RINARM) | MESH_BITS(LM_ROUTARM) | MESH_BITS(LM_RHAND);
			PoseBones(ID_LARA, GetFrame(item), mask, extraRotations, pose, false, nullptr, nullptr, &item);
		}
		break;

		case LaraWeaponType::Pistol:
		case LaraWeaponType::Uzi:
		case LaraWeaponType::Revolver:
		default:
		{
			bool isRevolver = (weaponType == LaraWeaponType::Revolver);
			bool isStill = (item.Animation.Velocity.Length() < EPSILON);

			// HACK: The revolver arm orientations are not symmetrical and get messed up while moving.
			bool movingModifier = !(isRevolver && isStill) && player.LeftArm.FrameNumber;
			bool sideJumpModifier = !(isRevolver && IsSideJumpState(item.Animation.ActiveState));
			bool transformUpperArm = (IsCrouching(&item) || player.LeftArm.Locked || movingModifier) && sideJumpModifier;

			int leftFrameNumber = GetNormalizedArmAnimFrame(player.LeftArm.AnimObjectID, player.LeftArm.FrameNumber);
			const auto& leftFrame = GetAnimData(player.LeftArm.AnimObjectID, player.LeftArm.AnimNumber).Frames[leftFrameNumber];

			int mask = MESH_BITS(LM_LOUTARM) | MESH_BITS(LM_LHAND);
			if (transformUpperArm)
			{
				PoseBones(ID_LARA, leftFrame, MESH_BITS(LM_LINARM), extraRotations, pose, true, nullptr, nullptr, &item);
			}
			else
			{
				mask |= MESH_BITS(LM_LINARM);
			}

			PoseBones(ID_LARA, leftFrame, mask, extraRotations, pose, false, nullptr, nullptr, &item);

			// HACK: Same as above, but for right arm.
			movingModifier = !(isRevolver && isStill) && player.RightArm.FrameNumber;
			transformUpperArm = IsCrouching(&item) || player.RightArm.Locked || movingModifier;

			int rightFrameNumber = GetNormalizedArmAnimFrame(player.RightArm.AnimObjectID, player.RightArm.FrameNumber);
			const auto& rightFrame = GetAnimData(player.RightArm.AnimObjectID, player.RightArm.AnimNumber).Frames[rightFrameNumber];

			mask = MESH_BITS(LM_ROUTARM) | MESH_BITS(LM_RHAND);
			if (transformUpperArm)
			{
				PoseBones(ID_LARA, rightFrame, MESH_BITS(LM_RINARM), extraRotations, pose, true, nullptr, nullptr, &item);
			}
			else
			{
				mask |= MESH_BITS(LM_RINARM);
			}

			PoseBones(ID_LARA, rightFrame, mask, extraRotations, pose, false, nullptr, nullptr, &item);
		}
		break;
		}
	}

	// Player pose: legs, hips, torso and head from the body animation, then the arms from the body animation or, while
	// handling a weapon, from the weapon arm animations.
	static void GetPlayerPose(const ItemInfo& item, SkeletonPose& pose)
	{
		const auto& player = GetLaraInfo(item);

		auto extraRotations = BoneRotations{};
		extraRotations[LM_TORSO] = player.ExtraTorsoRot.ToQuaternion();
		extraRotations[LM_HEAD] = player.ExtraHeadRot.ToQuaternion();

		auto rootMotionCounteract = GetAnimData(item).GetRootMotionCounteraction(item.Animation.FrameNumber);
		const auto* blend = item.Animation.Blend.IsEnabled() ? &item.Animation.Blend : nullptr;
		const auto& frame = GetFrame(item);

		int mask = MESH_BITS(LM_HIPS) | MESH_BITS(LM_LTHIGH) | MESH_BITS(LM_LSHIN) | MESH_BITS(LM_LFOOT) | MESH_BITS(LM_RTHIGH) |
				   MESH_BITS(LM_RSHIN) | MESH_BITS(LM_RFOOT) | MESH_BITS(LM_TORSO) | MESH_BITS(LM_HEAD);
		PoseBones(ID_LARA, frame, mask, extraRotations, pose, false, blend, &rootMotionCounteract, &item);

		auto weaponType = player.Control.Weapon.GunType;
		auto handStatus = player.Control.HandStatus;

		// HACK: Treat binoculars as two-handed weapon.
		if (player.Control.Look.IsUsingBinoculars && player.Control.HandStatus == HandStatus::Free)
		{
			weaponType = LaraWeaponType::Shotgun;
			handStatus = HandStatus::WeaponReady;
		}

		bool areArmsFree = (weaponType != LaraWeaponType::Flare && (handStatus == HandStatus::Free || handStatus == HandStatus::Busy)) ||
						   (weaponType == LaraWeaponType::Flare && !player.Flare.ControlLeft);
		if (areArmsFree)
		{
			mask = MESH_BITS(LM_LINARM) | MESH_BITS(LM_LOUTARM) | MESH_BITS(LM_LHAND) | MESH_BITS(LM_RINARM) | MESH_BITS(LM_ROUTARM) | MESH_BITS(LM_RHAND);
			PoseBones(ID_LARA, frame, mask, extraRotations, pose, false, blend, nullptr, &item);
		}
		else
		{
			PosePlayerWeaponArms(item, player, weaponType, extraRotations, pose);
		}
	}

	void GetItemPose(const ItemInfo& item, SkeletonPose& pose, BoneRotations* extraRotations)
	{
		pose.Transforms.fill(Matrix::Identity);
		pose.Orientations.fill(Quaternion::Identity);

		// Objects without animations, such as body parts borrowing a single mesh, keep identity transforms.
		const auto& object = Objects[item.ObjectNumber];
		if (!object.loaded || object.Animations.empty())
			return;

		if (item.IsLara())
		{
			GetPlayerPose(item, pose);
			return;
		}

		auto localRotations = BoneRotations{};
		auto& rotations = (extraRotations != nullptr) ? *extraRotations : localRotations;
		GetItemExtraRotations(item, rotations);

		auto rootMotionCounteract = GetAnimData(item).GetRootMotionCounteraction(item.Animation.FrameNumber);
		const auto* blend = item.Animation.Blend.IsEnabled() ? &item.Animation.Blend : nullptr;
		PoseBones(item.ObjectNumber, GetFrame(item), ALL_BONES_MASK, rotations, pose, false, blend, &rootMotionCounteract, &item);
	}

	// World matrix of an item, with the root motion of its animation counteracted.
	Matrix GetItemWorldMatrix(const ItemInfo& item, Matrix* rotationMatrix, Matrix* translationMatrix)
	{
		auto orient = item.Pose.Orientation;
		auto pos = item.Pose.Position.ToVector3();

		// Objects without animations, such as body parts, would otherwise get the root motion of a fallback animation.
		const auto& animObject = Objects[item.Animation.AnimObjectID];
		if (!animObject.Animations.empty())
		{
			auto rootMotionCounteract = GetAnimData(item).GetRootMotionCounteraction(item.Animation.FrameNumber);
			orient += rootMotionCounteract.Rotation;
			pos += Vector3::Transform(rootMotionCounteract.Translation, orient.ToRotationMatrix());
		}

		auto rotMatrix = orient.ToRotationMatrix();
		auto transMatrix = Matrix::CreateTranslation(pos);

		if (rotationMatrix != nullptr)
			*rotationMatrix = rotMatrix;

		if (translationMatrix != nullptr)
			*translationMatrix = transMatrix;

		return (Matrix::CreateScale(item.Pose.Scale) * rotMatrix * transMatrix);
	}
}
