#include "framework.h"
#include "Objects/Effects/LensFlare.h"

#include "Game/camera.h"
#include "Game/control/los.h"
#include "Math/Math.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/level.h"
#include "Game/Lara/lara_helpers.h"

using namespace TEN::Math;

namespace TEN::Entities::Effects
{
	constexpr float MAX_INTENSITY = 1.0f;		// Maximum intensity
	constexpr float FADE_SPEED = 0.10f;			// Speed of fade-in/out per frame
	constexpr float SHIMMER_STRENGTH = 0.15f;	// Max shimmer amplitude
	constexpr float DEFAULT_FALLOFF_RADIUS = BLOCK(64);

	float GlobalLensFlareIntensity = 0;

	std::vector<LensFlare> LensFlares;
	
	static void UpdateLensFlareIntensity(bool isVisible, float& intensity)
	{
		// Target intensity based on visibility.
		float targetIntensity = isVisible ? MAX_INTENSITY : 0.0f;

		// Fade-in or fade-out.
		if (intensity < targetIntensity)
			intensity = std::min(intensity + FADE_SPEED, targetIntensity);
		else if (intensity > targetIntensity)
			intensity = std::max(intensity - FADE_SPEED, targetIntensity);
	}

	static void AdjustLensflarePosition(Vector3& lensFlarePos, const Vector3& targetPos, float divisor, float threshold)
	{
		// Gradually narrow lensflare vector down to target vector, until threshold is met.
		auto delta = Vector3(FLT_MAX);
		while (delta.Length() > threshold)
		{
			delta = lensFlarePos - targetPos;
			lensFlarePos -= delta / divisor;
		}
	}

	void SetupLensFlare(const Vector3& pos, int roomNumber, const Color& color, float* intensity, int spriteID)
	{
		auto cameraPos = Camera.pos.ToVector3();
		auto cameraTarget = Camera.target.ToVector3();

		auto forward = (cameraTarget - cameraPos);
		forward.Normalize();

		// Discard lensflares behind camera.
		if (forward.Dot(pos - cameraPos) < 0.0f)
			return;

		bool isGlobal = roomNumber == NO_VALUE;
		bool isVisible = true;

		// Don't draw global lensflare if camera is in a room with no lensflare flag set.
		if (isGlobal && TestEnvironment(ENV_FLAG_NO_LENSFLARE, Camera.pos.RoomNumber))
			isVisible = false;

		auto lensFlarePos = pos;

		if (isGlobal)
		{
			// Gradually move lensflare position to a nearest point within closest room.
			AdjustLensflarePosition(lensFlarePos, cameraPos, 8.0f, BLOCK(256));
			AdjustLensflarePosition(lensFlarePos, cameraPos, 4.0f, BLOCK(32));

			// FAILSAFE: Break while loop if room can't be found (e.g. camera is in the void).
			int narrowingCycleCount = 0;

			while (roomNumber == NO_VALUE && narrowingCycleCount < 16)
			{
				int foundRoomNumber = IsRoomOutside(lensFlarePos.x, lensFlarePos.y, lensFlarePos.z);
				if (foundRoomNumber != NO_VALUE)
				{
					roomNumber = foundRoomNumber;
					break;
				}

				AdjustLensflarePosition(lensFlarePos, cameraPos, 2.0f, BLOCK(32));
				narrowingCycleCount++;
			}

			// Don't draw global lensflare, if not in room or in rooms where skybox is not visible.
			if (roomNumber == NO_VALUE || TestEnvironment(ENV_FLAG_NOT_NEAR_SKYBOX, roomNumber))
				isVisible = false;
		}

		// Do occlusion tests only if lensflare passed the previous checks.
		if (isVisible)
		{
			auto origin = GameVector(lensFlarePos, roomNumber);
			auto target = Camera.pos;
			auto distance = Vector3::Distance(origin.ToVector3(), target.ToVector3());

			// Check room occlusion.
			isVisible = LOS(&origin, &target);

			StaticMesh* mesh = nullptr;
			auto pointOfContact = Vector3i();

			// Check occlusion for all static meshes and moveables but player.
			bool collided = isVisible && ObjectOnLOS2(&origin, &target, &pointOfContact, &mesh) != NO_LOS_ITEM;
			if (collided && Vector3::Distance(pointOfContact.ToVector3(), origin.ToVector3()) < distance)
				isVisible = false;

			// Check occlusion only for player.
			int playerItemNumber = isVisible ? ObjectOnLOS2(&origin, &target, &pointOfContact, nullptr, ID_LARA) : NO_LOS_ITEM;
			collided = isVisible && playerItemNumber != NO_LOS_ITEM && !GetLaraInfo(g_Level.Items[playerItemNumber]).Control.Look.IsUsingBinoculars;
			if (collided && Vector3::Distance(pointOfContact.ToVector3(), origin.ToVector3()) < distance)
				isVisible = false;
		}

		float inputIntensity = (intensity != nullptr) ? *intensity : 1.0f;

		// Fade in/out lensflares depending on their visibility.
		if (intensity != nullptr)
			UpdateLensFlareIntensity(isVisible, inputIntensity);

		// Lensflare is completely invisible.
		if (!isVisible && inputIntensity == 0.0f)
			return;

		// Generate slight shimmer.
		float shimmer = Random::GenerateFloat(-SHIMMER_STRENGTH, SHIMMER_STRENGTH);
		float finalIntensity = std::clamp(Smoothstep(inputIntensity) + shimmer * inputIntensity, 0.0f, MAX_INTENSITY);

		auto lensFlare = LensFlare{};
		lensFlare.Position = pos;
		lensFlare.RoomNumber = roomNumber;
		lensFlare.IsGlobal = isGlobal;
		lensFlare.Color = color * Smoothstep(finalIntensity);
		lensFlare.SpriteID = spriteID;

		LensFlares.push_back(lensFlare);

		if (intensity != nullptr)
			*intensity = inputIntensity;
	}

	void UpdateGlobalLensFlare()
	{
		constexpr auto BASE_POS = Vector3(0.0f, 0.0f, BLOCK(256));

		if (!g_GameFlow->GetLevel(CurrentLevel)->GetLensFlareEnabled())
			return;

		auto orient   = EulerAngles(g_GameFlow->GetLevel(CurrentLevel)->GetLensFlarePitch(), g_GameFlow->GetLevel(CurrentLevel)->GetLensFlareYaw(), 0);
		auto color    = g_GameFlow->GetLevel(CurrentLevel)->GetLensFlareColor();
		auto spriteID = g_GameFlow->GetLevel(CurrentLevel)->GetLensFlareSunSpriteID();
		
		auto pos = Camera.pos.ToVector3();
		auto rotMatrix = orient.ToRotationMatrix();

		pos += Vector3::Transform(BASE_POS, rotMatrix);
		SetupLensFlare(pos, NO_VALUE, color, &GlobalLensFlareIntensity, spriteID);
	}

	void ControlLensFlare(int itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		auto color = item.Model.Color;

		// If OCB is set, it specifies radius in blocks, after which flare starts to fadeout.
		float radius   = (item.TriggerFlags > 0) ? (float)(item.TriggerFlags * BLOCK(1)) : DEFAULT_FALLOFF_RADIUS;
		float distance = Vector3i::Distance(item.Pose.Position, Camera.pos.ToVector3i());

		if (distance > radius)
		{
			float fadeMultiplier = std::max((1.0f - ((distance - radius) / radius)), 0.0f);

			// Discard flare, if it is out of falloff sphere radius.
			if (fadeMultiplier <= 0.0f)
				return;

			color *= fadeMultiplier;
		}

		// Intensity value can be modified inside lensflare setup function.
		float currentIntensity = (float)item.ItemFlags[0] / LENSFLARE_ITEMFLAG_BRIGHTNESS_SCALE;
		SetupLensFlare(item.Pose.Position.ToVector3(), item.RoomNumber, color, &currentIntensity, SPRITE_TYPES::SPR_LENS_FLARE_3);
		item.ItemFlags[0] = short(currentIntensity * LENSFLARE_ITEMFLAG_BRIGHTNESS_SCALE);
	}

	void ClearLensFlares()
	{
		LensFlares.clear();
	}
}
