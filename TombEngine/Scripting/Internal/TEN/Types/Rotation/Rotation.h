#pragma once

#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"

class EulerAngles;
class Pose;

namespace sol { class state; }

namespace TEN::Scripting
{
	class Rotation
	{
	public:
		static void Register(sol::table& parent);

		// Fields

		float x = 0;
		float y = 0;
		float z = 0;

		// Constructors

		Rotation() = default;
		Rotation(float x, float y, float z);
		Rotation(const Vector3& vec);
		Rotation(const EulerAngles& eulers);

		// Utilities

		Rotation Lerp(const Rotation& rot, float alpha) const;
		Vec3	 Direction() const;

		// Converters

		std::string ToString() const;
		EulerAngles ToEulerAngles() const;

		// Operators

		operator Vector3() const;

		bool	  operator ==(const Rotation& rot) const;
		Rotation  operator +(const Rotation& rot) const;
		Rotation  operator -(const Rotation& rot) const;
		Rotation& operator +=(const Rotation& rot);
		Rotation& operator -=(const Rotation& rot);

	private:
		// Helpers

		float WrapAngle(float angle) const;
	};
}
