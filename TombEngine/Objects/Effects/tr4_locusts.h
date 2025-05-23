#pragma once
#include "Game/items.h"
#include "Math/Math.h"

struct LOCUST_INFO
{
    bool on;
    Pose pos;
    ItemInfo* target;
    short roomNumber;
    short randomRotation;
    short escapeXrot;
    short escapeYrot;
    short escapeZrot;
    BYTE counter;

    Matrix Transform     = Matrix::Identity;
    Matrix PrevTransform = Matrix::Identity;

    void StoreInterpolationData()
    {
        PrevTransform = Transform;
    }
};

namespace TEN::Entities::TR4 
{
    constexpr auto MAX_LOCUSTS = 64;
    constexpr auto LOCUST_LARA_DAMAGE = 3;
    constexpr auto LOCUST_ENTITY_DAMAGE = 1;

    extern LOCUST_INFO Locusts[MAX_LOCUSTS];

    extern int CreateLocust();
    extern void SpawnLocust(ItemInfo* item);
    extern void InitializeLocust(short itemNumber);
    extern void LocustControl(short itemNumber);
    extern void UpdateLocusts();
    extern void ClearLocusts();
    extern void DrawLocust();
}