#include "framework.h"

#ifdef USE_OPENGL

#include "Renderer/Native/OpenGL/GLSpriteFont.h"
#include "Renderer/Native/OpenGL/GLErrorHelper.h"
#include "Specific/trutils.h"

namespace TEN::Renderer::Native::OpenGL
{
	// DirectXTK .spritefont binary format parser.
	// Format: https://github.com/microsoft/DirectXTK/wiki/MakeSpriteFont
	bool GLSpriteFont::ParseSpriteFontFile(const std::wstring& path)
	{
		std::ifstream file(path, std::ios::binary);
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
		uint32_t glyphCount;
		file.read(reinterpret_cast<char*>(&glyphCount), 4);

		// Read glyph table.
		struct RawGlyph
		{
			uint32_t Character;
			int32_t TexLeft, TexTop, TexRight, TexBottom;
			float XOffset, YOffset, XAdvance;
		};

		std::vector<RawGlyph> rawGlyphs(glyphCount);
		file.read(reinterpret_cast<char*>(rawGlyphs.data()), glyphCount * sizeof(RawGlyph));

		// Read line spacing.
		file.read(reinterpret_cast<char*>(&_lineSpacing), 4);

		// Read default character.
		uint32_t defaultChar;
		file.read(reinterpret_cast<char*>(&defaultChar), 4);
		_defaultChar = (wchar_t)defaultChar;

		// Read texture data.
		uint32_t texWidth, texHeight;
		uint32_t texFormat; // DXGI format.
		uint32_t texStride;
		uint32_t texRows;

		file.read(reinterpret_cast<char*>(&texWidth), 4);
		file.read(reinterpret_cast<char*>(&texHeight), 4);
		file.read(reinterpret_cast<char*>(&texFormat), 4);
		file.read(reinterpret_cast<char*>(&texStride), 4);
		file.read(reinterpret_cast<char*>(&texRows), 4);

		_atlasWidth = texWidth;
		_atlasHeight = texHeight;

		uint32_t texDataSize = texStride * texRows;
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

	GLSpriteFont::GLSpriteFont(const std::wstring& fontPath)
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

	const GLSpriteFont::FontGlyph* GLSpriteFont::FindGlyphInternal(uint32_t character) const
	{
		auto it = _glyphs.find(character);
		if (it != _glyphs.end())
			return &it->second;

		it = _glyphs.find((uint32_t)_defaultChar);
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
				auto* g = FindGlyphInternal((uint32_t)*str);
				if (g)
					x += g->XAdvance;
			}
			str++;
		}
		return Vector2(std::max(maxX, x), y);
	}

	Vector2 GLSpriteFont::MeasureString(std::wstring str)
	{
		return MeasureStringInternal(str.c_str());
	}

	Vector2 GLSpriteFont::MeasureString(wchar_t* str)
	{
		return MeasureStringInternal(str);
	}

	Glyph GLSpriteFont::FindGlyph(char c)
	{
		auto* g = FindGlyphInternal((uint32_t)(unsigned char)c);
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

	Glyph GLSpriteFont::FindGlyph(wchar_t c)
	{
		auto* g = FindGlyphInternal((uint32_t)c);
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
		// Simple implementation: draw each glyph as a sprite quad via the sprite batch.
		// rotation and origin are ignored for simplicity (matching DirectXTK behavior for common cases).
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
				auto* g = FindGlyphInternal((uint32_t)*text);
				if (g)
				{
					float gx = x + g->XOffset * scale;
					float gy = y + g->YOffset * scale;
					float gw = (float)(g->Subrect.Right - g->Subrect.Left) * scale;
					float gh = (float)(g->Subrect.Bottom - g->Subrect.Top) * scale;

					RendererRectangle area((int)gx, (int)gy, (int)(gx + gw), (int)(gy + gh));
					// NOTE: We'd need to pass UV subrect too. For now, the SpriteBatch::Draw
					// interface doesn't support subrects, so this is a simplified version.
					// A full implementation would need the atlas texture and subrect UVs.
					spriteBatch->Draw(nullptr, area, color);

					x += g->XAdvance * scale;
				}
			}
			text++;
		}
	}

	void GLSpriteFont::DrawString(ISpriteBatch* spriteBatch, std::wstring text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale)
	{
		DrawStringInternal(spriteBatch, text.c_str(), position, color, rotation, origin, scale);
	}

	void GLSpriteFont::DrawString(ISpriteBatch* spriteBatch, wchar_t* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale)
	{
		DrawStringInternal(spriteBatch, text, position, color, rotation, origin, scale);
	}

	void GLSpriteFont::DrawString(ISpriteBatch* spriteBatch, std::string text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale)
	{
		std::wstring wtext(text.begin(), text.end());
		DrawStringInternal(spriteBatch, wtext.c_str(), position, color, rotation, origin, scale);
	}

	void GLSpriteFont::DrawString(ISpriteBatch* spriteBatch, char* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale)
	{
		std::string stext(text);
		std::wstring wtext(stext.begin(), stext.end());
		DrawStringInternal(spriteBatch, wtext.c_str(), position, color, rotation, origin, scale);
	}
}

#endif
