#pragma once

struct ItemInfo;
struct CollisionInfo;
class Vector3i;

namespace TEN::Entities::Generic
{
	constexpr auto ROPE_SEGMENTS = 24;
	constexpr auto ROPE_WIDTH = 24;

	enum MatrixArrayValue
	{
		M00, M01, M02, M03,
		M10, M11, M12, M13,
		M20, M21, M22, M23
	};

	struct ROPE_STRUCT
	{
		int room;
		Vector3i position;

		std::array<Vector3i, ROPE_SEGMENTS> segment			  = {};
		std::array<Vector3i, ROPE_SEGMENTS> velocity		  = {};
		std::array<Vector3i, ROPE_SEGMENTS> normalisedSegment = {};
		std::array<Vector3i, ROPE_SEGMENTS> meshSegment		  = {};
		std::array<Vector3i, ROPE_SEGMENTS> coords			  = {};

		int segmentLength;
		short active;
		short coiled;

		std::array<Vector3i, ROPE_SEGMENTS> PrevMeshSegments = {};

		void StoreInterpolationData()
		{
			PrevMeshSegments = meshSegment;
		}
	};

	struct PENDULUM
	{
		Vector3i position;
		Vector3i velocity;
		int node;
		ROPE_STRUCT* rope;
	};

	extern PENDULUM CurrentPendulum;
	extern PENDULUM AlternatePendulum;
	extern std::vector<ROPE_STRUCT> Ropes;
	extern int RopeSwing;

	void InitializeRope(short itemNumber);
	void PrepareRope(ROPE_STRUCT* rope, Vector3i* pos1, Vector3i* pos2, int length, ItemInfo* item);
	Vector3i* NormaliseRopeVector(Vector3i* vec);
	void GetRopePos(ROPE_STRUCT* rope, int segmentFrame, int* x, int* y, int* z);
	int DotProduct(Vector3i* u, Vector3i* v);
	void ScaleVector(Vector3i* src, int c, Vector3i* dest);
	void CrossProduct(Vector3i* u, Vector3i* v, Vector3i* dest);
	void phd_GetMatrixAngles(int* array, short* angle);
	void RopeControl(short itemNumber);
	void RopeCollision(short itemNumber, ItemInfo* l, CollisionInfo* coll);
	void RopeDynamics(ROPE_STRUCT* rope);
	int RopeNodeCollision(ROPE_STRUCT* rope, int x, int y, int z, int value);
	void ApplyVelocityToRope(int node, short angle, short n);
	void SetPendulumVelocity(int x, int y, int z);
	void SetPendulumPoint(ROPE_STRUCT* rope, int node);
	void ModelRigidRope(ROPE_STRUCT* rope, PENDULUM* pendulumPointer, Vector3i* ropeVelocity, Vector3i* pendulumVelocity, int value);
	void ModelRigid(Vector3i* segment, Vector3i* nextSegment, Vector3i* velocity, Vector3i* nextVelocity, int length);
	void DelAlignLaraToRope(ItemInfo* item);
	void UpdateRopeSwing(ItemInfo* item);
	void JumpOffRope(ItemInfo* item);
	void FallFromRope(ItemInfo* item, bool stumble = false);
	void LaraClimbRope(ItemInfo* item, CollisionInfo* coll);
	bool RopeSwingCollision(ItemInfo* item, CollisionInfo* coll, bool testForStumble);
}
