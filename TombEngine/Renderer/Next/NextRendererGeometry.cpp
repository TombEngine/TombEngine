#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/Structures/RendererBucket.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;
using TEN::Renderer::Graphics::Vertices::Vertex;

namespace TEN::Renderer::Next
{
    void NextRenderer::AddGeometrySection(MeshSource source, const std::vector<Vertex>& sourceVertices, const std::vector<int>& sourceIndices, std::vector<Vertex>& vertices)
    {
        auto& section = _geometrySections[(int)source];
        section.BaseVertex = (int)vertices.size();
        section.VertexCount = (int)sourceVertices.size();
        section.Indices = &sourceIndices;
        vertices.insert(vertices.end(), sourceVertices.begin(), sourceVertices.end());
    }

    // Rooms, statics and moveables share one GPU-only vertex buffer and one index buffer. Indices are relative to their
    // section, which draws offset with its base vertex and index. Vertices keep the legacy order, so player skin and hair
    // changes can be uploaded in place. Merging identical polygon corners was measured below 1%: corners differ by UV,
    // face normal, color and tangent, and the search doubled the upload time.
    bool NextRenderer::CreateLevelGeometry(const LegacyLevelGeometry& geometry)
    {
        auto vertices = std::vector<Vertex>{};
        AddGeometrySection(MeshSource::Rooms, *geometry.RoomVertices, *geometry.RoomIndices, vertices);
        AddGeometrySection(MeshSource::Statics, *geometry.StaticVertices, *geometry.StaticIndices, vertices);
        AddGeometrySection(MeshSource::Moveables, *geometry.MoveableVertices, *geometry.MoveableIndices, vertices);

        auto indices = std::vector<int>{};
        for (auto& section : _geometrySections)
        {
            section.BaseIndex = (int)indices.size();
            indices.insert(indices.end(), section.Indices->begin(), section.Indices->end());
        }

        _geometry = {};
        int vertexDataSize = (int)(vertices.size() * sizeof(Vertex));
        int indexDataSize = (int)(indices.size() * sizeof(int));
        if (vertexDataSize == 0 || indexDataSize == 0)
            return false;

        _geometry.Vertices = _device->CreateBuffer(BufferDesc{ vertexDataSize, 0, BufferUsage::Vertex, "Level.Vertices" });
        _geometry.Indices = _device->CreateBuffer(BufferDesc{ indexDataSize, 0, BufferUsage::Index, "Level.Indices" });
        if (_geometry.Vertices == nullptr || _geometry.Indices == nullptr)
            return false;

        UploadBuffer(_geometry.Vertices.get(), vertices.data(), vertexDataSize);
        UploadBuffer(_geometry.Indices.get(), indices.data(), indexDataSize);
        return true;
    }

    // Called when the legacy renderer rewrites the moveable vertices of player skin joints and hair.
    void NextRenderer::UpdateMoveableVertices(const std::vector<Vertex>& vertices)
    {
        if (_device == nullptr || _geometry.Vertices == nullptr)
            return;

        const auto& section = _geometrySections[(int)MeshSource::Moveables];
        if ((int)vertices.size() != section.VertexCount)
            return;

        auto lock = std::lock_guard<std::mutex>(_frameMutex);
        UploadBuffer(_geometry.Vertices.get(), vertices.data(), (int)(vertices.size() * sizeof(Vertex)), section.BaseVertex * (int)sizeof(Vertex));
    }

    // Object shaders read the instance index stream from the second vertex buffer.
    void NextRenderer::BindLevelGeometry(IRenderPass& renderPass, bool hasInstanceStream)
    {
        BufferBinding vertexBindings[2] = { { _geometry.Vertices.get(), 0 }, { _instanceIndexBuffer.get(), 0 } };
        renderPass.BindVertexBuffers(0, vertexBindings, hasInstanceStream ? 2 : 1);
        renderPass.BindIndexBuffer(BufferBinding{ _geometry.Indices.get(), 0 }, IndexSize::Bits32);
    }

    void NextRenderer::DrawBucket(IRenderPass& renderPass, MeshSource source, const RendererBucket& bucket, int instanceCount, int firstInstance)
    {
        const auto& section = _geometrySections[(int)source];

        _frameStats.DrawCalls++;
        renderPass.DrawIndexed(bucket.NumIndices, instanceCount, section.BaseIndex + bucket.StartIndex, section.BaseVertex, firstInstance);
    }
}
