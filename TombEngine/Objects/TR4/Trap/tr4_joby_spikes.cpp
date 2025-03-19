#include "framework.h"
#include "Objects/TR4/Trap/tr4_joby_spikes.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;

// TODO: Need to test and adapt formula for scaling to other heights.

namespace TEN::Entities::Traps
{
    constexpr auto JOBY_SPIKES_HARM_DAMAGE          = 8;
    constexpr auto JOBY_SPIKES_EXTRA_ROTATION_SPEED = 2;
    constexpr auto JOBY_SPIKES_SCALE_INCREMENT      = 3;
    constexpr auto JOBY_SPIKES_MAX_SCALE            = BLOCK(3.25f);

    void InitializeJobySpikes(short itemNumber)
    {
        auto& item = g_Level.Items[itemNumber];

        auto& angleRotationSpeed    = item.ItemFlags[0];
        auto& spikeLength           = item.ItemFlags[1];
        auto& clockworkDirection    = item.ItemFlags[2]; // ??
        auto& maxExtensionLength    = item.ItemFlags[3];

		// Set bone mutators to EulerAngles identity by default.
		for (auto& mutator : item.Model.Mutators)
            mutator.Scale.y = 0.0f;

        item.Pose.Orientation.y = Random::GenerateInt(0, INT16_MAX) * ANGLE(5.5f);
        clockworkDirection = Random::GenerateInt(0, 1);

        auto pointColl = GetPointCollision(item);
        maxExtensionLength = (short)(4096 * (pointColl.GetFloorHeight() - pointColl.GetCeilingHeight()) / JOBY_SPIKES_MAX_SCALE);
    }

    void ControlJobySpikes(short itemNumber)
    {
        auto& item = g_Level.Items[itemNumber];

        if (!TriggerActive(&item))
            return;

        auto& angleRotationSpeed    = item.ItemFlags[0];
        auto& spikeLength           = item.ItemFlags[1];
        auto& clockworkDirection    = item.ItemFlags[2];
        auto& maxExtensionLength    = item.ItemFlags[3];

        SoundEffect(SFX_TR4_METAL_SCRAPE_LOOP1, &item.Pose);
        auto frameData = GetFrameInterpData(*LaraItem);

        // Damage player.
        int playerHeight = LaraItem->Pose.Position.y + frameData.Keyframe0.BoundingBox.Y1;
        int spikeHeight = JOBY_SPIKES_MAX_SCALE * spikeLength / 4096;

        if (LaraItem->HitPoints > 0)
        {
            if (item.Pose.Position.y + spikeHeight > playerHeight)
            {
                if (abs(item.Pose.Position.x - LaraItem->Pose.Position.x) < BLOCK(0.5))
                {
                    if (abs(item.Pose.Position.z - LaraItem->Pose.Position.z) < BLOCK(0.5))
                    {
                        DoDamage(LaraItem, JOBY_SPIKES_HARM_DAMAGE);

                        int bloodPosX = LaraItem->Pose.Position.x + Random::GenerateInt(-64, 64);
                        int bloodPosY = playerHeight + Random::GenerateInt( 0, item.Pose.Position.y - playerHeight + spikeHeight);
                        int bloodPosZ = LaraItem->Pose.Position.z + Random::GenerateInt(-64, 64);

                        DoBloodSplat(bloodPosX, bloodPosY, bloodPosZ, Random::GenerateInt (2,5), 2 * GetRandomControl(), item.RoomNumber);
                        
                    }
                }
            }
        }

        // Rotate.
        if (clockworkDirection == 1)
        {
            if (angleRotationSpeed < 4096)
                angleRotationSpeed = (spikeLength / 64) + spikeLength + JOBY_SPIKES_EXTRA_ROTATION_SPEED;
        }
        else
        {
            if (angleRotationSpeed > -4096)
                angleRotationSpeed = spikeLength + (spikeLength / 64) - JOBY_SPIKES_EXTRA_ROTATION_SPEED;
        }

        if (spikeLength < maxExtensionLength)
            spikeLength += JOBY_SPIKES_SCALE_INCREMENT;

        item.Pose.Orientation.y += angleRotationSpeed;

		// Scale.
		if (spikeLength)
		{
            for (auto& mutator : item.Model.Mutators)
				mutator.Scale = Vector3(1.0f, spikeLength / 4096.0f, 1.0f);
		}
    }
}
