#pragma once
#include "Math/Math.h"

using namespace TEN::Math;

struct ItemInfo;
struct CollisionInfo;

namespace TEN::Effects::LightCone
{
	// Stell das gern global passend ein

	static constexpr int MAX_TRANSIENT_LIGHT_CONES = 64;


	struct LightConeEffect
	{
		static constexpr auto SUBDIVISION_COUNT = 8;

		Vector4 Color = Vector4(1, 1, 1, 1);

		std::array<Vector3, SUBDIVISION_COUNT * 2> Vertices{};
		std::array<Vector3, SUBDIVISION_COUNT * 2> OldVertices{};

		bool IsActive = false;

		void StoreInterpolationData()
		{
			OldVertices = Vertices;
		}
	};

	struct LightConeSlot
	{
		LightConeEffect Effect;

		// Pose
		Vector3     Pos = Vector3::Zero; // Apex
		int         RoomNumber = 0;
		EulerAngles Orientation;

		// Form
		float StartRadius = 0.0f; // Weltmaß (ggf. skaliert mit BLOCK(..))
		float EndRadius = 0.0f; // Weltmaß
		float MaxLength = 0.0f; // Max. Reichweite (Weltmaß, 0 => MAX_VISIBILITY_DISTANCE)

		// Laufzeit
		float Life = 0.0f;
		bool  On = false;

		// Kollisions-/LOS-Flags
		bool CollideLara = true;
		bool CollideItems = false;
		bool CollideStatics = true;
		bool CollideBridges = true;
	};

	// ——— API ———

	// Emittiert (oder "refresht") einen transienten Lichtkegel für wenige Frames.
	// lifeSeconds klein wählen (z. B. 0.05–0.2), und pro Frame erneut emitten, solange er sichtbar sein soll.
	void EmitTransientLightCone(
		const GameVector& apex,
		const EulerAngles& orientation,
		float startRadius, float endRadius,
		float maxLength,
		const Vector4& color,
		float lifeSeconds = 0.1f,
		bool collideLara = true,
		bool collideItems = false,
		bool collideStatics = true,
		bool collideBridges = true);

	// Einmal pro Frame aus dem Gameloop (wie bei deinen transienten Lasern)
	void UpdateTransientLightCones();

	// Für den Renderer
	const std::array<LightConeSlot, MAX_TRANSIENT_LIGHT_CONES>& GetTransientLightConePool();

	// Beim Levelende / Neuladen
	void ClearLightConeEffects();
}
