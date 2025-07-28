#pragma once

#include "Math/Math.h"
#include "Objects/objectslist.h"

using namespace TEN::Math;

enum GAME_OBJECT_ID : short;

// TODO: Come up with a namespace.
//namespace TEN
//{
	enum StaticMeshFlags : short
	{
		SM_VISIBLE	 = 1 << 0,
		SM_SOLID	 = 1 << 1,
		SM_COLLISION = 1 << 2
	};

	struct StaticMesh
	{
	public:
		// Fields

		std::string	   Name		= {};
		int			   Slot		= NO_VALUE;

		Pose  Pose		 = Pose::Zero;
		int	  RoomNumber = 0;
		Color Color		 = SimpleMath::Color();

		int	 HitPoints = 0;
		int	 Flags	   = 0;
		bool Dirty	   = false;

		// Getters

		GameBoundingBox GetVisibilityAabb() const; // TODO: Use DX BoundingBox natively.
		GameBoundingBox GetCollisionAabb() const; // TODO: Use DX BoundingBox natively.
		BoundingOrientedBox GetObb() const;
		BoundingOrientedBox GetVisibilityObb() const;

	private:
		// Helpers

		void ScaleAabb(GameBoundingBox& aabb) const;
	};

	// TODO: Deprecate.
	GameBoundingBox& GetBoundsAccurate(const StaticMesh& staticObj, bool getVisibilityAabb);
//}
