#include "framework.h"
#include "Renderer/Font.h"

#include "Specific/trutils.h"

namespace TEN::Renderer
{
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
