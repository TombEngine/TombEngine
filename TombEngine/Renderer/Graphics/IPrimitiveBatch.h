#pragma once

#include "Renderer/Graphics/Vertices/Vertex.h"

using namespace TEN::Renderer::Graphics::Vertices;

namespace TEN::Renderer::Graphics
{
    class IPrimitiveBatch
    {
    public:
        virtual ~IPrimitiveBatch() = default;

        virtual void Begin() = 0;
        virtual void End() = 0;
        virtual void DrawLine(Vertex const& v1, Vertex const& v2) = 0;
        virtual void DrawTriangle(Vertex const& v1, Vertex const& v2, Vertex const& v3) = 0;
        virtual void DrawQuad(Vertex const& v1, Vertex const& v2, Vertex const& v3, Vertex const& v4) = 0;
    };
}