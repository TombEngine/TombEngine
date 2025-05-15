#pragma once
#include "framework.h"
#include "Renderer/RendererEnums.h"

struct POLYGON
{
	int shape;
	int animatedSequence;
	int animatedFrame;
	float shineStrength;
	std::vector<int> indices;
	std::vector<Vector2> textureCoordinates;
	std::vector<Vector3> normals;
	std::vector<Vector3> tangents;
	std::vector<Vector3> binormals;
};

struct BUCKET
{
	int texture;
	BlendMode blendMode;
	bool animated;
	int numQuads;
	int numTriangles;
	std::vector<POLYGON> polygons;
};

struct VERTEXCOLORS
{
	Vector3 ColorB1;
	Vector3 ColorB2;
	Vector3 ColorB3;
};
