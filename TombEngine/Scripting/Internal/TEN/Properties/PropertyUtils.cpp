#include "framework.h"
#include "Scripting/Internal/TEN/Properties/PropertyUtils.h"

#include "Game/Setup.h"
#include "Objects/game_object_ids.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Specific/level.h"

using namespace TEN::Utils;

namespace TEN::Scripting::Properties
{
	void InitializeProperties()
	{
		// Global object type property application.
		for (int objectID = 0; objectID < ID_NUMBER_OBJECTS; objectID++)
		{
			auto& object = Objects[objectID];
			if (!object.loaded)
				continue;

			object.HitPoints = PropertyHandler::Get((GAME_OBJECT_ID)objectID, GetHash("HitPoints"), object.HitPoints);
		}

		// Per-item property application.
		for (int i = 0; i < g_Level.NumItems; i++)
		{
			auto& item = g_Level.Items[i];
			item.HitPoints = PropertyHandler::Get(item, GetHash("HitPoints"), item.HitPoints);
		}

	}
}
