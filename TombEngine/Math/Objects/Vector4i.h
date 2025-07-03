#pragma once

//namespace TEN::Math
//{
class Vector4i
{
public:
	// Fields

	int x = 0;
	int y = 0;
	int z = 0;
	int w = 0;

	// Constants

	static const Vector4i Zero;

	// Constructors

	constexpr Vector4i() = default;
	constexpr Vector4i(int x, int y, int z, int w) { this->x = x; this->y = y; this->z = z; this->w = w; };
	Vector4i(const Vector4& vector);

	// Utilities

	static float	Distance(const Vector4i& origin, const Vector4i& target);
	static float	DistanceSquared(const Vector4i& origin, const Vector4i& target);
	void			Lerp(const Vector4i& target, float alpha);
	static Vector4i Lerp(const Vector4i& origin, const Vector4i& target, float alpha);

	// Converters

	Vector4 ToVector4() const;

	// Operators

	bool	  operator ==(const Vector4i& vector) const;
	bool	  operator !=(const Vector4i& vector) const;
	Vector4i& operator =(const Vector4i& vector);
	Vector4i& operator +=(const Vector4i& vector);
	Vector4i& operator -=(const Vector4i& vector);
	Vector4i& operator *=(const Vector4i& vector);
	Vector4i& operator *=(float scalar);
	Vector4i& operator /=(float scalar);
	Vector4i  operator +(const Vector4i& vector) const;
	Vector4i  operator -(const Vector4i& vector) const;
	Vector4i  operator *(const Vector4i& vector) const;
	Vector4i  operator *(float scalar) const;
	Vector4i  operator /(float scalar) const;
};
//}

namespace std
{
	template <>
	struct hash<Vector4i>
	{
		size_t operator ()(const Vector4i& vector) const
		{
			size_t seed = 0;
			seed ^= hash<int>()(vector.x) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= hash<int>()(vector.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			seed ^= hash<int>()(vector.z) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
			return seed;
		}
	};
}
