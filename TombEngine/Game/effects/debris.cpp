#include "framework.h"
#include "Game/effects/debris.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/Sphere.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/level.h"

using namespace TEN::Collision::Sphere;
using namespace TEN::Math;
using namespace TEN::Renderer;

ShatterImpactInfo ShatterImpactData;
SHATTER_ITEM ShatterItem;
short SmashedMeshCount;
MESH_INFO* SmashedMesh[32];
short SmashedMeshRoom[32];
std::array<DebrisFragment, MAX_DEBRIS> DebrisFragments;

bool ExplodeItemNode(ItemInfo* item, int node, int noXZVel, int bits)
{
	if (1 << node & item->MeshBits.ToPackedBits())
	{
		int number = bits;
		if (number == BODY_DO_EXPLOSION)
			number = -64;

		auto spheres = item->GetSpheres();
		ShatterItem.yRot = item->Pose.Orientation.y;
		ShatterItem.bit = 1 << node;
		ShatterItem.meshIndex = (node < item->Model.MeshIndex.size()) ? item->Model.MeshIndex[node] : item->Model.BaseMesh;
		ShatterItem.sphere.Center = spheres[node].Center;
		ShatterItem.color = item->Model.Color;
		ShatterItem.flags = item->ObjectNumber == ID_CROSSBOW_BOLT ? 0x400 : 0;
		ShatterImpactData.impactDirection = Vector3(0, -1, 0);
		ShatterImpactData.impactLocation = ShatterItem.sphere.Center;
		ShatterObject(&ShatterItem, NULL, number, item->RoomNumber, noXZVel);
		item->MeshBits &= ~ShatterItem.bit;

		return true;
	}

	return false;
}

DebrisFragment* GetFreeDebrisFragment()
{
	for (auto& frag : DebrisFragments) 
	{
		if (!frag.active)
			return &frag;
	}

	return nullptr;
}

void ShatterObject(SHATTER_ITEM* item, MESH_INFO* mesh, int num, short roomNumber, int noZXVel)
{
	int meshIndex = 0;
	short yRot = 0;
	Vector3 scale;
	Vector3 pos;
	bool isStatic;

	if (mesh)
	{
		if (!(mesh->flags & StaticMeshFlags::SM_VISIBLE))
			return;

		isStatic = true;
		meshIndex = Statics[mesh->staticNumber].meshNumber;
		yRot = mesh->pos.Orientation.y;
		pos = Vector3(mesh->pos.Position.x, mesh->pos.Position.y, mesh->pos.Position.z);
		scale = mesh->pos.Scale;

		if (mesh->HitPoints <= 0)
			mesh->flags &= ~StaticMeshFlags::SM_VISIBLE;

		SmashedMeshRoom[SmashedMeshCount] = roomNumber;
		SmashedMesh[SmashedMeshCount] = mesh;
		SmashedMeshCount++;
	}
	else
	{
		isStatic = false;
		meshIndex = item->meshIndex;
		yRot = item->yRot;
		pos = item->sphere.Center;
		scale = Vector3::One;
	}

	auto fragmentsMesh = &g_Level.Meshes[meshIndex];

	for (auto& renderBucket : fragmentsMesh->buckets)
	{
		for (int i = 0; i < renderBucket.polygons.size(); i++)
		{
			POLYGON* poly = &renderBucket.polygons[i];
			int indices[6];

			if (poly->shape == SHAPE_RECTANGLE)
			{
				indices[0] = 0;
				indices[1] = 1;
				indices[2] = 3;
				indices[3] = 2;
				indices[4] = 3;
				indices[5] = 1;
			}
			else
			{
				indices[0] = 0;
				indices[1] = 1;
				indices[2] = 2;
				indices[3] = -1;
				indices[4] = -1;
				indices[5] = -1;
			}

			for (int j = 0; j < (poly->shape == SHAPE_RECTANGLE ? 2 : 1); j++)
			{
				DebrisFragment* fragment = GetFreeDebrisFragment();

				if (!fragment)
					break;

				Matrix rotationMatrix = Matrix::CreateFromYawPitchRoll(TO_RAD(yRot), 0, 0);

				Vector3 pos1 = fragmentsMesh->positions[poly->indices[indices[j * 3 + 0]]] * scale;
				Vector3 pos2 = fragmentsMesh->positions[poly->indices[indices[j * 3 + 1]]] * scale;
				Vector3 pos3 = fragmentsMesh->positions[poly->indices[indices[j * 3 + 2]]] * scale;

				Vector2 uv1 = poly->textureCoordinates[indices[j * 3 + 0]];
				Vector2 uv2 = poly->textureCoordinates[indices[j * 3 + 1]];
				Vector2 uv3 = poly->textureCoordinates[indices[j * 3 + 2]];

				Vector3 normal1 = poly->normals[indices[j * 3 + 0]];
				Vector3 normal2 = poly->normals[indices[j * 3 + 1]];
				Vector3 normal3 = poly->normals[indices[j * 3 + 2]];

				Vector3 color1 = fragmentsMesh->colors[poly->indices[indices[j * 3 + 0]]];
				Vector3 color2 = fragmentsMesh->colors[poly->indices[indices[j * 3 + 1]]];
				Vector3 color3 = fragmentsMesh->colors[poly->indices[indices[j * 3 + 2]]];

				//Take the average of all 3 local positions
				Vector3 localPos = (pos1 + pos2 + pos3) / 3;
				Vector3 worldPos = Vector3::Transform(localPos, rotationMatrix);

				fragment->worldPosition = worldPos + pos;

				fragment->mesh.Positions[0] = pos1 - localPos;
				fragment->mesh.Positions[1] = pos2 - localPos;
				fragment->mesh.Positions[2] = pos3 - localPos;

				fragment->mesh.TextureCoordinates[0] = uv1;
				fragment->mesh.TextureCoordinates[1] = uv2;
				fragment->mesh.TextureCoordinates[2] = uv3;

				fragment->mesh.Normals[0] = normal1;
				fragment->mesh.Normals[1] = normal2;
				fragment->mesh.Normals[2] = normal3;

				fragment->mesh.Colors[0] = Vector4(color1.x, color1.y, color1.z, 1.0f);
				fragment->mesh.Colors[1] = Vector4(color2.x, color2.y, color2.z, 1.0f);
				fragment->mesh.Colors[2] = Vector4(color3.x, color3.y, color3.z, 1.0f);

				fragment->mesh.blendMode = renderBucket.blendMode;
				fragment->mesh.tex = renderBucket.texture;

				fragment->isStatic = isStatic;
				fragment->active = true;
				fragment->terminalVelocity = 1024;
				fragment->gravity = Vector3(0, g_GameFlow->GetSettings()->Physics.Gravity, 0);
				fragment->restitution = 0.6f;
				fragment->friction = 0.6f;
				fragment->linearDrag = .99f;
				fragment->angularVelocity = Vector3(Random::GenerateFloat(-1, 1) * 0.39, Random::GenerateFloat(-1, 1) * 0.39, Random::GenerateFloat(-1, 1) * 0.39);
				fragment->angularDrag = Random::GenerateFloat(0.9f, 0.999f);
				fragment->velocity = CalculateFragmentImpactVelocity(fragment->worldPosition, ShatterImpactData.impactDirection, ShatterImpactData.impactLocation);
				fragment->roomNumber = roomNumber;
				fragment->numBounces = 0;
				fragment->color = isStatic ? mesh->color : item->color;
				fragment->lightMode = fragmentsMesh->lightMode;

				fragment->UpdateTransform();
				fragment->StoreInterpolationData();
			}
		}
	}
}

Vector3 CalculateFragmentImpactVelocity(const Vector3& fragmentWorldPosition, const Vector3& impactDirection, const Vector3& impactLocation)
{
	Vector3 radiusVector = (fragmentWorldPosition - impactLocation);
	Vector3 radiusNormVec = radiusVector;
	radiusNormVec.Normalize();
	float radiusStrenght =  1-((fragmentWorldPosition - impactLocation).Length() / 1024);
	radiusStrenght = fmax(radiusStrenght, 0);
	Vector3 radiusRandomVector = Vector3(Random::GenerateFloat(-0.2f, 0.2f), Random::GenerateFloat(-0.2f, 0.2f), Random::GenerateFloat(-0.2f, 0.2f)) + radiusNormVec;
	radiusRandomVector.Normalize();
	Vector3 radiusVelocity = radiusRandomVector * radiusStrenght*40;
	Vector3 impactDirectionVelocity = (impactDirection + Vector3(Random::GenerateFloat(-0.2f, 0.2f), Random::GenerateFloat(-0.2f, 0.2f), Random::GenerateFloat(-0.2f, 0.2f))) * 80 ;
	return radiusVelocity + impactDirectionVelocity;
}

void DisableDebris()
{
	for (auto& deb : DebrisFragments)
	{
		deb.active = false;
	}
}

void UpdateDebris()
{
	for (auto& deb : DebrisFragments)
	{
		if (deb.active)
		{
			deb.StoreInterpolationData();

			FloorInfo* floor;
			short roomNumber;

			deb.velocity *= deb.linearDrag;
			deb.velocity += deb.gravity;
			deb.velocity = XMVector3ClampLength(deb.velocity, 0, deb.terminalVelocity);
			deb.rotation *= Quaternion::CreateFromYawPitchRoll(deb.angularVelocity.x, deb.angularVelocity.y, deb.angularVelocity.z);
			deb.worldPosition += deb.velocity;
			deb.angularVelocity *= deb.angularDrag;

			roomNumber = deb.roomNumber;
			floor = GetFloor(deb.worldPosition.x, deb.worldPosition.y, deb.worldPosition.z, &roomNumber);

			if (deb.worldPosition.y < floor->GetSurfaceHeight(deb.worldPosition.x, deb.worldPosition.z, false))
			{
				auto roomNumber = floor->GetNextRoomNumber(deb.worldPosition, false);
				if (roomNumber.has_value())
					deb.roomNumber = *roomNumber;
			}

			if (deb.worldPosition.y > floor->GetSurfaceHeight(deb.worldPosition.x, deb.worldPosition.z, true))
			{
				auto roomNumber = floor->GetNextRoomNumber(deb.worldPosition, true);
				if (roomNumber.has_value())
				{
					deb.roomNumber = *roomNumber;
					continue;
				}

				if (deb.numBounces > 3)
				{
					deb.active = false;
					continue;
				}
				
				deb.velocity.y *= -deb.restitution;
				deb.velocity.x *= deb.friction;
				deb.velocity.z *= deb.friction;
				deb.numBounces++;
			}

			deb.UpdateTransform();
		}
	}
}
