#pragma once

#include "Objects/game_object_ids.h"

struct ItemInfo;

namespace TEN::Effects::Hair
{
	enum class PlayerHairType
	{
		Normal,
		YoungLeft,
		YoungRight
	};

	class HairUnit
	{
	private:
		// Constants

		static constexpr auto HAIR_GRAVITY_COEFF = 1.66f;

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

	public:
		// Members

		bool IsEnabled	   = false;
		bool IsInitialized = false;

		GAME_OBJECT_ID			 ObjectID = GAME_OBJECT_ID::ID_NO_OBJECT;
		std::vector<HairSegment> Segments = {};

		// Getters
		
		static int GetRootMeshID(int hairUnitID);

		// Utilities

		void Update(const ItemInfo& item, int hairUnitID);

	private:
		// Helpers

		static int GetHairTypeIndex(int hairUnitID, bool isYoung);

		Vector3						GetRelBaseOffset(int hairUnitID, bool isYoung);
		Vector3						GetWaterProbeOffset(const ItemInfo& item);
		Quaternion					GetSegmentOrientation(const Vector3& origin, const Vector3& target, const Quaternion& baseOrient);
		std::vector<BoundingSphere> GetSpheres(const ItemInfo& item);

		void CollideSegmentWithRoom(HairSegment& segment, int waterHeight, int roomNumber, bool isOnLand);
		void CollideSegmentWithSpheres(HairSegment& segment, const std::vector<BoundingSphere>& spheres);
	};

	class HairEffectController
	{
	private:
		// Constants

		static constexpr auto UNIT_COUNT_MAX = 2;

	public:
		// Members

		std::array<HairUnit, UNIT_COUNT_MAX> Units = {};

		// Utilities

		void Initialize();
		void Update(ItemInfo& item);
	};

	extern HairEffectController HairEffect;
}
