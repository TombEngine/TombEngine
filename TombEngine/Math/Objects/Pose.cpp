#include "framework.h"
#include "Math/Objects/Pose.h"

#include "Math/Geometry.h"
#include "Math/Objects/EulerAngles.h"
#include "Math/Objects/Vector3i.h"

using namespace TEN::Math;

//namespace TEN::Math
//{
	const Pose Pose::Zero = Pose(Vector3i::Zero, EulerAngles::Identity, Vector3::One);

	Pose::Pose(const Vector3i& pos, const EulerAngles& orient, const Vector3& scale)
	{
		Position = pos;
		Orientation = orient;
		Scale = scale;
	}

	void Pose::Translate(short headingAngle, float forward, float down, float right)
	{
		Position = Geometry::TranslatePoint(Position, headingAngle, forward, down, right);
	}

	void Pose::Translate(const EulerAngles& orient, float dist)
	{
		Position = Geometry::TranslatePoint(Position, orient, dist);
	}

	void Pose::Translate(const Vector3& dir, float dist)
	{
		Position = Geometry::TranslatePoint(Position, dir, dist);
	}

	Matrix Pose::ToMatrix() const
	{
		auto translationMatrix = Matrix::CreateTranslation(Position.ToVector3());
		auto rotMatrix = Orientation.ToRotationMatrix();
		auto scaleMatrix = Matrix::CreateScale(Scale);
		return (scaleMatrix * rotMatrix * translationMatrix);
	}

	bool Pose::operator ==(const Pose& pose) const
	{
		return (Position == pose.Position && Orientation == pose.Orientation && Scale == pose.Scale);
	}

	bool Pose::operator !=(const Pose& pose) const
	{
		return !(*this == pose);
	}
//}
