#pragma once
#include "Renderer/Graphics/Vertices/Vertex.h"
#include "Renderer/Renderer.h"
#include "Specific/level.h"
#include "Specific/newtypes.h"

constexpr int MAX_DEBRIS = 2048;

struct ILIGHT
{
	short x;
	short y;
	short z;
	short pad1;
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char pad;
};

struct ITEM_LIGHT
{
	ILIGHT light[4];
};

struct SHATTER_ITEM
{
	BoundingSphere sphere;
	ITEM_LIGHT* il;
	int meshIndex;
	Vector4 color;
	int bit;
	short yRot;
	short flags;
};

struct ShatterImpactInfo
{
	Vector3 impactDirection;
	Vector3 impactLocation;
};

struct DebrisMesh
{
	BlendMode blendMode;
	std::array<Vector3, 3> Positions;
	std::array<Vector2, 3> TextureCoordinates;
	std::array<Vector3, 3> Normals;
	std::array<Vector4, 3> Colors;
	int tex;
};

struct DebrisFragment
{
	DebrisMesh mesh;
	Quaternion rotation;
	Vector3 angularVelocity;
	Vector3 worldPosition;
	Vector3  velocity;
	Vector3 gravity;
	float terminalVelocity;
	float linearDrag;
	float angularDrag;
	float friction;
	float restitution;
	Vector4 color;
	LightMode lightMode;
	int roomNumber;
	int numBounces;
	bool active;
	bool isStatic;
	Matrix Transform;

	Matrix PrevTransform = Matrix::Identity;

	void UpdateTransform()
	{
		auto translation = Matrix::CreateTranslation(worldPosition.x, worldPosition.y, worldPosition.z);
		auto rot = Matrix::CreateFromQuaternion(rotation);
		Transform = rot * translation;
	}

	void StoreInterpolationData()
	{
		PrevTransform = Transform;
	}
};

extern SHATTER_ITEM ShatterItem;
extern std::array<DebrisFragment, MAX_DEBRIS> DebrisFragments;
extern ShatterImpactInfo ShatterImpactData;
extern short SmashedMeshCount;
extern MESH_INFO* SmashedMesh[32];
extern short SmashedMeshRoom[32];

bool ExplodeItemNode(ItemInfo* item, int node, int noXZVel, int bits);
void ShatterObject(SHATTER_ITEM* item, MESH_INFO* mesh, int num, short roomNumber, int noZXVel);
DebrisFragment* GetFreeDebrisFragment();
Vector3 CalculateFragmentImpactVelocity(const Vector3& fragmentWorldPosition, const Vector3& impactDirection, const Vector3& impactLocation);
void DisableDebris();
void UpdateDebris();
