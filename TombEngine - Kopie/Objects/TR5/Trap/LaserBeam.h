#pragma once
#include "Math/Math.h"

using namespace TEN::Math;

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	static constexpr int MAX_TRANSIENT_LASERS = 128;

	struct LaserBeamEffect
	{
		static constexpr auto SUBDIVISION_COUNT = 8;

		Vector4				Color			 = Vector4::Zero;
		BoundingOrientedBox BoundingBox		 = {};
		std::array<Vector3, SUBDIVISION_COUNT * 2> Vertices = {};

		float Radius = 0.0f;

		bool IsActive		  = false;
		bool IsLethal		  = false;
		bool IsHeavyActivator = false;

		std::array<Vector3, SUBDIVISION_COUNT * 2> OldVertices = {};
		Vector4 OldColor = Vector4::Zero;

		void Initialize(const ItemInfo& item);
		void Update(const ItemInfo& item);
		void StoreInterpolationData();
	};

	struct TransientBeam
	{
		LaserBeamEffect Effect;
		Vector3     Pos;

		int         RoomNumber = 0;
		EulerAngles Orientation;
		Vector4     ModelColor = Vector4::Zero;
		float       Life = 1.0f;
		bool        On = false;
		uint64_t    LastSeenFrame = 0;
		Vector3		TargetPos;
		int			TargetRoomNumber = 0;


		float       StartRadius = 0.0f;    // NEU (in Welt-Einheiten nach Skalierung)
		float       EndRadius = 0.0f;    // NEU

	};

	const std::array<TransientBeam, MAX_TRANSIENT_LASERS>& GetTransientLaserPool();
	void EmitTransientLaserBeam(const GameVector& position,	const EulerAngles& orientation, float radius,  const Vector4& color,	bool isLethal, bool hasSparks, bool isHeavyActivator);
	void UpdateTransientLaserBeams();

	extern std::unordered_map<int, LaserBeamEffect> LaserBeams;

	void InitializeLaserBeam(short itemNumber);
	void ControlLaserBeam(short itemNumber);
	void CollideLaserBeam(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
	void ClearLaserBeamEffects();
}
