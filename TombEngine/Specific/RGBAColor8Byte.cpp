#include "framework.h"
#include "Specific/RGBAColor8Byte.h"

static unsigned char FloatComponentToByte(float value)
{
	int byteValue = std::clamp((int)std::lroundf(value * (float)UCHAR_MAX), 0, UCHAR_MAX);
	return (unsigned char)byteValue;
}

static float ByteComponentToFloat(unsigned char b)
{
	float value = (b / 255.0f);
	return value;
}

RGBAColor8Byte::RGBAColor8Byte(D3DCOLOR color)
{
	b = color & 0xFF;
	color >>= 8;
	g = color & 0xFF;
	color >>= 8;
	r = color & 0xFF;
	color >>= 8;
	a = color & 0xFF;
}

RGBAColor8Byte::RGBAColor8Byte(unsigned char r, unsigned char g, unsigned char b)
{
	SetR(r);
	SetG(g);
	SetB(b);
}

RGBAColor8Byte::RGBAColor8Byte(unsigned char r, unsigned char g, unsigned char b, unsigned char a) :
	RGBAColor8Byte(r, g, b)
{
	SetA(a);
}

RGBAColor8Byte::RGBAColor8Byte(const Vector3& color)
{
	r = FloatComponentToByte(color.x);
	g = FloatComponentToByte(color.y);
	b = FloatComponentToByte(color.z);
}

RGBAColor8Byte::RGBAColor8Byte(const Vector4& color)
{
	r = FloatComponentToByte(color.x);
	g = FloatComponentToByte(color.y);
	b = FloatComponentToByte(color.z);
	a = FloatComponentToByte(color.w);
}

unsigned char RGBAColor8Byte::GetR() const
{
	return r;
}

void RGBAColor8Byte::SetR(unsigned char v)
{
	r = std::clamp<unsigned char>(v, 0, UCHAR_MAX);
}

unsigned char RGBAColor8Byte::GetG() const
{
	return g;
}

void RGBAColor8Byte::SetG(unsigned char v)
{
	g = std::clamp<unsigned char>(v, 0, UCHAR_MAX);
}

unsigned char RGBAColor8Byte::GetB() const
{
	return b;
}

void RGBAColor8Byte::SetB(unsigned char v)
{
	b = std::clamp<unsigned char>(v, 0, UCHAR_MAX);
}

unsigned char RGBAColor8Byte::GetA() const
{
	return a;
}

void RGBAColor8Byte::SetA(unsigned char v)
{
	a = std::clamp<unsigned char>(v, 0, UCHAR_MAX);
}

RGBAColor8Byte::operator Color() const
{
	return Color(ByteComponentToFloat(r), ByteComponentToFloat(g), ByteComponentToFloat(b), ByteComponentToFloat(a));
}

RGBAColor8Byte::operator Vector3() const
{
	return Vector3(ByteComponentToFloat(r), ByteComponentToFloat(g), ByteComponentToFloat(b));
}

RGBAColor8Byte::operator Vector4() const
{
	return Vector4(ByteComponentToFloat(r), ByteComponentToFloat(g), ByteComponentToFloat(b), ByteComponentToFloat(a));
}

RGBAColor8Byte::operator D3DCOLOR() const
{
	D3DCOLOR color = a;
	color <<= 8;
	color += r;
	color <<= 8;
	color += g;
	color <<= 8;
	color += b;

	return color;
}
