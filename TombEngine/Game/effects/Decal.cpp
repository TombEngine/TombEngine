#include "framework.h"
#include "Game/effects/Decal.h"

#include "Game/camera.h"
#include "Game/effects/effects.h"
#include "Game/Lara/lara.h"
#include "Specific/configuration.h"
#include "Specific/level.h"

namespace TEN::Effects::Decal
{
	std::vector<Decal> Decals;

// --- Pending Decals ---

struct PendingDecal
{
	Vector3 pos;
	int room;
	DecalType type;
	int framesLeft;
	bool active = false;
};

constexpr auto MAX_PENDING_DECALS = 16;
static PendingDecal s_PendingDecals[MAX_PENDING_DECALS];

static void UpdatePendingDecals()
{
	for (auto& p : s_PendingDecals)
	{
		if (!p.active)
			continue;
		if (--p.framesLeft <= 0)
		{
			p.active = false;
			SpawnDecal(p.pos, p.room, p.type);
		}
	}
}

	void Decal::UpdateNeighbors()
	{
		Neighbors.fill(NO_VALUE);

		int neighborCount = 0;
		for (int i : g_Level.Rooms[RoomNumber].NeighborRoomNumbers)
		{
			if (g_Level.Rooms[i].Aabb.Intersects(Sphere))
			{
				Neighbors[neighborCount] = i;
				neighborCount++;
			}

			if (neighborCount >= Neighbors.size())
				break;
		}
	}

	void SpawnDecal(Vector3 pos, int roomNumber, DecalType type, int delayFrames)
	{
		if (delayFrames > 0)
		{
			for (auto& p : s_PendingDecals)
			{
				if (p.active)
					continue;
				p.pos = pos;
				p.room = roomNumber;
				p.type = type;
				p.framesLeft = delayFrames;
				p.active = true;
				return;
			}
			// Queue full – spawn immediately as fallback.
		}

		if (!g_Configuration.EnableDecals)
			return;

		auto distance = Vector3::Distance(Camera.pos.ToVector3(), pos);
		if (type == DecalType::BulletHole && !Lara.Control.Look.IsUsingLasersight && distance > COLLISION_CHECK_DISTANCE)
			return;

		auto& decal = GetNewEffect(Decals, Decal::COUNT_MAX);

		auto radius = 1.0f;
		auto opacity = 1.0f;
		auto life = Decal::LIFE_MAX;

		switch (type)
		{
			default:
			case DecalType::BulletHole:
				radius = CLICK(0.2f) * Random::GenerateFloat(0.9f, 1.1f);
				opacity = Random::GenerateFloat(0.4f, 0.6f) * Random::GenerateFloat(0.8f, 1.2f);
				break;

			case DecalType::Explosion:
				radius = CLICK(3.0f) * Random::GenerateFloat(0.7f, 1.3f);
				opacity = Random::GenerateFloat(0.9f, 1.0f) * Random::GenerateFloat(0.8f, 1.2f);
				life *= 2.0f;
				break;
		}

		decal.Type = type;
		decal.StartOpacity = opacity;
		decal.Life = life;
		decal.LifeStartFading = Decal::LIFE_START_FADING;

		decal.Sphere.Center = pos;
		decal.Sphere.Radius = radius;
		decal.RoomNumber = roomNumber;

		decal.UpdateNeighbors();
	}

	void UpdateDecals()
	{
		UpdatePendingDecals();
		if (Decals.empty())
			return;

		for (auto& decal : Decals)
		{
			if (decal.Life <= 0)
				continue;

			if (decal.Life <= decal.LifeStartFading)
			{
				float alpha = 1.0f - ((float)decal.Life / (float)decal.LifeStartFading);
				decal.Opacity = Lerp(decal.StartOpacity, 0.0f, alpha);
			}
			else
			{
				decal.Opacity = decal.StartOpacity;
			}

			decal.Life--;
		}

		if ((int)Decals.size() > Decal::COUNT_THRESHOLD)
		{
			int excess = (int)Decals.size() - Decal::COUNT_THRESHOLD;

			for (auto& decal : Decals)
			{
				if (decal.LifeStartFading <= Decal::LIFE_QUEUE_FADEOUT)
				{
					excess--;
					continue;
				}
			}

			if (excess > 0)
			{
				for (auto& decal : Decals)
				{
					if (decal.LifeStartFading <= Decal::LIFE_QUEUE_FADEOUT)
						continue;

					decal.Life = ((float)decal.Life / (float)decal.LifeStartFading) * Decal::LIFE_QUEUE_FADEOUT;
					decal.LifeStartFading = Decal::LIFE_QUEUE_FADEOUT;
					excess--;

					if (excess <= 0)
						break;
				}
			}
		}

		ClearInactiveEffects(Decals);
	}

	void ClearDecals()
	{
		Decals.clear();
	}
}
