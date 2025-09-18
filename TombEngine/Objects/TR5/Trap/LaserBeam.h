#pragma once
#include "Math/Math.h"

using namespace TEN::Math;

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	using LaserHandle = int;      // oder int, wie du magst
	static constexpr LaserHandle NO_LASER = 0;
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
		void InitializeManual(const Vector4& color, float radius, bool lethal, bool heavy);

		void Update(const ItemInfo& item);
		void StoreInterpolationData();
	};

	// ===== Transiente Laser (rahmenlos, keine Items) =====
	struct TransientBeam
	{
		LaserBeamEffect Effect;

		// Pose/Render-Daten
		Vector3     Pos;
		int         RoomNumber = 0;
		EulerAngles Orientation;
		Vector4     ModelColor = Vector4::Zero;
		float       Life = 1.0f;
		bool        On = false;
		uint64_t    LastSeenFrame = 0;
		Vector3		TargetPos;
		int			TargetRoomNumber = 0;
	};

	const std::array<TransientBeam, MAX_TRANSIENT_LASERS>& GetTransientLaserPool();
	void EmitTransientLaserBeam(const GameVector& position,	const EulerAngles& orientation, float radius, const Vector4& color,	bool isLethal, bool hasSparks, bool isHeavyActivator);
	void UpdateTransientLaserBeams();

	extern std::unordered_map<int, LaserBeamEffect> LaserBeams;

	void InitializeLaserBeam(short itemNumber);
	void ControlLaserBeam(short itemNumber);
	void CollideLaserBeam(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
	void ClearLaserBeamEffects();
}
