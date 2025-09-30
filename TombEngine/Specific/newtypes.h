#pragma once
#include "framework.h"
#include "Renderer/RendererEnums.h"

struct POLYGON
{
	int shape;
	int animatedSequence;
	int animatedFrame;
	float shineStrength;
	Vector3 normal;
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
	int materialIndex;
	bool animated;
	Vector4 floatParameters0;
	Vector4 floatParameters1;
	Vector4 floatParameters2;
	Vector4 floatParameters3;
	int numQuads;
	int numTriangles;
	std::vector<POLYGON> polygons;
};
