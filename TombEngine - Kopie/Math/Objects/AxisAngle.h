#pragma once

class EulerAngles;

//namespace TEN::Math
//{
	class AxisAngle
	{
	private:
		// Fields

		Vector3 _axis  = Vector3::Backward;
		short	_angle = 0;

	public:
		// Constants

		static const AxisAngle Identity;

		// Constructors

		AxisAngle() = default;
		AxisAngle(const Vector3& axis, short angle);
		AxisAngle(const EulerAngles& eulers);
		AxisAngle(const Quaternion& quat);
		AxisAngle(const Matrix& rotMatrix);

		// Getters

		Vector3 GetAxis() const;
		short	GetAngle() const;

		// Setters

		void SetAxis(const Vector3& axis);
		void SetAngle(short angle);

		// Utilities

		void			 Slerp(const AxisAngle& axisAngleTo, float alpha);
		static AxisAngle Slerp(const AxisAngle& axisAngleFrom, const AxisAngle& axisAngleTo, float alpha);

		// Converters

		Vector3		ToDirection() const;
		EulerAngles ToEulerAngles() const;
		Quaternion	ToQuaternion() const;
		Matrix		ToRotationMatrix() const;

		// Operators

		bool	   operator ==(const AxisAngle& axisAngle) const;
		bool	   operator !=(const AxisAngle& axisAngle) const;
		AxisAngle& operator =(const AxisAngle& axisAngle);
		AxisAngle& operator *=(const AxisAngle& axisAngle);
		AxisAngle  operator *(const AxisAngle& axisAngle) const;
	};
//}
