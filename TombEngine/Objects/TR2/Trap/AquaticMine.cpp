#include "framework.h"
#include "Objects/TR2/Trap/AquaticMine.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/Sphere.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Scripting/Internal/TEN/Properties/PropertyNames.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Sphere;
using namespace TEN::Effects::Environment;
using namespace TEN::Math;
using namespace TEN::Math::Random;
using namespace TEN::Scripting::Properties;

namespace TEN::Entities::Traps
{
	constexpr auto AQUATIC_MINE_HARM_MESH = 2;
	constexpr auto MINE_CHAIN_EXPLOSION_RADIUS = BLOCK(1); // 1 sector for chain explosions.

	void InitializeAquaticMine(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
	}

	void ControlAquaticMine(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (TestLastFrame(item, 0))
			return;

		AnimateItem(&item);
	}

	void CollideAquaticMine(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.Status == ITEM_INVISIBLE)
			return;

		if (!TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
			return;

		if (!HandleItemSphereCollision(item, *playerItem))
			return;

		if (!item.TouchBits.TestAny())
			return;

		// Prüfe, ob der HARM_MESH (Bit-Maske) berührt wurde.
		auto spheres = item.GetSpheres();
		bool harmMeshTouched = false;
		for (int i = 0; i < spheres.size(); i++)
		{
			if (item.TouchBits.Test(i) && ((AQUATIC_MINE_HARM_MESH >> i) & 1))
			{
				harmMeshTouched = true;
				break;
			}
		}

		if (!harmMeshTouched)
			return;

		// Nur Lara schwimmend (kein Fahrzeug).
		auto* lara = GetLaraInfo(playerItem);
		if (lara->Context.Vehicle != NO_VALUE)
			return;

		if (PropertyHandler::Get(item, PropName_HarmPlayer, false, true))
		{
			TriggerMineExplosion(itemNumber);
			DoDamage(playerItem, INT_MAX);
		}
	}

	void TriggerMineExplosion(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!item.Active)
			return;

		if (item.Status == ITEM_INVISIBLE || item.Status == ITEM_DEACTIVATED)
			return;

		// Deactivate immediately to prevent re-triggering before effects play.
		item.Active = false;

		// Use the position of the harm mesh (mesh 2) for explosion effects.
		auto spheres = item.GetSpheres();
		Vector3i explosionPos = item.Pose.Position;
		if (AQUATIC_MINE_HARM_MESH < (int)spheres.size())
		{
			auto pos = spheres[AQUATIC_MINE_HARM_MESH].Center;
			explosionPos = Vector3i((int)pos.x, (int)pos.y, (int)pos.z);
		}

		SoundEffect(SFX_TR4_EXPLOSION1, &item.Pose);
		SoundEffect(SFX_TR4_EXPLOSION2, &item.Pose);

		TriggerExplosionSparks(explosionPos.x, explosionPos.y, explosionPos.z, 3, -2, 0, item.RoomNumber);
		TriggerExplosionSparks(explosionPos.x, explosionPos.y, explosionPos.z, 3, -1, 0, item.RoomNumber);

		auto shockwavePose = Pose(explosionPos);
		TriggerShockwave(&shockwavePose, 48, 304, (GetRandomControl() & 0x1F) + 112, 255, 160, 0, 32, EulerAngles(2048, 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);

		Weather.Flash(255, 192, 64, 0.03f);

		KillItem(itemNumber);

		// Ketten-Explosion: Andere Minen im 1-Sector-Umkreis triggern.
		for (short i = 0; i < g_Level.Items.size(); i++)
		{
			if (i == itemNumber)
				continue;

			auto& other = g_Level.Items[i];

			if (other.ObjectNumber != ID_UNDERWATER_MINE)
				continue;

			if (!other.Active)
				continue;

			if (other.Status == ITEM_INVISIBLE || other.Status == ITEM_DEACTIVATED)
				continue;

			if (Vector3i::Distance(item.Pose.Position, other.Pose.Position) > MINE_CHAIN_EXPLOSION_RADIUS)
				continue;

			TriggerMineExplosion(i);
		}
	}

	void TriggerNearbyMines(const Vector3i& position)
	{
		for (short i = 0; i < g_Level.Items.size(); i++)
		{
			auto& other = g_Level.Items[i];

			if (other.ObjectNumber != ID_UNDERWATER_MINE)
				continue;

			if (!other.Active)
				continue;

			if (other.Status == ITEM_INVISIBLE || other.Status == ITEM_DEACTIVATED)
				continue;

			// Use the bounding sphere of the harm mesh as the detection radius.
			auto& obj = Objects[ID_UNDERWATER_MINE];
			auto meshIndex = obj.meshIndex + AQUATIC_MINE_HARM_MESH;
			if (meshIndex >= g_Level.Meshes.size())
				continue;



			TriggerMineExplosion(i);
			return;
		}
	}
}
