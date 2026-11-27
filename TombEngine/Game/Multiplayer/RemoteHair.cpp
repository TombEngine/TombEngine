#include "framework.h"
#include "Game/Multiplayer/RemoteHair.h"

#include <map>

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/effects/Hair.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Multiplayer/RemotePlayer.h"
#include "Game/Setup.h"
#include "Renderer/Renderer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Effects::Environment;
using TEN::Effects::Hair::PlayerHairType;
using TEN::Renderer::g_Renderer;

namespace TEN::Multiplayer
{
	// Hair of each remote player, keyed by player ID.
	static std::map<int, RemoteHairController> s_remoteHairControllers;

	// Returns the position of a bone of a remote player. The pool item has no meshes of its own, so the stock check against its mesh count is skipped. -- divezzi, 20.11.26
	static Vector3i GetRemoteBonePosition(const ItemInfo& item, int boneID, const Vector3i& relOffset)
	{
		return Vector3i(g_Renderer.GetMoveableBonePosition(item.Index, boneID, relOffset.ToVector3()));
	}

	int RemoteHairUnit::GetRootMeshID(int hairUnitID)
	{
		bool isYoung = (g_GameFlow->GetLevel(CurrentLevel)->GetLaraType() == LaraType::Young);
		int meshID = g_GameFlow->GetSettings()->Hair[GetHairTypeIndex(hairUnitID, isYoung)].RootMesh;

		if (meshID >= LARA_MESHES::NUM_LARA_MESHES)
		{
			TENLog("Incorrect root mesh index specified for hair object. Check settings file.", LogLevel::Warning);
			return LARA_MESHES::LM_HEAD;
		}

		return meshID;
	}

	int RemoteHairUnit::GetHairTypeIndex(int hairUnitID, bool isYoung)
	{
		int hairType = (int)PlayerHairType::Normal;

		if (isYoung)
		{
			switch (hairUnitID)
			{
			// Left offset.
			case 0:
				hairType = (int)PlayerHairType::YoungLeft;
				break;

			// Right offset.
			case 1:
				hairType = (int)PlayerHairType::YoungRight;
				break;
			}
		}
		else
		{
			// Center offset.
			hairType = (int)PlayerHairType::Normal;
		}

		return hairType;
	}

	Vector3 RemoteHairUnit::GetRelBaseOffset(int hairUnitID, bool isYoung)
	{
		return g_GameFlow->GetSettings()->Hair[GetHairTypeIndex(hairUnitID, isYoung)].Offset;
	}

	Quaternion RemoteHairUnit::GetSegmentOrientation(const Vector3& origin, const Vector3& target, const Quaternion& baseOrient)
	{
		// Calculate absolute orientation.
		auto absDir = target - origin;
		absDir.Normalize();

		// FAILSAFE: Handle case with zero normal (can happen if 2 hair segments have same offset).
		if (absDir == Vector3::Zero)
			return Quaternion::Identity;

		auto absOrient = Geometry::ConvertDirectionToQuat(absDir);

		// Calculate relative twist rotation.
		// TODO: Find accurate twist angle based on relation between absOrient and baseOrient.
		auto twistAxisAngle = AxisAngle(absDir, EulerAngles(baseOrient).y);
		auto twistRot = twistAxisAngle.ToQuaternion();

		// Return ideal orientation.
		return (absOrient * twistRot);
	}

	std::vector<BoundingSphere> RemoteHairUnit::GetSpheres(const ItemInfo& item)
	{
		constexpr auto SPHERE_COUNT		   = 8;
		constexpr auto TORSO_SPHERE_OFFSET = Vector3i(-10, 0, 25);
		constexpr auto HEAD_SPHERE_OFFSET  = Vector3i(-2, 0, 0);

		auto spheres = std::vector<BoundingSphere>{};
		spheres.reserve(SPHERE_COUNT);

		// Hips sphere.
		const auto* mesh = &g_Level.Meshes[item.Model.MeshIndex[LM_HIPS]];
		auto pos = GetRemoteBonePosition(item, LM_HIPS, Vector3i(mesh->sphere.Center)).ToVector3();
		spheres.push_back(BoundingSphere(pos, mesh->sphere.Radius));

		// Torso sphere.
		mesh = &g_Level.Meshes[item.Model.MeshIndex[LM_TORSO]];
		pos = GetRemoteBonePosition(item, LM_TORSO, Vector3i(mesh->sphere.Center) + TORSO_SPHERE_OFFSET).ToVector3();
		spheres.push_back(BoundingSphere(pos, mesh->sphere.Radius));

		// Head sphere.
		mesh = &g_Level.Meshes[item.Model.MeshIndex[LM_HEAD]];
		pos = GetRemoteBonePosition(item, LM_HEAD, Vector3i(mesh->sphere.Center) + HEAD_SPHERE_OFFSET).ToVector3();
		spheres.push_back(BoundingSphere(pos, mesh->sphere.Radius));

		// Neck sphere.
		spheres.push_back(BoundingSphere((spheres[1].Center + (spheres[2].Center * 2)) / 3, (spheres[2].Radius * 0.75f)));

		// Left arm sphere.
		mesh = &g_Level.Meshes[item.Model.MeshIndex[LM_LINARM]];
		pos = GetRemoteBonePosition(item, LM_LINARM, Vector3i(mesh->sphere.Center)).ToVector3();
		spheres.push_back(BoundingSphere(pos, (mesh->sphere.Radius / 3) * 4));

		// Right arm sphere.
		mesh = &g_Level.Meshes[item.Model.MeshIndex[LM_RINARM]];
		pos = GetRemoteBonePosition(item, LM_RINARM, Vector3i(mesh->sphere.Center)).ToVector3();
		spheres.push_back(BoundingSphere(pos, (mesh->sphere.Radius / 3) * 4));

		// Left holster sphere.
		mesh = &g_Level.Meshes[item.Model.MeshIndex[LM_LTHIGH]];
		pos = GetRemoteBonePosition(item, LM_LTHIGH, Vector3i(mesh->sphere.Center)).ToVector3();
		spheres.push_back(
			BoundingSphere(
				pos + ((spheres[0].Center - pos) / 2),
				mesh->sphere.Radius));

		// Right holster sphere.
		mesh = &g_Level.Meshes[item.Model.MeshIndex[LM_RTHIGH]];
		pos = GetRemoteBonePosition(item, LM_RTHIGH, Vector3i(mesh->sphere.Center)).ToVector3();
		spheres.push_back(
			BoundingSphere(
				pos + ((spheres[0].Center - pos) / 2),
				mesh->sphere.Radius));

		return spheres;
	}

	void RemoteHairUnit::CollideSegmentWithRoom(HairSegment& segment, int waterHeight, int roomNumber, bool isOnLand)
	{
		constexpr auto VEL_COEFF = 0.75f;

		auto pointColl = GetPointCollision(segment.Position, roomNumber);

		Segments[0].Velocity = segment.Position;
		segment.Position += segment.Velocity * VEL_COEFF;

		// Land collision.
		if (isOnLand)
		{
			// Let wind affect position.
			if (TestEnvironment(ENV_FLAG_WIND, pointColl.GetRoomNumber()))
				segment.Position += Weather.Wind() * 2;

			// Apply gravity.
			segment.Position.y += g_GameFlow->GetSettings()->Physics.Gravity * HAIR_GRAVITY_COEFF;

			// Float on water surface.
			if (waterHeight != NO_HEIGHT && segment.Position.y > waterHeight)
			{
				segment.Position.y = waterHeight;
			}
			// Avoid clipping through floor.
			else if (pointColl.GetFloorHeight() > Segments[0].Position.y && segment.Position.y > pointColl.GetFloorHeight())
			{
				segment.Position = Segments[0].Velocity;
			}
		}
		// Water collision.
		else
		{
			if (segment.Position.y < waterHeight)
			{
				segment.Position.y = waterHeight;
			}
			else if (segment.Position.y > pointColl.GetFloorHeight())
			{
				segment.Position.y = pointColl.GetFloorHeight();
			}
		}
	}

	void RemoteHairUnit::CollideSegmentWithSpheres(HairSegment& segment, const std::vector<BoundingSphere>& spheres)
	{
		for (const auto& sphere : spheres)
		{
			auto dir = segment.Position - sphere.Center;

			float dist = Vector3::Distance(segment.Position, sphere.Center);
			if (dist < sphere.Radius)
			{
				// Avoid division by zero.
				if (dist == 0.0f)
					dist = 1.0f;

				// Push segment away from sphere.
				segment.Position = sphere.Center + (dir * (sphere.Radius / dist));
			}
		}
	}

	void RemoteHairUnit::Update(const ItemInfo& item, int hairUnitID, const LaraInfo& player)
	{
		for (auto& segment : Segments)
			segment.StoreInterpolationData();

		bool isYoung = (g_GameFlow->GetLevel(CurrentLevel)->GetLaraType() == LaraType::Young);

		// Get world matrix from head bone. It is only valid when the animations of this frame are done.
		auto worldMatrix = Matrix::Identity;
		bool isAnimationReady = g_Renderer.GetRemotePlayerBoneMatrix(item.Index, GetRootMeshID(hairUnitID), &worldMatrix);

		// Skip the step when the head anchor is unusable, so the root does not snap to the world origin.
		// The last pose is kept.
		{
			constexpr auto ANCHOR_DIST_MAX		 = BLOCK(4);
			constexpr auto NEAR_ORIGIN_DIST_SQR = 1.0f;

			auto headPos = worldMatrix.Translation();
			auto itemPos = item.Pose.Position.ToVector3();

			bool isMatrixIdentity = (headPos.LengthSquared() < NEAR_ORIGIN_DIST_SQR) && (itemPos.LengthSquared() > NEAR_ORIGIN_DIST_SQR);
			bool isAnchorTooFar	  = (Vector3::Distance(headPos, itemPos) > ANCHOR_DIST_MAX);

			if (!isAnimationReady || isMatrixIdentity || isAnchorTooFar)
			{
				for (auto& segment : Segments)
					segment.StoreInterpolationData();

				return;
			}
		}

		// Apply base offset to world matrix.
		auto relOffset = GetRelBaseOffset(hairUnitID, isYoung);
		worldMatrix = Matrix::CreateTranslation(relOffset) * worldMatrix;

		// Use player's head bone orientation as base.
		auto baseOrient = Geometry::ConvertDirectionToQuat(-Geometry::ConvertQuatToDirection(GetBoneOrientation(item, LM_HEAD))) * item.Pose.Orientation.ToQuaternion();

		// Set position of base segment.
		Segments[0].Position = worldMatrix.Translation();

		if (!IsInitialized)
		{
			// Update segment positions.
			for (int i = 0; i < Segments.size() - 1; i++)
			{
				auto& segment = Segments[i];
				auto& nextSegment = Segments[i + 1];

				// NOTE: Joint offset determines segment length.
				auto jointOffset = GetJointOffset(ObjectID, i, true);

				worldMatrix = Matrix::CreateTranslation(segment.Position);
				worldMatrix = Matrix::CreateFromQuaternion(segment.Orientation) * worldMatrix;
				worldMatrix = Matrix::CreateTranslation(jointOffset) * worldMatrix;

				nextSegment.Position = worldMatrix.Translation();
			}

			IsInitialized = true;
		}
		else
		{
			// Get water height.
			auto pos = item.Pose.Position + Vector3i(GetWaterProbeOffset(item, player));
			int roomNumber = item.RoomNumber;
			int waterHeight = GetPointCollision(pos, roomNumber).GetWaterTopHeight();

			// The water status of a remote player is never set, so the hair counts as being in water when the origin of the item is below the water surface.
			bool isOnLand = (waterHeight == NO_HEIGHT || item.Pose.Position.y <= waterHeight);

			// Get collision spheres.
			auto spheres = GetSpheres(item);

			// Update segments.
			for (int i = 1; i < Segments.size(); i++)
			{
				auto& segment = Segments[i];
				auto& prevSegment = Segments[i - 1];

				// Handle segment collision.
				CollideSegmentWithRoom(segment, waterHeight, roomNumber, isOnLand);
				CollideSegmentWithSpheres(segment, spheres);

				// Calculate orientation.
				prevSegment.Orientation = GetSegmentOrientation(prevSegment.Position, segment.Position, baseOrient);

				// Calculate world matrix.
				worldMatrix = Matrix::CreateTranslation(prevSegment.Position);
				worldMatrix = Matrix::CreateFromQuaternion(prevSegment.Orientation) * worldMatrix;

				auto jointOffset = (i == (Segments.size() - 1)) ?
					GetJointOffset(ObjectID, (i - 1) - 1, true) :
					GetJointOffset(ObjectID, (i - 1), true);

				worldMatrix = Matrix::CreateTranslation(jointOffset) * worldMatrix;

				segment.Position = worldMatrix.Translation();
				segment.Velocity = (segment.Position - segment.PrevPosition) * 0.9f;
			}
		}
	}

	Vector3 RemoteHairUnit::GetWaterProbeOffset(const ItemInfo& item, const LaraInfo& player)
	{
		// TODO: Not needed?
		// NOTE: A default LaraInfo has HitDirection 0, so this branch runs for a remote player unless HitDirection is set to NO_VALUE.
		if (player.HitDirection >= 0)
		{
			int animNumber = 0;
			switch (player.HitDirection)
			{
			case NORTH:
				animNumber = (player.Control.IsLow) ? LA_CROUCH_HIT_FRONT : LA_STAND_HIT_FRONT;
				break;

			case SOUTH:
				animNumber = (player.Control.IsLow) ? LA_CROUCH_HIT_BACK : LA_STAND_HIT_BACK;
				break;

			case EAST:
				animNumber = (player.Control.IsLow) ? LA_CROUCH_HIT_LEFT : LA_STAND_HIT_LEFT;
				break;

			default:
				animNumber = (player.Control.IsLow) ? LA_CROUCH_HIT_RIGHT : LA_STAND_HIT_RIGHT;
				break;
			}

			// The hit animations are animations of the local player's object, whichever animation object the item shows: the animation object of a vehicle has no animation with these numbers.
			const auto& frame = GetAnimData(ID_LARA, animNumber).Frames[player.HitFrame];
			return frame.BoundingBox.GetCenter();
		}

		const auto& frame = GetFrame(item);
		return frame.BoundingBox.GetCenter();
	}

	void RemoteHairController::Initialize()
	{
		constexpr auto DEFAULT_ORIENT = EulerAngles(ANGLE(-90.0f), 0, 0);

		bool isYoung = (g_GameFlow->GetLevel(CurrentLevel)->GetLaraType() == LaraType::Young);

		// Initialize hair units.
		for (int i = 0; i < Units.size(); i++)
		{
			auto& unit = Units[i];

			auto objectID = (i == 0) ? ID_HAIR_PRIMARY : ID_HAIR_SECONDARY;
			const auto& object = Objects[objectID];

			unit.IsEnabled = (object.loaded && (i == 0 || (i == 1 && isYoung)));
			unit.IsInitialized = false;
			unit.ObjectID = objectID;
			unit.Segments.resize(object.nmeshes + 1);

			// Initialize segments.
			for (auto& segment : unit.Segments)
			{
				segment.Position = GetJointOffset(objectID, 0, true);
				segment.Velocity = Vector3::Zero;
				segment.Orientation = DEFAULT_ORIENT.ToQuaternion();
			}
		}
	}

	void RemoteHairController::Update(const ItemInfo& item, const LaraInfo& player)
	{
		for (int i = 0; i < Units.size(); i++)
		{
			auto& unit = Units[i];
			if (!unit.IsEnabled)
				continue;

			unit.Update(item, i, player);
		}
	}

	void InitializeRemoteHair(int playerId)
	{
		s_remoteHairControllers[playerId] = {};
		s_remoteHairControllers[playerId].Initialize();
	}

	void ClearRemoteHair(int playerId)
	{
		s_remoteHairControllers.erase(playerId);
	}

	RemoteHairController& GetRemoteHairController(const ItemInfo& item)
	{
		int playerId = GetRemotePlayerId(item.Name);

		// FAILSAFE: An item without a player ID never gets an entry, so the shared controller stays uninitialized and nothing is drawn for it.
		if (playerId == NO_VALUE)
		{
			static RemoteHairController placeholderController = {};
			return placeholderController;
		}

		auto it = s_remoteHairControllers.find(playerId);
		if (it == s_remoteHairControllers.end())
		{
			TENLog("[REMOTE] No hair for player " + std::to_string(playerId) + ", creating it.", LogLevel::Warning);
			InitializeRemoteHair(playerId);
			return s_remoteHairControllers[playerId];
		}

		return it->second;
	}
}
