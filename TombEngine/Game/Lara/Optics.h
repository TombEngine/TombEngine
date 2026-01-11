#pragma once

#include "Specific/clock.h"

struct ItemInfo;

constexpr auto OPTICS_FADE_SPEED	= 6.0f / FPS;
constexpr auto OPTICS_RANGE_DEFAULT	= ANGLE(0.7f);

bool HandlePlayerOptics(ItemInfo& item);
