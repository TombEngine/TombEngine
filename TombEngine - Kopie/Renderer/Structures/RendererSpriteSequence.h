#pragma once
#include <vector>

namespace TEN::Renderer::Structures
{
	struct RendererSpriteSequence
	{
		int Id;
		std::vector<RendererSprite*> SpritesList;
		int NumSprites;

		RendererSpriteSequence()
		{
			Id = 0;
			NumSprites = 0;
		}

		RendererSpriteSequence(int id, int num)
		{
			Id = id;
			NumSprites = num;
			SpritesList = std::vector<RendererSprite*>(NumSprites);
		}

		RendererSpriteSequence(const RendererSpriteSequence& rhs)
		{
			Id = rhs.Id;
			NumSprites = rhs.NumSprites;
			SpritesList = rhs.SpritesList;
		}

		RendererSpriteSequence& operator=(const RendererSpriteSequence& other)
		{
			if (this != &other)
			{
				Id = other.Id;
				NumSprites = other.NumSprites;
				SpritesList = std::vector<RendererSprite*>(NumSprites);
				std::copy(other.SpritesList.begin(), other.SpritesList.end(), back_inserter(SpritesList));
			}
			return *this;
		}
	};
}
