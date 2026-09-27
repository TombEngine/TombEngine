#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include <memory>
#include <SpriteFont.h>
#include <wrl/client.h>
#include "Renderer/Native/DirectX11/DX11SpriteBatch.h"
#include "Renderer/Graphics/ISpriteFont.h"

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Graphics;
	using namespace DirectX;

	using Microsoft::WRL::ComPtr;

	class DX11SpriteFont final : public ISpriteFont
	{
		// TODO: in the future, use cross platform font libraries for handling fonts also with support of true type fonts and extended characters.

	private:
		std::unique_ptr<SpriteFont> _gameFont;

	public:
		DX11SpriteFont();
		~DX11SpriteFont() = default;

		DX11SpriteFont(ID3D11Device* device, std::string fontPath);

		float GetLineSpacing() override;
		Vector2 MeasureString(const std::string& str) override;
		Glyph FindGlyph(char c) override;
		void DrawString(ISpriteBatch* spriteBatch, const std::string& text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale = 1) override;
	};
}

#endif
