#pragma once
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

	extern std::vector<DisplaySprite> DisplaySprites;
	
	void AddDisplaySprite(GAME_OBJECT_ID objectID, int spriteID, const Vector2& pos, short orient, const Vector2& scale, const Vector4& color,
						  int priority, DisplaySpriteAlignMode alignMode, DisplaySpriteScaleMode scaleMode, 
						  BlendMode blendMode, DisplaySpritePhase source);
	void ClearAllDisplaySprites();
	void ClearDrawPhaseDisplaySprites();
}
