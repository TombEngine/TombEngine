#pragma once

typedef unsigned int D3DCOLOR;

class RGBAColor8Byte
{
private:
	// Members
	unsigned char r = 0;
	unsigned char g = 0;
	unsigned char b = 0;
	unsigned char a = 255;

public:
	// Constructors
	RGBAColor8Byte() = default;
	RGBAColor8Byte(D3DCOLOR color);
	RGBAColor8Byte(unsigned char r, unsigned char g, unsigned char b);
	RGBAColor8Byte(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
	RGBAColor8Byte(const Vector3& color);
	RGBAColor8Byte(const Vector4& color);

	// Getters
	unsigned char GetR() const;
	unsigned char GetG() const;
	unsigned char GetB() const;
	unsigned char GetA() const;

	// Setters
	void SetR(unsigned char value);
	void SetG(unsigned char value);
	void SetB(unsigned char value);
	void SetA(unsigned char value);

	// Operators
	operator Color() const;
	operator Vector3() const;
	operator Vector4() const;
	operator D3DCOLOR() const;
};
