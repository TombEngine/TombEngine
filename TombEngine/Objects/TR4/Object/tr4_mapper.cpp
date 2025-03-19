#include "framework.h"
#include "Objects/TR4/Object/tr4_mapper.h"

#include "Specific/level.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Sound/sound.h"
#include "Game/Animation/Animation.h"
#include "Game/Lara/lara.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Renderer/RendererEnums.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;

namespace TEN::Entities::TR4
{
    void InitializeMapper(short itemNumber)
    {
        g_Level.Items[itemNumber].MeshBits = -3;
    }

    void MapperControl(short itemNumber)
    {
        auto* item = &g_Level.Items[itemNumber];

        if (!TriggerActive(item))
            return;

        if (item->Animation.FrameNumber >= 200)
        {
            SoundEffect(SFX_TR4_MAPPER_LASER, &item->Pose);

            item->MeshBits |= 2;

            auto pos =  GetJointPosition(item, 0);
            byte color = (GetRandomControl() & 0x1F) + 192;
            SpawnDynamicLight(pos.x, pos.y, pos.z, (GetRandomControl() & 3) + 16, color, color, 0);

            int height = GetPointCollision(*item).GetFloorHeight();

            for (int i = 0; i < 2; i++)
            {
                auto* spark = GetFreeParticle();

                spark->on = 1;
                spark->sR = (GetRandomControl() & 0x7F) + 64;
                spark->sG = spark->sR - (GetRandomControl() & 0x1F);
                spark->sB = 0;
                spark->dR = (GetRandomControl() & 0x7F) + 64;
                spark->dG = spark->dR - (GetRandomControl() & 0x1F);
                spark->dB = 0;
                spark->colFadeSpeed = 4;
                spark->fadeToBlack = 4;
                spark->blendMode = BlendMode::Additive;
                spark->life = 12;
                spark->sLife = 12;
                spark->x = pos.x;
                spark->y = height;
                spark->z = pos.z;
                spark->xVel = (GetRandomControl() & 0x3FF) - 512;
                spark->yVel = -256 - (GetRandomControl() & 0x7F);
                spark->zVel = (GetRandomControl() & 0x3FF) - 512;
                spark->friction = 4;
                spark->scalar = 2;
                spark->sSize = spark->size = (GetRandomControl() & 0x0F) + 16;
                spark->dSize = (GetRandomControl() & 1) + 3;
                spark->maxYvel = 0;
                spark->gravity = (GetRandomControl() & 0x1F) + 32;
                spark->flags = SP_SCALE | SP_DEF;
            }
        }

        AnimateItem(*item);
    }
}
