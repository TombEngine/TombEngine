#include "framework.h"
#include "Objects/TR5/Emitter/Waterfall.h"

#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/los.h"
#include "Game/effects/effects.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Utils/object_helper.h"
#include "Specific/clock.h"

using namespace TEN::Collision::Point;
using namespace TEN::Math;

constexpr int WATERFALL_SPRITE_SIZE = 62;

// NOTES
// item.TriggetFlags: Waterfall width. 1 unit = BLOCK(1 / 8.0f).

namespace TEN::Effects::WaterfallEmitter
{
    enum WaterfallItemFlags
    {
        Velocity,
        WaterfallSpriteScale,
        Sparseness,
        MistSpriteScale,
        Sound
    };

    constexpr auto WATERFALL_LIFE_MAX = 100;
    constexpr auto WATERFALL_SPLASH_SPRITE_ID = 0;
    constexpr auto WATERFALL_STREAM_1_SPRITE_ID = 1;
    constexpr auto WATERFALL_STREAM_2_SPRITE_ID = 2;

    void InitializeWaterfall(short itemNumber)
    {
        auto& item = g_Level.Items[itemNumber];

        // Customize x and z vel.
        item.ItemFlags[WaterfallItemFlags::Velocity] = 100;

        // Customize waterfall sprite scale.
        item.ItemFlags[WaterfallItemFlags::WaterfallSpriteScale] = 3;

        // Customize density.
        item.ItemFlags[WaterfallItemFlags::Sparseness] = 120;

        // Customize waterfallmist sprite scale.
        item.ItemFlags[WaterfallItemFlags::MistSpriteScale] = 3;

        // Customize waterfall sound.   0 = ON, 1 = OFF.
        item.ItemFlags[WaterfallItemFlags::Sound] = 0;
    }

    void ControlWaterfall(short itemNumber)
    {
        auto& item = g_Level.Items[itemNumber];

        if (!TriggerActive(&item))
            return;

        float customVel = item.ItemFlags[WaterfallItemFlags::Velocity] / 256.0f;

        short scale = item.ItemFlags[WaterfallItemFlags::WaterfallSpriteScale];
        scale = Random::GenerateInt(scale, scale + 2);

        float density = item.TriggerFlags < 5 ? std::clamp(int(item.ItemFlags[WaterfallItemFlags::Sparseness]), 10, 256) : std::clamp(int(item.ItemFlags[WaterfallItemFlags::Sparseness]), 80, 256);
        density = density / 256.0f;

        if (!item.ItemFlags[WaterfallItemFlags::Sound])
            SoundEffect(SFX_TR4_WATERFALL_LOOP, &item.Pose);

        float waterfallWidth = std::max(CLICK(float(item.TriggerFlags)), CLICK(0.1f));
        auto vel = item.Pose.Orientation.ToDirection() * BLOCK(customVel);

        auto startColor = (item.Model.Color / 4) * SCHAR_MAX;
        auto endColor = (item.Model.Color / 8) * UCHAR_MAX;

        // Spawn particles.
        unsigned int partCount = (int)round(waterfallWidth / BLOCK(density));
        for (int i = 0; i < partCount; i++)
        {
            auto& part = *GetFreeParticle();

            auto rotMatrix = item.Pose.Orientation.ToRotationMatrix();
            auto relOffset = Vector3(Random::GenerateFloat(-waterfallWidth / 2.0f, waterfallWidth / 2.0f), 0.0f, 0.0f);
            auto offset = Vector3::Transform(relOffset, rotMatrix);
            auto pos = item.Pose.Position.ToVector3() + offset;

            vel.y = Random::GenerateFloat(0.0f, 16.0f);

            part.on = true;
            part.SpriteSeqID = ID_WATERFALL_SPRITES;
            part.SpriteID = Random::TestProbability(1 / 2.2f) ? WATERFALL_STREAM_2_SPRITE_ID : WATERFALL_SPLASH_SPRITE_ID;
            part.x = pos.x;
            part.y = pos.y;
            part.z = pos.z;
            part.roomNumber = item.RoomNumber;
            part.xVel = vel.x;
            part.yVel = vel.y;
            part.zVel = vel.z;
            part.friction = -2;
            part.fxObj = itemNumber;
            part.gravity = 120;

            // Calculate target position.
            Vector3 targetPos = pos;
            Vector3 velocity = vel;
            int gravity = 240;
            int maxYvel = 0;
            int friction = part.friction;
            int yVel = velocity.y;
            const int stepSize = 20;

            while (true)
            {
                yVel += gravity;
                if (maxYvel && yVel > maxYvel)
                    yVel = maxYvel;

                if (friction & 0xF)
                {
                    velocity.x -= static_cast<int>(velocity.x) >> (friction & 0xF);
                    velocity.z -= static_cast<int>(velocity.z) >> (friction & 0xF);
                }

                if (friction & 0xF0)
                    yVel -= yVel >> (friction >> 4);

                targetPos.x += velocity.x / (84 / stepSize);
                targetPos.y += yVel;
                targetPos.z += velocity.z / (84 / stepSize);

                auto pointColl = GetPointCollision(targetPos, item.RoomNumber);

                auto originPoint = GameVector(pos, item.RoomNumber);
                auto target = GameVector(targetPos, pointColl.GetRoomNumber());

                if (TestEnvironment(ENV_FLAG_WATER, Vector3i(targetPos.x, targetPos.y, targetPos.z), part.roomNumber) ||
                    TestEnvironment(ENV_FLAG_SWAMP, Vector3i(targetPos.x, targetPos.y, targetPos.z), part.roomNumber))
                {
                    targetPos.y = GetPointCollision(Vector3i(targetPos.x, targetPos.y, targetPos.z), part.roomNumber).GetWaterSurfaceHeight();
                    break;
                }

                else if (!LOS(&originPoint, &target))
                {
                    if (pointColl.GetRoomNumber() == NO_VALUE || pointColl.GetSector().IsWall(targetPos.x, targetPos.z))
                    {
                        targetPos.y -= (yVel / 2.7f);
                        break;
                    }
                    else
                    {
                        targetPos.y = pointColl.GetFloorHeight();
                        break;
                    }
                }
            }

            part.targetPos = targetPos;

            char colorOffset = Random::GenerateInt(-8, 8);

            part.sR = std::clamp((int)startColor.x + colorOffset, 0, UCHAR_MAX);
            part.sG = std::clamp((int)startColor.y + colorOffset, 0, UCHAR_MAX);
            part.sB = std::clamp((int)startColor.z + colorOffset, 0, UCHAR_MAX);
            part.dR = std::clamp((int)endColor.x + colorOffset, 0, UCHAR_MAX);
            part.dG = std::clamp((int)endColor.y + colorOffset, 0, UCHAR_MAX);
            part.dB = std::clamp((int)endColor.z + colorOffset, 0, UCHAR_MAX);

            part.roomNumber = part.roomNumber;
            part.colFadeSpeed = 2;
            part.blendMode = BlendMode::Additive;
            part.life = part.sLife = WATERFALL_LIFE_MAX;
            part.fadeToBlack = 0;
            part.rotAng = Random::GenerateAngle();
            part.rotAdd = Random::GenerateAngle(ANGLE(-0.1f), ANGLE(0.1f));
            part.scalar = scale;
            part.maxYvel = 0;
            part.sSize = part.size = (item.TriggerFlags < 10 ? Random::GenerateFloat(40.0f, 51.0f) : Random::GenerateFloat(49.0f, 87.0f)) / 2;
            part.dSize = item.TriggerFlags < 10 ? Random::GenerateFloat(40.0f, 51.0f) : Random::GenerateFloat(98.0f, 174.0f);
            part.flags = SP_SCALE | SP_DEF | SP_ROTATE;
        }
    }

    void SpawnWaterfallMist(const Vector3& pos, int roomNumber, float scalar, float size, const Color& color)
    {
        auto& part = *GetFreeParticle();

        auto colorOffset = Vector3i(40.0f, 40.0f, 40.0f);

        auto startColor = (Vector3i(color.x, color.y, color.z) + colorOffset);
        auto endColor = (Vector3i(color.x, color.y, color.z) + colorOffset);

        part.on = true;

        part.SpriteSeqID = ID_WATERFALL_SPRITES;
        part.SpriteID = Random::TestProbability(1 / 2.0f) ? WATERFALL_STREAM_2_SPRITE_ID : WATERFALL_STREAM_1_SPRITE_ID;

        part.StoreInterpolationData();

        part.PrevX = pos.x;
        part.PrevY = pos.y;
        part.PrevZ = pos.z;
        part.PrevScalar = scalar;

        part.x = pos.x;
        part.y = Random::GenerateInt(-16, 0) + pos.y;
        part.z = pos.z;

        part.roomNumber = roomNumber;

        int colorVariation = (Random::GenerateInt(-8, 8));
        part.sR = std::clamp((int)startColor.x + colorVariation, 0, UCHAR_MAX);
        part.sG = std::clamp((int)startColor.y + colorVariation, 0, UCHAR_MAX);
        part.sB = std::clamp((int)startColor.z + colorVariation, 0, UCHAR_MAX);
        part.dR = std::clamp((int)endColor.x + colorVariation, 0, UCHAR_MAX);
        part.dG = std::clamp((int)endColor.y + colorVariation, 0, UCHAR_MAX);
        part.dB = std::clamp((int)endColor.z + colorVariation, 0, UCHAR_MAX);

        part.colFadeSpeed = 1;
        part.blendMode = BlendMode::Additive;
        part.life =
            part.sLife = 8;
        part.fadeToBlack = part.life;

        part.xVel = 0;
        part.yVel = -Random::GenerateInt(63, 64);
        part.zVel = 0;

        part.friction = 3;
        part.rotAng = Random::GenerateAngle();
        part.scalar = scalar;
        part.targetPos = Vector3::Zero;

        part.rotAdd = Random::GenerateInt(-16, 16);
        part.gravity = 0;
        part.maxYvel = 0;

        float size1 = (GetRandomControl() & 8) + size;
        part.size =
            part.sSize = size1 / 4;
        part.dSize = size1;

        part.flags = SP_SCALE | SP_DEF | SP_ROTATE;
    }

    bool HandleWaterfallParticle(Particle& particle)
    {
        if (particle.SpriteSeqID != ID_WATERFALL_SPRITES)
            return false;

        if (particle.targetPos == Vector3::Zero)
            return false;

        if (particle.y < particle.targetPos.y)
            return false;

        particle.targetPos.y = particle.y - 80;
        particle.targetPos.x = particle.x;
        particle.targetPos.z = particle.z;

        if (particle.fxObj != NO_VALUE)
        {
            auto& item = g_Level.Items[particle.fxObj];
            if (Random::TestProbability(1.0f / 2.0f))
                SpawnWaterfallMist(particle.targetPos, particle.roomNumber, item.ItemFlags[3], WATERFALL_SPRITE_SIZE, Color(particle.sR, particle.sG, particle.sB));
        }

        particle.life = 0;
        particle.on = false;
        return true;
    }
}
