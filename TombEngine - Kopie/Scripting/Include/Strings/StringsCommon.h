#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"

using CallbackDrawString = std::function<
	void(
		const std::string&,
		D3DCOLOR,
		Vec2,
		Vec2,  // Area
		float, // Scale
		int)>; // Flags
