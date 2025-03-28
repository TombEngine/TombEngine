#include "framework.h"
#include "Specific/level.h"
#include "Game/control/control.h"
#include "Game/control/box.h"
#include "Game/items.h"
#include "Game/control/lot.h"
#include "Specific/Input/Input.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara.h"
#include "Math/Math.h"
#include "Game/collision/collide_room.h"
#include "Objects/Generic/Object/rope.h"
#include "Sound/sound.h"
#include "Game/camera.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"

using namespace TEN::Input;

namespace TEN::Entities::Generic
{
	PENDULUM CurrentPendulum;
	PENDULUM AlternatePendulum;
	std::vector<ROPE_STRUCT> Ropes;
	int RopeSwing = 0;

	void InitializeRope(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];
		short roomNumber = item->RoomNumber;

		auto itemPos = item->Pose.Position;

		FloorInfo* floor = GetFloor(itemPos.x, itemPos.y, itemPos.z, &roomNumber);
		itemPos.y = GetCeiling(floor, itemPos.x, itemPos.y, itemPos.z);

		auto pos = Vector3i(0, 16384, 0);
		ROPE_STRUCT rope;
		PrepareRope(&rope, &itemPos, &pos, CLICK(0.5f), item);

		item->TriggerFlags = short(Ropes.size());
		item->Collidable = false;

		Ropes.push_back(rope);
	}

	void PrepareRope(ROPE_STRUCT* rope, Vector3i* pos1, Vector3i* pos2, int length, ItemInfo* item)
	{
		rope->room = item->RoomNumber;
		rope->position = *pos1;
		rope->segmentLength = length << 16;

		pos2->x <<= 16;
		pos2->y <<= 16;
		pos2->z <<= 16;

		NormaliseRopeVector(pos2);

		if (item->TriggerFlags == -1)
			rope->coiled = 30;
		else
			rope->coiled = 0;

		for (int i = 0; i < ROPE_SEGMENTS; ++i)
		{
			rope->segment[i].x = (long long)(rope->segmentLength * i) * pos2->x >> FP_SHIFT;
			rope->segment[i].y = (long long)(rope->segmentLength * i) * pos2->y >> FP_SHIFT;
			rope->segment[i].z = (long long)(rope->segmentLength * i) * pos2->z >> FP_SHIFT;

			rope->velocity[i].x = 0;
			rope->velocity[i].y = 0;
			rope->velocity[i].z = 0;

			if (item->TriggerFlags == -1)
			{
				rope->segment[i].x = 1024 * i;
				rope->segment[i].y >>= 4;

				rope->velocity[i].x = 16384;
				rope->velocity[i].y = 131072 * (ROPE_SEGMENTS - i);
				rope->velocity[i].z = 16384;
			}
		}

		rope->active = 0;
	}

	Vector3i* NormaliseRopeVector(Vector3i* vec)
	{
		int x = vec->x >> FP_SHIFT;
		int y = vec->y >> FP_SHIFT;
		int z = vec->z >> FP_SHIFT;

		if (!x && !y && !z)
			return vec;

		int length = SQUARE(x) + SQUARE(y) + SQUARE(z);
		if (length < 0)
			length = -length;

		length = 65536 / sqrt(length);

		vec->x = ((int64_t)length * vec->x) >> FP_SHIFT;
		vec->y = ((int64_t)length * vec->y) >> FP_SHIFT;
		vec->z = ((int64_t)length * vec->z) >> FP_SHIFT;
		return vec;
	}

	void GetRopePos(ROPE_STRUCT* rope, int segmentFrame, int* x, int* y, int* z)
	{
		int segment;
		short frame;

		segment = segmentFrame / 128;
		frame = segmentFrame & 0x7F;

		*x = (rope->normalisedSegment[segment].x * frame >> FP_SHIFT) + (rope->meshSegment[segment].x >> FP_SHIFT) + rope->position.x;
		*y = (rope->normalisedSegment[segment].y * frame >> FP_SHIFT) + (rope->meshSegment[segment].y >> FP_SHIFT) + rope->position.y;
		*z = (rope->normalisedSegment[segment].z * frame >> FP_SHIFT) + (rope->meshSegment[segment].z >> FP_SHIFT) + rope->position.z;
	}

	int DotProduct(Vector3i* u, Vector3i* v)
	{
		return (u->x * v->x + u->y * v->y + u->z * v->z) >> W2V_SHIFT;
	}

	void ScaleVector(Vector3i* u, int c, Vector3i* destination)
	{
		destination->x = c * u->x >> W2V_SHIFT;
		destination->y = c * u->y >> W2V_SHIFT;
		destination->z = c * u->z >> W2V_SHIFT;
	}

	void CrossProduct(Vector3i* u, Vector3i* v, Vector3i* destination)
	{
		destination->x = (u->y * v->z - u->z * v->y) >> W2V_SHIFT;
		destination->y = (u->z * v->x - u->x * v->z) >> W2V_SHIFT;
		destination->z = (u->x * v->y - u->y * v->x) >> W2V_SHIFT;
	}

	void phd_GetMatrixAngles(int* matrix, short* angle)
	{
		angle[0] = phd_atan(sqrt(SQUARE(matrix[M22]) + SQUARE(matrix[M02])), matrix[M12]);
		if (matrix[M12] >= 0 && angle[0] > 0 || matrix[M12] < 0 && angle[0] < 0)
			angle[0] = -angle[0];

		angle[1] = phd_atan(matrix[M22], matrix[M02]);
		angle[2] = phd_atan(matrix[M00] * phd_cos(angle[1]) - matrix[M20] * phd_sin(angle[1]), matrix[M21] * phd_sin(angle[1]) - matrix[M01] * phd_cos(angle[1]));
	}

	void RopeControl(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];
		auto* rope = &Ropes[item->TriggerFlags];

		if (TriggerActive(item))
		{
			rope->active = 1;
			rope->StoreInterpolationData();

			RopeDynamics(rope);
		}
		else
			rope->active = 0;
	}

	void RopeCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* laraInfo = GetLaraInfo(laraItem);
		auto* ropeItem = &g_Level.Items[itemNumber];
		auto* rope = &Ropes[ropeItem->TriggerFlags];
		
		if (IsHeld(In::Action) &&
			laraInfo->Control.HandStatus == HandStatus::Free &&
			(laraItem->Animation.ActiveState == LS_REACH || laraItem->Animation.ActiveState == LS_JUMP_UP) &&
			laraItem->Animation.IsAirborne &&
			laraItem->Animation.Velocity.y > 0.0f &&
			rope->active)
		{
			auto frame = GameBoundingBox(laraItem);

			int segment = RopeNodeCollision(
				rope,
				laraItem->Pose.Position.x,
				laraItem->Pose.Position.y + frame.Y1 + 512,
				laraItem->Pose.Position.z + frame.Z2 * phd_cos(laraItem->Pose.Orientation.y),
				laraItem->Animation.ActiveState == LS_REACH ? 128 : 320);

			if (segment >= 0)
			{
				if (laraItem->Animation.ActiveState == LS_REACH)
				{
					laraItem->Animation.AnimNumber = LA_REACH_TO_ROPE_SWING;
					laraItem->Animation.ActiveState = LS_ROPE_SWING;
					laraInfo->Control.Rope.Frame  = (GetAnimData(*laraItem, LA_ROPE_SWING).frameBase + 32) << 8;
					laraInfo->Control.Rope.DFrame = (GetAnimData(*laraItem, LA_ROPE_SWING).frameBase + 60) << 8;
				}
				else
				{
					laraItem->Animation.AnimNumber = LA_JUMP_UP_TO_ROPE_START;
					laraItem->Animation.ActiveState = LS_ROPE_IDLE;
				}

				laraItem->Animation.FrameNumber = GetAnimData(laraItem).frameBase;
				laraItem->Animation.Velocity.y = 0;
				laraItem->Animation.IsAirborne = false;

				laraInfo->Control.HandStatus = HandStatus::Busy;
				laraInfo->Control.Rope.Ptr = ropeItem->TriggerFlags;
				laraInfo->Control.Rope.Segment = segment;
				laraInfo->Control.Rope.Y = laraItem->Pose.Orientation.y;

				DelAlignLaraToRope(laraItem);

				CurrentPendulum.velocity.x = 0;
				CurrentPendulum.velocity.y = 0;
				CurrentPendulum.velocity.z = 0;

				ApplyVelocityToRope(segment, laraItem->Pose.Orientation.y, 16 * laraItem->Animation.Velocity.z);
			}
		}
	}

	void RopeDynamics(ROPE_STRUCT* rope)
	{
		PENDULUM* pendulumPointer;
		Vector3i vec, vec2;

		int flag = 0;
		if (rope->coiled)
		{
			--rope->coiled;
			if (!rope->coiled)
			{
				for (int i = 0; i < ROPE_SEGMENTS; ++i)
					rope->velocity[i].y = 0;
			}
		}

		if (Lara.Control.Rope.Ptr != -1 && rope == &Ropes[Lara.Control.Rope.Ptr])
		{
			pendulumPointer = &CurrentPendulum;
			if (CurrentPendulum.node != Lara.Control.Rope.Segment + 1)
			{
				SetPendulumPoint(rope, Lara.Control.Rope.Segment + 1);
				flag = 1;
			}
		}
		else
		{
			pendulumPointer = &AlternatePendulum;
			if (Lara.Control.Rope.Ptr == -1 && CurrentPendulum.rope)
			{
				for (int i = 0; i < CurrentPendulum.node; i++)
				{
					CurrentPendulum.rope->velocity[i].x = CurrentPendulum.rope->velocity[CurrentPendulum.node].x;
					CurrentPendulum.rope->velocity[i].y = CurrentPendulum.rope->velocity[CurrentPendulum.node].y;
					CurrentPendulum.rope->velocity[i].z = CurrentPendulum.rope->velocity[CurrentPendulum.node].z;
				}

				CurrentPendulum.position.x = 0;
				CurrentPendulum.position.y = 0;
				CurrentPendulum.position.z = 0;

				CurrentPendulum.velocity.x = 0;
				CurrentPendulum.velocity.y = 0;
				CurrentPendulum.velocity.z = 0;

				CurrentPendulum.node = -1;
				CurrentPendulum.rope = NULL;
			}
		}

		if (Lara.Control.Rope.Ptr != -1)
		{
			vec.x = pendulumPointer->position.x - rope->segment[0].x;
			vec.y = pendulumPointer->position.y - rope->segment[0].y;
			vec.z = pendulumPointer->position.z - rope->segment[0].z;
			NormaliseRopeVector(&vec);

			for (int i = pendulumPointer->node; i >= 0; --i)
			{
				rope->segment[i].x = rope->meshSegment[std::clamp(i - 1, 0, ROPE_SEGMENTS)].x + ((int64_t)rope->segmentLength * vec.x >> FP_SHIFT);
				rope->segment[i].y = rope->meshSegment[std::clamp(i - 1, 0, ROPE_SEGMENTS)].y + ((int64_t)rope->segmentLength * vec.y >> FP_SHIFT);
				rope->segment[i].z = rope->meshSegment[std::clamp(i - 1, 0, ROPE_SEGMENTS)].z + ((int64_t)rope->segmentLength * vec.z >> FP_SHIFT);

				rope->velocity[i].x = 0;
				rope->velocity[i].y = 0;
				rope->velocity[i].z = 0;
			}

			if (flag)
			{
				vec2.x = pendulumPointer->position.x - rope->segment[pendulumPointer->node].x;
				vec2.y = pendulumPointer->position.y - rope->segment[pendulumPointer->node].y;
				vec2.z = pendulumPointer->position.z - rope->segment[pendulumPointer->node].z;

				rope->segment[pendulumPointer->node].x = pendulumPointer->position.x;
				rope->segment[pendulumPointer->node].y = pendulumPointer->position.y;
				rope->segment[pendulumPointer->node].z = pendulumPointer->position.z;
				
				for (int i = pendulumPointer->node; i < ROPE_SEGMENTS; ++i)
				{
					rope->segment[i].x -= vec2.x;
					rope->segment[i].y -= vec2.y;
					rope->segment[i].z -= vec2.z;

					rope->velocity[i].x = 0;
					rope->velocity[i].y = 0;
					rope->velocity[i].z = 0;
				}
			}

			ModelRigidRope(
				rope, 
				pendulumPointer,
				&rope->velocity[0], 
				&pendulumPointer->velocity,
				rope->segmentLength * pendulumPointer->node);
		
			pendulumPointer->velocity.y += (int)g_GameFlow->GetSettings()->Physics.Gravity << FP_SHIFT;

			pendulumPointer->position.x += pendulumPointer->velocity.x;
			pendulumPointer->position.y += pendulumPointer->velocity.y;
			pendulumPointer->position.z += pendulumPointer->velocity.z;

			pendulumPointer->velocity.x -= pendulumPointer->velocity.x >> 8;
			pendulumPointer->velocity.z -= pendulumPointer->velocity.z >> 8;
		}

		for (int i = pendulumPointer->node; i < ROPE_SEGMENTS - 1; ++i)
			ModelRigid(
				&rope->segment[i],
				&rope->segment[i + 1],
				&rope->velocity[i],
				&rope->velocity[i + 1], 
				rope->segmentLength);

		for (int i = 0; i < ROPE_SEGMENTS; ++i)
		{
			rope->segment[i].x += rope->velocity[i].x;
			rope->segment[i].y += rope->velocity[i].y;
			rope->segment[i].z += rope->velocity[i].z;
		}

		for (int i = pendulumPointer->node; i < ROPE_SEGMENTS; ++i)
		{
			rope->velocity[i].y += 3 << FP_SHIFT;

			if (pendulumPointer->rope)
			{
				rope->velocity[i].x -= rope->velocity[i].x >> 4;
				rope->velocity[i].z -= rope->velocity[i].z >> 4;
			}
			else
			{
				rope->velocity[i].x -= rope->velocity[i].x >> 4;
				rope->velocity[i].z -= rope->velocity[i].z >> 4;
			}
		}

		rope->segment[0].x = 0;
		rope->segment[0].y = 0;
		rope->segment[0].z = 0;

		rope->velocity[0].x = 0;
		rope->velocity[0].y = 0;
		rope->velocity[0].z = 0;

		for (INT i = 0; i < ROPE_SEGMENTS - 1; ++i)
		{
			rope->normalisedSegment[i].x = rope->segment[i + 1].x - rope->segment[i].x;
			rope->normalisedSegment[i].y = rope->segment[i + 1].y - rope->segment[i].y;
			rope->normalisedSegment[i].z = rope->segment[i + 1].z - rope->segment[i].z;
			NormaliseRopeVector(&rope->normalisedSegment[i]);
		}

		if (Lara.Control.Rope.Ptr != -1 && rope != &Ropes[Lara.Control.Rope.Ptr])
		{
			rope->meshSegment[0].x = rope->segment[0].x;
			rope->meshSegment[0].y = rope->segment[0].y;
			rope->meshSegment[0].z = rope->segment[0].z;

			rope->meshSegment[1].x = rope->segment[0].x + ((int64_t)rope->segmentLength * rope->normalisedSegment[0].x >> FP_SHIFT);
			rope->meshSegment[1].y = rope->segment[0].y + ((int64_t)rope->segmentLength * rope->normalisedSegment[0].y >> FP_SHIFT);
			rope->meshSegment[1].z = rope->segment[0].z + ((int64_t)rope->segmentLength * rope->normalisedSegment[0].z >> FP_SHIFT);

			for (int i = 2; i < ROPE_SEGMENTS; i++)
			{
				rope->meshSegment[i].x = rope->meshSegment[i - 1].x + ((int64_t)rope->segmentLength * rope->normalisedSegment[i - 1].x >> FP_SHIFT);
				rope->meshSegment[i].y = rope->meshSegment[i - 1].y + ((int64_t)rope->segmentLength * rope->normalisedSegment[i - 1].y >> FP_SHIFT);
				rope->meshSegment[i].z = rope->meshSegment[i - 1].z + ((int64_t)rope->segmentLength * rope->normalisedSegment[i - 1].z >> FP_SHIFT);
			}
		}
		else
		{
			rope->meshSegment[pendulumPointer->node].x = rope->segment[pendulumPointer->node].x;
			rope->meshSegment[pendulumPointer->node].y = rope->segment[pendulumPointer->node].y;
			rope->meshSegment[pendulumPointer->node].z = rope->segment[pendulumPointer->node].z;

			rope->meshSegment[pendulumPointer->node + 1].x = rope->segment[pendulumPointer->node].x + ((int64_t)rope->segmentLength * rope->normalisedSegment[pendulumPointer->node].x >> FP_SHIFT);
			rope->meshSegment[pendulumPointer->node + 1].y = rope->segment[pendulumPointer->node].y + ((int64_t)rope->segmentLength * rope->normalisedSegment[pendulumPointer->node].y >> FP_SHIFT);
			rope->meshSegment[pendulumPointer->node + 1].z = rope->segment[pendulumPointer->node].z + ((int64_t)rope->segmentLength * rope->normalisedSegment[pendulumPointer->node].z >> FP_SHIFT);

			for (int i = pendulumPointer->node + 1; i < ROPE_SEGMENTS - 1; ++i)
			{
				rope->meshSegment[i + 1].x = rope->meshSegment[i].x + ((int64_t)rope->segmentLength * rope->normalisedSegment[i].x >> FP_SHIFT);
				rope->meshSegment[i + 1].y = rope->meshSegment[i].y + ((int64_t)rope->segmentLength * rope->normalisedSegment[i].y >> FP_SHIFT);
				rope->meshSegment[i + 1].z = rope->meshSegment[i].z + ((int64_t)rope->segmentLength * rope->normalisedSegment[i].z >> FP_SHIFT);
			}

			for (int i = 0; i < pendulumPointer->node; i++)
			{
				rope->meshSegment[i].x = rope->segment[i].x;
				rope->meshSegment[i].y = rope->segment[i].y;
				rope->meshSegment[i].z = rope->segment[i].z;
			}
		}
	}

	int RopeNodeCollision(ROPE_STRUCT* rope, int x, int y, int z, int radius)
	{
		for (int i = 0; i < ROPE_SEGMENTS - 2; ++i)
		{
			if (y > rope->position.y + (rope->meshSegment[i].y >> FP_SHIFT)
				&& y < rope->position.y + (rope->meshSegment[i + 1].y >> FP_SHIFT))
			{
				int dx = x - ((rope->meshSegment[i + 1].x + rope->meshSegment[i].x) >> (FP_SHIFT + 1)) - rope->position.x;
				int dy = y - ((rope->meshSegment[i + 1].y + rope->meshSegment[i].y) >> (FP_SHIFT + 1)) - rope->position.y;
				int dz = z - ((rope->meshSegment[i + 1].z + rope->meshSegment[i].z) >> (FP_SHIFT + 1)) - rope->position.z;

				if (pow(dx, 2) + pow(dy, 2) + pow(dz, 2) < pow(radius + 64, 2))
					return i;
			}
		}

		return -1;
	}

	void ApplyVelocityToRope(int node, short angle, short n)
	{
		SetPendulumVelocity(n * phd_sin(angle) * 4096, 0, n * phd_cos(angle) * 4096);
	}

	void SetPendulumVelocity(int x, int y, int z)
	{
		int node = 2 * (CurrentPendulum.node >> 1);
		if (node < ROPE_SEGMENTS)
		{
			int val = 4096 / (ROPE_SEGMENTS - node) * 256;

			x = (int64_t)val * x >> FP_SHIFT;
			y = (int64_t)val * y >> FP_SHIFT;
			z = (int64_t)val * z >> FP_SHIFT;
		}

		CurrentPendulum.velocity.x += x;
		CurrentPendulum.velocity.y += y;
		CurrentPendulum.velocity.z += z;
	}

	void SetPendulumPoint(ROPE_STRUCT* rope, int node)
	{
		CurrentPendulum.position.x = rope->segment[node].x;
		CurrentPendulum.position.y = rope->segment[node].y;
		CurrentPendulum.position.z = rope->segment[node].z;

		if (CurrentPendulum.node == -1)
		{
			CurrentPendulum.velocity.x += rope->velocity[node].x;
			CurrentPendulum.velocity.y += rope->velocity[node].y;
			CurrentPendulum.velocity.z += rope->velocity[node].z;
		}

		CurrentPendulum.node = node;
		CurrentPendulum.rope = rope;
	}

	void ModelRigidRope(ROPE_STRUCT* rope, PENDULUM* pendulumPointer, Vector3i* ropeVelocity, Vector3i* pendulumVelocity, int value)
	{
		Vector3i vec;
		vec.x = pendulumPointer->position.x + pendulumVelocity->x - rope->segment[0].x;
		vec.y = pendulumPointer->position.y + pendulumVelocity->y - rope->segment[0].y;
		vec.z = pendulumPointer->position.z + pendulumVelocity->z - rope->segment[0].z;

		int result = 65536 * sqrt(abs(pow(vec.x >> FP_SHIFT, 2) + pow(vec.y >> FP_SHIFT, 2) + pow(vec.z >> FP_SHIFT, 2))) - value;
		NormaliseRopeVector(&vec);

		pendulumVelocity->x -= (int64_t)result * vec.x >> FP_SHIFT;
		pendulumVelocity->y -= (int64_t)result * vec.y >> FP_SHIFT;
		pendulumVelocity->z -= (int64_t)result * vec.z >> FP_SHIFT;
	}

	void ModelRigid(Vector3i* segment, Vector3i* nextSegment, Vector3i* velocity, Vector3i* nextVelocity, int length)
	{
		Vector3i vec;
		vec.x = nextSegment->x + nextVelocity->x - segment->x - velocity->x;
		vec.y = nextSegment->y + nextVelocity->y - segment->y - velocity->y;
		vec.z = nextSegment->z + nextVelocity->z - segment->z - velocity->z;

		int result = (65536 * sqrt(abs(pow(vec.x >> FP_SHIFT, 2) + pow(vec.y >> FP_SHIFT, 2) + pow(vec.z >> FP_SHIFT, 2))) - length) / 2;
		NormaliseRopeVector(&vec);

		vec.x = (int64_t)result * vec.x >> FP_SHIFT;
		vec.y = (int64_t)result * vec.y >> FP_SHIFT;
		vec.z = (int64_t)result * vec.z >> FP_SHIFT;

		velocity->x += vec.x;
		velocity->y += vec.y;
		velocity->z += vec.z;

		nextVelocity->x -= vec.x;
		nextVelocity->y -= vec.y;
		nextVelocity->z -= vec.z;
	}

	void UpdateRopeSwing(ItemInfo* item)
	{
		if (Lara.Control.Rope.MaxXForward > 9000)
			Lara.Control.Rope.MaxXForward = 9000;

		if (Lara.Control.Rope.MaxXBackward > 9000)
			Lara.Control.Rope.MaxXBackward = 9000;

		if (Lara.Control.Rope.Direction)
		{
			if (item->Pose.Orientation.x > 0 && item->Pose.Orientation.x - Lara.Control.Rope.LastX < -100)
			{
				Lara.Control.Rope.ArcFront = Lara.Control.Rope.LastX;
				Lara.Control.Rope.Direction = 0;
				Lara.Control.Rope.MaxXBackward = 0;
				int frame = (15 * Lara.Control.Rope.MaxXForward / 18000 + GetAnimData(*item, LA_ROPE_SWING).frameBase + 47) << 8;

				if (frame > Lara.Control.Rope.DFrame)
				{
					Lara.Control.Rope.DFrame = frame;
					RopeSwing = 1;
				}
				else
					RopeSwing = 0;

				SoundEffect(SFX_TR4_LARA_ROPE_CREAK, &item->Pose);
			}
			else if (Lara.Control.Rope.LastX < 0 && Lara.Control.Rope.Frame == Lara.Control.Rope.DFrame)
			{
				RopeSwing = 0;
				Lara.Control.Rope.DFrame = (15 * Lara.Control.Rope.MaxXBackward / 18000 + GetAnimData(*item, LA_ROPE_SWING).frameBase + 47) << 8;
				Lara.Control.Rope.FrameRate = 15 * Lara.Control.Rope.MaxXBackward / 9000 + 1;
			}
			else if (Lara.Control.Rope.FrameRate < 512)
			{
				int num = RopeSwing ? 31 : 7;
				Lara.Control.Rope.FrameRate += num * Lara.Control.Rope.MaxXBackward / 9000 + 1;
			}
		}
		else
		{
			if (item->Pose.Orientation.x < 0 && item->Pose.Orientation.x - Lara.Control.Rope.LastX > 100)
			{
				Lara.Control.Rope.ArcBack = Lara.Control.Rope.LastX;
				Lara.Control.Rope.Direction = 1;
				Lara.Control.Rope.MaxXForward = 0;

				int frame = (GetAnimData(*item, LA_ROPE_SWING).frameBase - 15 * Lara.Control.Rope.MaxXBackward / 18000 + 17) << 8;
				if (frame < Lara.Control.Rope.DFrame)
				{
					Lara.Control.Rope.DFrame = frame;
					RopeSwing = 1;
				}
				else
					RopeSwing = 0;

				SoundEffect(SFX_TR4_LARA_ROPE_CREAK, &item->Pose);
			}
			else if (Lara.Control.Rope.LastX > 0 && Lara.Control.Rope.Frame == Lara.Control.Rope.DFrame)
			{
				RopeSwing = 0;
				Lara.Control.Rope.DFrame = (GetAnimData(*item, LA_ROPE_SWING).frameBase - 15 * Lara.Control.Rope.MaxXForward / 18000 + 17) << 8;
				Lara.Control.Rope.FrameRate = 15 * Lara.Control.Rope.MaxXForward / 9000 + 1;
			}
			else if (Lara.Control.Rope.FrameRate < 512)
			{
				int num = RopeSwing ? 31 : 7;
				Lara.Control.Rope.FrameRate += num * Lara.Control.Rope.MaxXForward / 9000 + 1;
			}
		}

		Lara.Control.Rope.LastX = item->Pose.Orientation.x;
		if (Lara.Control.Rope.Direction)
		{
			if (item->Pose.Orientation.x > Lara.Control.Rope.MaxXForward)
				Lara.Control.Rope.MaxXForward = item->Pose.Orientation.x;
		}
		else
		{
			if (item->Pose.Orientation.x < -Lara.Control.Rope.MaxXBackward)
				Lara.Control.Rope.MaxXBackward = abs(item->Pose.Orientation.x);
		}
	}

	bool RopeSwingCollision(ItemInfo* item, CollisionInfo* coll, bool testForStumble)
	{
		auto* lara = GetLaraInfo(item);

		coll->Setup.Mode = CollisionProbeMode::FreeForward;
		coll->Setup.ForwardAngle = lara->Control.Rope.Direction ? item->Pose.Orientation.y : -item->Pose.Orientation.y;
		GetCollisionInfo(coll, item);

		bool stumble = testForStumble &&
			((coll->CollisionType != CollisionType::None && 
			  coll->CollisionType != CollisionType::Top &&
			  coll->CollisionType != CollisionType::TopFront) || 
			  coll->HitStatic);

		if (stumble || 
			TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, item->RoomNumber) ||
			TestEnvironment(RoomEnvFlags::ENV_FLAG_SWAMP, item->RoomNumber))
		{
			ShiftItem(item, coll);
			FallFromRope(item, stumble);
			return true;
		}

		return false;
	}

	void JumpOffRope(ItemInfo* item)
	{
		if (Lara.Control.Rope.Ptr != -1)
		{
			if (item->Pose.Orientation.x >= 0)
			{
				item->Animation.Velocity.y = -112;
				item->Animation.Velocity.z = item->Pose.Orientation.x / 128;
			}
			else
			{
				item->Animation.Velocity.z = 0;
				item->Animation.Velocity.y = -20;
			}

			item->Pose.Orientation.x = 0;
			item->Animation.IsAirborne = true;

			Lara.Control.HandStatus = HandStatus::Free;

			if (item->Animation.FrameNumber - GetAnimData(*item, LA_ROPE_SWING).frameBase > 42)
				item->Animation.AnimNumber = LA_ROPE_SWING_TO_REACH_1;
			else if (item->Animation.FrameNumber - GetAnimData(*item, LA_ROPE_SWING).frameBase > 21)
				item->Animation.AnimNumber = LA_ROPE_SWING_TO_REACH_2;
			else
				item->Animation.AnimNumber = LA_ROPE_SWING_TO_REACH_3;

			item->Animation.FrameNumber = GetAnimData(item).frameBase;
			item->Animation.ActiveState = LS_REACH;
			item->Animation.TargetState = LS_REACH;
			Lara.Control.Rope.Ptr = -1;
		}
	}

	void FallFromRope(ItemInfo* item, bool stumble)
	{
		item->Animation.Velocity.z = abs(CurrentPendulum.velocity.x >> FP_SHIFT) + abs(CurrentPendulum.velocity.z >> FP_SHIFT) / 2;
		item->Pose.Orientation.x = 0;
		item->Pose.Position.y += 320;

		SetAnimation(item, stumble ? LA_JUMP_WALL_SMASH_START : LA_FALL_START);

		item->Animation.Velocity.y = 0;
		item->Animation.IsAirborne = true;

		auto* lara = GetLaraInfo(item);
		lara->Control.HandStatus = HandStatus::Free;
		lara->Control.Rope.Ptr = -1;

		if (stumble)
		{
			item->Animation.Velocity.z = -item->Animation.Velocity.z;
			DoDamage(item, 0);
		}
	}

	void LaraClimbRope(ItemInfo* item, CollisionInfo* coll)
	{
		if (!IsHeld(In::Action))
			FallFromRope(item);
		else
		{
			Camera.targetAngle = ANGLE(30.0f);

			if (Lara.Control.Rope.Count)
			{
				if (!Lara.Control.Rope.Flag)
				{
					--Lara.Control.Rope.Count;
					Lara.Control.Rope.Offset += Lara.Control.Rope.DownVel;

					if (!Lara.Control.Rope.Count)
						Lara.Control.Rope.Flag = 1;

					return;
				}
			}
			else
			{
				if (!Lara.Control.Rope.Flag)
				{
					ROPE_STRUCT* rope = &Ropes[Lara.Control.Rope.Ptr];
					Lara.Control.Rope.Offset = 0;
					Lara.Control.Rope.DownVel = (unsigned int)(rope->meshSegment[Lara.Control.Rope.Segment + 1].y - rope->meshSegment[Lara.Control.Rope.Segment].y) >> 17;
					Lara.Control.Rope.Count = 0;
					Lara.Control.Rope.Offset += Lara.Control.Rope.DownVel;
					Lara.Control.Rope.Flag = 1;
					return;
				}
			}

			if (item->Animation.AnimNumber == LA_ROPE_DOWN && item->Animation.FrameNumber == GetAnimData(item).frameEnd)
			{
				SoundEffect(SFX_TR4_LARA_POLE_SLIDE_LOOP, &LaraItem->Pose);
				item->Animation.FrameNumber = GetAnimData(item).frameBase;
				Lara.Control.Rope.Flag = 0;
				++Lara.Control.Rope.Segment;
				Lara.Control.Rope.Offset = 0;
			}

			if (!IsHeld(In::Back) || Lara.Control.Rope.Segment >= 21)
				item->Animation.TargetState = LS_ROPE_IDLE;
		}
	}

	void DelAlignLaraToRope(ItemInfo* item)
	{
		ROPE_STRUCT* rope;
		short ropeY;
		Vector3i vec, vec2, vec3, vec4, vec5, pos, pos2, diff, diff2;
		int matrix[12];
		short angle[3];

		vec.x = 4096;
		vec.y = 0;
		vec.z = 0;

		const auto& offset = GetBestFrame(*item).Offset;
		ropeY = Lara.Control.Rope.Y - ANGLE(90.0f);
		rope = &Ropes[Lara.Control.Rope.Ptr];

		GetRopePos(rope, ((Lara.Control.Rope.Segment - 1) << 7) + offset.y, &pos.x, &pos.y, &pos.z);
		GetRopePos(rope, ((Lara.Control.Rope.Segment - 1) << 7) + offset.y - 192, &pos2.x, &pos2.y, &pos2.z);

		diff.x = (pos.x - pos2.x) << 16;
		diff.y = (pos.y - pos2.y) << 16;
		diff.z = (pos.z - pos2.z) << 16;
		NormaliseRopeVector(&diff);
		diff.x >>= 2;
		diff.y >>= 2;
		diff.z >>= 2;
		ScaleVector(&diff, DotProduct(&vec, &diff), &vec2);
		vec2.x = vec.x - vec2.x;
		vec2.y = vec.y - vec2.y;
		vec2.z = vec.z - vec2.z;

		vec3.x = vec2.x;
		vec3.y = vec2.y;
		vec3.z = vec2.z;
		vec4.x = vec2.x;
		vec4.y = vec2.y;
		vec4.z = vec2.z;

		diff2.x = diff.x;
		diff2.y = diff.y;
		diff2.z = diff.z;

		ScaleVector(&vec3, 16384 * phd_cos(ropeY), &vec3);
		ScaleVector(&diff2, DotProduct(&diff2, &vec2), &diff2);
		ScaleVector(&diff2, 4096 - 16384 * phd_cos(ropeY), &diff2);

		CrossProduct(&diff, &vec2, &vec4);
		ScaleVector(&vec4, 16384 * phd_sin(ropeY), &vec4);
		diff2.x += vec3.x;
		diff2.y += vec3.y;
		diff2.z += vec3.z;

		vec2.x = (diff2.x + vec4.x) << 16;
		vec2.y = (diff2.y + vec4.y) << 16;
		vec2.z = (diff2.z + vec4.z) << 16;
		NormaliseRopeVector(&vec2);
		vec2.x >>= 2;
		vec2.y >>= 2;
		vec2.z >>= 2;

		CrossProduct(&diff, &vec2, &vec5);

		vec5.x <<= 16;
		vec5.y <<= 16;
		vec5.z <<= 16;
		NormaliseRopeVector(&vec5);
		vec5.x >>= 2;
		vec5.y >>= 2;
		vec5.z >>= 2;

		matrix[M00] = vec5.x;
		matrix[M01] = diff.x;
		matrix[M02] = vec2.x;
		matrix[M10] = vec5.y;
		matrix[M11] = diff.y;
		matrix[M12] = vec2.y;
		matrix[M20] = vec5.z;
		matrix[M21] = diff.z;
		matrix[M22] = vec2.z;

		phd_GetMatrixAngles(matrix, angle);

		item->Pose.Position.x = rope->position.x + (rope->meshSegment[Lara.Control.Rope.Segment].x >> FP_SHIFT);
		item->Pose.Position.y = rope->position.y + (rope->meshSegment[Lara.Control.Rope.Segment].y >> FP_SHIFT) + Lara.Control.Rope.Offset;
		item->Pose.Position.z = rope->position.z + (rope->meshSegment[Lara.Control.Rope.Segment].z >> FP_SHIFT);

		auto rotMatrix = Matrix::CreateFromYawPitchRoll(
			TO_RAD(angle[1]),
			TO_RAD(angle[0]),
			TO_RAD(angle[2])
		);

		rotMatrix = rotMatrix.Transpose();

		item->Pose.Position.x += -112 * rotMatrix.m[0][2];
		item->Pose.Position.y += -112 * rotMatrix.m[1][2];
		item->Pose.Position.z += -112 * rotMatrix.m[2][2];

		item->Pose.Orientation.x = angle[0];
		item->Pose.Orientation.y = angle[1];
		item->Pose.Orientation.z = angle[2];
	}
}
