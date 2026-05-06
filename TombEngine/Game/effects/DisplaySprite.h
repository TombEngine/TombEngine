#pragma once
#include "Math/Math.h"
#include "Objects/game_object_ids.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Effects::DisplaySprite
{
	enum class DisplaySpriteAlignMode
	{
		Center,
		CenterTop,
		CenterBottom,
		CenterLeft,
		CenterRight,
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight
	};

	enum class DisplaySpriteScaleMode
	{
		Fit,
		Fill,
		Stretch
	};

	enum class DisplaySpritePhase
	{
		Control,
		Draw
	};
	
	struct DisplaySprite
	{
		GAME_OBJECT_ID ObjectID = ID_DEFAULT_SPRITES;
		int			   SpriteID = 0;

		Vector2 Position	= Vector2::Zero;
		short	Orientation = 0;
		Vector2 Scale		= Vector2::One;
		Vector4 Color		= Vector4::One;

		int					   Priority	 = 0;
		DisplaySpriteAlignMode AlignMode = DisplaySpriteAlignMode::Center;
		DisplaySpriteScaleMode ScaleMode = DisplaySpriteScaleMode::Fit;
		BlendMode			   BlendMode = BlendMode::AlphaBlend;

		DisplaySpritePhase Source = DisplaySpritePhase::Control;
	};

	// Result of display sprite layout calculation.
	struct DisplaySpriteLayoutData
	{
		Vector2 HalfSize		 = Vector2::Zero;
		Vector2 AspectCorrection = Vector2::One;
		Vector2 Offset			 = Vector2::Zero;
	};

	extern std::vector<DisplaySprite> DisplaySprites;
	
	void AddDisplaySprite(GAME_OBJECT_ID objectID, int spriteID, const Vector2& pos, short orient, const Vector2& scale, const Vector4& color,
						  int priority, DisplaySpriteAlignMode alignMode, DisplaySpriteScaleMode scaleMode, 
						  BlendMode blendMode, DisplaySpritePhase source);
	void ClearAllDisplaySprites();
	void ClearDrawPhaseDisplaySprites();

	// Calculate complete layout data for a display sprite.
	// 
	// NOTE: This function is defined inline in the header for performance reasons.
	// It is called every frame for each display sprite in CollectDisplaySprites() and GetAnchors(),
	// so avoiding function call overhead is important in scenarios with many sprites.
	//
	// @param spriteAspect: Aspect ratio of the sprite (width / height).
	// @param scale: Scale factor (0.0 - 1.0 range, already converted from percent).
	// @param orientation: Rotation angle (in internal angle units).
	// @param alignMode: Alignment mode.
	// @param scaleMode: Scale mode.
	// @param screenAspect: Screen aspect ratio.
	// @param aspectCorrectionBase: Base aspect correction factor (screenAspect / DISPLAY_SPACE_ASPECT).
	// @return Layout data containing half size, aspect correction, and rotated offset.
	inline DisplaySpriteLayoutData CalculateDisplaySpriteLayout(float spriteAspect, const Vector2& scale,
																short orientation, DisplaySpriteAlignMode alignMode,
																DisplaySpriteScaleMode scaleMode, float screenAspect,
																float aspectCorrectionBase)
	{
		auto layout = DisplaySpriteLayoutData{};

		// Step 1: Calculate half size and aspect correction based on scale mode.
		// The scale mode determines how the sprite scales relative to screen and sprite aspect ratios.
		switch (scaleMode)
		{
		case DisplaySpriteScaleMode::Fit:
			// Fit: Scale to fit within bounds, preserving aspect ratio (may have letterboxing).
			if (screenAspect >= spriteAspect)
			{
				// Screen is wider than sprite: scale by height.
				layout.HalfSize = (Vector2(DISPLAY_SPACE_RES.y) * scale) / 2.0f;
				layout.HalfSize.x *= (spriteAspect >= 1.0f) ? spriteAspect : (1.0f / spriteAspect);
				layout.AspectCorrection.x = 1.0f / aspectCorrectionBase;
			}
			else
			{
				// Screen is taller than sprite: scale by width.
				layout.HalfSize = (Vector2(DISPLAY_SPACE_RES.x) * scale) / 2.0f;
				layout.HalfSize.y *= (spriteAspect >= 1.0f) ? (1.0f / spriteAspect) : spriteAspect;
				layout.AspectCorrection.y = aspectCorrectionBase;
			}
			break;

		case DisplaySpriteScaleMode::Fill:
			// Fill: Scale to fill bounds, preserving aspect ratio (may crop).
			if (screenAspect >= spriteAspect)
			{
				// Screen is wider than sprite: scale by width to fill.
				layout.HalfSize = (Vector2(DISPLAY_SPACE_RES.x) * scale) / 2.0f;
				layout.HalfSize.y *= (spriteAspect >= 1.0f) ? (1.0f / spriteAspect) : spriteAspect;
				layout.AspectCorrection.y = aspectCorrectionBase;
			}
			else
			{
				// Screen is taller than sprite: scale by height to fill.
				layout.HalfSize = (Vector2(DISPLAY_SPACE_RES.y) * scale) / 2.0f;
				layout.HalfSize.x *= (spriteAspect >= 1.0f) ? spriteAspect : (1.0f / spriteAspect);
				layout.AspectCorrection.x = 1.0f / aspectCorrectionBase;
			}
			break;

		case DisplaySpriteScaleMode::Stretch:
		default:
			// Stretch: Scale to fill bounds, ignoring aspect ratio.
			if (screenAspect >= 1.0f)
			{
				layout.HalfSize = (Vector2(DISPLAY_SPACE_RES.x) * scale) / 2.0f;
				layout.HalfSize.y *= 1.0f / screenAspect;
				layout.AspectCorrection.y = aspectCorrectionBase;
			}
			else
			{
				layout.HalfSize = (Vector2(DISPLAY_SPACE_RES.y) * scale) / 2.0f;
				layout.HalfSize.x *= 1.0f / screenAspect;
				layout.AspectCorrection.x = 1.0f / aspectCorrectionBase;
			}
			break;
		}

		// Step 2: Calculate alignment offset based on align mode.
		// The offset shifts the sprite's anchor point from center to the specified alignment.
		switch (alignMode)
		{
		case DisplaySpriteAlignMode::CenterTop:
			layout.Offset = Vector2(0.0f, layout.HalfSize.y);
			break;

		case DisplaySpriteAlignMode::CenterBottom:
			layout.Offset = Vector2(0.0f, -layout.HalfSize.y);
			break;

		case DisplaySpriteAlignMode::CenterLeft:
			layout.Offset = Vector2(layout.HalfSize.x, 0.0f);
			break;

		case DisplaySpriteAlignMode::CenterRight:
			layout.Offset = Vector2(-layout.HalfSize.x, 0.0f);
			break;

		case DisplaySpriteAlignMode::TopLeft:
			layout.Offset = Vector2(layout.HalfSize.x, layout.HalfSize.y);
			break;

		case DisplaySpriteAlignMode::TopRight:
			layout.Offset = Vector2(-layout.HalfSize.x, layout.HalfSize.y);
			break;

		case DisplaySpriteAlignMode::BottomLeft:
			layout.Offset = Vector2(layout.HalfSize.x, -layout.HalfSize.y);
			break;

		case DisplaySpriteAlignMode::BottomRight:
			layout.Offset = Vector2(-layout.HalfSize.x, -layout.HalfSize.y);
			break;

		case DisplaySpriteAlignMode::Center:
		default:
			layout.Offset = Vector2::Zero;
			break;
		}

		// Step 3: Apply rotation to offset and correct for aspect ratio.
		// This ensures the offset rotates with the sprite while maintaining proper screen proportions.
		// Optimization: Skip matrix creation when no rotation is applied (common case for non-rotated sprites).
		if (orientation != 0)
		{
			auto rotMatrix = Matrix::CreateRotationZ(TO_RAD(orientation));
			layout.Offset = Vector2::Transform(layout.Offset, rotMatrix);
		}

		layout.Offset *= layout.AspectCorrection;

		return layout;
	}
}
