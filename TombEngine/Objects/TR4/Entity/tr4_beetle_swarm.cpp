#include "framework.h"
#include "Objects/TR4/Entity/tr4_beetle_swarm.h"

#include "Game/collision/collide_room.h"
#include "Game/control/flipeffect.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::TR4
{
	BeetleData BeetleSwarm[NUM_BEETLES];
	int NextBeetle;

	void InitializeBeetleSwarm(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		item->ItemFlags[0] = (item->TriggerFlags / 1000) & 1;
		item->ItemFlags[1] = (item->TriggerFlags / 1000) & 2;
		item->ItemFlags[2] = (item->TriggerFlags / 1000) & 4;

		item->TriggerFlags = item->TriggerFlags % 1000;

		if (!item->ItemFlags[1])
		{
			if (item->Pose.Orientation.y <= ANGLE(22.5f) ||
				item->Pose.Orientation.y >= ANGLE(157.5f))
			{
				if (!(item->Pose.Orientation.y >= -ANGLE(22.5f) ||
					item->Pose.Orientation.y <= -ANGLE(157.5f)))
				{
					item->Pose.Position.x += CLICK(2);
				}
			}
			else
				item->Pose.Position.x -= CLICK(2);

			if (item->Pose.Orientation.y <= -ANGLE(45.0f) ||
				item->Pose.Orientation.y >= ANGLE(45.0f))
			{
				if (item->Pose.Orientation.y < -ANGLE(112.5f) ||
					item->Pose.Orientation.y > ANGLE(112.5f))
				{
					item->Pose.Position.z += CLICK(2);
				}
			}
			else
				item->Pose.Position.z -= CLICK(2);
		}
	}

	void BeetleSwarmControl(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (item->TriggerFlags)
		{
			if (!item->ItemFlags[2] || Random::TestProbability(0.06f))
			{
				item->TriggerFlags--;
				if (item->ItemFlags[2])
				{
					if (Random::TestProbability(1 / 2.0f))
						item->ItemFlags[2]--;
				}

				short beetleNumber = GetFreeBeetle();
				if (beetleNumber != NO_VALUE)
				{
					auto* beetle = &BeetleSwarm[beetleNumber];

					beetle->Pose.Position = item->Pose.Position;
					beetle->RoomNumber = item->RoomNumber;

					if (item->ItemFlags[0])
					{
						beetle->Pose.Orientation.y = GetRandomControl() * 2;
						beetle->VerticalVelocity = -16 - (GetRandomControl() & 0x1F);
					}
					else
					{
						beetle->Pose.Orientation.y = item->Pose.Orientation.y + (GetRandomControl() & 0x3FFF) - ANGLE(45.0f);
						beetle->VerticalVelocity = 0;
					}

					beetle->Pose.Orientation.x = 0;
					beetle->Pose.Orientation.z = 0;
					beetle->On = true;
					beetle->Velocity = (GetRandomControl() & 0x1F) + 1;
					beetle->Flags = 0;
				}
			}
		}
	}

	void ClearBeetleSwarm()
	{
		if (Objects[ID_LITTLE_BEETLE].loaded)
		{
			ZeroMemory(BeetleSwarm, NUM_BEETLES * sizeof(BeetleData));
			NextBeetle = 0;
			FlipEffect = NO_VALUE;
		}
	}

	short GetFreeBeetle()
	{
		auto* beetle = &BeetleSwarm[NextBeetle];
		short result = NextBeetle;

		int i = 0;
		while (beetle->On)
		{
			if (result == NUM_BEETLES - 1)
			{
				beetle = &BeetleSwarm[0];
				result = 0;
			}
			else
			{
				result++;
				beetle++;
			}

			if (++i >= NUM_BEETLES)
				return NO_VALUE;
		}

		NextBeetle = (result + 1) & (NUM_BEETLES - 1);
		return result;
	}

	void UpdateBeetleSwarm()
	{
		for (int i = 0; i < NUM_BEETLES; i++)
		{
			auto* beetle = &BeetleSwarm[i];

			if (beetle->On)
			{
				beetle->StoreInterpolationData();

				auto oldPos = beetle->Pose.Position;

				beetle->Pose.Position.x += beetle->Velocity * phd_sin(beetle->Pose.Orientation.y);
				beetle->Pose.Position.y += beetle->VerticalVelocity;
				beetle->Pose.Position.z += beetle->Velocity * phd_cos(beetle->Pose.Orientation.y);

				beetle->VerticalVelocity += g_GameFlow->GetSettings()->Physics.Gravity;

				int dx = LaraItem->Pose.Position.x - beetle->Pose.Position.x;
				int dy = LaraItem->Pose.Position.y - beetle->Pose.Position.y;
				int dz = LaraItem->Pose.Position.z - beetle->Pose.Position.z;

				short angle = phd_atan(dz, dx) - beetle->Pose.Orientation.y;

				if (abs(dx) < 85 &&
					abs(dy) < 85 &&
					abs(dz) < 85)
				{
					LaraItem->HitPoints--;
					LaraItem->HitStatus = true;
				}

				if (beetle->Flags)
				{
					if (abs(dx) + abs(dz) <= BLOCK(1))
					{
						if (beetle->Velocity & 1)
							beetle->Pose.Orientation.y += ANGLE(2.8f);
						else
							beetle->Pose.Orientation.y -= ANGLE(2.8f);

						beetle->Velocity = 48 - Lara.Torch.IsLit * 64 - (abs(angle) / 128);
						if (beetle->Velocity < -16)
							beetle->Velocity = i & 0xF;
					}
					else
					{
						if (beetle->Velocity < (i & 0x1F) + 24)
							beetle->Velocity++;
						
						if (abs(angle) >= ANGLE(22.5f))
						{
							if (angle >= 0)
								beetle->Pose.Orientation.y += ANGLE(5.6f);
							else
								beetle->Pose.Orientation.y -= ANGLE(5.6f);
						}
						else
							beetle->Pose.Orientation.y += 8 * (Wibble - i);
					}
				}

				FloorInfo* floor = GetFloor(beetle->Pose.Position.x, beetle->Pose.Position.y, beetle->Pose.Position.z, &beetle->RoomNumber);
				int height = GetFloorHeight(floor, beetle->Pose.Position.x, beetle->Pose.Position.y, beetle->Pose.Position.z);
				if (height < (beetle->Pose.Position.y - BLOCK(1.25f)) || height == NO_HEIGHT)
				{
					// Beetle has hit a wall a high step.
					if (angle <= 0)
						beetle->Pose.Orientation.y -= ANGLE(90.0f);
					else
						beetle->Pose.Orientation.y += ANGLE(90.0f);

					beetle->Pose.Position = oldPos;
					beetle->Pose.Orientation.x = 0;
					beetle->Pose.Orientation.z = 0;
					beetle->VerticalVelocity = 0;
				}
				else
				{
					// Beetle is below the floor.
					if (beetle->Pose.Position.y > height)
					{
						beetle->Pose.Position.y = height;
						beetle->Pose.Orientation.x = 0;
						beetle->Pose.Orientation.z = 0;
						beetle->VerticalVelocity = 0;
						beetle->Flags = 1;
					}
				}

				if (beetle->VerticalVelocity >= 500)
				{
					NextBeetle = 0;
					beetle->On = false;
				}
				else
					beetle->Pose.Orientation.x = beetle->VerticalVelocity * -64;

				Matrix translation = Matrix::CreateTranslation(beetle->Pose.Position.x, beetle->Pose.Position.y, beetle->Pose.Position.z);
				Matrix rotation = beetle->Pose.Orientation.ToRotationMatrix();
				beetle->Transform = rotation * translation;
			}
		}
	}
}
