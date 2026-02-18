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
    // Wrecking Ball state.
    // ---------------------------------------------------------------------

    struct WreckingBallState
    {
        enum class Phase
        {
            IdleAtTop,
            MovingHorizontal,
            PreparingDrop,
            Falling,
            WinchingUp
        };

        Phase PhaseState = Phase::IdleAtTop;

        int   MoveAxis = 0;   // 0 = none, 1 = X, 2 = Z
        int   Timer = 0;   // Reserved / wander timer if needed
        int   DropDelay = 0;   // NEW: delay before opening / dropping

        short BaseObject = -1;
        short ChainObject = -1;

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

        // Find anchor object (WRECKINGBALL_ANCHOR)
        auto anchors = FindAllItems(ID_WRECKINGBALL_ANCHOR);
        if (!anchors.empty())
            state.BaseObject = anchors[0];

        // Find chain object (WRECKINGBALL_CHAIN)
        auto chains = FindAllItems(ID_WRECKINGBALL_CHAIN);
        if (!chains.empty())
            state.ChainObject = chains[0];

        // Validate both objects exist
        if (state.BaseObject < 0 || state.ChainObject < 0)
        {
            TENLog(
                "WreckingBall ERROR: Missing required objects. "
                "Expected Anchor ID=" + std::to_string(ID_WRECKINGBALL_ANCHOR) +
                " Chain ID=" + std::to_string(ID_WRECKINGBALL_CHAIN) +
                " | Found Anchor ItemIndex=" + std::to_string(state.BaseObject) +
                " Chain ItemIndex=" + std::to_string(state.ChainObject),
                LogLevel::Error);

            item.Flags |= IFLAG_INVISIBLE;
            item.Status = ITEM_NOT_ACTIVE;
            return;
        }

        // Position ball below ceiling as before.
        item.Pose.Position.y = pointColl.GetCeilingHeight() + 1644;

        if (pointColl.GetRoomNumber() != item.RoomNumber)
            ItemNewRoom(itemNumber, pointColl.GetRoomNumber());

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

    static void UpdateChain(ItemInfo& item, WreckingBallState& state)
    {
        if (state.ChainObject < 0 || state.BaseObject < 0)
            return;

        auto& chain = g_Level.Items[state.ChainObject];
        auto& anchor = g_Level.Items[state.BaseObject];

        // 1. Chain follows anchor horizontally.
        chain.Pose.Position.x = anchor.Pose.Position.x;
        chain.Pose.Position.z = anchor.Pose.Position.z;

        // 2. Chain pivot stays exactly at anchor height.
        chain.Pose.Position.y = anchor.Pose.Position.y;

        // 3. Compute vertical distance to wrecking ball, with a TEST offset.
        constexpr float TEST_OFFSET_Y = 1000.0f; // makes chain effectively shorter / higher
        float distance = (float)item.Pose.Position.y - (float)anchor.Pose.Position.y - TEST_OFFSET_Y;
        if (distance < 0.0f)
            distance = 0.0f;

        // 4. Chain mesh length.
        constexpr float CHAIN_LENGTH = 3500.0f;

        float scaleY = distance / CHAIN_LENGTH;
        if (scaleY < 0.1f)
            scaleY = 0.1f;

        chain.Pose.Scale.y = scaleY;

        // 5. Room update.
        short room = chain.RoomNumber;
        GetFloor(chain.Pose.Position.x, chain.Pose.Position.y, chain.Pose.Position.z, &room);
        if (room != chain.RoomNumber)
            ItemNewRoom(state.ChainObject, room);
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

            // TR5: item_flags[1] = 1; trigger_flags = 30;
            state.PhaseState = WreckingBallState::Phase::PreparingDrop;
            state.DropDelay = 30; // ~0.5s at 60fps, tweak if needed
            state.MoveAxis = 0;
        }
    }

    static void UpdatePreparingDrop(ItemInfo& item, WreckingBallState& state)
    {
        // TR5: if (trigger_flags) trigger_flags--;
        if (state.DropDelay > 0)
        {
            state.DropDelay--;
            return;
        }

        // TR5: if (!current_anim_state) goal_anim_state = 1;
        if (item.Animation.ActiveState == 0)
        {
            item.Animation.TargetState = 1; // open (Anim 1)
            return;
        }

        // TR5: else if (frame_number == anims[anim_number].frame_end) { drop... }
        if (TestLastFrame(item))
        {
            SoundEffect(SFX_TR5_BASE_CLAW_DROP, &item.Pose);

            state.PhaseState = WreckingBallState::Phase::Falling;

            // TR5: fallspeed = 6; pos.y += fallspeed;
            item.Animation.Velocity.y = 6.0f;
            item.Pose.Position.y += item.Animation.Velocity.y;
        }
    }

    static void UpdateFalling(ItemInfo& item, WreckingBallState& state)
    {
        item.Animation.Velocity.y += 24.0f;
        item.Pose.Position.y += item.Animation.Velocity.y;

        short room = item.RoomNumber;
        int height = GetFloorHeight(
            GetFloor(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, &room),
            item.Pose.Position.x,
            item.Pose.Position.y,
            item.Pose.Position.z);

        // TR5: if (c - pos.y_pos < 1536 && current_anim_state) goal_anim_state = 0;
        if (height - item.Pose.Position.y < 1536 && item.Animation.ActiveState != 0)
        {
            item.Animation.TargetState = 0; // close (Anim 2) before impact
        }

        if (height < item.Pose.Position.y)
        {
            item.Pose.Position.y = height;

            if (item.Animation.Velocity.y > 48.0f)
            {
                BounceCamera(&item, 64, 8192);
                item.Animation.Velocity.y = -item.Animation.Velocity.y / 8.0f;
            }
            else
            {
                item.Animation.Velocity.y = 0.0f;
                state.PhaseState = WreckingBallState::Phase::WinchingUp;
            }
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
        UpdateChain(item, state);
        AnimateItem(item);
    }
}
