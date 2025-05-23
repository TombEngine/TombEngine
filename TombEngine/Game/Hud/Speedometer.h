#pragma once

namespace TEN::Hud
{
	class SpeedometerController
	{
	private:
		// Constants

		static constexpr auto LIFE_MAX = 0.75f;

		// Fields

		bool _hasValueUpdated = false;

		float _value		= 0.0f;
		short _pointerAngle = 0;
		float _opacity		= 0.0f;
		float _life			= 0.0f;

		short _prevPointerAngle = 0;
		float _prevOpacity = 0.0f;

		void StoreInterpolationData()
		{
			_prevPointerAngle = _pointerAngle;
			_prevOpacity = _opacity;
		}

	public:
		// Utilities

		void UpdateValue(float value);

		void Update();
		void Draw() const;
		void Clear();

		void DrawDebug() const;
	};
}
