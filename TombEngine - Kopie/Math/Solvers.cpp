#include "framework.h"
#include "Math/Solvers.h"

#include "Math/Constants.h"

namespace TEN::Math::Solvers
{
	std::optional<QuadraticSolution> SolveQuadratic(float a, float b, float c)
	{
		if (abs(a) < EPSILON)
		{
			// 0 solutions.
			if (abs(b) < EPSILON)
				return std::nullopt;

			// 1 solution.
			float root = -c / b;
			return QuadraticSolution{ root, root };
		}

		// 0 solutions.
		float discriminant = SQUARE(b) - (4.0f * a * c);
		if (discriminant < 0.0f)
			return std::nullopt;

		float inv2a = 1.0f / (2.0f * a);

		// 1 solution.
		if (discriminant < EPSILON)
		{
			float root = -b * inv2a;
			return QuadraticSolution{ root, root };
		}

		// 2 solutions.
		discriminant = sqrt(discriminant);
		float root0 = (-b - discriminant) * inv2a;
		float root1 = (-b + discriminant) * inv2a;
		return QuadraticSolution{ root0, root1 };
	}

	IK2DSolution SolveIK2D(const Vector2& origin, const Vector2& target, float length0, float length1)
	{
		auto scaledTarget = target;
		auto lengthMax = length0 + length1;
		auto dir = target - origin;
		dir.Normalize();

		// Check if target is within reach.
		float dist = Vector2::Distance(origin, target);
		if (dist > lengthMax)
			scaledTarget = origin + (dir * lengthMax);

		// Ensure line slope is well defined.
		bool flipXY = (scaledTarget.x < scaledTarget.y);

		float a = flipXY ? (scaledTarget.y - origin.y) : (scaledTarget.x - origin.x);
		float b = flipXY ? (scaledTarget.x - origin.x) : (scaledTarget.y - origin.y);
		TENAssert(abs(a) >= EPSILON, "SolveIK2D() failed.");

		float m = ((SQUARE(length0) - SQUARE(length1)) + (SQUARE(a) + SQUARE(b))) / (2.0f * a);
		float n = b / a;

		auto quadraticSol = SolveQuadratic(1.0f + SQUARE(n), -2.0f * (m * n), SQUARE(m) - SQUARE(length0));
		auto middle = Vector2::Zero;

		// Solution is valid; define middle accurately.
		if (quadraticSol.has_value())
		{
			middle = origin + (flipXY ?
				Vector2(quadraticSol->Root1, (m - (n * quadraticSol->Root1))) :
				Vector2(quadraticSol->Root0, (m - (n * quadraticSol->Root0))));
		}
		// Solution is invalid: define middle as point on line segment between origin and target.
		else
		{
			middle = origin + (dir * length0);
		}

		return IK2DSolution{ origin, middle, scaledTarget };
	}

	IK3DSolution SolveIK3D(const Vector3& origin, const Vector3& target, const Vector3& pole, float length0, float length1)
	{
		auto dir = (target - origin);
		dir.Normalize();
		auto normal = dir.Cross(pole - origin);
		normal.Normalize();

		// Construct transform matrix.
		auto tMatrix = Matrix(
			Vector4(normal.Cross(dir)),
			Vector4(dir),
			Vector4(normal),
			Vector4(origin.x, origin.y, origin.z, 1.0f));

		// Get relative 2D IK solution.
		auto inverseMatrix = tMatrix.Invert();
		auto relTarget = Vector3::Transform(target, inverseMatrix);
		auto ikSolution2D = SolveIK2D(Vector2::Zero, Vector2(relTarget), length0, length1);

		// Calculate absolute middle position.
		auto relMiddle = Vector3(ikSolution2D.Middle.x, ikSolution2D.Middle.y, 0.0f);
		auto middle = Vector3::Transform(relMiddle, tMatrix);

		// Calculate absolute end position.
		auto localEnd = Vector3(ikSolution2D.End.x, ikSolution2D.End.y, 0.0f);
		auto end = Vector3::Transform(localEnd, tMatrix);

		return IK3DSolution{ origin, middle, end };
	}
}
