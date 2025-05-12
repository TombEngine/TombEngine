#include "framework.h"
#include "Math/Utils.h"

#include "Math/Constants.h"

namespace TEN::Math
{
	float FloorToStep(float value, float step)
	{
		return (floor(value / step) * step);
	}

	float CeilToStep(float value, float step)
	{
		return (ceil(value / step) * step);
	}

	float RoundToStep(float value, float step)
	{
		return (round(value / step) * step);
	}

	float Remap(float value, float min0, float max0, float min1, float max1)
	{
		float alpha = (value - min0) / (max0 - min0);
		return Lerp(min1, max1, alpha);
	}

	Vector3 RoundNormal(const Vector3& normal, float epsilon)
	{
		return Vector3(
			round(normal.x / epsilon),
			round(normal.y / epsilon),
			round(normal.z / epsilon)) * epsilon;
	}

	float Lerp(float value0, float value1, float alpha)
	{
		alpha = std::clamp(alpha, 0.0f, 1.0f);
		return (((1.0f - alpha) * value0) + (alpha * value1));
	}

	float Smoothstep(float value0, float value1, float alpha)
	{
		alpha = std::clamp(alpha, value0, value1);

		// Scale, bias, and saturate alpha to [0, 1] range.
		alpha = std::clamp((alpha - value0) / (value1 - value0), 0.0f, 1.0f);

		// Evaluate polynomial.
		return (CUBE(alpha) * (alpha * ((alpha * 6) - 15.0f) + 10.0f));
	}

	float Smoothstep(float alpha)
	{
		return Smoothstep(0.0f, 1.0f, alpha);
	}

	float EaseInSine(float value0, float value1, float alpha)
	{
		alpha = std::clamp(alpha, 0.0f, 1.0f);
		return Lerp(value0, value1, 1.0f - cos((alpha * PI) / 2));
	}

	float EaseInSine(float alpha)
	{
		return EaseInSine(0.0f, 1.0f, alpha);
	}

	float EaseOutSine(float value0, float value1, float alpha)
	{
		alpha = std::clamp(alpha, 0.0f, 1.0f);
		return Lerp(value0, value1, sin((alpha * PI) / 2));
	}
	
	float EaseOutSine(float alpha)
	{
		return EaseOutSine(0.0f, 1.0f, alpha);
	}

	float EaseInOutSine(float value0, float value1, float alpha)
	{
		alpha = std::clamp(alpha, 0.0f, 1.0f);
		return Lerp(value0, value1, (1.0f - cos(alpha * PI)) / 2);
	}
	
	float EaseInOutSine(float alpha)
	{
		return EaseInOutSine(0.0f, 1.0f, alpha);
	}

	float Luma(const Vector3& color)
	{
		constexpr auto RED_COEFF   = 0.2126f;
		constexpr auto GREEN_COEFF = 0.7152f;
		constexpr auto BLUE_COEFF  = 0.0722f;

		// Use Rec.709 trichromat formula to get perceptive luma value.
		return float((color.x * RED_COEFF) + (color.y * GREEN_COEFF) + (color.z * BLUE_COEFF));
	}

	Vector3 Screen(const Vector3& ambient, const Vector3& tint)
	{
		float luma = Luma(tint);
		auto multiplicative = ambient * tint;
		auto additive = ambient + tint;

		return Vector3(
			Lerp(multiplicative.x, additive.x, luma),
			Lerp(multiplicative.y, additive.y, luma),
			Lerp(multiplicative.z, additive.z, luma));
	}

	Vector4 Screen(const Vector4& ambient, const Vector4& tint)
	{
		auto result = Screen(Vector3(ambient), Vector3(tint));
		return Vector4(result.x, result.y, result.z, ambient.w * tint.w);
	}
}
