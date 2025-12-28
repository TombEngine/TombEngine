#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11SpriteFont.h"
#include "Specific/trutils.h"

namespace TEN::Renderer::Native::DirectX11
{
	DX11SpriteFont::DX11SpriteFont(ID3D11Device* device, std::wstring fontPath)
	{
		_gameFont = std::make_unique<SpriteFont>(device, fontPath.c_str());
		_gameFont->SetDefaultCharacter(L' ');
	}

	float DX11SpriteFont::GetLineSpacing()
	{
		return _gameFont->GetLineSpacing();
	}

	Vector2 DX11SpriteFont::MeasureString(std::wstring str)
	{
		return _gameFont->MeasureString(str.c_str());
	}

	Vector2 DX11SpriteFont::MeasureString(wchar_t* str)
	{
		return _gameFont->MeasureString(str);
	}

	Glyph DX11SpriteFont::FindGlyph(char c)
	{
		auto dxGlyph = _gameFont->FindGlyph(c);

		Glyph glyph;

		glyph.Character = dxGlyph->Character;
		glyph.XAdvance = dxGlyph->XAdvance;
		glyph.XOffset = dxGlyph->XOffset;
		glyph.YOffset = dxGlyph->YOffset;

		return glyph;
	}

	Glyph DX11SpriteFont::FindGlyph(wchar_t c)
	{
		auto dxGlyph = _gameFont->FindGlyph(c);

		Glyph glyph;

		glyph.Character = dxGlyph->Character;
		glyph.XAdvance = dxGlyph->XAdvance;
		glyph.XOffset = dxGlyph->XOffset;
		glyph.YOffset = dxGlyph->YOffset;

		return glyph;
	}

	void DX11SpriteFont::DrawString(ISpriteBatch* spriteBatch, std::wstring text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale)
	{
		auto dxSpriteBatch = static_cast<DX11SpriteBatch*>(spriteBatch);
		_gameFont->DrawString(dxSpriteBatch->GetNativeSpriteBatch(), text.c_str(), position, color, rotation, origin, scale);
	}

	void DX11SpriteFont::DrawString(ISpriteBatch* spriteBatch, wchar_t* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale)
	{
		auto dxSpriteBatch = static_cast<DX11SpriteBatch*>(spriteBatch);
		_gameFont->DrawString(dxSpriteBatch->GetNativeSpriteBatch(), text, position, color, rotation, origin, scale);
	}

	void DX11SpriteFont::DrawString(ISpriteBatch* spriteBatch, std::string text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale)
	{
		auto dxSpriteBatch = static_cast<DX11SpriteBatch*>(spriteBatch);
		_gameFont->DrawString(dxSpriteBatch->GetNativeSpriteBatch(), text.c_str(), position, color, rotation, origin, scale);
	}

	void DX11SpriteFont::DrawString(ISpriteBatch* spriteBatch, char* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale)
	{
		auto dxSpriteBatch = static_cast<DX11SpriteBatch*>(spriteBatch);
		_gameFont->DrawString(dxSpriteBatch->GetNativeSpriteBatch(), text, position, color, rotation, origin, scale);
	}
}

#endif