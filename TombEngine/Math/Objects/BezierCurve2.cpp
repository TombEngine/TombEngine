#include "framework.h"
#include "Math/Objects/BezierCurve2.h"

namespace TEN::Math
{
	const BezierCurve2 BezierCurve2::Zero      = BezierCurve2(Vector2::Zero, Vector2::Zero, Vector2::Zero, Vector2::Zero);
	const BezierCurve2 BezierCurve2::Linear    = BezierCurve2(Vector2::Zero, Vector2::One, Vector2::Zero, Vector2::One);
	const BezierCurve2 BezierCurve2::EaseIn    = BezierCurve2(Vector2::Zero, Vector2::One, Vector2(0.5f, 0.0f), Vector2::One);
	const BezierCurve2 BezierCurve2::EaseOut   = BezierCurve2(Vector2::Zero, Vector2::One, Vector2::Zero, Vector2(0.5f, 1.0f));
	const BezierCurve2 BezierCurve2::EaseInOut = BezierCurve2(Vector2::Zero, Vector2::One, Vector2(0.5f, 0.0f), Vector2(0.5f, 1.0f));

	BezierCurve2::BezierCurve2(const Vector2& start, const Vector2& end, const Vector2& startHandle, const Vector2& endHandle)
	{
		SetStart(start);
		SetEnd(end);
		SetStartHandle(startHandle);
		SetEndHandle(endHandle);
	}

	const Vector2& BezierCurve2::GetStart() const
	{
		return _controlPoints[0];
	}

	const Vector2& BezierCurve2::GetEnd() const
	{
		return _controlPoints[3];
	}

	const Vector2& BezierCurve2::GetStartHandle() const
	{
		return _controlPoints[1];
	}

	const Vector2& BezierCurve2::GetEndHandle() const
	{
		return _controlPoints[2];
	}

	Vector2 BezierCurve2::GetPoint(float alpha) const
	{
		alpha = std::clamp(alpha, 0.0f, 1.0f);

		// De Casteljau interpolation for point at alpha.
		auto points = _controlPoints;
		for (int i = 1; i < _controlPoints.size(); i++)
		{
			for (int j = 0; j < (_controlPoints.size() - i); j++)
				points[j] = Vector2::Lerp(points[j], points[j + 1], alpha);
		}

		return points.front();
	}

	float BezierCurve2::GetY(float x) const
	{
		constexpr float TOLERANCE           = 0.001f;
		constexpr int   ITERATION_COUNT_MAX = 100;

		// Directly return Y for exact endpoint.
		if (x <= (GetStart().x + TOLERANCE))
		{
			return GetStart().y;
		}
		else if (x >= (GetEnd().x - TOLERANCE))
		{
			return GetEnd().y;
		}

		// Newton-Raphson iteration for approximate Y alpha.
		float alpha = x / GetEnd().x;
		for (int i = 0; i < ITERATION_COUNT_MAX; i++)
		{
			auto point = GetPoint(alpha);
			auto derivative = GetDerivative(alpha);

			if (abs(derivative.x) <= TOLERANCE)
				break;

			float delta = (point.x - x) / derivative.x;
			alpha -= delta;
			if (abs(delta) <= TOLERANCE)
				break;
		}

		return GetPoint(alpha).y;
	}

	void BezierCurve2::SetStart(const Vector2& point)
	{
		_controlPoints[0] = point;
	}

	void BezierCurve2::SetEnd(const Vector2& point)
	{
		_controlPoints[3] = point;
	}

	void BezierCurve2::SetStartHandle(const Vector2& point)
	{
		_controlPoints[1] = point;
	}

	void BezierCurve2::SetEndHandle(const Vector2& point)
	{
		_controlPoints[2] = point;
	}

	Vector2 BezierCurve2::GetDerivative(float alpha) const
	{
		alpha = std::clamp(alpha, 0.0f, 1.0f);

		auto points = _controlPoints;
		int count = (int)_controlPoints.size() - 1;

		// Compute derivative control points.
		for (int i = 0; i < count; i++)
			points[i] = (_controlPoints[i + 1] - _controlPoints[i]) * count;

		// Reduce points using De Casteljau interpolation.
		for (int i = 1; i < count; i++)
		{
			for (int j = 0; j < (count - i); j++)
				points[j] = Vector2::Lerp(points[j], points[j + 1], alpha);
		}

		return points.front();
	}

	bool BezierCurve2::operator ==(const BezierCurve2& curve) const
	{
		return (GetStart() == curve.GetStart() &&
				GetEnd() == curve.GetEnd() &&
				GetStartHandle() == curve.GetStartHandle() &&
				GetEndHandle() == curve.GetEndHandle());
	}

	bool BezierCurve2::operator !=(const BezierCurve2& curve) const
	{
		return !(*this == curve);
	}
}
