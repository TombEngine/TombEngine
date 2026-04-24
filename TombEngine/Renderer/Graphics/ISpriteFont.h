#pragma once

#include "Renderer/Graphics/ISpriteBatch.h"
#include <SimpleMath.h>
#include <string>

using namespace DirectX::SimpleMath;

namespace TEN::Renderer::Graphics
{
    struct Glyph
    {
        unsigned int Character;
        RendererRectangle Subrect;
        float XOffset;
        float YOffset;
        float XAdvance;
    };

    class ISpriteFont
    {
    public:
        virtual ~ISpriteFont() = default;

        virtual float GetLineSpacing() = 0;
        virtual Vector2 MeasureString(std::wstring str) = 0;
        virtual Vector2 MeasureString(wchar_t* str) = 0;
        virtual Glyph FindGlyph(char c) = 0;
        virtual Glyph FindGlyph(wchar_t c) = 0;
        virtual void DrawString(ISpriteBatch* spriteBatch, std::wstring text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale = 1) = 0;
        virtual void DrawString(ISpriteBatch* spriteBatch, wchar_t* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale = 1) = 0;
        virtual void DrawString(ISpriteBatch* spriteBatch, std::string text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale = 1) = 0;
        virtual void DrawString(ISpriteBatch* spriteBatch, char* text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale = 1) = 0;
    };
}