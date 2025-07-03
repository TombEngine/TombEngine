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
	MaterialShaderType materialType;
	Vector4 floatParameters0;
	Vector4 floatParameters1;
	Vector4 floatParameters2;
	Vector4 floatParameters3;
	Vector4i integerParameters0;
	Vector4i integerParameters1;
	Vector4i integerParameters2;
	Vector4i integerParameters3;
	Vector2 vector2Parameters0;
	Vector2 vector2Parameters1;
	Vector2 vector2Parameters2;
	Vector2 vector2Parameters3;
	Vector3 vector3Parameters0;
	Vector3 vector3Parameters1;
	Vector3 vector3Parameters2;
	Vector3 vector3Parameters3;
	Vector4 vector4Parameters0;
	Vector4 vector4Parameters1;
	Vector4 vector4Parameters2;
	Vector4 vector4Parameters3;
	bool animated;
	int numQuads;
	int numTriangles;
	std::vector<POLYGON> polygons;
};
