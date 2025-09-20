#include "framework.h"
#include "Math/Objects/AxisAngle.h"

#include "Math/Constants.h"
#include "Math/Geometry.h"
#include "Math/Legacy.h"
#include "Math/Objects/EulerAngles.h"

using namespace TEN::Math;

//namespace TEN::Math
//{
	const AxisAngle AxisAngle::Identity = AxisAngle(Vector3::Backward, 0);

	AxisAngle::AxisAngle(const Vector3& axis, short angle)
	{
		auto normalizedAxis = axis;
		normalizedAxis.Normalize();

		_axis = normalizedAxis;
		_angle = angle;
	}

	AxisAngle::AxisAngle(const EulerAngles& eulers)
	{
		*this = AxisAngle(eulers.ToQuaternion());
	}

	// NOTE: Some precision drift may occur.
	AxisAngle::AxisAngle(const Quaternion& quat)
	{
		float scale = sqrt(1.0f - (quat.w * quat.w));
		auto axis = Vector3(quat) / scale;
		axis.Normalize();
		float angle = 2.0f * acos(quat.w);

		_axis = axis;
		_angle = FROM_RAD(angle);
	}

	AxisAngle::AxisAngle(const Matrix& rotMatrix)
	{
		// Decompose matrix into quaternion.
		auto scale = Vector3::Zero;
		auto quat = Quaternion::Identity;
		auto translation = Vector3::Zero;
		auto rotMatrixCopy = rotMatrix;
		rotMatrixCopy.Decompose(scale, quat, translation);

		// Convert quaternion to AxisAngle.
		*this = AxisAngle(quat);

		// Extract rotation axis from matrix.
		auto rotAxis = Vector3::TransformNormal(Vector3::Right, rotMatrix);
		
		// Check if rotation axis and unit axis are pointing in opposite directions.
		float dot = rotAxis.Dot(_axis);
		if (dot < 0.0f)
		{
			// Negate angle and unit axis to ensure angle stays within [0, PI] range.
			_angle = -_angle;
			_axis = -_axis;
		}
	}

	Vector3 AxisAngle::GetAxis() const
	{
		return _axis;
	}

	short AxisAngle::GetAngle() const
	{
		return _angle;
	}

	void AxisAngle::SetAxis(const Vector3& axis)
	{
		auto normalizedAxis = axis;
		normalizedAxis.Normalize();
		
		_axis = normalizedAxis;
	}

	void AxisAngle::SetAngle(short angle)
	{
		_angle = angle;
	}

	void AxisAngle::Slerp(const AxisAngle& axisAngleTo, float alpha)
	{
		*this = Slerp(*this, axisAngleTo, alpha);
	}

	AxisAngle AxisAngle::Slerp(const AxisAngle& axisAngleFrom, const AxisAngle& axisAngleTo, float alpha)
	{
		auto axis = Vector3::Zero;
		float angle = 0.0f;

		// If angle between axes is close to 0, do simple interpolation of angle values.
		float angleBetweenAxes = acos(axisAngleFrom.GetAxis().Dot(axisAngleTo.GetAxis()));
		if (abs(angleBetweenAxes) <= EPSILON)
		{
			axis = axisAngleFrom.GetAxis();
			angle = TO_RAD(axisAngleFrom.GetAngle()) + (TO_RAD(axisAngleTo.GetAngle() - axisAngleFrom.GetAngle()) * alpha);
			return AxisAngle(axis, FROM_RAD(angle));
		}

		float sinAngle = sin(angleBetweenAxes);
		float weight0 = sin((1.0f - alpha) * angleBetweenAxes) / sinAngle;
		float weight1 = sin(alpha * angleBetweenAxes) / sinAngle;

		// Slerp axes and angles.
		axis = (axisAngleFrom.GetAxis() * weight0) + (axisAngleTo.GetAxis() * weight1);
		angle = (TO_RAD(axisAngleFrom.GetAngle()) * weight0) + (TO_RAD(axisAngleTo.GetAngle()) * weight1);
		return AxisAngle(axis, FROM_RAD(angle));
	}

	Vector3 AxisAngle::ToDirection() const
	{
		// TODO: Works, but need to find a way without EulerAngles. -- Sezz 2023.03.08
		auto refDir = Geometry::RotatePoint(Vector3::Right, EulerAngles(_axis));
		return Geometry::RotatePoint(refDir, *this);
	}

	EulerAngles AxisAngle::ToEulerAngles() const
	{
		return EulerAngles(*this);
	}

	Quaternion AxisAngle::ToQuaternion() const
	{
		return Quaternion::CreateFromAxisAngle(_axis, TO_RAD(_angle));
	}

	Matrix AxisAngle::ToRotationMatrix() const
	{
		return Matrix::CreateFromAxisAngle(_axis, TO_RAD(_angle));
	}

	bool AxisAngle::operator ==(const AxisAngle& axisAngle) const
	{
		return ((_axis == axisAngle.GetAxis()) && (_angle == axisAngle.GetAngle()));
	}

	bool AxisAngle::operator !=(const AxisAngle& axisAngle) const
	{
		return !(*this == axisAngle);
	}

	AxisAngle& AxisAngle::operator =(const AxisAngle& axisAngle)
	{
		_axis = axisAngle.GetAxis();
		_angle = axisAngle.GetAngle();
		return *this;
	}

	AxisAngle& AxisAngle::operator *=(const AxisAngle& axisAngle)
	{
		*this = *this * axisAngle;
		return *this;
	}

	AxisAngle AxisAngle::operator *(const AxisAngle& axisAngle) const
	{
		auto quat0 = ToQuaternion();
		auto quat1 = axisAngle.ToQuaternion();
		return AxisAngle(quat0 * quat1);
	}
//}
