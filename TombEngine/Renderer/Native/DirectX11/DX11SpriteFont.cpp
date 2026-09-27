#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11SpriteFont.h"
#include "Specific/trutils.h"

namespace TEN::Renderer::Native::DirectX11
{
	DX11SpriteFont::DX11SpriteFont(ID3D11Device* device, std::string fontPath)
	{
		auto wFontPath = TEN::Utils::ToWString(fontPath);
		_gameFont = std::make_unique<SpriteFont>(device, wFontPath.c_str());
		_gameFont->SetDefaultCharacter(L' ');
	}

	float DX11SpriteFont::GetLineSpacing()
	{
		return _gameFont->GetLineSpacing();
	}

	Vector2 DX11SpriteFont::MeasureString(const std::string& str)
	{
		auto wStr = TEN::Utils::ToWString(str);
		return _gameFont->MeasureString(wStr.c_str());
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

	void DX11SpriteFont::DrawString(ISpriteBatch* spriteBatch, const std::string& text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale)
	{
		auto dxSpriteBatch = static_cast<DX11SpriteBatch*>(spriteBatch);
		auto wText = TEN::Utils::ToWString(text);
		_gameFont->DrawString(dxSpriteBatch->GetNativeSpriteBatch(), wText.c_str(), position, color, rotation, origin, scale);
	}
}

#endif
