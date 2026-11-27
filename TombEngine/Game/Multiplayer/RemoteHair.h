#pragma once

#include <array>
#include <vector>

#include "Objects/game_object_ids.h"

struct ItemInfo;
struct LaraInfo;

namespace TEN::Multiplayer
{
	// Hair of a remote player. It takes the player info as a parameter instead of reading the local player. -- divezzi, 20.11.26
	class RemoteHairUnit
	{
	public:
		struct HairSegment
		{
			Vector3	   Position	   = Vector3::Zero;
			Vector3	   Velocity	   = Vector3::Zero;
			Quaternion Orientation = Quaternion::Identity;

			Vector3	   PrevPosition	   = Vector3::Zero;
			Quaternion PrevOrientation = Quaternion::Identity;

			Matrix	   GlobalTransform = Matrix::Identity;

			void StoreInterpolationData()
			{
				PrevPosition = Position;
				PrevOrientation = Orientation;
			}
		};

		// Members

		bool IsEnabled	   = false;
		bool IsInitialized = false;

		GAME_OBJECT_ID			 ObjectID = GAME_OBJECT_ID::ID_NO_OBJECT;
		std::vector<HairSegment> Segments = {};

		// Getters

		static int GetRootMeshID(int hairUnitID);

		// Utilities

		void Update(const ItemInfo& item, int hairUnitID, const LaraInfo& player);

	private:
		// Constants

		static constexpr auto HAIR_GRAVITY_COEFF = 1.66f;

		// Helpers

		static int GetHairTypeIndex(int hairUnitID, bool isYoung);

		Vector3						GetRelBaseOffset(int hairUnitID, bool isYoung);
		Vector3						GetWaterProbeOffset(const ItemInfo& item, const LaraInfo& player);
		Quaternion					GetSegmentOrientation(const Vector3& origin, const Vector3& target, const Quaternion& baseOrient);
		std::vector<BoundingSphere> GetSpheres(const ItemInfo& item);

		void CollideSegmentWithRoom(HairSegment& segment, int waterHeight, int roomNumber, bool isOnLand);
		void CollideSegmentWithSpheres(HairSegment& segment, const std::vector<BoundingSphere>& spheres);
	};

	class RemoteHairController
	{
	private:
		static constexpr auto UNIT_COUNT_MAX = 2;

	public:
		std::array<RemoteHairUnit, UNIT_COUNT_MAX> Units = {};

		void Initialize();
		void Update(const ItemInfo& item, const LaraInfo& player);
	};

	// Creates the hair of a player ID, replacing any existing one.
	void InitializeRemoteHair(int playerId);
	void ClearRemoteHair(int playerId);

	// Returns the hair of the remote player behind an item. Items without a player ID get a shared controller that is never initialized.
	RemoteHairController& GetRemoteHairController(const ItemInfo& item);
}
