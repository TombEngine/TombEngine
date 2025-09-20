#pragma once
#include "Math/Objects/EulerAngles.h"
#include "Math/Objects/Vector3i.h"

//namespace TEN::Math
//{
	// TODO: Rename to "Transform".
	class Pose
	{
	public:
		// Fields

		Vector3i	Position	= Vector3i::Zero;
		EulerAngles Orientation = EulerAngles::Identity;
		Vector3		Scale		= Vector3::One;

		// Constants

		static const Pose Zero;

		// Constructors

		Pose() = default;
		Pose(const Vector3i& pos, const EulerAngles& orient = EulerAngles::Identity, const Vector3& scale = Vector3::One);

		// Utilities

		void Translate(short headingAngle, float forward, float down = 0.0f, float right = 0.0f);
		void Translate(const EulerAngles& orient, float dist);
		void Translate(const Vector3& dir, float dist);

		// Converters

		Matrix ToMatrix() const;

		// Operators

		bool operator ==(const Pose& pose) const;
		bool operator !=(const Pose& pose) const;
	};
//}
