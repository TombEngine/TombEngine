// ============================================================================
// SkyQuality.cpp  Accessor for the player's atmospheric sky quality setting.
//
// Isolated in its own translation unit so renderer modules do not need to
// include configuration.h (which transitively drags Windows.h via the sound
// system and would pollute std::min / std::max with macro definitions).
// ============================================================================

#include "framework.h"
#include "Renderer/SkyQuality.h"

#include "Specific/configuration.h"

namespace TEN
{
	AtmosphericSkyQuality GetCurrentSkyQuality()
	{
		int q = (int)g_Configuration.AtmosphericSkyQuality;
		if (q < 0)
			q = 0;
		if (q > (int)AtmosphericSkyQuality::High)
			q = (int)AtmosphericSkyQuality::High;

		return (AtmosphericSkyQuality)q;
	}
}
