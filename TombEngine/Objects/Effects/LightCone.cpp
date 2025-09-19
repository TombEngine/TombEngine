#include "framework.h"
#include "Objects/effects/LightCone.h"

#include "Game/collision/Los.h"
#include "Game/collision/Point.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/effects/spark.h"
#include "Game/control/Los.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Specific/level.h"
#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/people.h"
#include "Objects/TR5/Trap/LaserBeam.h"

using namespace TEN::Entities::Traps;
using namespace TEN::Collision::Los;
using namespace TEN::Collision::Point;
using namespace TEN::Effects::Spark;
using namespace TEN::Math;
using namespace TEN::Renderer;

namespace TEN::Effects::LightCone
{
	// --------- Pool ---------
	static std::array<LightConeSlot, MAX_TRANSIENT_LIGHT_CONES> GLightConePool{};

	// --------- Helpers ---------

	// Nächstliegenden Treffer über Room / Items / Statics bestimmen.
	struct WorldRayHit
	{
		bool    Hit = false;
		Vector3 Position = Vector3::Zero;
		int     Room = NO_VALUE;
		float   Distance = 0.0f;
	};

	static WorldRayHit CastWorldRayClosest(
		const Vector3& origin, int roomNumber,
		const Vector3& dir, float dist,
		bool collideItems, bool collideStatics, bool collideBridges)
	{
		WorldRayHit out{};

		// Room-Geom zuerst (mit/ohne Bridges)
		auto roomLos = GetRoomLosCollision(origin, roomNumber, dir, dist, collideBridges);
		float best = dist;
		if (roomLos.IsIntersected)
		{
			out.Hit = true;
			out.Position = roomLos.Position;
			out.Room = roomLos.RoomNumber;
			out.Distance = roomLos.Distance;
			best = roomLos.Distance;
		}

		// Aggregierte Treffer: Items/Spheres/Statics (wir brauchen hier nur Items+Statics).
		auto los = GetLosCollision(origin, roomNumber, dir, dist,
			/*collideItems*/ collideItems,
			/*collideSpheres*/ false,
			/*collideStatics*/ collideStatics);

		if (collideItems)
		{
			for (const auto& il : los.Items) // sortiert
			{
				if (il.Distance < best)
				{
					best = il.Distance;
					out.Hit = true;
					out.Position = il.Position;
					out.Room = il.RoomNumber;
					out.Distance = il.Distance;
				}
				else break;
			}
		}
		if (collideStatics)
		{
			for (const auto& st : los.Statics) // sortiert
			{
				if (st.Distance < best)
				{
					best = st.Distance;
					out.Hit = true;
					out.Position = st.Position;
					out.Room = st.RoomNumber;
					out.Distance = st.Distance;
				}
				else break;
			}
		}

		if (!out.Hit)
		{
			out.Position = origin + dir * dist;
			out.Room = roomNumber;
			out.Distance = dist;
		}

		return out;
	}

	// Lara-Test entlang eines Strahls: nutzt GetSphereLosCollision (nur Spieler)
	static bool RayHitsLaraSpheres(const Vector3& origin, int roomNumber, const Vector3& dir, float dist)
	{
		// true => Spieler-Spheres werden berücksichtigt
		auto s = GetSphereLosCollision(origin, roomNumber, dir, dist, /*collidePlayer*/ true);
		if (!s.has_value())
			return false;
		// Sicherheitshalber prüfen
		return (s->Item && s->Item->IsLara() && s->Distance <= dist + 1e-3f);
	}

	// Freien Slot aus dem Pool holen
	static int AcquireIndex()
	{
		int freeIndex = -1;

		for (int i = 0; i < MAX_TRANSIENT_LIGHT_CONES; ++i)
		{
			const auto& s = GLightConePool[i];
			if (!s.On || !s.Effect.IsActive || s.Life <= 0.0f)
			{
				freeIndex = i;
				break;
			}
		}
		if (freeIndex != -1)
			return freeIndex;

		// Recycle: kürzeste Life
		float shortest = FLT_MAX;
		int best = 0;
		for (int i = 0; i < MAX_TRANSIENT_LIGHT_CONES; ++i)
		{
			const auto& s = GLightConePool[i];
			if (s.Life < shortest)
			{
				shortest = s.Life;
				best = i;
			}
		}
		return best;
	}

	// --------- API ---------

	void EmitTransientLightCone(
		const GameVector& apex,
		const EulerAngles& orientation,
		float startRadius, float endRadius,
		float maxLength,
		const Vector4& color,
		float lifeSeconds,
		bool collideLara,
		bool collideItems,
		bool collideStatics,
		bool collideBridges)
	{
		const int idx = AcquireIndex();
		auto& slot = GLightConePool[idx];

		// Grunddaten
		slot.Pos = apex.ToVector3();
		// robusten Startraum bestimmen
		slot.RoomNumber = apex.RoomNumber;

		slot.Orientation = orientation;
		slot.Effect.Color = color;
		slot.Effect.IsActive = true;
		slot.On = true;
		slot.Life = 2.0f;

		// Form
		constexpr auto RADIUS_STEP = BLOCK(0.002f); // wenn du Konsistenz mit Laser willst
		slot.StartRadius = std::max(0.0f, startRadius) * RADIUS_STEP;
		slot.EndRadius = std::max(0.0f, endRadius) * RADIUS_STEP;
		slot.MaxLength = (maxLength > 0.0f ? maxLength : MAX_VISIBILITY_DISTANCE);

		// Flags
		slot.CollideLara = collideLara;
		slot.CollideItems = collideItems;
		slot.CollideStatics = collideStatics;
		slot.CollideBridges = collideBridges;

		// Interpolation sichern
		slot.Effect.StoreInterpolationData();

		// Richtung/Rotation + Bias
		auto dir = slot.Orientation.ToDirection();
		dir.Normalize(dir);
		auto rot = slot.Orientation.ToRotationMatrix();

		// Startraum robust halten

		const Vector3 origin = slot.Pos + dir * 4.0f; // kleiner Vorwärts-Bias
		const float   maxLen = (slot.MaxLength > 0.0f ? slot.MaxLength : MAX_VISIBILITY_DISTANCE);

		// Mittelachse primär für Licht/Backup-Ende
		auto centerHit = GetRoomLosCollision(origin, slot.RoomNumber, dir, maxLen, slot.CollideBridges);
		Vector3 centerEnd = centerHit.IsIntersected ? centerHit.Position : (origin + dir * maxLen);
		int     centerRm = centerHit.IsIntersected ? centerHit.RoomNumber : slot.RoomNumber;

		// Pro-Vertex Rim-LOS + optionale Lara-Sphere-Checks
		bool laraHit = false;
		float a = 0.0f;
		for (int i = 0; i < LightConeEffect::SUBDIVISION_COUNT; ++i)
		{
			const float si = sin(a), co = cos(a);

			const Vector3 rel0 = Vector3(slot.StartRadius * si, slot.StartRadius * co, 0.0f);
			const Vector3 rel1 = Vector3(slot.EndRadius * si, slot.EndRadius * co, 0.0f);

			const Vector3 vStart = origin + Vector3::Transform(rel0, rot);
			const Vector3 vEndT = centerEnd + Vector3::Transform(rel1, rot);

			Vector3 rayDir = vEndT - vStart;
			float   rayLen = rayDir.Length();
			if (rayLen <= 0.001f) { rayDir = dir; rayLen = maxLen; }
			else { rayDir /= rayLen; }

			// Welt-Treffer (Room/Items/Statics)
			auto wh = CastWorldRayClosest(vStart, slot.RoomNumber, rayDir, rayLen, slot.CollideItems, slot.CollideStatics, slot.CollideBridges);
			Vector3 vEnd = wh.Position;
			int     vEndRm = wh.Room;

			// Lara-Sphere-Check (sparsam)
			if (!laraHit && slot.CollideLara)
			{
				if (RayHitsLaraSpheres(vStart, slot.RoomNumber, rayDir, std::min(wh.Distance, rayLen)))
				{
					laraHit = true;

					// Grobes Feedback (Lichtimpuls)
					//TEN::Entities::Traps::SpawnLaserBeamLight(vEnd, vEndRm, s.Effect.Color, 0.2f, 0.1f);

					// Schaden/Effekte wie beim Laser möchtest du evtl. NICHT im Licht;
					// falls doch, kannst du hier analog reagieren.
				}
			}

			// Vertices schreiben
			slot.Effect.Vertices[i] = vStart;
			slot.Effect.Vertices[LightConeEffect::SUBDIVISION_COUNT + i] = vEnd;

			a += PI_MUL_2 / LightConeEffect::SUBDIVISION_COUNT;
		}



		// Interp stabilisieren
		slot.Effect.StoreInterpolationData();
	}

	void UpdateTransientLightCones()
	{

		for (int i = 0; i < MAX_TRANSIENT_LIGHT_CONES; ++i)
		{
			auto& s = GLightConePool[i];

			s.Life -= 1.2f;
			if (!s.On || s.Life <= 0.0f || !s.Effect.IsActive)
				continue;

			if (s.Life <= 0.0f)
			{
				s.On = false;
				s.Effect.IsActive = false;
				continue;
			}
		}
	}

	const std::array<LightConeSlot, MAX_TRANSIENT_LIGHT_CONES>& GetTransientLightConePool()
	{
		return GLightConePool;
	}

	void ClearLightConeEffects()
	{
		for (auto& s : GLightConePool)
			s = {};
	}
}
