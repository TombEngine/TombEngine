#include "framework.h"
#include "Math/Objects/Vector2i.h"

namespace TEN::Math
{
	const Vector2i Vector2i::Zero = Vector2i(0, 0);

	Vector2i::Vector2i(const Vector2& vector)
	{
		x = (int)round(vector.x);
		y = (int)round(vector.y);
	}

	float Vector2i::Distance(const Vector2i& origin, const Vector2i& target)
	{
		return std::sqrt(DistanceSquared(origin, target));
	}
	
	float Vector2i::DistanceSquared(const Vector2i& origin, const Vector2i& target)
	{
		return (SQUARE(target.x - origin.x) + SQUARE(target.y - origin.y));
	}

	Vector2 Vector2i::ToVector2() const
	{
		return Vector2(x, y);
	}

	bool Vector2i::operator ==(const Vector2i& vector) const
	{
		return ((x == vector.x) && (y == vector.y));
	}

	bool Vector2i::operator !=(const Vector2i& vector) const
	{
		return !(*this == vector);
	}

	Vector2i& Vector2i::operator =(const Vector2i& vector)
	{
		x = vector.x;
		y = vector.y;
		return *this;
	}

	Vector2i& Vector2i::operator +=(const Vector2i& vector)
	{
		x += vector.x;
		y += vector.y;
		return *this;
	}

	Vector2i& Vector2i::operator -=(const Vector2i& vector)
	{
		x -= vector.x;
		y -= vector.y;
		return *this;
	}

	Vector2i& Vector2i::operator *=(const Vector2i& vector)
	{
		x *= vector.x;
		y *= vector.y;
		return *this;
	}

	Vector2i& Vector2i::operator *=(float scalar)
	{
		x *= scalar;
		y *= scalar;
		return *this;
	}

	Vector2i& Vector2i::operator /=(float scalar)
	{
		x /= scalar;
		y /= scalar;
		return *this;
	}

	Vector2i Vector2i::operator +(const Vector2i& vector) const
	{
		return Vector2i(x + vector.x, y + vector.y);
	}

	Vector2i Vector2i::operator -(const Vector2i& vector) const
	{
		return Vector2i(x - vector.x, y - vector.y);
	}

	Vector2i Vector2i::operator *(const Vector2i& vector) const
	{
		return Vector2i(x * vector.x, y * vector.y);
	}

	Vector2i Vector2i::operator *(float scalar) const
	{
		return Vector2i((int)round(x * scalar), (int)round(y * scalar));
	}

	Vector2i Vector2i::operator /(float scalar) const
	{
		return Vector2i((int)round(x / scalar), (int)round(y / scalar));
	}
}
