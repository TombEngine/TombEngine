#pragma once

#include "Renderer/Graphics/ITextureBase.h"
#include "Renderer/Structures/RendererRectangle.h"

using namespace TEN::Renderer::Graphics;
using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Graphics
{
    class ISpriteBatch
    {
    public:
        virtual ~ISpriteBatch() = default;

        virtual void Begin(SpriteSortingMode sortingMode, BlendMode blendMode) = 0;
        virtual void End() = 0;
        virtual void Draw(ITextureBase* texture, RendererRectangle area, Vector4 color) = 0;
    };
}