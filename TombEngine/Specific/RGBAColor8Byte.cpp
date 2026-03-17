#include "framework.h"
#include "Specific/RGBAColor8Byte.h"

static byte FloatComponentToByte(float value)
{
	// TODO: Look into what these actually do and test them to see if they are actually not undefined.
	int byteValue = std::clamp((int)std::lroundf(value * (float)UCHAR_MAX), 0, UCHAR_MAX);
	return (byte)byteValue;
}

static float ByteComponentToFloat(byte b)
{
	// TODO: Look into what these actually do and test them to see if they are actually not undefined.
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

RGBAColor8Byte::RGBAColor8Byte(byte r, byte g, byte b)
{
	SetR(r);
	SetG(g);
	SetB(b);
}

RGBAColor8Byte::RGBAColor8Byte(byte r, byte g, byte b, byte a) :
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

byte RGBAColor8Byte::GetR() const
{
	return r;
}

void RGBAColor8Byte::SetR(byte v)
{
	r = std::clamp<byte>(v, 0, 255);
}

byte RGBAColor8Byte::GetG() const
{
	return g;
}

void RGBAColor8Byte::SetG(byte v)
{
	g = std::clamp<byte>(v, 0, 255);
}

byte RGBAColor8Byte::GetB() const
{
	return b;
}

void RGBAColor8Byte::SetB(byte v)
{
	b = std::clamp<byte>(v, 0, 255);
}

byte RGBAColor8Byte::GetA() const
{
	return a;
}

void RGBAColor8Byte::SetA(byte v)
{
	a = std::clamp<byte>(v, 0, 255);
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
