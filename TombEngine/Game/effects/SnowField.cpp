#include "framework.h"
#include "Game/effects/SnowField.h"

#include <algorithm>
#include <cmath>

#include "Game/Animation/Animation.h"
#include "Game/collision/Point.h"
#include "Game/items.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Setup.h"
#include "Specific/level.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"

namespace TEN::Effects::SnowField
{
	// Foot brush radius in heightmap pixels at default settings. Roughly matches a
	// 32-unit ground footprint for Lara at a 16-block field radius.
	constexpr int FOOT_BRUSH_RADIUS = 4;

	// Maximum stamp intensity per foot impression (0..255 mapped to MaxDepth in shader).
	constexpr unsigned char STAMP_INTENSITY = 255;

	// TEN runs at 30 game ticks per second.
	constexpr float TICK_RATE = 30.0f;

	struct SnowFieldState
	{
		std::vector<unsigned char> Heightmap;
		Vector2 WorldCentre = Vector2::Zero;
		float   WorldRadius = 0.0f;
		bool    Active	    = false;
	};

	static SnowFieldState State;

	void Initialize()
	{
		const auto& settings = g_GameFlow->GetSettings()->Snow;

		if (!settings.Enabled)
		{
			State.Heightmap.clear();
			State.Heightmap.shrink_to_fit();
			State.Active = false;
			return;
		}

		State.Heightmap.assign(RESOLUTION * RESOLUTION, 0);
		State.WorldRadius = (float)std::max(BLOCK(1), settings.FieldRadius);
		State.WorldCentre = Vector2::Zero;
		State.Active = true;
	}

	void Deinitialize()
	{
		State.Heightmap.clear();
		State.Heightmap.shrink_to_fit();
		State.WorldCentre = Vector2::Zero;
		State.WorldRadius = 0.0f;
		State.Active = false;
	}

	bool IsActive()
	{
		return State.Active;
	}

	const std::vector<unsigned char>& GetHeightmap()
	{
		return State.Heightmap;
	}

	Vector2 GetWorldCentre()
	{
		return State.WorldCentre;
	}

	float GetWorldRadius()
	{
		return State.WorldRadius;
	}

	// Stamps an additive circular impression. Higher of existing/incoming value wins.
	static void StampCircle(int cx, int cy, int radius, unsigned char amplitude)
	{
		int xMin = std::max(0, cx - radius);
		int xMax = std::min(RESOLUTION - 1, cx + radius);
		int yMin = std::max(0, cy - radius);
		int yMax = std::min(RESOLUTION - 1, cy + radius);
		int r2 = radius * radius;
		if (r2 == 0)
			return;

		for (int y = yMin; y <= yMax; y++)
		{
			for (int x = xMin; x <= xMax; x++)
			{
				int dx = x - cx;
				int dy = y - cy;
				int d2 = dx * dx + dy * dy;
				if (d2 > r2)
					continue;

				float falloff = 1.0f - ((float)d2 / (float)r2);
				unsigned char stamp = (unsigned char)((float)amplitude * falloff);

				auto& cell = State.Heightmap[y * RESOLUTION + x];
				if (stamp > cell)
					cell = stamp;
			}
		}
	}

	// Translates heightmap pixels so the new world centre matches the player. Newly
	// exposed edges are cleared to zero (pristine snow). Performs whole-pixel shifts
	// only; sub-pixel residue is left in place to avoid blurring.
	static void Recenter(const Vector2& targetCentre)
	{
		float pxPerUnit = (float)RESOLUTION / (State.WorldRadius * 2.0f);

		Vector2 delta = targetCentre - State.WorldCentre;
		int dxPx = (int)(delta.x * pxPerUnit);
		int dyPx = (int)(delta.y * pxPerUnit);

		if (dxPx == 0 && dyPx == 0)
			return;

		std::vector<unsigned char> shifted(RESOLUTION * RESOLUTION, 0);
		for (int y = 0; y < RESOLUTION; y++)
		{
			int srcY = y + dyPx;
			if (srcY < 0 || srcY >= RESOLUTION)
				continue;

			for (int x = 0; x < RESOLUTION; x++)
			{
				int srcX = x + dxPx;
				if (srcX < 0 || srcX >= RESOLUTION)
					continue;

				shifted[y * RESOLUTION + x] = State.Heightmap[srcY * RESOLUTION + srcX];
			}
		}

		State.Heightmap.swap(shifted);
		State.WorldCentre += Vector2((float)dxPx / pxPerUnit, (float)dyPx / pxPerUnit);
	}

	// Linear decay toward zero. DecayRate is interpreted as fraction-per-second of
	// the full byte range, so 1.0 erases a fresh stamp in roughly one second.
	static void ApplyDecay()
	{
		const auto& settings = g_GameFlow->GetSettings()->Snow;
		if (settings.DecayRate <= 0.0f)
			return;

		float perTick = settings.DecayRate / TICK_RATE;
		int dec = (int)std::ceil(perTick * 255.0f);
		if (dec <= 0)
			return;

		for (auto& v : State.Heightmap)
			v = (v > dec) ? (unsigned char)((int)v - dec) : (unsigned char)0;
	}

	void Update(const ItemInfo& player)
	{
		if (!State.Active)
			return;

		auto playerPos = Vector2((float)player.Pose.Position.x, (float)player.Pose.Position.z);
		Recenter(playerPos);
		ApplyDecay();

		auto lFoot = GetJointPosition(player, LM_LFOOT);
		auto rFoot = GetJointPosition(player, LM_RFOOT);

		float pxPerUnit = (float)RESOLUTION / (State.WorldRadius * 2.0f);
		Vector2 origin = State.WorldCentre - Vector2(State.WorldRadius, State.WorldRadius);

		// Only stamp the player's feet when she is actually on the ground; skip while
		// jumping, falling or otherwise airborne so trails do not appear under her.
		auto playerPoint = TEN::Collision::Point::GetPointCollision(player);
		int  playerFloorY = playerPoint.GetFloorHeight();
		bool playerOnGround = (player.Pose.Position.y >= playerFloorY - CLICK(1));

		if (playerOnGround)
		{
			int lx = (int)(((float)lFoot.x - origin.x) * pxPerUnit);
			int ly = (int)(((float)lFoot.z - origin.y) * pxPerUnit);
			int rx = (int)(((float)rFoot.x - origin.x) * pxPerUnit);
			int ry = (int)(((float)rFoot.z - origin.y) * pxPerUnit);

			StampCircle(lx, ly, FOOT_BRUSH_RADIUS, STAMP_INTENSITY);
			StampCircle(rx, ry, FOOT_BRUSH_RADIUS, STAMP_INTENSITY);
		}

		// All other active moveables impress the snow using their object's collision
		// radius (Objects[id].radius). Skip the player (already stamped above), inactive
		// items, items without geometry, and anything not touching the floor (flying
		// enemies, jumping characters, projectiles in the air, etc.).
		int playerIndex = player.Index;

		// Threshold for "on the ground": item must be within this many world units of
		// the actual floor at its XZ. Equals one click; a single step of allowance.
		constexpr int GROUND_TOLERANCE = CLICK(1);

		for (int i = 0; i < g_Level.Items.size(); i++)
		{
			const auto& item = g_Level.Items[i];

			if (i == playerIndex)
				continue;

			if (item.Status != ITEM_ACTIVE || !item.Active)
				continue;

			if (item.ObjectNumber == ID_NO_OBJECT)
				continue;

			int worldRadius = Objects[item.ObjectNumber].radius;
			if (worldRadius <= 0)
				continue;

			// Vertical reject: only stamp when the item is actually standing on (or
			// very close to) the floor at its XZ. Catches flying enemies and any
			// airborne movables (incl. Lara mid-jump for non-player items).
			auto pointColl = TEN::Collision::Point::GetPointCollision(item);
			int floorY = pointColl.GetFloorHeight();

			if (item.Pose.Position.y < floorY - GROUND_TOLERANCE)
				continue;

			Stamp(item.Pose.Position.ToVector3(), (float)worldRadius, 1.0f);
		}
	}

	void Stamp(const Vector3& worldPos, float worldRadius, float depth)
	{
		if (!State.Active || worldRadius <= 0.0f)
			return;

		// Reject stamps that fall fully outside the moving heightmap window.
		float dx = worldPos.x - State.WorldCentre.x;
		float dz = worldPos.z - State.WorldCentre.y;
		float reach = State.WorldRadius + worldRadius;
		if (dx * dx + dz * dz > reach * reach)
			return;

		float pxPerUnit = (float)RESOLUTION / (State.WorldRadius * 2.0f);
		Vector2 origin = State.WorldCentre - Vector2(State.WorldRadius, State.WorldRadius);

		int cx = (int)((worldPos.x - origin.x) * pxPerUnit);
		int cy = (int)((worldPos.z - origin.y) * pxPerUnit);
		int radiusPx = std::max(1, (int)(worldRadius * pxPerUnit));

		float clamped = std::clamp(depth, 0.0f, 1.0f);
		unsigned char amplitude = (unsigned char)(clamped * 255.0f);
		if (amplitude == 0)
			return;

		StampCircle(cx, cy, radiusPx, amplitude);
	}
}
