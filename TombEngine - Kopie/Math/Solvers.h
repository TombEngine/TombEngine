#pragma once

namespace TEN::Math::Solvers
{
	struct QuadraticSolution
	{
		float Root0 = 0.0f;
		float Root1 = 0.0f;
	};

	struct IK2DSolution
	{
		Vector2 Base   = Vector2::Zero;
		Vector2 Middle = Vector2::Zero;
		Vector2 End	   = Vector2::Zero;
	};

	struct IK3DSolution
	{
		Vector3 Base   = Vector3::Zero;
		Vector3 Middle = Vector3::Zero;
		Vector3 End	   = Vector3::Zero;
	};

	std::optional<QuadraticSolution> SolveQuadratic(float a, float b, float c);
	IK2DSolution					 SolveIK2D(const Vector2& origin, const Vector2& target, float length0, float length1);
	IK3DSolution					 SolveIK3D(const Vector3& origin, const Vector3& target, const Vector3& pole, float length0, float length1);
}
