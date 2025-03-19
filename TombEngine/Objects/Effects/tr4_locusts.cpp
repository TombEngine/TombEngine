#include "framework.h"
#include "Objects/Effects/tr4_locusts.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Math;

namespace TEN::Entities::TR4 
{
	LOCUST_INFO Locusts[MAX_LOCUSTS];

	int CreateLocust()
	{
		for (int i = 0; i < MAX_LOCUSTS; i++)
		{
			auto* locust = &Locusts[i];
			if (!locust->on)
				return i;
		}

		return NO_VALUE;
	}

	void SpawnLocust(ItemInfo* item)
	{
		Vector3i origin, target;
	   short locustNumber = CreateLocust();
	   EulerAngles orient;
		if (locustNumber != NO_VALUE)
		{
			auto* locust = &Locusts[locustNumber];

			// Emitter.
			if (item->ObjectNumber == ID_LOCUSTS_EMITTER)
			{
				target = item->Pose.Position;
				orient = EulerAngles(0, item->Pose.Orientation.y + ANGLE(180.0f), 0);
			}
			// Mutant.
			else
			{
				origin = GetJointPosition(item, 9, Vector3i(0, -96, 144));
				target = GetJointPosition(item, 9, Vector3i(0, -128, 288));
				orient = Geometry::GetOrientToPoint(origin.ToVector3(), target.ToVector3());
			}

			// NOTE: this is not present in original TR4 code
			//target = GetCreatureInfo(item)->enemy;

			locust->on = true;
			//locust->target = target != nullptr ? target : nullptr;
			locust->pos.Position = target;
			locust->pos.Orientation.x = (GetRandomControl() & 0x3FF) + orient.x - ANGLE(2.8f);
			locust->pos.Orientation.y = (GetRandomControl() & 0x7FF) + orient.y - ANGLE(5.6f);
			locust->roomNumber = item->RoomNumber;
			locust->randomRotation = (GetRandomControl() & 0x1F) + 0x10;
			locust->escapeYrot = GetRandomControl() & 0x1FF;
			locust->escapeXrot = (GetRandomControl() & 0x1F) + 16;
			locust->counter = 20 * ((GetRandomControl() & 0x7) + 0xF);
		}
	}

	void InitializeLocust(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

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

	void LocustControl(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (TriggerActive(item))
		{
			if (item->TriggerFlags)
			{
				SpawnLocust(item);
				item->TriggerFlags--;
			}
			else
				KillItem(itemNumber);
		}
	}

	void UpdateLocusts()
	{
		for (int i = 0; i < MAX_LOCUSTS; i++)
		{
			auto* locust = &Locusts[i];
			if (locust->on)
			{
				locust->StoreInterpolationData();

				// NOTE: not present in original TR4 code
				//if (locust->target == nullptr)
				//    locust->target = LaraItem;
				if ((LaraItem->Effect.Type != EffectType::None || LaraItem->HitPoints <= 0) && locust->counter >= 90 && !(GetRandomControl() & 7))
					locust->counter = 90;

				locust->counter--;
				if (locust->counter == 0)
				{
					locust->on = false;
					break;
				}

				if (!(GetRandomControl() & 7))
				{
					locust->escapeYrot = GetRandomControl() % 640 + 128;
					locust->escapeXrot = (GetRandomControl() & 0x7F) - 64;
					locust->escapeZrot = (GetRandomControl() & 0x7F) - 64;
				}

				auto orient = Geometry::GetOrientToPoint(
					locust->pos.Position.ToVector3(),
					Vector3(
						LaraItem->Pose.Position.x + locust->escapeXrot * 8,
						LaraItem->Pose.Position.y - locust->escapeYrot,
						LaraItem->Pose.Position.z + locust->escapeZrot * 8
					));

				int distance = SQUARE(LaraItem->Pose.Position.z - locust->pos.Position.z) + SQUARE(LaraItem->Pose.Position.x - locust->pos.Position.x);
				int square = int(sqrt(distance)) / 8;
				if (square <= 128)
				{
					if (square < 48)
						square = 48;
				}
				else
					square = 128;

				if (locust->randomRotation < square)
					locust->randomRotation += 1;
				else if (locust->randomRotation > square)
					locust->randomRotation -= 1;

				if (locust->counter > 90)
				{
					short resultYrot, resultXrot;
					int shiftYrot, shiftXrot;
					int random = locust->randomRotation * 128;
					resultYrot = orient.y - locust->pos.Orientation.y;

					if (abs(resultYrot) > ANGLE(180.0f))
						resultYrot = locust->pos.Orientation.y - orient.y;

					resultXrot = orient.x - locust->pos.Orientation.x;

					if (abs(resultXrot) > ANGLE(180.0f))
						resultXrot = locust->pos.Orientation.x - orient.y;

					shiftXrot = resultXrot / 8;
					shiftYrot = resultYrot / 8;

					if (shiftYrot > random || shiftYrot < -random)
						shiftYrot = -random;

					if (shiftXrot > random || shiftXrot < -random)
						shiftXrot = -random;

					locust->pos.Orientation.x += shiftXrot;
					locust->pos.Orientation.y += shiftYrot;
				}

				locust->pos.Position.x += locust->randomRotation * phd_cos(locust->pos.Orientation.x) * phd_sin(locust->pos.Orientation.y);
				locust->pos.Position.y += locust->randomRotation * phd_sin(-locust->pos.Orientation.x);
				locust->pos.Position.z += locust->randomRotation * phd_cos(locust->pos.Orientation.x) * phd_cos(locust->pos.Orientation.y);
				
				if (ItemNearTarget(locust->pos.Position, LaraItem, CLICK(1) / 2))
				{
					TriggerBlood(locust->pos.Position.x, locust->pos.Position.y, locust->pos.Position.z, 2 * GetRandomControl(), 2);
					DoDamage(LaraItem, LOCUST_LARA_DAMAGE);
				}

				if (locust->counter > 0)
					SoundEffect(SFX_TR4_LOCUSTS_LOOP, &locust->pos);
				
				Matrix translation = Matrix::CreateTranslation(locust->pos.Position.x, locust->pos.Position.y, locust->pos.Position.z);
				Matrix rotation = locust->pos.Orientation.ToRotationMatrix();
				locust->Transform = rotation * translation;
			}
		}
	}

	void ClearLocusts()
	{
		for (int i = 0; i < MAX_LOCUSTS; i++)
			Locusts[i].on = false;
	}

	void DrawLocust()
	{
		// TODO: no render for the locusts !
	}
}
