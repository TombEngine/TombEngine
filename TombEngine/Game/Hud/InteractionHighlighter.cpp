#include "framework.h"
#include "Game/Hud/InteractionHighlighter.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/Los.h"
#include "Game/effects/DisplaySprite.h"
#include "Game/items.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_tests.h"
#include "Game/spotcam.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Specific/configuration.h"
#include "Specific/trutils.h"

using namespace TEN::Collision::Los;
using namespace TEN::Math;
using namespace TEN::Effects::DisplaySprite;
using TEN::Renderer::g_Renderer;

namespace TEN::Hud
{
	constexpr auto FADE_SPEED = 0.1f;
	constexpr auto FADE_RATE  = 2.0f * FPS;

	constexpr auto SPRITE_SCALE = 0.07f;

	constexpr float INTERACTION_PADDING = CLICK(0.75f);
	constexpr float INTERACTION_DISTANCE = BLOCK(2);
	constexpr float INTERACTION_DISTANCE_TOLERANCE = CLICK(1);
	constexpr float INTERACTION_INTERSECTION_TOLERANCE = CLICK(0.2f);
	constexpr float INTERACTION_ANGLE = TO_RAD(ANGLE(35.0f));

	constexpr float PICKUP_OFFSET = CLICK(0.75f);

	bool InteractionHighlighterController::TestInteractionConditions(ItemInfo& player, ItemInfo& item, InteractionMode mode)
	{
		if (!player.IsLara())
			return false;

		auto& lara = GetLaraInfo(player);

		// Never highlight in optics mode.
		if (lara.Control.Look.IsUsingBinoculars || lara.Control.Look.IsUsingLasersight)
			return false;

		// Never highlight if flyby camera is active.
		if (UseSpotCam)
			return false;

		// Never highlight in vehicle mode.
		if (lara.Context.Vehicle != NO_VALUE)
			return false;

		bool armsBusy = (lara.Control.IsMoving || lara.Control.HandStatus != HandStatus::Free);

		// HACK: Arms are set to busy when Lara is crawling for some reason, so we override that.
		if (Objects[item.ObjectNumber].isPickup && item.ObjectNumber != ID_BURNING_TORCH_ITEM &&
			TestState(player.Animation.ActiveState, CRAWL_STATES))
		{
			armsBusy = false;
		}

		// Filter custom interaction type based on interaction mode object ID.
		bool conditionsMet = true;

		switch (mode)
		{
			case InteractionMode::Always:
				conditionsMet = true;
				break;

			case InteractionMode::Activation:
				conditionsMet = (item.Status == ITEM_NOT_ACTIVE);
				break;

			case InteractionMode::Custom:
				switch (item.ObjectNumber)
				{
				case ID_ZIPLINE_HANDLE:
					conditionsMet = (item.Status != ITEM_ACTIVE && item.StartPose.Position == item.Pose.Position);
					break;

				case ID_CROWDOVE_SWITCH:
					conditionsMet = ((item.MeshBits & 4) != 0);
					break;

				case ID_TIGHT_ROPE:
					conditionsMet = lara.Control.Tightrope.TimeOnTightrope == 0 && player.GetObb().Contains(BoundingSphere(item.Pose.Position.ToVector3(), CLICK(2)));
					break;

				case ID_FLAME_EMITTER:
				case ID_FLAME_EMITTER2:
				{
					bool hasTorch = lara.Control.Weapon.GunType == LaraWeaponType::Torch &&
									lara.Control.HandStatus == HandStatus::WeaponReady &&
									!lara.LeftArm.Locked;

					bool canIgnite = hasTorch && TestLaraTorchFlame(&player, &item);

					// Flame emitter interaction overrides hand status checks, if player carries unlit torch.
					if (hasTorch)
						armsBusy = false;

					conditionsMet = hasTorch && canIgnite && player.GetObb().Contains(BoundingSphere(item.Pose.Position.ToVector3(), CLICK(3)));
					break;
				}
				break;

				default:
					return false;
			}
		}

		return !armsBusy && conditionsMet;
	}

	void InteractionHighlighterController::Test(ItemInfo& player, ItemInfo& item, InteractionMode mode)
	{
		// Interaction highlighter is disabled, don't do tests to conserve CPU.
		if (!g_Configuration.EnableInteractionHighlighter)
			return;

		// Another interaction highlight takes priority.
		if (_isActive)
			return;

		// Rough interaction distance test.
		auto distance = Vector3::Distance(player.Pose.Position.ToVector3(), item.Pose.Position.ToVector3());
		if (distance > INTERACTION_DISTANCE)
			return;

		// Rough room adjacency test.
		if (!TEN::Utils::Contains(g_Level.Rooms[player.RoomNumber].NeighborRoomNumbers, (int)item.RoomNumber))
			return;

		// Discard invisible objects.
		if (item.Status == ITEM_INVISIBLE)
			return;

		// Test object interaction conditions.
		if (!TestInteractionConditions(player, item, mode))
			return;

		const auto playerBoundingBox = player.GetObb();

		// Inflate object bounding box a little to increase highlight tolerance.
		auto itemBoundingBox = item.GetObb();
		auto inflatedBoundingBox = itemBoundingBox;
		inflatedBoundingBox.Extents = itemBoundingBox.Extents + Vector3::One * INTERACTION_PADDING;

		auto dir = itemBoundingBox.Center - playerBoundingBox.Center;
		auto dist = dir.Length() - INTERACTION_INTERSECTION_TOLERANCE;
		dir.Normalize();

		// Check if there's a line of sight between objects.
		auto losColl = GetRoomLosCollision(playerBoundingBox.Center, player.RoomNumber, dir, dist, true);
		if (losColl.IsIntersected)
			return;

		// Only check bounding box intersection if not in custom mode.
		if (!Objects[item.ObjectNumber].Hidden && !playerBoundingBox.Intersects(inflatedBoundingBox))
			return;

		auto position = itemBoundingBox.Center;
		auto type = InteractionType::Undefined;
		
		int checkDirectionDir = 0;
		bool checkFacing = false;

		// Decide on interaction highlight parameters based on object type.
		if (Objects[item.ObjectNumber].isPickup)
		{
			type = InteractionType::Pickup;
			checkFacing = false;

			if (!item.TriggerFlags)
				position.y = GetPointCollision(item).GetFloorHeight() - PICKUP_OFFSET;
			else
				position.y -= PICKUP_OFFSET;
		}
		else if (item.IsCreature())
		{
			type = InteractionType::Talk;
			position.y -= itemBoundingBox.Extents.y * 1.5f;
			checkFacing = true;
		}
		else if (item.Data.is<DOOR_DATA>())
		{
			checkFacing = true;
			checkDirectionDir = (item.ObjectNumber >= ID_PUSHPULL_DOOR1 && item.ObjectNumber <= ID_KICK_DOOR4) ? 0 : 1;
		}
		else
		{
			type = InteractionType::Use;

			// If object bounds are too narrow, show highlighter above the object.
			if (abs(itemBoundingBox.Extents.y) > CLICK(1))
				position.y += abs(itemBoundingBox.Extents.y) / 3.0f;
			else
				position.y -= itemBoundingBox.Extents.y;

			// HACK: Extend for other direction-agnostic objects if necessary.
			checkFacing = item.ObjectNumber != ID_TIGHT_ROPE;
			checkDirectionDir = (item.ObjectNumber >= ID_SEARCH_OBJECT1 && item.ObjectNumber <= ID_SEARCH_OBJECT4) ? -1 : 0;
		}

		// Direction check to make sure objects are oriented towards each other (1) or in the same direction (-1).
		if (checkDirectionDir)
		{
			auto playerYaw = TO_RAD(player.Pose.Orientation.y);
			auto itemYaw = TO_RAD(item.Pose.Orientation.y);

			// Forward direction vectors in XZ plane.
			auto playerForward = Vector3(sin(playerYaw), 0.0f, cos(playerYaw));
			auto itemForward = Vector3(sin(itemYaw), 0.0f, cos(itemYaw));

			// The objects face each other if their facing vectors are roughly opposite.
			float facingDot = playerForward.Dot(-itemForward * checkDirectionDir);

			if (facingDot < INTERACTION_ANGLE)
				return;
		}

		// Facing test to make sure objects are within interaction cone.
		if (checkFacing)
		{
			dir = itemBoundingBox.Center - player.Pose.Position.ToVector3();
			dir.y = 0.0f;
			dir.Normalize();

			auto playerYaw = TO_RAD(player.Pose.Orientation.y);
			auto playerForward = Vector3(sin(playerYaw), 0.0f, cos(playerYaw));

			bool intersectionTest = false;
			bool directionTest = (playerForward.Dot(dir) >= INTERACTION_ANGLE);

			// Perform additional intersection test, if object bounds are too big (e.g. pushables or doors).

			if (!directionTest && (itemBoundingBox.Extents.x > CLICK(1) || itemBoundingBox.Extents.z > CLICK(1)))
			{
				auto dir = player.Pose.Orientation.ToDirection();
				auto center = (Vector3)playerBoundingBox.Center;

				float distance = INTERACTION_DISTANCE;
				intersectionTest = itemBoundingBox.Intersects(center, dir, distance);
			}

			if (!directionTest && !intersectionTest)
				return;
		}

		// If interaction target changes significantly, start crossfade.
		if (Vector3::Distance(_current.Position, position) > INTERACTION_DISTANCE_TOLERANCE || _current.Type != type)
		{
			_previous = _current;
			_current.Fade = 0.0f;
		}

		// Show the highlight.
		_current.Position = position;
		_current.Type = type;
		_isActive = true;
	}

	void InteractionHighlighterController::Draw() const
	{
		if (!g_Configuration.EnableInteractionHighlighter)
			return;

		if (_previous.Fade == 0.0f && _current.Fade == 0.0f)
			return;

		if (!Objects[ID_INTERACTION_SPRITES].loaded || Objects[ID_INTERACTION_SPRITES].nmeshes == 0)
		{
			TENLog("Missing sprite sequence " + GetObjectName(ID_INTERACTION_SPRITES) + " for drawing interaction highlighter", LogLevel::Warning);
			return;
		}

		auto distance = Vector3::Distance(Camera.pos.ToVector3(), _current.Position);
		float scale = std::min(SPRITE_SCALE, INTERACTION_DISTANCE / distance * SPRITE_SCALE);
		float distanceAlpha = std::min(1.0f, scale * 10.0f);

		auto drawState = [&](const HighlightState& state)
		{
			if (state.Fade <= 0.0f)
				return;

			auto pos2D = g_Renderer.Get2DPosition(state.Position);
			if (!pos2D.has_value())
				return;

			int spriteID = std::max(0, (int)state.Type - 1);
			if (abs(Objects[ID_INTERACTION_SPRITES].nmeshes) <= spriteID)
				spriteID = 0;

			float alpha = state.Fade;
			float phase = (GlobalCounter % (int)FADE_RATE) / FADE_RATE;
			float oscillation = 0.75f + 0.25f * sin(phase * PI * 2.0f);
			alpha *= oscillation;
			alpha *= distanceAlpha;

			auto color = Vector4(1.0f, 1.0f, 1.0f, alpha);

			AddDisplaySprite(
				ID_INTERACTION_SPRITES, spriteID,
				*pos2D, 0, Vector2(scale), color,
				0, DisplaySpriteAlignMode::Center, DisplaySpriteScaleMode::Fill,
				BlendMode::Additive, DisplaySpritePhase::Draw);
		};

		drawState(_previous);
		drawState(_current);
	}

	void InteractionHighlighterController::Update()
	{
		if (_isActive)
		{
			_current.Fade = std::min(1.0f, _current.Fade + FADE_SPEED);
		}
		else
		{
			_current.Fade = std::max(0.0f, _current.Fade - FADE_SPEED);
		}

		// Always fade out previous highlight.
		if (_previous.Fade > 0.0f)
			_previous.Fade = std::max(0.0f, _previous.Fade - FADE_SPEED);

		// Reset for next frame — if Show() not called again, we fade out
		_isActive = false;
	}

	void InteractionHighlighterController::Clear()
	{
		_isActive = false;

		_previous = {};
		_current  = {};
	}
}