#pragma once

typedef DWORD D3DCOLOR;

class RGBAColor8Byte
{
private:
	// Members
	byte r = 0;
	byte g = 0;
	byte b = 0;
	byte a = 255;

public:
	// Constructors
	RGBAColor8Byte(D3DCOLOR color);
	RGBAColor8Byte(byte r, byte g, byte b);
	RGBAColor8Byte(byte r, byte g, byte b, byte a);
	RGBAColor8Byte(const Vector3& color);
	RGBAColor8Byte(const Vector4& color);

	// Getters
	byte GetR() const;
	byte GetG() const;
	byte GetB() const;
	byte GetA() const;

	// Setters
	void SetR(byte value);
	void SetG(byte value);
	void SetB(byte value);
	void SetA(byte value);

	// Operators
	operator Color() const;
	operator Vector3() const;
	operator Vector4() const;
	operator D3DCOLOR() const;
};
