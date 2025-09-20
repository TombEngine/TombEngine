#pragma once

namespace TEN::Math
{
	class Vector2i
	{
	public:
		// Fields

		int x = 0;
		int y = 0;

		// Constants

		static const Vector2i Zero;

		// Constructors

		constexpr Vector2i() = default;
		constexpr Vector2i(int x, int y) { this->x = x; this->y = y; };
				  Vector2i(const Vector2& vector);

		// Utilities

		static float Distance(const Vector2i& origin, const Vector2i& target);
		static float DistanceSquared(const Vector2i& origin, const Vector2i& target);

		// Converters

		Vector2 ToVector2() const;

		// Operators

		bool	  operator ==(const Vector2i& vector) const;
		bool	  operator !=(const Vector2i& vector) const;
		Vector2i& operator =(const Vector2i& vector);
		Vector2i& operator +=(const Vector2i& vector);
		Vector2i& operator -=(const Vector2i& vector);
		Vector2i& operator *=(const Vector2i& vector);
		Vector2i& operator *=(float scalar);
		Vector2i& operator /=(float scalar);
		Vector2i  operator +(const Vector2i& vector) const;
		Vector2i  operator -(const Vector2i& vector) const;
		Vector2i  operator *(const Vector2i& vector) const;
		Vector2i  operator *(float scalar) const;
		Vector2i  operator /(float scalar) const;
	};
}
