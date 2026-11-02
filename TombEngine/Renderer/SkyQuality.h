#pragma once

// ============================================================================
// SkyQuality.h — Global atmospheric sky quality preset.
//
// Single user-facing quality knob that scales all sky-related GPU effects:
//   - AltocumulusMid volumetric clouds (step counts, render scale, temporal).
//   - God rays (radial sample count).
//   - Volumetric dust storm (raymarch step count).
//   - Atmospheric / underwater sky dome (reserved for future tuning).
//
// At High everything keeps its content-author-supplied values. Low and Medium
// clamp the most expensive per-pixel work to cheaper budgets.
// ============================================================================

namespace TEN
{
	enum class AtmosphericSkyQuality
	{
		Low	   = 0,
		Medium = 1,
		High   = 2,

		Count
	};

	// --- Per-quality caps for individually-tunable effects ---

	struct SkyQualityCaps
	{
		int GodRaySampleCountMax    = 128;
		int DustStormStepCountMax   = 12;
	};

	inline SkyQualityCaps GetSkyQualityCaps(AtmosphericSkyQuality quality)
	{
		switch (quality)
		{
		case AtmosphericSkyQuality::Low:
			return SkyQualityCaps{ 20, 4 };

		case AtmosphericSkyQuality::Medium:
			return SkyQualityCaps{ 40, 7 };

		case AtmosphericSkyQuality::High:
		default:
			return SkyQualityCaps{ 128, 12 };
		}
	}

	// Reads g_Configuration.AtmosphericSkyQuality without forcing callers
	// to include the configuration header (which drags in Windows headers
	// that can shadow std::min/std::max via the min/max macros).
	AtmosphericSkyQuality GetCurrentSkyQuality();
}
