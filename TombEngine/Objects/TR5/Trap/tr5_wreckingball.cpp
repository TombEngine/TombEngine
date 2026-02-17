#include "framework.h"
#include "Objects/TR5/Trap/tr5_wreckingball.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/room.h"
#include "Objects/TR5/Light/tr5_light.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

#include <unordered_map>

using namespace TEN::Animation;
using namespace TEN::Effects::Environment;

namespace TEN::Entities::Traps
{
    // ---------------------------------------------------------------------
    // Per?wrecking?ball state.
    // ---------------------------------------------------------------------

    struct WreckingBallState
    {
        enum class Phase
        {
            IdleAtTop,        // Previously -1
            MovingHorizontal, // Previously 0
            PreparingDrop,    // Previously 1
            Falling,          // Previously 2
            WinchingUp        // Previously 3
        };

        Phase PhaseState = Phase::IdleAtTop;

        int  MoveAxis = 0;      // 0 = none, 1 = X, 2 = Z
        int  Timer = 0;      // Reserved for future use
        short BaseObject = -1;    // Anchor object ID (ANIMATING16)

        int TargetX = 0;
        int TargetZ = 0;
    };

    static std::unordered_map<short, WreckingBallState> WreckingBallStates;

    // ---------------------------------------------------------------------
    // Initialization
    // ---------------------------------------------------------------------

    void InitializeWreckingBall(short itemNumber)
    {
        auto& item = g_Level.Items[itemNumber];
        auto& state = WreckingBallStates[itemNumber];

        auto pointColl = GetPointCollision(item);

        // Find anchor object (ANIMATING16) – keep same behaviour as old code.
        auto anchors = FindAllItems(ID_ANIMATING16);
        if (!anchors.empty())
            state.BaseObject = anchors[0];

        // Position ball below ceiling as before.
        item.Pose.Position.y = pointColl.GetCeilingHeight() + 1644;

        if (pointColl.GetRoomNumber() != item.RoomNumber)
            ItemNewRoom(itemNumber, pointColl.GetRoomNumber());

        // Initial target is current position.
        state.TargetX = item.Pose.Position.x;
        state.TargetZ = item.Pose.Position.z;
    }

    // ---------------------------------------------------------------------
    // Collision (kept close to original)
    // ---------------------------------------------------------------------

    void CollideWreckingBall(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
    {
        auto& item = g_Level.Items[itemNumber];

        if (TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
        {
            auto prevPos = playerItem->Pose.Position;

            bool killZone = false;
            if ((prevPos.x & WALL_MASK) > CLICK(1) &&
                (prevPos.x & WALL_MASK) < CLICK(3) &&
                (prevPos.z & WALL_MASK) > CLICK(1) &&
                (prevPos.z & WALL_MASK) < CLICK(3))
            {
                killZone = true;
            }

            int damage = (item.Animation.Velocity.y > 0.0f) ? 96 : 0;

            if (ItemPushItem(&item, playerItem, coll, coll->Setup.EnableSpasm, 1))
            {
                if (killZone)
                    DoDamage(playerItem, INT_MAX);
                else
                    DoDamage(playerItem, damage);

                prevPos -= playerItem->Pose.Position;

                if (damage != 0)
                {
                    for (int i = 14 + (GetRandomControl() & 3); i > 0; --i)
                    {
                        TriggerBlood(
                            playerItem->Pose.Position.x + (GetRandomControl() & 63) - 32,
                            playerItem->Pose.Position.y - (GetRandomControl() & 511) - 256,
                            playerItem->Pose.Position.z + (GetRandomControl() & 63) - 32,
                            -1,
                            1);
                    }
                }

                if (!coll->Setup.EnableObjectPush || killZone)
                    playerItem->Pose.Position += prevPos;
            }
        }
    }

    // ---------------------------------------------------------------------
    // Behaviour helpers
    // ---------------------------------------------------------------------

    static void UpdateAnchor(ItemInfo& item, WreckingBallState& state)
    {
        if (state.BaseObject < 0)
            return;

        auto& anchor = g_Level.Items[state.BaseObject];

        anchor.Pose.Position.x = item.Pose.Position.x;
        anchor.Pose.Position.z = item.Pose.Position.z;

        short room = anchor.RoomNumber;
        anchor.Pose.Position.y = GetCeiling(
            GetFloor(anchor.Pose.Position.x, anchor.Pose.Position.y, anchor.Pose.Position.z, &room),
            anchor.Pose.Position.x,
            anchor.Pose.Position.y,
            anchor.Pose.Position.z);

        if (room != anchor.RoomNumber)
            ItemNewRoom(state.BaseObject, room);

        TriggerAlertLight(
            anchor.Pose.Position.x,
            anchor.Pose.Position.y + 64,
            anchor.Pose.Position.z,
            255, 64, 0,
            64 * (GlobalCounter & 0x3F),
            anchor.RoomNumber,
            24);

        TriggerAlertLight(
            anchor.Pose.Position.x,
            anchor.Pose.Position.y + 64,
            anchor.Pose.Position.z,
            255, 64, 0,
            64 * ((GlobalCounter - 32) & 0x3F),
            anchor.RoomNumber,
            24);
    }

    static void UpdateIdle(ItemInfo& item, WreckingBallState& state)
    {
        // Simple behaviour: when idle, always target Lara's current position.
        state.TargetX = LaraItem->Pose.Position.x;
        state.TargetZ = LaraItem->Pose.Position.z;
        state.PhaseState = WreckingBallState::Phase::MovingHorizontal;
        state.MoveAxis = 0;
    }

    static void UpdateHorizontalMovement(ItemInfo& item, WreckingBallState& state)
    {
        constexpr int Speed = 32;

        // Snap target to grid like original.
        int targetX = (state.TargetX & ~0x3FF) | 512;
        int targetZ = (state.TargetZ & ~0x3FF) | 512;

        int dx = targetX - item.Pose.Position.x;
        int dz = targetZ - item.Pose.Position.z;

        // Decide axis if not chosen.
        if (state.MoveAxis == 0)
        {
            if (abs(dx) > abs(dz))
                state.MoveAxis = 1;
            else
                state.MoveAxis = 2;
        }

        if (state.MoveAxis == 1)
        {
            int step = std::clamp(dx, -Speed, Speed);
            if (step != 0)
            {
                item.Pose.Position.x += step;
                SoundEffect(SFX_TR5_BASE_CLAW_MOTOR_B_LOOP, &item.Pose);
            }
            else
            {
                state.MoveAxis = 0;
            }
        }
        else if (state.MoveAxis == 2)
        {
            int step = std::clamp(dz, -Speed, Speed);
            if (step != 0)
            {
                item.Pose.Position.z += step;
                SoundEffect(SFX_TR5_BASE_CLAW_MOTOR_B_LOOP, &item.Pose);
            }
            else
            {
                state.MoveAxis = 0;
            }
        }

        // Reached target (close enough)?
        if (abs(dx) <= Speed && abs(dz) <= Speed)
        {
            StopSoundEffect(SFX_TR5_BASE_CLAW_MOTOR_B_LOOP);
            SoundEffect(SFX_TR5_BASE_CLAW_MOTOR_C, &item.Pose);

            state.PhaseState = WreckingBallState::Phase::PreparingDrop;
            state.MoveAxis = 0;
        }
    }

    static void UpdatePreparingDrop(ItemInfo& item, WreckingBallState& state)
    {
        if (!item.Animation.ActiveState)
            item.Animation.TargetState = 1;

        if (TestLastFrame(item))
        {
            SoundEffect(SFX_TR5_BASE_CLAW_DROP, &item.Pose);
            state.PhaseState = WreckingBallState::Phase::Falling;
            item.Animation.Velocity.y = g_GameFlow->GetSettings()->Physics.Gravity;
            item.Pose.Position.y += item.Animation.Velocity.y;
        }
    }

    static void UpdateFalling(ItemInfo& item, WreckingBallState& state)
    {
        item.Animation.Velocity.y += 24;
        item.Pose.Position.y += item.Animation.Velocity.y;

        short room = item.RoomNumber;
        int height = GetFloorHeight(
            GetFloor(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, &room),
            item.Pose.Position.x,
            item.Pose.Position.y,
            item.Pose.Position.z);

        if (height < item.Pose.Position.y)
        {
            item.Pose.Position.y = height;

            if (item.Animation.Velocity.y > 48)
            {
                BounceCamera(&item, 64, 8192);
                item.Animation.Velocity.y = -item.Animation.Velocity.y / 8.0f;
            }
            else
            {
                item.Animation.Velocity.y = 0;
                state.PhaseState = WreckingBallState::Phase::WinchingUp;
            }
        }
        else if (height - item.Pose.Position.y < 1536 && item.Animation.ActiveState)
        {
            item.Animation.TargetState = 0;
        }
    }

    static void UpdateWinchUp(ItemInfo& item, WreckingBallState& state)
    {
        if (state.BaseObject < 0)
        {
            state.PhaseState = WreckingBallState::Phase::IdleAtTop;
            item.Animation.Velocity.y = 0;
            return;
        }

        auto& anchor = g_Level.Items[state.BaseObject];
        int targetY = anchor.Pose.Position.y + 1644;

        item.Animation.Velocity.y -= 3;
        item.Pose.Position.y += item.Animation.Velocity.y;

        if (item.Pose.Position.y < targetY)
        {
            StopSoundEffect(SFX_TR5_BASE_CLAW_WINCH_UP_LOOP);
            item.Pose.Position.y = targetY;

            if (item.Animation.Velocity.y < -32.0f)
            {
                SoundEffect(SFX_TR5_BASE_CLAW_TOP_IMPACT, &item.Pose, SoundEnvironment::Land, 1.0f, 0.5f);
                item.Animation.Velocity.y = -item.Animation.Velocity.y / 8.0f;
                BounceCamera(&item, 16, 8192);
            }
            else
            {
                item.Animation.Velocity.y = 0;
                state.PhaseState = WreckingBallState::Phase::IdleAtTop;
            }
        }
        else
        {
            SoundEffect(SFX_TR5_BASE_CLAW_WINCH_UP_LOOP, &item.Pose);
        }
    }

    // ---------------------------------------------------------------------
    // Main control
    // ---------------------------------------------------------------------

    void ControlWreckingBall(short itemNumber)
    {
        auto& item = g_Level.Items[itemNumber];
        auto& state = WreckingBallStates[itemNumber];

        // Keep ball in correct room.
        short room = item.RoomNumber;
        GetFloor(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, &room);
        if (room != item.RoomNumber)
            ItemNewRoom(itemNumber, room);

        switch (state.PhaseState)
        {
        case WreckingBallState::Phase::IdleAtTop:
            UpdateIdle(item, state);
            break;

        case WreckingBallState::Phase::MovingHorizontal:
            UpdateHorizontalMovement(item, state);
            break;

        case WreckingBallState::Phase::PreparingDrop:
            UpdatePreparingDrop(item, state);
            break;

        case WreckingBallState::Phase::Falling:
            UpdateFalling(item, state);
            break;

        case WreckingBallState::Phase::WinchingUp:
            UpdateWinchUp(item, state);
            break;
        }

        UpdateAnchor(item, state);
        AnimateItem(item);
    }
}
