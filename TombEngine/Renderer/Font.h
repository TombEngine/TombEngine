#pragma once

#include <SimpleMath.h>

namespace TEN::Renderer
{
	using namespace DirectX::SimpleMath;

	// Glyph of a DirectXTK .spritefont file, with its rectangle in the font texture.
	struct FontGlyph
	{
		unsigned int Character = 0;
		int          Left      = 0;
		int          Top       = 0;
		int          Right     = 0;
		int          Bottom    = 0;
		float        XOffset   = 0.0f;
		float        YOffset   = 0.0f;
		float        XAdvance  = 0.0f;
	};

	// DirectXTK .spritefont as written by MakeSpriteFont: glyphs, line spacing and one texture. Strings are measured
	// as DirectXTK SpriteFont measures them; missing characters use the space glyph, as the legacy renderer set.
	class Font
	{
	public:
		static constexpr auto TEXTURE_FORMAT_RGBA8 = 28u; // DXGI formats of the font texture.
		static constexpr auto TEXTURE_FORMAT_BC2   = 74u;
		static constexpr auto TEXTURE_FORMAT_BC3   = 77u;

		std::unordered_map<unsigned int, FontGlyph> Glyphs           = {};
		float                                       LineSpacing      = 0.0f;
		unsigned int                                DefaultCharacter = L' ';

		int                        TextureWidth  = 0;
		int                        TextureHeight = 0;
		unsigned int               TextureFormat = 0;
		std::vector<unsigned char> TextureData   = {};

		bool             Load(const std::string& path);
		const FontGlyph* FindGlyph(unsigned int character) const;
		Vector2          MeasureString(const std::string& text) const;
	};
}
