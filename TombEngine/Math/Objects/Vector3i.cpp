#include "framework.h"
#include "Math/Objects/Vector3i.h"

//namespace TEN::Math
//{
	const Vector3i Vector3i::Zero = Vector3i(0, 0, 0);

	Vector3i::Vector3i(const Vector3& vector)
	{
		x = (int)round(vector.x);
		y = (int)round(vector.y);
		z = (int)round(vector.z);
	}

	float Vector3i::Distance(const Vector3i& origin, const Vector3i& target)
	{
		return std::sqrt(DistanceSquared(origin, target));
	}

	float Vector3i::DistanceSquared(const Vector3i& origin, const Vector3i& target)
	{
		return (SQUARE(target.x - origin.x) + SQUARE(target.y - origin.y) + SQUARE(target.z - origin.z));
	}

	void Vector3i::Lerp(const Vector3i& target, float alpha)
	{
		*this = Lerp(*this, target, alpha);
	}

	Vector3i Vector3i::Lerp(const Vector3i& origin, const Vector3i& target, float alpha)
	{
		return Vector3i(Vector3::Lerp(origin.ToVector3(), target.ToVector3(), alpha));
	}

	Vector3 Vector3i::ToVector3() const
	{
		return Vector3(x, y, z);
	}

	bool Vector3i::operator ==(const Vector3i& vector) const
	{
		return ((x == vector.x) && (y == vector.y) && (z == vector.z));
	}

	bool Vector3i::operator !=(const Vector3i& vector) const
	{
		return !(*this == vector);
	}

	Vector3i& Vector3i::operator =(const Vector3i& vector)
	{
		x = vector.x;
		y = vector.y;
		z = vector.z;
		return *this;
	}

	Vector3i& Vector3i::operator +=(const Vector3i& vector)
	{
		x += vector.x;
		y += vector.y;
		z += vector.z;
		return *this;
	}

	Vector3i& Vector3i::operator -=(const Vector3i& vector)
	{
		x -= vector.x;
		y -= vector.y;
		z -= vector.z;
		return *this;
	}

	Vector3i& Vector3i::operator *=(const Vector3i& vector)
	{
		x *= vector.x;
		y *= vector.y;
		z *= vector.z;
		return *this;
	}

	Vector3i& Vector3i::operator *=(float scalar)
	{
		x *= scalar;
		y *= scalar;
		z *= scalar;
		return *this;
	}

	Vector3i& Vector3i::operator /=(float scalar)
	{
		x /= scalar;
		y /= scalar;
		z /= scalar;
		return *this;
	}

	Vector3i Vector3i::operator +(const Vector3i& vector) const
	{
		return Vector3i(x + vector.x, y + vector.y, z + vector.z);
	}

	Vector3i Vector3i::operator -(const Vector3i& vector) const
	{
		return Vector3i(x - vector.x, y - vector.y, z - vector.z);
	}

	Vector3i Vector3i::operator *(const Vector3i& vector) const
	{
		return Vector3i(x * vector.x, y * vector.y, z * vector.z);
	}

	Vector3i Vector3i::operator *(float scalar) const
	{
		return Vector3i((int)round(x * scalar), (int)round(y * scalar), (int)round(z * scalar));
	}

	Vector3i Vector3i::operator /(float scalar) const
	{
		return Vector3i((int)round(x / scalar), (int)round(y / scalar), (int)round(z / scalar));
	}
//}
