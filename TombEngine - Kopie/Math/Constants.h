#pragma once
#include <climits>

//namespace TEN::Math
//{
	// Math constants

	constexpr auto PI		= 3.14159265358979323846264338327950288419716939937510f;
	constexpr auto PI_MUL_2 = PI * 2;
	constexpr auto PI_DIV_2 = PI / 2;
	constexpr auto PI_DIV_4 = PI / 4;
	constexpr auto RADIAN	= PI / 180;
	constexpr auto SQRT_2	= 1.41421356237309504880168872420969807856967187537694f;
	constexpr auto EPSILON	= 0.00001f;

	constexpr auto SQUARE = [](auto x) { return (x * x); };
	constexpr auto CUBE	  = [](auto x) { return (x * x * x); };

	// Geometry constants

	constexpr auto BOX_VERTEX_COUNT = 8;
	constexpr auto BOX_EDGE_COUNT	= 12;
	constexpr auto BOX_FACE_COUNT	= 6;

	// World constants

	constexpr auto BLOCK_UNIT = 1024;
	constexpr auto NO_HEIGHT  = INT_MIN + UCHAR_MAX;
	constexpr auto MAX_HEIGHT = INT_MIN + 1; // NOTE: +1 prevents issues with sign change.
	constexpr auto DEEP_WATER = INT_MAX - 1; // NOTE: -1 prevents issues with sign change.

	constexpr auto BLOCK = [](auto x) { return (BLOCK_UNIT * x); };
	constexpr auto CLICK = [](auto x) { return ((BLOCK(1) / 4) * x); };

	constexpr auto WALL_MASK	  = BLOCK(1) - 1;
	constexpr auto GRID_SNAP_SIZE = (int)BLOCK(1 / 8.0f);
//}
