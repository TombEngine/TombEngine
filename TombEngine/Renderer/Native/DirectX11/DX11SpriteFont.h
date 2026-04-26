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

		DX11SpriteFont(ID3D11Device* device, std::wstring fontPath);

		float GetLineSpacing();

		Vector2 MeasureString(std::wstring str);
		Vector2 MeasureString(wchar_t* str);

		Glyph FindGlyph(char c);
		Glyph FindGlyph(wchar_t c);

		void DrawString(ISpriteBatch* spriteBatch, std::wstring text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale = 1);
		void DrawString(ISpriteBatch* spriteBatch, wchar_t* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale = 1);
		void DrawString(ISpriteBatch* spriteBatch, std::string text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale = 1);
		void DrawString(ISpriteBatch* spriteBatch, char* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale = 1);
	};
}

#endif
