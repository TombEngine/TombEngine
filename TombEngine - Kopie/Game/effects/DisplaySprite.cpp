#include "framework.h"
#include "Game/effects/DisplaySprite.h"

#include "Game/effects/effects.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/objectslist.h"
#include "Renderer/Renderer.h"

using namespace TEN::Math;

namespace TEN::Effects::DisplaySprite
{
	std::vector<DisplaySprite> DisplaySprites = {};

	void AddDisplaySprite(GAME_OBJECT_ID objectID, int spriteID, const Vector2& pos, short orient, const Vector2& scale, const Vector4& color,
						  int priority, DisplaySpriteAlignMode alignMode, DisplaySpriteScaleMode scaleMode, 
						  BlendMode blendMode, DisplaySpritePhase source)
	{
		auto displaySprite = DisplaySprite{};
		displaySprite.ObjectID = objectID;
		displaySprite.SpriteID = spriteID;
		displaySprite.Position = pos;
		displaySprite.Orientation = orient;
		displaySprite.Scale = scale;
		displaySprite.Color = color;
		displaySprite.Priority = priority;
		displaySprite.AlignMode = alignMode;
		displaySprite.ScaleMode = scaleMode;
		displaySprite.BlendMode = blendMode;
		displaySprite.Source = source;

		DisplaySprites.push_back(displaySprite);
	}

	void ClearAllDisplaySprites()
	{
		DisplaySprites.clear();
	}

	void ClearDrawPhaseDisplaySprites()
	{
		DisplaySprites.erase(
			std::remove_if(
				DisplaySprites.begin(), DisplaySprites.end(),
				[](const DisplaySprite& displaySprite)
				{
					return (displaySprite.Source == DisplaySpritePhase::Draw);
				}),
			DisplaySprites.end());
	}
}
