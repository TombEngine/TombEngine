#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include "Renderer/Native/DirectX11/DX11SpriteBatch.h"
#include "Renderer/Graphics/ISpriteFont.h"
#include <wrl/client.h>
#include <SpriteFont.h>
#include <memory>

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

		DX11SpriteFont(ID3D11Device* device, std::wstring fontPath)
		{
			_gameFont = std::make_unique<SpriteFont>(device, fontPath.c_str());
			_gameFont->SetDefaultCharacter(L' ');
		}

		float GetLineSpacing()
		{
			return _gameFont->GetLineSpacing();
		}

		Vector2 MeasureString(std::wstring str)
		{
			return _gameFont->MeasureString(str.c_str());
		}

		Vector2 MeasureString(wchar_t* str)
		{
			return _gameFont->MeasureString(str);
		}

		Glyph FindGlyph(char c)
		{
			auto dxGlyph = _gameFont->FindGlyph(c);

			Glyph glyph;

			glyph.Character = dxGlyph->Character;
			glyph.XAdvance = dxGlyph->XAdvance;
			glyph.XOffset = dxGlyph->XOffset;
			glyph.YOffset = dxGlyph->YOffset;

			return glyph;
		}

		Glyph FindGlyph(wchar_t c)
		{
			auto dxGlyph = _gameFont->FindGlyph(c);

			Glyph glyph;

			glyph.Character = dxGlyph->Character;
			glyph.XAdvance = dxGlyph->XAdvance;
			glyph.XOffset = dxGlyph->XOffset;
			glyph.YOffset = dxGlyph->YOffset;

			return glyph;
		}

		void DrawString(ISpriteBatch* spriteBatch, std::wstring text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale = 1)
		{
			auto dxSpriteBatch = static_cast<DX11SpriteBatch*>(spriteBatch);
			_gameFont->DrawString(dxSpriteBatch->GetNativeSpriteBatch(), text.c_str(), position, color, rotation, origin, scale);
		}

		void DrawString(ISpriteBatch* spriteBatch, wchar_t* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale = 1)
		{
			auto dxSpriteBatch = static_cast<DX11SpriteBatch*>(spriteBatch);
			_gameFont->DrawString(dxSpriteBatch->GetNativeSpriteBatch(), text, position, color, rotation, origin, scale);
		}

		void DrawString(ISpriteBatch* spriteBatch, std::string text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale = 1)
		{
			auto dxSpriteBatch = static_cast<DX11SpriteBatch*>(spriteBatch);
			_gameFont->DrawString(dxSpriteBatch->GetNativeSpriteBatch(), text.c_str(), position, color, rotation, origin, scale);
		}

		void DrawString(ISpriteBatch* spriteBatch, char* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale = 1)
		{
			auto dxSpriteBatch = static_cast<DX11SpriteBatch*>(spriteBatch);
			_gameFont->DrawString(dxSpriteBatch->GetNativeSpriteBatch(), text, position, color, rotation, origin, scale);
		}
	};
}

#endif
