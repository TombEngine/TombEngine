#include "framework.h"
#include "Objects/TR2/Entity/Dragon.h"

#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Point.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/smoke.h"
#include "Game/effects/spark.h"
#include "Game/effects/Decal.h"
#include "Game/collision/Los.h"
#include "Game/Hud/Hud.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/misc.h"
#include "Game/pickup/pickup.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Scripting/Internal/TEN/Properties/PropertyNames.h"
#include "Specific/clock.h"
#include "Specific/Input/Input.h"

using namespace TEN::Collision::Point;
using namespace TEN::Hud;
using namespace TEN::Input;
using namespace TEN::Math;
using namespace TEN::Effects::Smoke;
using namespace TEN::Effects::Spark;
using namespace TEN::Effects::Items;
using namespace TEN::Collision::Los;
using namespace TEN::Effects::Decal;

// NOTES:
// OCB 0: Dragon dies when hitpoints reach 0.
// OCB 1: Dragon dies when player retrieves dagger.
//
// item.ItemFlags[0]: Back segment item number.
// item.ItemFlags[1]: Timer for temporary defeat and death in frame time.

namespace TEN::Entities::Creatures::TR2
{
    // Properties unique to this entity.
    static const auto PropName_FlameStartColor = GetHash("FlameStartColor");
    static const auto PropName_FlameEndColor = GetHash("FlameEndColor");
    static const auto PropName_EmberStartColor = GetHash("EmberStartColor");
    static const auto PropName_EmberEndColor = GetHash("EmberEndColor");
	static const auto PropName_ShockwaveToggle = GetHash("ShockwaveEnabled");
    static const auto PropName_DragonSwipeAttackDamage = GetHash("DragonSwipeAttackDamage");
	static const auto PropName_DragonContactDamage = GetHash("DragonContactDamage");
	static const auto PropName_DragonDeathOption = GetHash("DragonDeathOption");

    auto DragonDaggerBounds = ObjectCollisionBounds
    {
        GameBoundingBox::Zero,
        std::pair(
            EulerAngles(ANGLE(-10.0f), -LARA_GRAB_THRESHOLD + ANGLE(90.0f), ANGLE(-10.0f)),
            EulerAngles(ANGLE(10.0f),  LARA_GRAB_THRESHOLD + ANGLE(90.0f), ANGLE(10.0f)))
    };

    // Logical flame projectile used for LOS-based flame stopping and scorch decals.
    struct DragonFlameProjectile
    {
        Vector3 pos;
        Vector3 vel;
        short roomNumber = 0;
        int life = 0;
        bool blocked = false;
        Vector3 burnStartColor;
        Vector3 burnEndColor;
        short targetItem = NO_VALUE;
        bool hasCustomColor = false;
    };

    // Ember particle logic (physics-only). Visual spark is spawned separately.
    struct DragonEmber
    {
        Vector3 pos;
        Vector3 vel;
        short roomNumber;
        int life;
        ScriptColor startColor;
        ScriptColor endColor;
    };

    auto DragonDaggerPos = Vector3i::Zero;

    constexpr auto DRAGON_SWIPE_ATTACK_DAMAGE = 250;
    constexpr auto DRAGON_CONTACT_DAMAGE = 10;

    constexpr auto DRAGON_NEAR_RANGE = SQUARE(BLOCK(3));
    constexpr auto DRAGON_IDLE_RANGE = SQUARE(BLOCK(6));

    constexpr auto DRAGON_WALK_TURN_RATE_MAX = ANGLE(2.0f);
    constexpr auto DRAGON_TURN_THRESHOLD_ANGLE = ANGLE(1.0f);

    constexpr auto DRAGON_LIVE_TIME = 11 * FPS;
    constexpr auto DRAGON_ALMOST_LIVE = 100;

    const auto DragonMouthBite = CreatureBiteInfo(Vector3(35.0f, 171.0f, 1168.0f), 12);
    const auto DragonBackSpineJoints = std::vector<unsigned int>{ 21, 22, 23 };
    const auto DragonSwipeAttackJointsLeft = std::vector<unsigned int>{ 24, 25, 26, 27, 28, 29, 30 };
    const auto DragonSwipeAttackJointsRight = std::vector<unsigned int>{ 1, 2, 3, 4, 5, 6, 7 };

    // Containers for flame projectiles and ember physics.
    static std::vector<DragonFlameProjectile> FlameProjectiles;
    static std::vector<DragonEmber> DragonEmbers;

    enum class DragonLightEffectType
    {
        Yellow,
        Red
    };

    enum DragonState
    {
        DRAGON_STATE_WALK = 1,
        DRAGON_STATE_MOVE_LEFT = 2,
        DRAGON_STATE_MOVE_RIGHT = 3,
        DRAGON_STATE_AIM_1 = 4,
        DRAGON_STATE_FIRE_1 = 5,
        DRAGON_STATE_IDLE = 6,
        DRAGON_STATE_TURN_LEFT = 7,
        DRAGON_STATE_TURN_RIGHT = 8,
        DRAGON_STATE_SWIPE_ATTACK_LEFT = 9,
        DRAGON_STATE_SWIPE_ATTACK_RIGHT = 10,
        DRAGON_STATE_DEFEAT = 11
    };

    enum DragonAnim
    {
        DRAGON_ANIM_WALK = 0,
        DRAGON_ANIM_WALK_TO_MOVE_LEFT = 1,
        DRAGON_ANIM_MOVE_LEFT = 2,
        DRAGON_ANIM_MOVE_LEFT_TO_WALK = 3,
        DRAGON_ANIM_WALK_TO_MOVE_RIGHT = 4,
        DRAGON_ANIM_MOVE_RIGHT = 5,
        DRAGON_ANIM_MOVE_RIGHT_TO_WALK = 6,
        DRAGON_ANIM_WALK_TO_IDLE = 7,
        DRAGON_ANIM_IDLE = 8,
        DRAGON_ANIM_IDLE_TO_WALK = 9,
        DRAGON_ANIM_IDLE_TO_FIRE = 10,
        DRAGON_ANIM_FIRE = 11,
        DRAGON_ANIM_FIRE_TO_IDLE = 12,
        DRAGON_ANIM_TURNING_LEFT = 13,
        DRAGON_ANIM_TURNING_RIGHT = 14,
        DRAGON_ANIM_ATTACK_LEFT_1 = 15,
        DRAGON_ANIM_ATTACK_LEFT_2 = 16,
        DRAGON_ANIM_ATTACK_LEFT_3 = 17,
        DRAGON_ANIM_ATTACK_RIGHT_1 = 18,
        DRAGON_ANIM_ATTACK_RIGHT_2 = 19,
        DRAGON_ANIM_ATTACK_RIGHT_3 = 20,
        DRAGON_ANIM_DEATH = 21,
        DRAGON_ANIM_DEFEATED = 22,
        DRAGON_ANIM_RECOVER = 23
    };

    enum DragonDeath
    {
        DRAGON_DEATH_NORMAL = 0,
        DRAGON_DEATH_DAGGER = 1
    };

    static void InitializeDragonBones(const ItemInfo& item)
    {
        int frontBoneItemNumber = SpawnItem(item, ID_DRAGON_BONE_FRONT);
        int backBoneItemNumber = SpawnItem(item, ID_DRAGON_BONE_BACK);

        if (backBoneItemNumber == NO_VALUE || frontBoneItemNumber == NO_VALUE)
        {
            TENLog("Failed to create dragon skeleton objects.", LogLevel::Warning);
            return;
        }
    }

    static void InitializeDragonBack(ItemInfo& frontItem)
    {
        int backItemNumber = SpawnItem(frontItem, ID_DRAGON_BACK);

        if (backItemNumber == NO_VALUE)
        {
            TENLog("Failed to create dragon back body segment.", LogLevel::Warning);
            return;
        }

        auto& backItem = g_Level.Items[backItemNumber];
        backItem.Status = ITEM_INVISIBLE;

        backItem.MeshBits.Clear(DragonBackSpineJoints);

        frontItem.ItemFlags[0] = backItemNumber;
        backItem.ItemFlags[0] = NO_VALUE;
    }

    void InitializeDragon(short itemNumber)
    {
        auto& item = g_Level.Items[itemNumber];

        InitializeCreature(item.Index);
        SetAnimation(item, DRAGON_ANIM_IDLE);
        InitializeDragonBack(item);
    }

    static void SyncDragonBackSegment(ItemInfo& frontItem)
    {
        short& backItemNumber = frontItem.ItemFlags[0];
        auto& backItem = g_Level.Items[backItemNumber];

        backItem.Status = frontItem.Status;
        if (backItem.Status == ITEM_DEACTIVATED)
        {
            KillItem(backItem.Index);
            backItemNumber = NO_VALUE;
            return;
        }

        SetAnimation(backItem, frontItem.Animation.AnimNumber, frontItem.Animation.FrameNumber);

        backItem.Pose = frontItem.Pose;
        if (backItem.RoomNumber != frontItem.RoomNumber)
            ItemNewRoom(backItem.Index, frontItem.RoomNumber);
    }

    static void SpawnDragonLightEffect(const ItemInfo& item, DragonLightEffectType type)
    {
        auto pos = item.Pose.Position.ToVector3() + Vector3(0.0f, -CLICK(1), 0.0f);

        switch (type)
        {
        default:
        case DragonLightEffectType::Yellow:
        {
            auto color = Color(
                Random::GenerateFloat(0.8f, 0.9f),
                Random::GenerateFloat(0.4f, 0.5f),
                Random::GenerateFloat(0.2f, 0.3f));
            float falloff = Random::GenerateFloat(BLOCK(6), BLOCK(20));

            SpawnDynamicPointLight(pos, color, falloff);
        }
        break;

        case DragonLightEffectType::Red:
        {
            auto color = Color(
                Random::GenerateFloat(0.8f, 0.9f),
                Random::GenerateFloat(0.2f, 0.3f),
                Random::GenerateFloat(0.0f, 0.1f));
            float falloff = Random::GenerateFloat(BLOCK(6), BLOCK(12));

            SpawnDynamicPointLight(pos, color, falloff);
        }
        break;
        }
    }
    // Creates a logical ember physics object. The visible spark is drawn by UpdateDragonEmbers().
    static void SpawnDragonFlameEmber(const Particle& fire, const Vector3& dir, short roomNumber, const ScriptColor& emberStartColor, const ScriptColor& emberEndColor)
    {
        // Small random offset around the source fire particle.
        auto pos = Vector3(
            fire.x + Random::GenerateFloat(-12.0f, 12.0f),
            fire.y + Random::GenerateFloat(-12.0f, 12.0f),
            fire.z + Random::GenerateFloat(-12.0f, 12.0f));

        // Inherit the fire particle's velocity, plus a random spread.
        float emberSpeed = Random::GenerateFloat(0.8f, 1.2f);
        auto vel = Vector3(
            fire.xVel * emberSpeed + Random::GenerateFloat(-40.0f, 40.0f),
            fire.yVel * emberSpeed + Random::GenerateFloat(-20.0f, 20.0f),
            fire.zVel * emberSpeed + Random::GenerateFloat(-40.0f, 40.0f));

        DragonEmber e;
        e.pos = pos;
        e.vel = vel;
        e.roomNumber = roomNumber;
        e.life = Random::GenerateInt(30, 45);
        e.startColor = emberStartColor;
        e.endColor = emberEndColor;

        DragonEmbers.push_back(e);
    }

    // Main flame attack logic.
    // Performs LOS to determine flame length, spawns flame particles,
    // spawns logical flame projectiles (for scorch decals), and spawns ember sparks.
    static void SpawnDragonFireBreathEffect(const ItemInfo& item, const CreatureBiteInfo& bite, ItemInfo* enemy)
    {
        constexpr auto FIRE_COUNT = 6;
        constexpr auto SPHERE_RADIUS = BLOCK(0.2f);
        constexpr auto FLAME_SPEED = BLOCK(10.0f);
        constexpr auto MAX_RANGE = BLOCK(16.0f);

        Vector3 origin = GetJointPosition(item, bite.BoneID, bite.Position).ToVector3();
        // Aim at the dragon's current target; fall back to Lara if none.
        Vector3 target;
        if (enemy)
        {
            if (enemy->IsLara())
                target = GetJointPosition(enemy, LM_HIPS).ToVector3();
            else
                target = GetJointPosition(enemy, 0).ToVector3();
        }
        else
        {
            target = GetJointPosition(LaraItem, LM_HIPS).ToVector3();
        }

        Vector3 dir = target - origin;
        if (dir.LengthSquared() < 1.0f)
            dir = Vector3(0.0f, 0.0f, 1.0f);

        dir.Normalize();

        float maxDist = MAX_RANGE;
        Vector3 hitPos = origin + dir * maxDist;
        short hitRoom = item.RoomNumber;

        {
            auto roomLos = GetRoomLosCollision(origin, item.RoomNumber, dir, maxDist);
            auto staticLos = GetStaticLosCollision(origin, item.RoomNumber, dir, maxDist, true);

            float roomDist = roomLos.IsIntersected ? roomLos.Distance : FLT_MAX;
            float staticDist = staticLos ? staticLos->Distance : FLT_MAX;

            if (roomDist < staticDist)
            {
                maxDist = roomDist;
                hitPos = roomLos.Position;
                hitRoom = roomLos.RoomNumber;
            }
            else if (staticLos)
            {
                maxDist = staticDist;
                hitPos = staticLos->Position;
                hitRoom = staticLos->RoomNumber;
            }
        }

        float travelTimeSeconds = maxDist / FLAME_SPEED;
        int lifeTicks = int(travelTimeSeconds * FPS);
        lifeTicks = std::max(lifeTicks, 4);

        // Resolve configurable flame colors once per attack, not per particle.
        const auto defaultFlameStartColor = ScriptColor(153, 63, 13);
        const auto defaultFlameEndColor = ScriptColor(89, 38, 5);
        auto flameStartColor = PropertyHandler::Get(item, PropName_FlameStartColor, defaultFlameStartColor);
        auto flameEndColor = PropertyHandler::Get(item, PropName_FlameEndColor, defaultFlameEndColor);
        auto emberStartColor = PropertyHandler::Get(item, PropName_EmberStartColor, ScriptColor(230, 102, 26));
        auto emberEndColor = PropertyHandler::Get(item, PropName_EmberEndColor, ScriptColor(204, 153, 77));

        // A custom burn colour is in use only if the resolved flame colour differs from the default.
        const bool hasCustomColor =
            flameStartColor.GetR() != defaultFlameStartColor.GetR() ||
            flameStartColor.GetG() != defaultFlameStartColor.GetG() ||
            flameStartColor.GetB() != defaultFlameStartColor.GetB() ||
            flameEndColor.GetR() != defaultFlameEndColor.GetR() ||
            flameEndColor.GetG() != defaultFlameEndColor.GetG() ||
            flameEndColor.GetB() != defaultFlameEndColor.GetB();

        for (int i = 0; i < FIRE_COUNT; i++)
        {
            BoundingSphere sphere(origin, SPHERE_RADIUS);
            Vector3 pos = Random::GeneratePointInSphere(sphere);

            DragonFlameProjectile p;
            p.pos = pos;
            p.vel = dir * FLAME_SPEED;
            p.life = lifeTicks;
            p.roomNumber = item.RoomNumber;
            p.targetItem = enemy ? enemy->Index : NO_VALUE;
            p.hasCustomColor = hasCustomColor;
            p.burnStartColor = Vector3(flameStartColor.GetR() / (float)UCHAR_MAX, flameStartColor.GetG() / (float)UCHAR_MAX, flameStartColor.GetB() / (float)UCHAR_MAX);
            p.burnEndColor = Vector3(flameEndColor.GetR() / (float)UCHAR_MAX, flameEndColor.GetG() / (float)UCHAR_MAX, flameEndColor.GetB() / (float)UCHAR_MAX);
            FlameProjectiles.push_back(p);

            auto& fire = *GetFreeParticle();
            fire.on = true;

            fire.x = pos.x;
            fire.y = pos.y;
            fire.z = pos.z;

            fire.xVel = p.vel.x;
            fire.yVel = p.vel.y;
            fire.zVel = p.vel.z;

            fire.animationType = ParticleAnimType::LifetimeSpread;
            fire.framerate = Random::GenerateFloat(0.5f, 1.5f);
            fire.SpriteSeqID = ID_DEFAULT_SPRITES;
            fire.SpriteID = Random::GenerateInt(0, 3);
            
            // Per-particle random variation, reproducing the original pre-custom ranges as fractions of the configured colour.
            fire.sR = Random::GenerateFloat(0.65f, 0.8f) * flameStartColor.GetR();
            fire.sG = Random::GenerateFloat(0.25f, 0.35f) * flameStartColor.GetG();
            fire.sB = Random::GenerateFloat(0.05f, 0.12f) * flameStartColor.GetB();

            fire.dR = Random::GenerateFloat(0.35f, 0.55f) * flameEndColor.GetR();
            fire.dG = Random::GenerateFloat(0.15f, 0.25f) * flameEndColor.GetG();
            fire.dB = Random::GenerateFloat(0.02f, 0.08f) * flameEndColor.GetB();

            fire.colFadeSpeed = 12;
            fire.fadeToBlack = 8;
            fire.blendMode = BlendMode::Additive;

            fire.life = fire.sLife = lifeTicks;

            fire.friction = 5;
            fire.gravity = 0;
            fire.maxYvel = 0;

            fire.flags = SP_SCALE | SP_DEF | SP_ROTATE | SP_EXPDEF | SP_LIGHT | SP_HAZE;

            fire.scalar = 8;
            fire.lightRadius = 4;
            fire.dSize = Random::GenerateFloat(28.0f, 40.0f);
            fire.sSize = fire.dSize * 0.5f;
            fire.size = fire.dSize;

            SpawnDragonFlameEmber(fire, dir, item.RoomNumber, emberStartColor, emberEndColor);
        }
    }

    // Places a scorch decal at the flame impact point.
    static void SpawnDragonScorchDecal(const Vector3& hitPos, short roomNumber)
    {
        auto pointColl = GetPointCollision(hitPos, roomNumber);
        Vector3 pos = hitPos;

        if (pointColl.GetFloorHeight() != NO_HEIGHT)
            pos.y = pointColl.GetFloorHeight() + 4.0f;

        SpawnDecal(pos, roomNumber, DecalType::Explosion);
    }

    // Updates logical flame projectiles.
    static void UpdateDragonFlameProjectiles()
    {
        static int scorchCounter = 0;

        for (auto it = FlameProjectiles.begin(); it != FlameProjectiles.end(); )
        {
            auto& p = *it;

            if (p.life-- <= 0)
            {
                it = FlameProjectiles.erase(it);
                continue;
            }

            Vector3 prev = p.pos;
            Vector3 next = p.pos + (p.vel * (1.0f / FPS));

            Vector3 rayDir = next - prev;
            float rayDist = rayDir.Length();

            if (rayDist > 0.0f)
            {
                rayDir.Normalize();

                auto roomLos = GetRoomLosCollision(prev, p.roomNumber, rayDir, rayDist);
                auto staticLos = GetStaticLosCollision(prev, p.roomNumber, rayDir, rayDist, true);

                float roomDist = roomLos.IsIntersected ? roomLos.Distance : FLT_MAX;
                float staticDist = staticLos ? staticLos->Distance : FLT_MAX;

                if (roomDist < staticDist)
                {
                    p.blocked = true;
                    SpawnDragonScorchDecal(roomLos.Position, roomLos.RoomNumber);
                    it = FlameProjectiles.erase(it);
                    continue;
                }
                else if (staticLos)
                {
                    p.blocked = true;
                    SpawnDragonScorchDecal(staticLos->Position, staticLos->RoomNumber);
                    it = FlameProjectiles.erase(it);
                    continue;
                }
            }

            p.pos = next;

            if (p.targetItem != NO_VALUE && p.targetItem < (int)g_Level.Items.size())
            {
                auto* target = &g_Level.Items[p.targetItem];
                if ((p.pos - target->Pose.Position.ToVector3()).Length() < BLOCK(0.5f))
                {
                    // Fall back to the standard engine burn when the default colour is in use.
                    if (p.hasCustomColor)
                        ItemCustomBurn(target, p.burnStartColor, p.burnEndColor);
                    else
                        ItemBurn(target);
                }
            }

            short newRoom = p.roomNumber;
            GetFloor((int)p.pos.x, (int)p.pos.y, (int)p.pos.z, &newRoom);
            p.roomNumber = newRoom;

            ++it;
        }
    }
    static void UpdateDragonEmbers()
    {
        for (auto it = DragonEmbers.begin(); it != DragonEmbers.end(); )
        {
            auto& e = *it;

            if (e.life-- <= 0)
            {
                it = DragonEmbers.erase(it);
                continue;
            }

            Vector3 prev = e.pos;
            Vector3 next = e.pos + (e.vel * (1.0f / FPS));

            Vector3 rayDir = next - prev;
            float rayDist = rayDir.Length();

            if (rayDist < 0.001f)
            {
                e.pos = next;
            }
            else
            {
                rayDir /= rayDist;

                auto roomLos = GetRoomLosCollision(prev, e.roomNumber, rayDir, rayDist);
                auto staticLos = GetStaticLosCollision(prev, e.roomNumber, rayDir, rayDist, true);

                float roomDist = roomLos.IsIntersected ? roomLos.Distance : FLT_MAX;
                float staticDist = staticLos ? staticLos->Distance : FLT_MAX;

                if (roomDist < staticDist)
                {
                    e.pos = roomLos.Position;

                    // Slide along the surface instead of bouncing: cancel the surface-normal velocity component.
                    Vector3 N = -rayDir;
                    e.vel = e.vel - (e.vel.Dot(N)) * N;
                    e.vel *= 0.8f;
                }
                else if (staticLos)
                {
                    e.pos = staticLos->Position;

                    // Slide along the surface instead of bouncing: cancel the surface-normal velocity component.
                    Vector3 N = -rayDir;
                    e.vel = e.vel - (e.vel.Dot(N)) * N;
                    e.vel *= 0.8f;
                }
                else
                {
                    e.pos = next;
                }
            }

            short newRoom = e.roomNumber;
            GetFloor((int)e.pos.x, (int)e.pos.y, (int)e.pos.z, &newRoom);
            e.roomNumber = newRoom;

            auto& ember = *GetFreeParticle();
            ember.on = true;

            ember.x = e.pos.x;
            ember.y = e.pos.y;
            ember.z = e.pos.z;

            ember.xVel = e.vel.x;
            ember.yVel = e.vel.y;
            ember.zVel = e.vel.z;

            ember.life = ember.sLife = std::max(e.life, 1);

            ember.SpriteSeqID = ID_SPARK_SPRITE;
            ember.SpriteID = 0;

            // Per-particle random variation, reproducing the original pre-custom ranges as fractions of the configured colour.
            ember.sR = Random::GenerateFloat(0.9f, 1.0f) * e.startColor.GetR();
            ember.sG = Random::GenerateFloat(0.4f, 0.6f) * e.startColor.GetG();
            ember.sB = Random::GenerateFloat(0.1f, 0.2f) * e.startColor.GetB();

            ember.dR = 0.8f * e.endColor.GetR();
            ember.dG = 0.6f * e.endColor.GetG();
            ember.dB = 0.3f * e.endColor.GetB();

            ember.colFadeSpeed = 10;
            ember.fadeToBlack = 6;
            ember.blendMode = BlendMode::Additive;

            ember.flags = SP_SCALE | SP_DEF | SP_ROTATE;

            ember.sSize = 6.0f;
            ember.dSize = 2.0f;
            ember.size = ember.sSize;

            ember.rotAng = Random::GenerateFloat(0.0f, PI * 2.0f);
            ember.rotAdd = Random::GenerateFloat(-0.2f, 0.2f);

            ++it;
        }
    }

    // Spawns thick, turbulent smoke while the dragon is charging its flame attack.
    static void SpawnDragonSmokeBreathEffect(const ItemInfo& item, const CreatureBiteInfo& bite)
    {
        constexpr auto SMOKE_COUNT = 6;
        constexpr auto SPHERE_RADIUS = BLOCK(0.25f);

        auto mouthPos = GetJointPosition(item, bite.BoneID, bite.Position).ToVector3();

        for (int i = 0; i < SMOKE_COUNT; i++)
        {
            auto sphere = BoundingSphere(mouthPos, SPHERE_RADIUS);
            auto pos = Random::GeneratePointInSphere(sphere);

            Vector3 vel(
                Random::GenerateFloat(-0.15f, 0.15f),
                Random::GenerateFloat(1.0f, 2.0f),
                Random::GenerateFloat(-0.15f, 0.15f)
            );

            Vector3 suction = (mouthPos - pos);
            suction.Normalize();
            vel += suction * Random::GenerateFloat(0.2f, 0.5f);

            vel.x += Random::GenerateFloat(-0.1f, 0.1f);
            vel.z += Random::GenerateFloat(-0.1f, 0.1f);

            for (auto& s : SmokeParticles)
            {
                if (!s.active)
                {
                    s.active = true;
                    s.position = pos;
                    s.velocity = vel;
                    s.room = item.RoomNumber;

                    float shadeStart = Random::GenerateFloat(0.65f, 0.85f);
                    float shadeEnd = Random::GenerateFloat(0.25f, 0.45f);

                    s.sourceColor = Vector4(shadeStart, shadeStart, shadeStart, 1.0f);
                    s.destinationColor = Vector4(shadeEnd, shadeEnd, shadeEnd, 0.0f);

                    s.sourceSize = BLOCK(Random::GenerateFloat(0.15f, 0.25f));
                    s.destinationSize = BLOCK(Random::GenerateFloat(0.6f, 0.9f));

                    s.age = 0.0f;
                    s.life = Random::GenerateFloat(25.0f, 45.0f);

                    s.gravity = -2.0f;
                    s.friction = 0.06f;
                    s.terminalVelocity = 2.5f;
                    s.affectedByWind = true;

                    s.rotation = Random::GenerateFloat(0.0f, PI * 2.0f);
                    s.angularVelocity = Random::GenerateFloat(-1.0f, 1.0f);
                    s.angularDrag = 0.92f;

                    s.sprite = Random::GenerateInt(0, 3);

                    s.StoreInterpolationData();
                    break;
                }
            }
        }
    }

    // Soft warm exhale after flame attack ends.
    static void SpawnDragonSoftExhaleEffect(const ItemInfo& item, const CreatureBiteInfo& bite)
    {
        constexpr auto SMOKE_COUNT = 2;
        constexpr auto SPHERE_RADIUS = BLOCK(0.15f);

        auto mouthPos = GetJointPosition(item, bite.BoneID, bite.Position).ToVector3();

        for (int i = 0; i < SMOKE_COUNT; i++)
        {
            auto sphere = BoundingSphere(mouthPos, SPHERE_RADIUS);
            auto pos = Random::GeneratePointInSphere(sphere);

            Vector3 vel(
                Random::GenerateFloat(-0.02f, 0.02f),
                Random::GenerateFloat(0.15f, 0.35f),
                Random::GenerateFloat(-0.02f, 0.02f)
            );

            vel.x += Random::GenerateFloat(-0.02f, 0.02f);
            vel.z += Random::GenerateFloat(-0.02f, 0.02f);

            for (auto& s : SmokeParticles)
            {
                if (!s.active)
                {
                    s.active = true;
                    s.position = pos;
                    s.velocity = vel;
                    s.room = item.RoomNumber;

                    float shadeStart = Random::GenerateFloat(0.20f, 0.30f);
                    float shadeEnd = Random::GenerateFloat(0.05f, 0.15f);

                    s.sourceColor = Vector4(shadeStart, shadeStart * 0.97f, shadeStart * 0.9f, 0.6f);
                    s.destinationColor = Vector4(shadeEnd, shadeEnd, shadeEnd, 0.0f);

                    s.sourceSize = BLOCK(Random::GenerateFloat(0.25f, 0.35f));
                    s.destinationSize = BLOCK(Random::GenerateFloat(1.2f, 1.6f));

                    s.age = 0.0f;
                    s.life = Random::GenerateFloat(65.0f, 95.0f);

                    s.gravity = -0.2f;
                    s.friction = 0.02f;
                    s.terminalVelocity = 1.0f;
                    s.affectedByWind = true;

                    s.rotation = Random::GenerateFloat(0.0f, PI * 2.0f);
                    s.angularVelocity = Random::GenerateFloat(-0.2f, 0.2f);
                    s.angularDrag = 0.96f;

                    s.sprite = Random::GenerateInt(0, 3);

                    s.StoreInterpolationData();
                    break;
                }
            }
        }
    }

    static void SpawnDragonShockwaveEffect(const ItemInfo& item, int jointIndex)
    {
		auto shockwaveEnabled = PropertyHandler::Get(item, PropName_ShockwaveToggle, true);

        if (!shockwaveEnabled)
            return;

        auto pos = GetJointPosition(item, jointIndex, Vector3i(0, -8, 0));

        if (item.Animation.AnimNumber == DRAGON_ANIM_ATTACK_LEFT_2 ||
            item.Animation.AnimNumber == DRAGON_ANIM_ATTACK_RIGHT_2)
        {
            if (item.Animation.FrameNumber == GetAnimData(item).EndFrameNumber)
            {
                auto pointColl = GetPointCollision(pos, item.RoomNumber);

                if (pointColl.GetFloorHeight() == NO_HEIGHT)
                {
                    pos.y -= CLICK(0.5f);
                }
                else
                {
                    pos.y = pointColl.GetFloorHeight() - CLICK(0.5f);
                }

				auto shockwaveColor = PropertyHandler::Get(item, PropName_PrimaryColor, ScriptColor(128, 128, 128));
                
                auto r = shockwaveColor.GetR();
				auto g = shockwaveColor.GetG();
				auto b = shockwaveColor.GetB();

				auto pose = Pose(pos, EulerAngles::Identity);
                TriggerShockwave(&pose, 24, 88, 256, r, g, b, 32,
                    EulerAngles::Identity, 8, true, false, false,
                    (int)ShockwaveStyle::Normal);

                Camera.bounce = -128;
            }
        }
    }

    void ControlDragon(short itemNumber)
    {
        if (!CreatureActive(itemNumber))
            return;

        auto& item = g_Level.Items[itemNumber];
        auto& creature = *GetCreatureInfo(&item);
        auto& timer = item.ItemFlags[1];

        short headingAngle = 0;
        short headOrient = 0;

        bool isTargetAhead = false;

        auto dragonSwipeAttackDamage = PropertyHandler::Get(item, PropName_DragonSwipeAttackDamage, DRAGON_SWIPE_ATTACK_DAMAGE);
		auto dragonContactDamage = PropertyHandler::Get(item, PropName_DragonContactDamage, DRAGON_CONTACT_DAMAGE);
        auto dragonDeathOption = PropertyHandler::Get(item, PropName_DragonDeathOption, (int)DRAGON_DEATH_NORMAL);

        // The dragon should not attempt the flame attack while Lara is underwater.
        bool laraUnderwater = (GetLaraInfo(LaraItem)->Control.WaterStatus == WaterStatus::Underwater);

        bool flagDaggerDeath = (item.TriggerFlags == DRAGON_DEATH_DAGGER || dragonDeathOption == (int)DRAGON_DEATH_DAGGER);

        if (item.HitPoints <= 0)
        {
            if (item.Animation.ActiveState != DRAGON_STATE_DEFEAT)
            {
                SetAnimation(item, DRAGON_ANIM_DEATH);
                timer = 0;
            }
            else
            {
                if (timer >= 0)
                {
                    SpawnDragonLightEffect(item, DragonLightEffectType::Yellow);
                    timer++;

                    if (timer == DRAGON_LIVE_TIME)
                        item.Animation.TargetState = DRAGON_STATE_IDLE;

                    if (timer == DRAGON_LIVE_TIME + DRAGON_ALMOST_LIVE)
                        item.HitPoints = Objects[ID_DRAGON_FRONT].HitPoints / 2;

                    if (!flagDaggerDeath)
                    {
                        if (item.Animation.AnimNumber == DRAGON_ANIM_DEFEATED)
                            timer = NO_VALUE;
                    }
                }
                else
                {
                    if (timer > -20)
                        SpawnDragonLightEffect(item, DragonLightEffectType::Red);

                    if (timer == -100)
                    {
                        InitializeDragonBones(item);

                        if (flagDaggerDeath)
                            CollectCarriedItems(&item);
                    }
                    else if (timer == -200)
                    {
                        DisableEntityAI(itemNumber);
                        KillItem(itemNumber);

                        if (!flagDaggerDeath)
                            DropPickups(&item);

                        item.Status = ITEM_DEACTIVATED;
                    }
                    else if (timer < -100)
                    {
                        item.Pose.Position.y += 10;
                    }

                    timer--;
                }
            }
        }
        else
        {
            AI_INFO ai;
            CreatureAIInfo(&item, &ai);

            GetCreatureMood(&item, &ai, true);
            CreatureMood(&item, &ai, true);
            headingAngle = CreatureTurn(&item, DRAGON_WALK_TURN_RATE_MAX);

            isTargetAhead = (ai.ahead && ai.distance > DRAGON_NEAR_RANGE && ai.distance < DRAGON_IDLE_RANGE);

            if (item.TouchBits.TestAny())
                DoDamage(creature.Enemy, dragonContactDamage);

            switch (item.Animation.ActiveState)
            {
            case DRAGON_STATE_IDLE:
                item.Pose.Orientation.y -= headingAngle;

                // NEW: Soft exhale during FIRE→IDLE transition
                if (item.Animation.AnimNumber == DRAGON_ANIM_FIRE_TO_IDLE &&
                    item.Animation.FrameNumber >= 6 &&
                    item.Animation.FrameNumber <= 15)
                {
                    SpawnDragonSoftExhaleEffect(item, DragonMouthBite);
                }

                if (!isTargetAhead)
                {
                    if (ai.distance > DRAGON_IDLE_RANGE || !ai.ahead)
                    {
                        item.Animation.TargetState = DRAGON_STATE_WALK;
                    }
                    else if (ai.ahead && ai.distance < DRAGON_NEAR_RANGE && !creature.Flags)
                    {
                        creature.Flags = 1;

                        if (ai.angle < 0)
                            item.Animation.TargetState = DRAGON_STATE_SWIPE_ATTACK_LEFT;
                        else
                            item.Animation.TargetState = DRAGON_STATE_SWIPE_ATTACK_RIGHT;
                    }
                    else if (ai.angle < 0)
                    {
                        item.Animation.TargetState = DRAGON_STATE_TURN_LEFT;
                    }
                    else
                    {
                        item.Animation.TargetState = DRAGON_STATE_TURN_RIGHT;
                    }
                }
                else if (!laraUnderwater)
                {
                    item.Animation.TargetState = DRAGON_STATE_AIM_1;
                }

                break;

            case DRAGON_STATE_SWIPE_ATTACK_LEFT:
                if (item.TouchBits.Test(DragonSwipeAttackJointsLeft))
                {
					DoDamage(creature.Enemy, dragonSwipeAttackDamage);
                    creature.Flags = 0;
                }

                SpawnDragonShockwaveEffect(item, DragonSwipeAttackJointsLeft[3]);
                break;

            case DRAGON_STATE_SWIPE_ATTACK_RIGHT:
                if (item.TouchBits.Test(DragonSwipeAttackJointsRight))
                {
                    DoDamage(creature.Enemy, dragonSwipeAttackDamage);
                    creature.Flags = 0;
                }

                SpawnDragonShockwaveEffect(item, DragonSwipeAttackJointsRight[3]);
                break;

            case DRAGON_STATE_WALK:
                creature.Flags = 0;

                if (isTargetAhead)
                {
                    item.Animation.TargetState = DRAGON_STATE_IDLE;
                }
                else if (headingAngle < -DRAGON_TURN_THRESHOLD_ANGLE)
                {
                    if (ai.distance < DRAGON_IDLE_RANGE && ai.ahead)
                        item.Animation.TargetState = DRAGON_STATE_IDLE;
                    else
                        item.Animation.TargetState = DRAGON_STATE_MOVE_LEFT;
                }
                else if (headingAngle > DRAGON_TURN_THRESHOLD_ANGLE)
                {
                    if (ai.distance < DRAGON_IDLE_RANGE && ai.ahead)
                        item.Animation.TargetState = DRAGON_STATE_IDLE;
                    else
                        item.Animation.TargetState = DRAGON_STATE_MOVE_RIGHT;
                }

                break;

            case DRAGON_STATE_MOVE_LEFT:
                if (headingAngle > -DRAGON_TURN_THRESHOLD_ANGLE || isTargetAhead)
                    item.Animation.TargetState = DRAGON_STATE_WALK;
                break;

            case DRAGON_STATE_MOVE_RIGHT:
                if (headingAngle < DRAGON_TURN_THRESHOLD_ANGLE || isTargetAhead)
                    item.Animation.TargetState = DRAGON_STATE_WALK;
                break;

            case DRAGON_STATE_TURN_LEFT:
                item.Pose.Orientation.y -= ANGLE(1.0f) - headingAngle;
                creature.Flags = 0;
                break;

            case DRAGON_STATE_TURN_RIGHT:
                item.Pose.Orientation.y += ANGLE(1.0f) - headingAngle;
                creature.Flags = 0;
                break;

            case DRAGON_STATE_AIM_1:
                item.Pose.Orientation.y -= headingAngle;

                if (ai.ahead)
                    headOrient = -ai.angle;

                // NEW: Charging smoke
                SpawnDragonSmokeBreathEffect(item, DragonMouthBite);

                if (isTargetAhead && !laraUnderwater)
                {
                    item.Animation.TargetState = DRAGON_STATE_FIRE_1;
                    creature.Flags = 30;
                }
                else
                {
                    item.Animation.TargetState = DRAGON_STATE_AIM_1;
                    creature.Flags = 0;
                }

                break;

            case DRAGON_STATE_FIRE_1:
                item.Pose.Orientation.y -= headingAngle;
                SoundEffect(SFX_TR2_DRAGON_FIRE, &item.Pose);

                if (ai.ahead)
                    headOrient = -ai.angle;

                if (creature.Flags)
                {
                    if (ai.ahead)
                        SpawnDragonFireBreathEffect(item, DragonMouthBite, creature.Enemy.Get());

                    creature.Flags--;
                }
                else
                {
                    item.Animation.TargetState = DRAGON_STATE_IDLE;
                }

                break;
            }
        }
        if (timer >= 0)
        {
            CreatureJoint(&item, 0, headOrient);
            CreatureAnimation(itemNumber, headingAngle, 0);
        }

        SyncDragonBackSegment(item);
        UpdateDragonFlameProjectiles();
        UpdateDragonEmbers();
    }

    static void HandleDaggerPickup(ItemInfo& item, ItemInfo& playerItem)
    {
        auto& player = GetLaraInfo(playerItem);

        g_Hud.InteractionHighlighter.Test(playerItem, item, InteractionMode::Always, InteractionType::Use);

        if ((IsHeld(In::Action) &&
            (item.Animation.AnimNumber == DRAGON_ANIM_DEFEATED ||
                (item.Animation.AnimNumber == DRAGON_ANIM_RECOVER &&
                    item.Animation.FrameNumber <= DRAGON_ALMOST_LIVE)) &&
            playerItem.Animation.ActiveState == LS_IDLE &&
            playerItem.Animation.AnimNumber == LA_STAND_IDLE &&
            player.Control.HandStatus == HandStatus::Free) ||
            (player.Control.IsMoving && player.Context.InteractedItem == item.Index))
        {
            auto bounds = GameBoundingBox(&item);

            DragonDaggerBounds.BoundingBox.X1 = bounds.X1 - BLOCK(1.0f);
            DragonDaggerBounds.BoundingBox.X2 = bounds.X2 + BLOCK(1.0f);
            DragonDaggerBounds.BoundingBox.Z1 = bounds.Z1 - BLOCK(1.0f);
            DragonDaggerBounds.BoundingBox.Z2 = bounds.Z2 + BLOCK(1.0f);

            DragonDaggerPos.x = bounds.X2 - BLOCK(2.5f);
            DragonDaggerPos.z = bounds.Z1 + BLOCK(0.9f);

            if (TestLaraPosition(DragonDaggerBounds, &item, &playerItem))
            {
                // Temporarily rotate dragon so Lara aligns correctly.
                short yOrient = item.Pose.Orientation.y;
                item.Pose.Orientation.y += ANGLE(90.0f);

                if (MoveLaraPosition(DragonDaggerPos, &item, &playerItem))
                {
                    SetAnimationFromSlot(playerItem, ID_LARA_EXTRA_ANIMS, LEA_PULL_DAGGER_FROM_DRAGON);
                    playerItem.Pose = item.Pose;

                    ResetPlayerFlex(&playerItem);
                    playerItem.Animation.FrameNumber = 0;
                    player.Control.IsMoving = false;
                    player.Control.HandStatus = HandStatus::Busy;

                    AnimateItem(playerItem);

                    // Setting ItemFlags[1] to negative value sets defeat status and triggers death.
                    item.ItemFlags[1] = -1;
                }
                else
                {
                    player.Context.InteractedItem = item.Index;
                }

                // Restore orientation.
                item.Pose.Orientation.y = yOrient;
            }
            else if (player.Control.IsMoving && player.Context.InteractedItem == item.Index)
            {
                player.Control.IsMoving = false;
                player.Control.HandStatus = HandStatus::Free;
            }
        }
    }

    void CollideDragonFront(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
    {
        auto& item = g_Level.Items[itemNumber];
        const auto& player = *GetLaraInfo(playerItem);

        auto dragonDeathOption = PropertyHandler::Get(item, PropName_DragonDeathOption, (int)DRAGON_DEATH_NORMAL);
        bool flagDaggerDeath = (item.TriggerFlags == DRAGON_DEATH_DAGGER || dragonDeathOption == (int)DRAGON_DEATH_DAGGER);

        if (item.Animation.ActiveState == DRAGON_STATE_DEFEAT && flagDaggerDeath)
        {
            HandleDaggerPickup(item, *playerItem);

            if ((player.Control.IsMoving &&
                player.Context.InteractedItem == item.Index) ||
                playerItem->Animation.AnimNumber == LEA_PULL_DAGGER_FROM_DRAGON)
            {
                return;
            }
            else
            {
                CreatureCollision(itemNumber, playerItem, coll);
            }
        }
        else
        {
            if (item.HitPoints > 0)
                CreatureCollision(itemNumber, playerItem, coll);
        }
    }

    void CollideDragonBack(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
    {
        const auto& item = g_Level.Items[itemNumber];

        if (item.HitPoints > 0)
            CreatureCollision(itemNumber, playerItem, coll);
    }
}


