#include "framework.h"
#include "Objects/TR4/Entity/Locust.h"

#include "Game/animation.h"
#include "Game/collision/collide_item.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::TR4 
{
	constexpr auto LOCUST_LARA_DAMAGE = 3;
	constexpr auto LOCUST_ENTITY_DAMAGE = 1;

	constexpr auto LOCUST_FLYOFF_TIMEOUT = 90;

	LOCUST_INFO Locusts[MAX_LOCUSTS];

	int CreateLocust()
	{
		for (int i = 0; i < MAX_LOCUSTS; i++)
		{
			auto* locust = &Locusts[i];

			if (!locust->On)
				return i;
		}

		return NO_VALUE;
	}

	void SpawnLocust(ItemInfo* item)
	{
		Vector3i origin, target;
		EulerAngles orient;
		int enemy = NO_VALUE;

		short locustNumber = CreateLocust();

		if (locustNumber == NO_VALUE)
			return;

		auto* locust = &Locusts[locustNumber];

		// Emitter.
		if (!item->IsCreature())
		{
			target = item->Pose.Position;
			orient = EulerAngles(0, item->Pose.Orientation.y + ANGLE(180.0f), 0);
			enemy = LaraItem->Index;
		}
		// Mutant.
		else
		{
			origin = GetJointPosition(item, 9, Vector3i(0, -96, 144));
			target = GetJointPosition(item, 9, Vector3i(0, -128, 288));
			orient = Geometry::GetOrientToPoint(origin.ToVector3(), target.ToVector3());
			enemy = GetCreatureInfo(item)->Enemy->Index;
		}

		locust->On = true;
		locust->Target = enemy;
		locust->Pose.Position = target;
		locust->Pose.Orientation.x = (GetRandomControl() & 0x3FF) + orient.x - ANGLE(2.8f);
		locust->Pose.Orientation.y = (GetRandomControl() & 0x7FF) + orient.y - ANGLE(5.6f);
		locust->RoomNumber = item->RoomNumber;
		locust->Velocity = (GetRandomControl() & 0x1F) + 16;
		locust->Offset.y = GetRandomControl() & 0x1FF;
		locust->Counter = 20 * ((GetRandomControl() & 0x7) + 15);
	}

	void InitializeLocustEmitter(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (!item->TriggerFlags)
			item->TriggerFlags = MAX_LOCUSTS;

		if (item->Pose.Orientation.y > 0)
		{
			if (item->Pose.Orientation.y == ANGLE(90.0f))
				item->Pose.Position.x += CLICK(2);
		}
		else if (item->Pose.Orientation.y < 0)
		{
			if (item->Pose.Orientation.y == -ANGLE(180.0f))
				item->Pose.Position.z -= CLICK(2);
			else if (item->Pose.Orientation.y == -ANGLE(90.0f))
				item->Pose.Position.x -= CLICK(2);
		}
		else
			item->Pose.Position.z += CLICK(2);
	}

	void LocustEmitterControl(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (!TriggerActive(item))
			return;

		if (item->TriggerFlags)
		{
			SpawnLocust(item);
			item->TriggerFlags--;
		}
		else
			KillItem(itemNumber);
	}

	void UpdateLocusts()
	{
		if (!Objects[ID_LOCUSTS_EMITTER].loaded)
			return;

		int closestDist = INT_MAX;
		int closestNumber = NO_VALUE;

		for (int i = 0; i < MAX_LOCUSTS; i++)
		{
			auto* locust = &Locusts[i];

			if (!locust->On)
				continue;

			locust->StoreInterpolationData();

			auto* targetItem = &g_Level.Items[locust->Target];

			if ((targetItem->Effect.Type != EffectType::None || targetItem->HitPoints <= 0) &&
				locust->Counter > LOCUST_FLYOFF_TIMEOUT && !(GetRandomControl() & 7))
			{
				locust->Counter = LOCUST_FLYOFF_TIMEOUT;
			}

			locust->Counter--;

			if (locust->Counter == 0)
			{
				locust->On = false;
				continue;
			}

			if (!(GetRandomControl() & 7))
			{
				locust->Offset.x = (GetRandomControl() & 0x7F) - 64;
				locust->Offset.y = GetRandomControl() % 640 + 128;
				locust->Offset.z = (GetRandomControl() & 0x7F) - 64;
			}

			auto angles = Geometry::GetOrientToPoint(
				locust->Pose.Position.ToVector3(),
				Vector3(
					targetItem->Pose.Position.x + locust->Offset.x * 8,
					targetItem->Pose.Position.y - locust->Offset.y,
					targetItem->Pose.Position.z + locust->Offset.z * 8
				));

			int distance = Vector3i::Distance(targetItem->Pose.Position, locust->Pose.Position);

			if (distance < closestDist && targetItem->IsLara())
			{
				closestDist = distance;
				closestNumber = i;
			}

			if (distance > BLOCK(1))
				distance = BLOCK(1);
			else if (distance < BLOCK(2.5f))
				distance = BLOCK(2.5f);

			if (locust->Velocity < distance / 8)
				locust->Velocity++;
			else if (locust->Velocity > distance / 8)
				locust->Velocity--;

			if (locust->Counter > LOCUST_FLYOFF_TIMEOUT)
			{
				short velocity = locust->Velocity * 128;

				short xAngle = abs(angles.x - locust->Pose.Orientation.x) / 8;
				short yAngle = abs(angles.y - locust->Pose.Orientation.y) / 8;

				if (xAngle < -velocity)
					xAngle = -velocity;
				else if (xAngle > velocity)
					xAngle = velocity;

				if (yAngle < -velocity)
					yAngle = -velocity;
				else if (yAngle > velocity)
					yAngle = velocity;

				locust->Pose.Orientation.x += xAngle;
				locust->Pose.Orientation.y += yAngle;
			}

			int velocity = locust->Velocity * phd_cos(locust->Pose.Orientation.x);

			locust->Pose.Position.x += velocity * phd_sin(locust->Pose.Orientation.y);
			locust->Pose.Position.y += locust->Velocity * phd_sin(-locust->Pose.Orientation.x);
			locust->Pose.Position.z += velocity * phd_cos(locust->Pose.Orientation.y);
				
			if (!(i % 1) && ItemNearTarget(locust->Pose.Position, targetItem, CLICK(1)))
			{
				TriggerBlood(locust->Pose.Position.x, locust->Pose.Position.y, locust->Pose.Position.z, 2 * GetRandomControl(), 2);
				DoDamage(targetItem, targetItem->IsLara() ? LOCUST_LARA_DAMAGE : LOCUST_ENTITY_DAMAGE);
			}
				
			Matrix translation = Matrix::CreateTranslation(locust->Pose.Position.x, locust->Pose.Position.y, locust->Pose.Position.z);
			Matrix rotation = locust->Pose.Orientation.ToRotationMatrix();
			locust->Transform = rotation * translation;
		}

		if (closestNumber != NO_VALUE)
			SoundEffect(SFX_TR4_LOCUSTS_LOOP, &Locusts[closestNumber].Pose);
	}

	void ClearLocusts()
	{
		for (int i = 0; i < MAX_LOCUSTS; i++)
			Locusts[i].On = false;
	}
}
