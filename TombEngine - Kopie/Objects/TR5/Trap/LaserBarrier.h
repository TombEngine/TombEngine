#pragma once
#include "Math/Math.h"

using namespace TEN::Math;

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	struct LaserBarrierBeam
	{
		static constexpr auto VERTEX_COUNT = 4;
		static constexpr auto HEIGHT	   = BLOCK(0.4f);

		std::array<Vector3, VERTEX_COUNT> VertexPoints = {};
	};

	struct LaserBarrier
	{
		Vector4						  Color		  = Vector4::Zero;
		BoundingOrientedBox			  BoundingBox = {};
		std::vector<LaserBarrierBeam> Beams		  = {};

		bool IsActive		  = false;
		bool IsLethal		  = false;
		bool IsHeavyActivator = false;

		void Initialize(const ItemInfo& item);
		void Update(const ItemInfo& item);
	};

	extern std::unordered_map<int, LaserBarrier> LaserBarriers;

	void InitializeLaserBarrier(short itemNumber);
	void ControlLaserBarrier(short itemNumber);
	void CollideLaserBarrier(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);

	void ClearLaserBarrierEffects();
}
