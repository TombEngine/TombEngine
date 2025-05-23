#pragma once

#include "Scripting/Internal/TEN/Types/Time/Time.h"

namespace sol{ class state; }

namespace TEN::Scripting
{
	struct Statistics
	{
		Time		 TimeTaken	 = {};
		unsigned int Distance	 = 0;
		unsigned int AmmoHits	 = 0;
		unsigned int AmmoUsed	 = 0;
		unsigned int HealthUsed	 = 0;
		unsigned int DamageTaken = 0;
		unsigned int Kills		 = 0;
		unsigned int Pickups	 = 0;
		unsigned int Secrets	 = 0;

		static void Register(sol::table&);
	};
}
