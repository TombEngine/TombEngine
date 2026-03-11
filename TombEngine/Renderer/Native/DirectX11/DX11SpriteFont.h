#pragma once

#ifdef HAS_DX11

#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include <unordered_map>
#include "Renderer/Graphics/ISpriteFont.h"
#include "Renderer/Native/DirectX11/DX11SpriteBatch.h"

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Graphics;

	using Microsoft::WRL::ComPtr;

	class DX11SpriteFont final : public ISpriteFont
	{
	private:
		struct FontGlyph
		{
			unsigned int Character;
			RendererRectangle Subrect;
			float XOffset;
			float YOffset;
			float XAdvance;
		};

		ComPtr<ID3D11ShaderResourceView> _atlasSRV;
		int _atlasWidth = 0;
		int _atlasHeight = 0;
		float _lineSpacing = 0;
		wchar_t _defaultChar = L' ';
		std::unordered_map<unsigned int, FontGlyph> _glyphs;

		bool ParseSpriteFontFile(ID3D11Device* device, const std::string& path);
		const FontGlyph* FindGlyphInternal(unsigned int character) const;
		Vector2 MeasureStringInternal(const wchar_t* str);
		void DrawStringInternal(ISpriteBatch* spriteBatch, const wchar_t* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale);

	public:
		DX11SpriteFont() = default;
		~DX11SpriteFont() = default;

		DX11SpriteFont(ID3D11Device* device, std::string fontPath);

		float GetLineSpacing() override;
		Vector2 MeasureString(const std::string& str) override;
		Glyph FindGlyph(char c) override;
		void DrawString(ISpriteBatch* spriteBatch, const std::string& text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale) override;
	};
}

#endif
