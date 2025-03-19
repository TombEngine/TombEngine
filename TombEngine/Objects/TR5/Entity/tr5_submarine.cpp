#include "framework.h"
#include "Objects/TR5/Entity/tr5_submarine.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/los.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR5
{
	void InitializeSubmarine(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, 0);

		if (!item->TriggerFlags)
			item->TriggerFlags = 120;
	}

	void TriggerSubmarineSparks(short itemNumber)
	{
		auto* spark = GetFreeParticle();

		spark->on = 1;
		spark->sR = -1;
		spark->sG = -1;
		spark->sB = -1;
		spark->colFadeSpeed = 2;
		spark->dG = (GetRandomControl() & 0x1F) - 32;
		spark->life = 2;
		spark->dR = spark->dG / 2;
		spark->dB = spark->dG / 2;
		spark->sLife = 2;
		spark->blendMode = BlendMode::Additive;
		spark->fadeToBlack = 0;
		spark->flags = 20650;
		spark->fxObj = itemNumber;
		spark->nodeNumber = 7;
		spark->x = 0;
		spark->z = 0;
		spark->y = 0;
		spark->xVel = 0;
		spark->yVel = 0;
		spark->zVel = 0;
		spark->maxYvel = 0;
		spark->gravity = 0;
		spark->scalar = 1;
		spark->SpriteSeqID = ID_DEFAULT_SPRITES;
		spark->SpriteID = SPR_LENS_FLARE_LIGHT;
		spark->dSize = spark->sSize = spark->size = (GetRandomControl() & 7) + 192;
	}

	void TriggerTorpedoBubbles(Vector3i* pos1, Vector3i* pos2, char factor)
	{
		auto* spark = GetFreeParticle();

		spark->on = 1;
		spark->sR = 32;
		spark->sG = 32;
		spark->sB = 32;
		spark->dR = 80;
		spark->dG = 80;
		spark->dB = 80;
		spark->colFadeSpeed = 2;
		spark->fadeToBlack = 8;
		spark->blendMode = BlendMode::Additive;
		spark->life = spark->sLife = (GetRandomControl() & 7) + 16;
		spark->x = pos1->x + (GetRandomControl() & 0x1F);
		spark->y = (GetRandomControl() & 0x1F) + pos1->y - 16;
		spark->z = (GetRandomControl() & 0x1F) + pos1->z - 16;
		spark->xVel = pos2->x + (GetRandomControl() & 0x7F) - pos1->x - 64;
		spark->yVel = pos2->y + (GetRandomControl() & 0x7F) - pos1->y - 64;
		spark->zVel = pos2->z + (GetRandomControl() & 0x7F) - pos1->z - 64;
		spark->friction = 0;
		spark->SpriteSeqID = ID_DEFAULT_SPRITES;
		spark->SpriteID = SPR_BUBBLE_CLUSTER;
		spark->maxYvel = 0;
		spark->gravity = -4 - (GetRandomControl() & 3);
		spark->scalar = 1;
		spark->flags = SP_ROTATE | SP_DEF | SP_SCALE;
		spark->rotAng = GetRandomControl() & 0xFFF;
		spark->rotAdd = (GetRandomControl() & 0x3F) - 32;
		spark->sSize = spark->size = ((GetRandomControl() & 0x0F) + 32) >> factor;
		spark->dSize = spark->size * 2;
	}

	void TriggerTorpedoSparks2(Vector3i* pos1, Vector3i* pos2, char scale)
	{
		auto* spark = GetFreeParticle();

		spark->on = 1;
		spark->sR = 32;
		spark->sG = 32;
		spark->sB = 32;
		spark->dR = -128;
		spark->dG = -128;
		spark->dB = -128;
		spark->colFadeSpeed = 2;
		spark->fadeToBlack = 8;
		spark->blendMode = BlendMode::Additive;
		spark->life = spark->sLife = (GetRandomControl() & 7) + 16;
		spark->x = pos1->x + (GetRandomControl() & 0x1F);
		spark->y = (GetRandomControl() & 0x1F) + pos1->y - 16;
		spark->z = (GetRandomControl() & 0x1F) + pos1->z - 16;
		spark->xVel = pos2->x + (GetRandomControl() & 0x7F) - pos1->x - 64;
		spark->yVel = pos2->y + (GetRandomControl() & 0x7F) - pos1->y - 64;
		spark->zVel = pos2->z + (GetRandomControl() & 0x7F) - pos1->z - 64;
		spark->friction = 51;
		spark->gravity = -4 - (GetRandomControl() & 3);
		spark->maxYvel = 0;
		spark->scalar = 2 - scale;
		spark->flags = SP_EXPDEF | SP_ROTATE | SP_SCALE;
		spark->rotAng = GetRandomControl() & 0xFFF;
		spark->rotAdd = (GetRandomControl() & 0x3F) - 32;
		spark->sSize = spark->size = (GetRandomControl() & 0x0F) + 32;
		spark->dSize = spark->size * 2;
	}

	void SubmarineAttack(ItemInfo* item)
	{
		short itemNumber = CreateItem();
		if (itemNumber == NO_VALUE)
			return;

		auto* torpedoItem = &g_Level.Items[itemNumber];

		SoundEffect(SFX_TR5_UNDERWATER_TORPEDO, &torpedoItem->Pose, SoundEnvironment::Always);

		torpedoItem->ObjectNumber = ID_TORPEDO;
		torpedoItem->Model.Color = Vector4(0.5f, 0.5f, 0.5f, 1.0f);

		auto pos1 = Vector3i::Zero;
		auto pos2 = Vector3i::Zero;
		for (int i = 0; i < 8; i++)
		{
			pos1 = GetJointPosition(
				item, 4,
				Vector3i(
					(GetRandomControl() & 0x7F) - 414,
					-320,
					352));

			pos2 = GetJointPosition(
				item, 4, Vector3i(
					(GetRandomControl() & 0x3FF) - 862,
					-320 - (GetRandomControl() & 0x3FF),
					(GetRandomControl() & 0x3FF) - 160));

			TriggerTorpedoSparks2(&pos1, &pos2, 0);
		}

		torpedoItem->RoomNumber = item->RoomNumber;
		GetFloor(pos1.x, pos1.y, pos1.z, &torpedoItem->RoomNumber);

		torpedoItem->Pose.Position = pos1;

		InitializeItem(itemNumber);

		torpedoItem->Pose.Orientation.x = 0;
		torpedoItem->Pose.Orientation.y = item->Pose.Orientation.y;
		torpedoItem->Pose.Orientation.z = 0;
		torpedoItem->Animation.Velocity.y = 0.0f;
		torpedoItem->Animation.Velocity.z = 0.0f;
		torpedoItem->ItemFlags[0] = -1;

		AddActiveItem(itemNumber);
	}

	void SubmarineControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		if (item->AIBits)
			GetAITarget(creature);
		else
			creature->Enemy = LaraItem;

		AI_INFO AI, laraAI;
		CreatureAIInfo(item, &AI);

		GetCreatureMood(item, &AI, true);
		CreatureMood(item, &AI, true);

		short angle = CreatureTurn(item, creature->MaxTurn);

		if (creature->Enemy == LaraItem)
		{
			laraAI.angle = AI.angle;
			laraAI.distance = AI.distance;
		}
		else
		{
			int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
			int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;

			laraAI.angle = phd_atan(dz, dx) - item->Pose.Orientation.y;
			laraAI.distance = pow(dx, 2) + pow(dz, 2);
			laraAI.ahead = true;
		}

		int tilt = item->ItemFlags[0] + (angle / 2);

		if (tilt > ANGLE(11.25f))
			tilt = ANGLE(11.25f);
		else if (tilt < ANGLE(-11.25f))
			tilt = ANGLE(-11.25f);

		item->ItemFlags[0] = tilt;

		if (abs(tilt) >= ANGLE(0.35f))
		{
			if (tilt > 0)
				item->ItemFlags[0] -= ANGLE(0.35f);
			else
				item->ItemFlags[0] += ANGLE(0.35f);
		}
		else
			item->ItemFlags[0] = 0;

		creature->MaxTurn = ANGLE(2.0f);

		short joint = AI.xAngle - ANGLE(45.0f);

		if (creature->Flags < item->TriggerFlags)
			creature->Flags++;

		auto* enemy = creature->Enemy;
		creature->Enemy = LaraItem;

		if (Targetable(item, &laraAI))
		{
			if (creature->Flags >= item->TriggerFlags &&
				laraAI.angle > -ANGLE(90.0f) &&
				laraAI.angle < ANGLE(90.0f))
			{
				SubmarineAttack(item);
				creature->Flags = 0;
			}

			if (laraAI.distance >= pow(BLOCK(3), 2))
			{
				item->Animation.TargetState = 1;
				SoundEffect(SFX_TR5_VEHICLE_DIVESUIT_LOOP, &item->Pose, SoundEnvironment::Always);
			}
			else
				item->Animation.TargetState = 0;

			if (AI.distance < pow(BLOCK(1), 2))
			{
				creature->MaxTurn = 0;
				if (abs(laraAI.angle) >= ANGLE(2.0f))
				{
					if (laraAI.angle >= 0)
						item->Pose.Orientation.y += ANGLE(2.0f);
					else
						item->Pose.Orientation.y -= ANGLE(2.0f);
				}
				else
					item->Pose.Orientation.y += laraAI.angle;
			}
		}
		else
			item->Animation.TargetState = 1;

		creature->Enemy = enemy;

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, joint);
		CreatureJoint(item, 1, joint);

		if (GlobalCounter & 1)
		{
			auto pos1 = GetJointPosition(item, 1, Vector3i(200, 320, 90));
			auto pos2 = GetJointPosition(item, 1, Vector3i(200, 1280, 90));
			TriggerTorpedoBubbles(&pos1, &pos2, 0);

			pos1 = GetJointPosition(item, 1, Vector3i(200, 230, -100));
			pos2 = GetJointPosition(item, 1, Vector3i(200, 1280, -100));
			TriggerTorpedoBubbles(&pos1, &pos2, 0);
		}
		else
		{
			auto pos1 = GetJointPosition(item, 2, Vector3i(-200, 320, 90));
			auto pos2 = GetJointPosition(item, 2, Vector3i(-200, 1280, 90));
			TriggerTorpedoBubbles(&pos1, &pos2, 0);

			pos1 = GetJointPosition(item, 2, Vector3i(-200, 320, -100));
			pos2 = GetJointPosition(item, 2, Vector3i(-200, 1280, -100));
			TriggerTorpedoBubbles(&pos1, &pos2, 0);
		}

		TriggerSubmarineSparks(itemNumber);

		auto origin = GameVector(GetJointPosition(item, 0, Vector3i(0, -600, -40)), item->RoomNumber);
		auto target = GameVector(GetJointPosition(item, 0, Vector3i(0, -15784, -40)));

		if (!LOS(&origin, &target))
		{
			int distance = sqrt(pow(target.x - origin.x, 2) + pow(target.y - origin.y, 2) + pow(target.z - origin.z, 2));
			if (distance < BLOCK(16))
			{
				distance = BLOCK(16) - distance;
				byte color = (GetRandomControl() & 0xF) + (distance / 128) + 64;
				SpawnDynamicLight(target.x, target.y, target.z, (GetRandomControl() & 1) + (distance / 2048) + 12, color / 2, color, color / 2);
			}
		}

		if (creature->ReachedGoal)
		{
			if (creature->Enemy)
			{
				if (creature->Flags & 2)
					item->ItemFlags[3] = LOBYTE(creature->Tosspad) - 1;

				item->ItemFlags[3]++;
				creature->ReachedGoal = false;
				creature->Enemy = nullptr;
			}
		}

		CreatureAnimation(itemNumber, angle, tilt);
		CreatureUnderwater(item, -14080);
	}

	void ChaffFlareControl(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (item->Animation.Velocity.y)
		{
			item->Pose.Orientation.x += ANGLE(3.0f);
			item->Pose.Orientation.z += ANGLE(5.0f);
		}

		int dx = item->Animation.Velocity.z * phd_sin(item->Pose.Orientation.y);
		int dz = item->Animation.Velocity.z * phd_cos(item->Pose.Orientation.y);

		item->Pose.Position.x += dx;
		item->Pose.Position.z += dz;

		if (TestEnvironment(ENV_FLAG_WATER, item->RoomNumber))
		{
			item->Animation.Velocity.y += (5.0f - item->Animation.Velocity.y) / 2.0f;
			item->Animation.Velocity.z += (5.0f - item->Animation.Velocity.z) / 2.0f;
		}
		else
		{
			item->Animation.Velocity.y += g_GameFlow->GetSettings()->Physics.Gravity;
		}

		item->Pose.Position.y += item->Animation.Velocity.y;

		DoProjectileDynamics(itemNumber, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, dx, item->Animation.Velocity.y, dz);

		auto pos1 = GetJointPosition(item, 0, Vector3i(0, 0, (GlobalCounter & 1) != 0 ? 48 : -48));
		auto pos2 = GetJointPosition(item, 0, Vector3i(0, 0, 8 * ((GlobalCounter & 1) != 0 ? 48 : -48)));
		TriggerTorpedoBubbles(&pos1, &pos2, 1);

		if (item->ItemFlags[0] >= 300)
		{
			if (!item->Animation.Velocity.y && !item->Animation.Velocity.z)
			{
				if (item->ItemFlags[1] <= 90)
					item->ItemFlags[1]++;
				else
					KillItem(itemNumber);
			}
		}
		else
			item->ItemFlags[0]++;
	}

	void TorpedoControl(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		SoundEffect(SFX_TR5_VEHICLE_DIVESUIT_HIT, &item->Pose, SoundEnvironment::Always);

		Vector3i pos;

		if (item->ItemFlags[0] == NO_VALUE)
		{
			bool found = false;
			for (int i = g_Level.NumItems; i < 256; i++)
			{
				auto* searchItem = &g_Level.Items[i];

				if (searchItem->ObjectNumber == ID_CHAFF && searchItem->Active)
				{
					item->ItemFlags[0] = i;
					pos = searchItem->Pose.Position;
					found = true;
					break;
				}
			}

			if (!found)
				pos = LaraItem->Pose.Position;
		}
		else
		{
			auto* chaffItem = &g_Level.Items[item->ItemFlags[0]];

			if (chaffItem->Active && chaffItem->ObjectNumber == ID_CHAFF)
			{
				pos = chaffItem->Pose.Position;
				item->Animation.ActiveState = pos.x / 4;
				item->Animation.TargetState = pos.y / 4;
				item->Animation.RequiredState = pos.z / 4;
			}
			else
			{
				pos.x = 4 * item->Animation.ActiveState;
				pos.y = 4 * item->Animation.TargetState;
				pos.z = 4 * (item->Animation.RequiredState == NO_VALUE) ? 0 : item->Animation.RequiredState;
			}
		}

		auto orient = Geometry::GetOrientToPoint(item->Pose.Position.ToVector3(), pos.ToVector3());

		if (item->Animation.Velocity.z >= 48.0f)
		{
			if (item->Animation.Velocity.z < 192.0f)
				item->Animation.Velocity.z++;
		}
		else
			item->Animation.Velocity.z += 4.0f;

		item->ItemFlags[1]++;

		if (item->ItemFlags[1] - 1 < 60)
		{
			short dry = orient.y - item->Pose.Orientation.y;
			if (abs(dry) > ANGLE(180.0f))
				dry = -dry;

			short drx = orient.x - item->Pose.Orientation.x;
			if (abs(drx) > ANGLE(180.0f))
				drx = -drx;

			drx /= 8;
			dry /= 8;

			if (drx <= 512)
			{
				if (drx < -512)
					drx = -512;
			}
			else
				drx = 512;

			if (dry <= 512)
			{
				if (dry < -512)
					dry = -512;
			}
			else
				dry = 512;

			item->Pose.Orientation.y += dry;
			item->Pose.Orientation.x += drx;
		}

		int x = item->Pose.Position.x;
		int y = item->Pose.Position.y;
		int z = item->Pose.Position.z;

		item->Pose.Orientation.z += 16 * item->Animation.Velocity.z;

		item->Pose.Translate(item->Pose.Orientation, item->Animation.Velocity.z);
		
		auto probe = GetPointCollision(*item);

		if (item->Pose.Position.y < probe.GetFloorHeight() &&
			item->Pose.Position.y > probe.GetCeilingHeight() &&
			TestEnvironment(ENV_FLAG_WATER, probe.GetRoomNumber()))
		{
			if (ItemNearLara(item->Pose.Position, 200))
			{
				LaraItem->HitStatus = true;
				KillItem(itemNumber);
				TriggerUnderwaterExplosion(item, 1);
				SoundEffect(SFX_TR5_UNDERWATER_EXPLOSION, &item->Pose, SoundEnvironment::Always);
				SoundEffect(SFX_TR5_VEHICLE_DIVESUIT_HIT, &LaraItem->Pose, SoundEnvironment::Always);
				DoDamage(LaraItem, 200);
			}
			else
			{
				//	if (ItemNearLara(&item->pos, 400) && Lara.anxiety < 0xE0)
				//		Lara.anxiety += 32;

				if (probe.GetRoomNumber() != item->RoomNumber)
					ItemNewRoom(itemNumber, probe.GetRoomNumber());

				auto pos1 = GetJointPosition(item, 0, Vector3i(0, 0, -64));
				auto pos2 = GetJointPosition(item, 0, Vector3i(0, 0, -64 << ((GlobalCounter & 1) + 2)));
				TriggerTorpedoBubbles(&pos1, &pos2, 1);
			}
		}
		else
		{
			item->Pose.Position.x = x;
			item->Pose.Position.y = y;
			item->Pose.Position.z = z;
			TriggerUnderwaterExplosion(item, 1);
			SoundEffect(SFX_TR5_UNDERWATER_EXPLOSION, &item->Pose, SoundEnvironment::Always);
			KillItem(itemNumber);
		}
	}
}
