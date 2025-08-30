#include "framework.h"
#include "Game/Hud/InteractionHighlighter.h"

#include "Game/collision/collide_item.h"
#include "Game/effects/DisplaySprite.h"
#include "Game/items.h"
#include "Game/Lara/lara_helpers.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Specific/configuration.h"

using namespace TEN::Math;
using namespace TEN::Effects::DisplaySprite;
using TEN::Renderer::g_Renderer;

namespace TEN::Hud
{
	constexpr auto FADE_SPEED = 0.1f;
	constexpr auto FADE_RATE  = 2.0f * FPS;

	constexpr float INTERACTION_PADDING = CLICK(0.75f);
	constexpr float INTERACTION_DISTANCE = BLOCK(2);
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

		// Never highlight in vehicle mode.
		if (lara.Context.Vehicle != NO_VALUE)
			return false;

		bool armsBusy = (lara.Control.IsMoving || lara.Control.HandStatus != HandStatus::Free);

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
					conditionsMet = player.GetObb().Contains(BoundingSphere(item.Pose.Position.ToVector3(), CLICK(2)));
					break;

				case ID_FLAME_EMITTER:
				{
					bool hasTorch = lara.Control.Weapon.GunType == LaraWeaponType::Torch &&
									lara.Control.HandStatus == HandStatus::WeaponReady &&
									!lara.LeftArm.Locked && !lara.Torch.IsLit;

					// Flame emitter interaction overrides hand status checks, if player carries unlit torch.
					if (hasTorch)
						armsBusy = false;

					conditionsMet = hasTorch && player.GetObb().Contains(BoundingSphere(item.Pose.Position.ToVector3(), CLICK(3)));
					break;
				}
				break;
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

		auto distance = Vector3::Distance(player.Pose.Position.ToVector3(), item.Pose.Position.ToVector3());
		if (distance > INTERACTION_DISTANCE)
			return;

		if (item.Status == ITEM_INVISIBLE)
			return;

		// Test object interaction conditions.
		if (!TestInteractionConditions(player, item, mode))
			return;

		// Inflate object bounding box a little to increase highlight tolerance.
		auto itemBoundingBox = item.GetObb();
		auto inflatedBoundingBox = itemBoundingBox;
		inflatedBoundingBox.Extents = itemBoundingBox.Extents + Vector3::One * INTERACTION_PADDING;

		const auto playerBoundingBox = player.GetObb();

		// Only check bounding box intersection if not in custom mode.
		if (Objects[item.ObjectNumber].drawRoutine != nullptr && !playerBoundingBox.Intersects(inflatedBoundingBox))
			return;

		// Determine interaction type and position.
		SetAttributes(item);

		// Don't check facing direction for pickups, because they are too small to check it.
		if (_checkDirection)
		{
			auto dir = itemBoundingBox.Center - player.Pose.Position.ToVector3();
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

		// Show the highlight.
		_isActive = true;
	}

	void InteractionHighlighterController::SetAttributes(ItemInfo& item, InteractionType type)
	{
		auto bounds = item.GetAabb();
		_position = bounds.Center;

		if (type != InteractionType::Undefined)
		{
			_type = type;
		}
		else
		{
			if (Objects[item.ObjectNumber].isPickup)
			{
				_type = InteractionType::Pickup;
				_checkDirection = false;

				if (!item.TriggerFlags)
					_position.y = GetPointCollision(item).GetFloorHeight() - PICKUP_OFFSET;
				else
					_position.y -= PICKUP_OFFSET;
			}
			else if (item.IsCreature())
			{
				_type = InteractionType::Talk;
				_position.y -= bounds.Extents.y * 1.5f;
				_checkDirection = true;
			}
			else
			{
				_type = InteractionType::Use;
				_position.y += abs(bounds.Extents.y) / 3.0f;

				// HACK: Extend for other direction-agnostic objects if necessary.
				_checkDirection = item.ObjectNumber != ID_TIGHT_ROPE;
			}
		}
	}

	void InteractionHighlighterController::Draw() const
	{
		if (!g_Configuration.EnableInteractionHighlighter)
			return;

		if (_fade <= 0.0f)
			return;

		// Project world to screen
		auto pos2D = g_Renderer.Get2DPosition(_position);
		if (!pos2D.has_value())
			return;

		if (!Objects[ID_INTERACTION_SPRITES].loaded || Objects[ID_INTERACTION_SPRITES].nmeshes == 0)
		{
			TENLog("Missing sprite sequence " + GetObjectName(ID_INTERACTION_SPRITES) + " for drawing interaction highlighter", LogLevel::Warning);
			return;
		}

		// Pick sprite based on type.
		int spriteID = std::max(0, (int)_type - 1);

		if (abs(Objects[ID_INTERACTION_SPRITES].nmeshes) <= spriteID)
			spriteID = 0;

		float alpha = _fade;

		// Oscillate brightness every 2 seconds (60 frames at 30 FPS).
		float phase = (GlobalCounter % (int)FADE_RATE) / FADE_RATE;
		float oscillation = 0.75f + 0.25f * sin(phase * PI * 2.0f);

		alpha *= oscillation;

		auto color = Vector4(1.0f, 1.0f, 1.0f, alpha);

		AddDisplaySprite(
			ID_INTERACTION_SPRITES, spriteID,
			*pos2D, 0, Vector2(0.1f), color,
			0, DisplaySpriteAlignMode::Center, DisplaySpriteScaleMode::Fill,
			BlendMode::Additive, DisplaySpritePhase::Draw);
	}

	void InteractionHighlighterController::Update()
	{
		// Fade in if active
		if (_isActive)
		{
			if (_fade < 1.0f)
				_fade = std::min(1.0f, _fade + FADE_SPEED);
		}
		else
		{
			if (_fade > 0.0f)
				_fade = std::max(0.0f, _fade - FADE_SPEED);
		}

		// Reset for next frame — if Show() not called again, we fade out
		_isActive = false;
	}

	void InteractionHighlighterController::Clear()
	{
		_isActive = false;
		_fade = 0.0f;
		_type = InteractionType::Use;
		_position = Vector3::Zero;
	}
}