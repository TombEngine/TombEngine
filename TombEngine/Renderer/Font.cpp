#include "framework.h"
#include "Renderer/Font.h"

#include "Specific/trutils.h"

namespace TEN::Renderer
{
	// Texture formats MakeSpriteFont writes, besides the ones the renderer loads directly.
	constexpr auto TEXTURE_FORMAT_RGBA8_SRGB = 29u;
	constexpr auto TEXTURE_FORMAT_BC1_SRGB   = 72u;
	constexpr auto TEXTURE_FORMAT_BC2_SRGB   = 75u;
	constexpr auto TEXTURE_FORMAT_BC3_SRGB   = 78u;
	constexpr auto TEXTURE_FORMAT_BGRA8      = 87u;
	constexpr auto TEXTURE_FORMAT_BGRX8      = 88u;
	constexpr auto TEXTURE_FORMAT_BGRA8_SRGB = 91u;
	constexpr auto TEXTURE_FORMAT_BGRA4      = 115u;

	// Converts a pixel of an uncompressed font texture to RGBA8.
	static void ConvertTexel(unsigned int format, const unsigned char* source, unsigned char* target)
	{
		switch (format)
		{
		case TEXTURE_FORMAT_BGRA8:
		case TEXTURE_FORMAT_BGRA8_SRGB:
		case TEXTURE_FORMAT_BGRX8:
			target[0] = source[2];
			target[1] = source[1];
			target[2] = source[0];
			target[3] = (format == TEXTURE_FORMAT_BGRX8) ? 255 : source[3];
			break;

		// 16 bits, little endian: blue in the lowest nibble, alpha in the highest. Nibbles expand to bytes.
		case TEXTURE_FORMAT_BGRA4:
			target[0] = (source[1] & 0x0F) * 17;
			target[1] = (source[0] >> 4) * 17;
			target[2] = (source[0] & 0x0F) * 17;
			target[3] = (source[1] >> 4) * 17;
			break;

		default:
			memcpy(target, source, 4);
			break;
		}
	}

	// Leaves the texture in a format the renderer loads: block compressed textures keep their blocks, as the sRGB
	// variants store the same data, and uncompressed ones become tightly packed RGBA8, as DirectXTK SpriteFont read
	// them. Returns false for formats MakeSpriteFont does not write.
	static bool NormalizeTexture(unsigned int& format, std::vector<unsigned char>& data, int width, int height, int pitch)
	{
		switch (format)
		{
		case Font::TEXTURE_FORMAT_BC1:
		case Font::TEXTURE_FORMAT_BC2:
		case Font::TEXTURE_FORMAT_BC3:
			return true;

		case TEXTURE_FORMAT_BC1_SRGB:
		case TEXTURE_FORMAT_BC2_SRGB:
		case TEXTURE_FORMAT_BC3_SRGB:
			format--;
			return true;

		case Font::TEXTURE_FORMAT_RGBA8:
		case TEXTURE_FORMAT_RGBA8_SRGB:
		case TEXTURE_FORMAT_BGRA8:
		case TEXTURE_FORMAT_BGRX8:
		case TEXTURE_FORMAT_BGRA8_SRGB:
		case TEXTURE_FORMAT_BGRA4:
			break;

		default:
			return false;
		}

		int texelSize = (format == TEXTURE_FORMAT_BGRA4) ? 2 : 4;
		if (pitch < (width * texelSize) || data.size() < ((size_t)pitch * height))
			return false;

		auto rgba = std::vector<unsigned char>((size_t)width * height * 4);
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
				ConvertTexel(format, &data[(size_t)y * pitch + (size_t)x * texelSize], &rgba[((size_t)y * width + x) * 4]);
		}

		data = std::move(rgba);
		format = Font::TEXTURE_FORMAT_RGBA8;
		return true;
	}

	bool Font::Load(const std::string& path)
	{
		auto file = std::ifstream(path, std::ios::binary);
		auto data = std::vector<unsigned char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		auto read = [&](size_t offset, void* destination, size_t size)
		{
			if (offset + size > data.size())
				return false;

			memcpy(destination, data.data() + offset, size);
			return true;
		};

		unsigned int glyphCount = 0;
		if (data.size() < 12 || memcmp(data.data(), "DXTKfont", 8) != 0 || !read(8, &glyphCount, 4))
		{
			TENLog("Invalid font " + path, LogLevel::Error);
			return false;
		}

		Glyphs.clear();
		size_t offset = 12;
		for (unsigned int i = 0; i < glyphCount; i++, offset += sizeof(FontGlyph))
		{
			auto glyph = FontGlyph{};
			if (!read(offset, &glyph, sizeof(glyph)))
				return false;

			Glyphs[glyph.Character] = glyph;
		}

		// Line spacing, default character of the file (replaced by the space), texture size, format, row pitch and rows.
		unsigned int header[7] = {};
		if (!read(offset, header, sizeof(header)))
			return false;

		memcpy(&LineSpacing, &header[0], sizeof(float));
		TextureWidth = (int)header[2];
		TextureHeight = (int)header[3];
		TextureFormat = header[4];
		offset += sizeof(header);

		size_t textureSize = (size_t)header[5] * header[6];
		if (offset + textureSize > data.size())
		{
			TENLog("Font texture is truncated in " + path, LogLevel::Error);
			return false;
		}

		TextureData.assign(data.begin() + offset, data.begin() + offset + textureSize);

		// Metrics stay usable with an unknown texture format; the renderer reports it when it loads the texture.
		NormalizeTexture(TextureFormat, TextureData, TextureWidth, TextureHeight, (int)header[5]);
		return true;
	}

	const FontGlyph* Font::FindGlyph(unsigned int character) const
	{
		auto it = Glyphs.find(character);
		if (it == Glyphs.end())
			it = Glyphs.find(DefaultCharacter);

		return ((it != Glyphs.end()) ? &it->second : nullptr);
	}

	// DirectXTK SpriteFont::MeasureString: whitespace without pixels is skipped, and each line is at least the line spacing high.
	Vector2 Font::MeasureString(const std::string& text) const
	{
		auto result = Vector2::Zero;
		float x = 0.0f;
		float y = 0.0f;

		for (wchar_t character : TEN::Utils::ToWString(text))
		{
			if (character == L'\r')
				continue;

			if (character == L'\n')
			{
				x = 0.0f;
				y += LineSpacing;
				continue;
			}

			const auto* glyph = FindGlyph(character);
			if (glyph == nullptr)
				continue;

			x = std::max(x + glyph->XOffset, 0.0f);

			float width = (float)(glyph->Right - glyph->Left);
			float height = (float)(glyph->Bottom - glyph->Top);
			if (!iswspace(character) || width > 1.0f || height > 1.0f)
			{
				float lineHeight = iswspace((wchar_t)glyph->Character) ? LineSpacing : std::max(height + glyph->YOffset, LineSpacing);
				result = Vector2::Max(result, Vector2(x + width, y + lineHeight));
			}

			x += width + glyph->XAdvance;
		}

		return result;
	}
}
