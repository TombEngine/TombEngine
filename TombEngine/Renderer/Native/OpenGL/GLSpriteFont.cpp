#include "framework.h"

#ifdef HAS_OPENGL

#include "Renderer/Native/OpenGL/GLSpriteFont.h"
#include "Renderer/Native/OpenGL/GLErrorHelper.h"
#include "Specific/trutils.h"

namespace TEN::Renderer::Native::OpenGL
{
	// DirectXTK .spritefont binary format parser.
	// Format: https://github.com/microsoft/DirectXTK/wiki/MakeSpriteFont
	bool GLSpriteFont::ParseSpriteFontFile(const std::string& path)
	{
		std::ifstream file(std::filesystem::path(path), std::ios::binary);
		if (!file)
			return false;

		// Read header: "DXTKfont" magic.
		char magic[8];
		file.read(magic, 8);
		if (std::string(magic, 8) != "DXTKfont")
		{
			TENLog("Invalid spritefont file format", LogLevel::Error);
			return false;
		}

		// Read glyph count.
		unsigned int glyphCount;
		file.read(reinterpret_cast<char*>(&glyphCount), 4);

		// Read glyph table.
		struct RawGlyph
		{
			unsigned int Character;
			int32_t TexLeft, TexTop, TexRight, TexBottom;
			float XOffset, YOffset, XAdvance;
		};

		std::vector<RawGlyph> rawGlyphs(glyphCount);
		file.read(reinterpret_cast<char*>(rawGlyphs.data()), glyphCount * sizeof(RawGlyph));

		// Read line spacing.
		file.read(reinterpret_cast<char*>(&_lineSpacing), 4);

		// Read default character.
		unsigned int defaultChar;
		file.read(reinterpret_cast<char*>(&defaultChar), 4);
		_defaultChar = (wchar_t)defaultChar;

		// Read texture data.
		unsigned int texWidth, texHeight;
		unsigned int texFormat; // DXGI format.
		unsigned int texStride;
		unsigned int texRows;

		file.read(reinterpret_cast<char*>(&texWidth), 4);
		file.read(reinterpret_cast<char*>(&texHeight), 4);
		file.read(reinterpret_cast<char*>(&texFormat), 4);
		file.read(reinterpret_cast<char*>(&texStride), 4);
		file.read(reinterpret_cast<char*>(&texRows), 4);

		_atlasWidth = texWidth;
		_atlasHeight = texHeight;

		unsigned int texDataSize = texStride * texRows;
		std::vector<unsigned char> texData(texDataSize);
		file.read(reinterpret_cast<char*>(texData.data()), texDataSize);

		// Create GL texture from atlas.
		// The texture format from DirectXTK is typically BC2 compressed or RGBA.
		// For simplicity, we handle the common case (DXGI_FORMAT_BC2_UNORM = 74).
		glCreateTextures(GL_TEXTURE_2D, 1, &_atlasTexture);

		if (texFormat == 74) // DXGI_FORMAT_BC2_UNORM
		{
			glTextureStorage2D(_atlasTexture, 1, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, texWidth, texHeight);
			glCompressedTextureSubImage2D(_atlasTexture, 0, 0, 0, texWidth, texHeight,
				GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, texDataSize, texData.data());
		}
		else if (texFormat == 28) // DXGI_FORMAT_R8G8B8A8_UNORM
		{
			glTextureStorage2D(_atlasTexture, 1, GL_RGBA8, texWidth, texHeight);
			glTextureSubImage2D(_atlasTexture, 0, 0, 0, texWidth, texHeight,
				GL_RGBA, GL_UNSIGNED_BYTE, texData.data());
		}
		else if (texFormat == 87) // DXGI_FORMAT_B8G8R8A8_UNORM
		{
			glTextureStorage2D(_atlasTexture, 1, GL_RGBA8, texWidth, texHeight);
			glTextureSubImage2D(_atlasTexture, 0, 0, 0, texWidth, texHeight,
				GL_BGRA, GL_UNSIGNED_BYTE, texData.data());
		}
		else
		{
			TENLog("Unsupported spritefont texture format: " + std::to_string(texFormat), LogLevel::Error);
			glDeleteTextures(1, &_atlasTexture);
			_atlasTexture = 0;
			return false;
		}

		glTextureParameteri(_atlasTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(_atlasTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		CheckGLError("SpriteFont atlas texture");

		// Store glyphs.
		for (const auto& rg : rawGlyphs)
		{
			FontGlyph g;
			g.Character = rg.Character;
			g.Subrect = RendererRectangle(rg.TexLeft, rg.TexTop, rg.TexRight, rg.TexBottom);
			g.XOffset = rg.XOffset;
			g.YOffset = rg.YOffset;
			g.XAdvance = rg.XAdvance;
			_glyphs[rg.Character] = g;
		}

		return true;
	}

	GLSpriteFont::GLSpriteFont(const std::string& fontPath)
	{
		if (!ParseSpriteFontFile(fontPath))
			TENLog("Failed to load spritefont", LogLevel::Error);
	}

	GLSpriteFont::~GLSpriteFont()
	{
		if (_atlasTexture)
			glDeleteTextures(1, &_atlasTexture);
	}

	float GLSpriteFont::GetLineSpacing()
	{
		return _lineSpacing;
	}

	const GLSpriteFont::FontGlyph* GLSpriteFont::FindGlyphInternal(unsigned int character) const
	{
		auto it = _glyphs.find(character);
		if (it != _glyphs.end())
			return &it->second;

		it = _glyphs.find((unsigned int)_defaultChar);
		if (it != _glyphs.end())
			return &it->second;

		return nullptr;
	}

	Vector2 GLSpriteFont::MeasureStringInternal(const wchar_t* str)
	{
		float x = 0, maxX = 0, y = _lineSpacing;
		while (*str)
		{
			if (*str == L'\n')
			{
				maxX = std::max(maxX, x);
				x = 0;
				y += _lineSpacing;
			}
			else
			{
				auto* g = FindGlyphInternal((unsigned int)*str);
				if (g)
				{
					// Match DirectXTK's ForEachGlyph: XOffset is left bearing,
					// XAdvance is right bearing, advance = glyph_width + XAdvance.
					x += g->XOffset;
					if (x < 0)
						x = 0;

					float w = (float)(g->Subrect.Right - g->Subrect.Left);
					maxX = std::max(maxX, x + w);
					x += w + g->XAdvance;
				}
			}
			str++;
		}
		return Vector2(std::max(maxX, x), y);
	}

	Vector2 GLSpriteFont::MeasureString(const std::string& str)
	{
		std::wstring wstr(str.begin(), str.end());
		return MeasureStringInternal(wstr.c_str());
	}

	Glyph GLSpriteFont::FindGlyph(char c)
	{
		auto* g = FindGlyphInternal((unsigned int)(unsigned char)c);
		Glyph result = {};
		if (g)
		{
			result.Character = g->Character;
			result.Subrect = g->Subrect;
			result.XOffset = g->XOffset;
			result.YOffset = g->YOffset;
			result.XAdvance = g->XAdvance;
		}
		return result;
	}

	void GLSpriteFont::DrawStringInternal(ISpriteBatch* spriteBatch, const wchar_t* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale)
	{
		if (!_atlasTexture || _atlasWidth == 0 || _atlasHeight == 0)
			return;

		auto* glBatch = static_cast<GLSpriteBatch*>(spriteBatch);
		float invW = 1.0f / (float)_atlasWidth;
		float invH = 1.0f / (float)_atlasHeight;

		float x = position.x - origin.x * scale;
		float y = position.y - origin.y * scale;

		while (*text)
		{
			if (*text == L'\n')
			{
				x = position.x - origin.x * scale;
				y += _lineSpacing * scale;
			}
			else
			{
				auto* g = FindGlyphInternal((unsigned int)*text);
				if (g)
				{
					// Match DirectXTK's ForEachGlyph algorithm:
					// XOffset is the left side bearing, XAdvance is the right side bearing.
					// The cursor advance per glyph = XOffset + glyph_width + XAdvance.
					x += g->XOffset * scale;

					float gy = y + g->YOffset * scale;
					float gw = (float)(g->Subrect.Right - g->Subrect.Left) * scale;
					float gh = (float)(g->Subrect.Bottom - g->Subrect.Top) * scale;

					RendererRectangle area((int)x, (int)gy, (int)(x + gw), (int)(gy + gh));

					float u0 = g->Subrect.Left * invW;
					float v0 = g->Subrect.Top * invH;
					float u1 = g->Subrect.Right * invW;
					float v1 = g->Subrect.Bottom * invH;

					glBatch->DrawWithUV(_atlasTexture, area, u0, v0, u1, v1, color);

					// Advance cursor by glyph width + right side bearing.
					x += (gw + g->XAdvance * scale);
				}
			}
			text++;
		}
	}

	void GLSpriteFont::DrawString(ISpriteBatch* spriteBatch, const std::string& text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale)
	{
		std::wstring wtext(text.begin(), text.end());
		DrawStringInternal(spriteBatch, wtext.c_str(), position, color, rotation, origin, scale);
	}
}

#endif
