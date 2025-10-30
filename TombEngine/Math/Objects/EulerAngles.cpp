#include "framework.h"
#include "Math/Objects/EulerAngles.h"

#include "Math/Constants.h"
#include "Math/Geometry.h"
#include "Math/Legacy.h"
#include "Math/Objects/AxisAngle.h"

using namespace TEN::Math;

//namespace TEN::Math
//{
	const EulerAngles EulerAngles::Identity = EulerAngles(0, 0, 0);

	EulerAngles::EulerAngles(const Vector3& dir)
	{
		auto normalizedDir = dir;
		normalizedDir.Normalize();

		x = FROM_RAD(-asin(normalizedDir.y));
		y = FROM_RAD(atan2(normalizedDir.x, normalizedDir.z));
		z = 0;
	}

	EulerAngles::EulerAngles(const AxisAngle& axisAngle)
	{
		*this = EulerAngles(axisAngle.ToQuaternion());
	}

	EulerAngles::EulerAngles(const Quaternion& quat)
	{
		constexpr auto SINGULARITY_THRESHOLD = 1.0f - EPSILON;

		// Handle singularity case.
		float sinP = ((quat.w * quat.x) - (quat.y * quat.z)) * 2;
		if (abs(sinP) > SINGULARITY_THRESHOLD)
		{
			if (sinP > 0.0f)
				*this = EulerAngles(FROM_RAD(PI_DIV_2), 0, FROM_RAD(atan2(quat.z, quat.w) * 2.0f));
			else
				*this = EulerAngles(FROM_RAD(-PI_DIV_2), 0, FROM_RAD(atan2(quat.z, quat.w) * -2.0f));
		}

		// Pitch (X axis)
		float pitch = 0.0f;
		if (abs(sinP) >= 1.0f)
			pitch = (sinP > 0) ? PI_DIV_2 : -PI_DIV_2;
		else
			pitch = asin(sinP);

		// Yaw (Y axis)
		float sinY = ((quat.w * quat.y) + (quat.z * quat.x)) * 2;
		float cosY = 1.0f - ((SQUARE(quat.x) + SQUARE(quat.y)) * 2);
		float yaw = atan2(sinY, cosY);

		// Roll (Z axis)
		float sinR = ((quat.w * quat.z) + (quat.x * quat.y)) * 2;
		float cosR = 1.0f - ((SQUARE(quat.z) + SQUARE(quat.x)) * 2);
		float roll = atan2(sinR, cosR);

		*this = EulerAngles(FROM_RAD(pitch), FROM_RAD(yaw), FROM_RAD(roll));
	}

	EulerAngles::EulerAngles(const Matrix& rotMatrix)
	{
		x = FROM_RAD(asin(-rotMatrix._32));
		y = FROM_RAD(atan2(rotMatrix._31, rotMatrix._33));
		z = FROM_RAD(atan2(rotMatrix._12, rotMatrix._22));
	}

	bool EulerAngles::Compare(const EulerAngles& eulers0, const EulerAngles& eulers1, short epsilon)
	{
		if (Compare(eulers0.x, eulers1.x, epsilon) &&
			Compare(eulers0.y, eulers1.y, epsilon) &&
			Compare(eulers0.z, eulers1.z, epsilon))
		{
			return true;
		}

		return false;
	}

	void EulerAngles::Lerp(const EulerAngles& eulersTo, float alpha, short epsilon)
	{
		*this = Lerp(*this, eulersTo, alpha, epsilon);
	}

	EulerAngles EulerAngles::Lerp(const EulerAngles& eulersFrom, const EulerAngles& eulersTo, float alpha, short epsilon)
	{
		return EulerAngles(
			Lerp(eulersFrom.x, eulersTo.x, alpha, epsilon),
			Lerp(eulersFrom.y, eulersTo.y, alpha, epsilon),
			Lerp(eulersFrom.z, eulersTo.z, alpha, epsilon));
	}

	void EulerAngles::Slerp(const EulerAngles& eulersTo, float alpha)
	{
		*this = Slerp(*this, eulersTo, alpha);
	}

	EulerAngles EulerAngles::Slerp(const EulerAngles& eulersFrom, const EulerAngles& eulersTo, float alpha)
	{
		auto quatFrom = eulersFrom.ToQuaternion();
		auto quatTo = eulersTo.ToQuaternion();

		auto quat = Quaternion::Slerp(quatFrom, quatTo, alpha);
		return EulerAngles(quat);
	}

	void EulerAngles::InterpolateConstant(const EulerAngles& eulersTo, short angularVel)
	{
		*this = InterpolateConstant(*this, eulersTo, angularVel);
	}

	EulerAngles EulerAngles::InterpolateConstant(const EulerAngles& eulersFrom, const EulerAngles& eulerTo, short angularVel)
	{
		return EulerAngles(
			InterpolateConstant(eulersFrom.x, eulerTo.x, angularVel),
			InterpolateConstant(eulersFrom.y, eulerTo.y, angularVel),
			InterpolateConstant(eulersFrom.z, eulerTo.z, angularVel));
	}

	Vector3 EulerAngles::ToDirection() const
	{
		float sinX = sin(TO_RAD(x));
		float cosX = cos(TO_RAD(x));
		float sinY = sin(TO_RAD(y));
		float cosY = cos(TO_RAD(y));

		// Return normalized direction vector.
		auto result = Vector3(sinY * cosX, -sinX, cosY * cosX);
		result.Normalize();

		return result;
	}

	AxisAngle EulerAngles::ToAxisAngle() const
	{
		return AxisAngle(*this);
	}

	Quaternion EulerAngles::ToQuaternion() const
	{
		return Quaternion::CreateFromYawPitchRoll(TO_RAD(y), TO_RAD(x), TO_RAD(z));
	}

	Matrix EulerAngles::ToRotationMatrix() const
	{
		return Matrix::CreateFromYawPitchRoll(TO_RAD(y), TO_RAD(x), TO_RAD(z));
	}

	bool EulerAngles::operator ==(const EulerAngles& eulers) const
	{
		return ((x == eulers.x) && (y == eulers.y) && (z == eulers.z));
	}

	bool EulerAngles::operator !=(const EulerAngles& eulers) const
	{
		return !(*this == eulers);
	}

	EulerAngles& EulerAngles::operator =(const EulerAngles& eulers)
	{
		x = eulers.x;
		y = eulers.y;
		z = eulers.z;
		return *this;
	}

	EulerAngles& EulerAngles::operator +=(const EulerAngles& eulers)
	{
		x += eulers.x;
		y += eulers.y;
		z += eulers.z;
		return *this;
	}

	EulerAngles& EulerAngles::operator -=(const EulerAngles& eulers)
	{
		x -= eulers.x;
		y -= eulers.y;
		z -= eulers.z;
		return *this;
	}

	EulerAngles& EulerAngles::operator *=(const EulerAngles& eulers)
	{
		x *= eulers.x;
		y *= eulers.y;
		z *= eulers.z;
		return *this;
	}

	EulerAngles& EulerAngles::operator *=(float scalar)
	{
		x *= scalar;
		y *= scalar;
		z *= scalar;
		return *this;
	}

	EulerAngles& EulerAngles::operator /=(float scalar)
	{
		x /= scalar;
		y /= scalar;
		z /= scalar;
		return *this;
	}

	EulerAngles EulerAngles::operator +(const EulerAngles& eulers) const
	{
		return EulerAngles(x + eulers.x, y + eulers.y, z + eulers.z);
	}

	EulerAngles EulerAngles::operator -(const EulerAngles& eulers) const
	{
		return EulerAngles(x - eulers.x, y - eulers.y, z - eulers.z);
	}

	EulerAngles EulerAngles::operator *(const EulerAngles& eulers) const
	{
		return EulerAngles(x * eulers.x, y * eulers.y, z * eulers.z);
	}

	EulerAngles EulerAngles::operator *(float scalar) const
	{
		return EulerAngles((short)round(x * scalar), (short)round(y * scalar), (short)round(z * scalar));
	}

	EulerAngles EulerAngles::operator /(float scalar) const
	{
		return EulerAngles((short)round(x / scalar), (short)round(y / scalar), (short)round(z / scalar));
	}

	float EulerAngles::ClampAlpha(float alpha)
	{
		return std::clamp(alpha, 0.0f, 1.0f);
	}

	bool EulerAngles::Compare(short angle0, short angle1, short epsilon)
	{
		short difference = Geometry::GetShortestAngle(angle0, angle1);
		if (abs(difference) <= epsilon)
			return true;

		return false;
	}

	short EulerAngles::Lerp(short angleFrom, short angleTo, float alpha, short epsilon)
	{
		alpha = ClampAlpha(alpha);

		if (Compare(angleFrom, angleTo, epsilon))
			return angleTo;

		short angleDelta = Geometry::GetShortestAngle(angleFrom, angleTo);
		return (short)round(angleFrom + (angleDelta * alpha));
	}

	short EulerAngles::InterpolateConstant(short angleFrom, short angleTo, short angularVel)
	{
		if (Compare(angleFrom, angleTo, angularVel))
			return angleTo;

		int sign = copysign(1, Geometry::GetShortestAngle(angleFrom, angleTo));
		return (angleFrom + (angularVel * sign));
	}
//}
