#pragma once

#ifdef USE_OPENGL

#include <glad/glad.h>
#include "Renderer/Graphics/ISpriteFont.h"
#include "Renderer/Native/OpenGL/GLSpriteBatch.h"

namespace TEN::Renderer::Native::OpenGL
{
	using namespace TEN::Renderer::Graphics;

	class GLSpriteFont final : public ISpriteFont
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

		GLuint _atlasTexture = 0;
		int _atlasWidth = 0;
		int _atlasHeight = 0;
		float _lineSpacing = 0;
		wchar_t _defaultChar = L' ';
		std::unordered_map<unsigned int, FontGlyph> _glyphs;

		bool ParseSpriteFontFile(const std::wstring& path);

	public:
		GLSpriteFont() = default;
		~GLSpriteFont();

		GLSpriteFont(const std::wstring& fontPath);

		float GetLineSpacing() override;
		Vector2 MeasureString(std::wstring str) override;
		Vector2 MeasureString(wchar_t* str) override;
		Glyph FindGlyph(char c) override;
		Glyph FindGlyph(wchar_t c) override;
		void DrawString(ISpriteBatch* spriteBatch, std::wstring text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale) override;
		void DrawString(ISpriteBatch* spriteBatch, wchar_t* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale) override;
		void DrawString(ISpriteBatch* spriteBatch, std::string text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale) override;
		void DrawString(ISpriteBatch* spriteBatch, char* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale) override;

	private:
		Vector2 MeasureStringInternal(const wchar_t* str);
		void DrawStringInternal(ISpriteBatch* spriteBatch, const wchar_t* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale);
		const FontGlyph* FindGlyphInternal(unsigned int character) const;
	};
}

#endif
