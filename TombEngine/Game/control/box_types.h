#pragma once

// TODO: Use DX BoundingBox class to store AABB.
struct BOX_INFO
{
	unsigned int left;
	unsigned int right;
	unsigned int top;
	unsigned int bottom;

	int height;
	int overlapIndex;
	int flags;
};

struct OVERLAP
{
	int box;
	int flags;
};
