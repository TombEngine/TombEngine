#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/Structures/RendererBucket.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;
using TEN::Renderer::Graphics::Vertices::Vertex;

namespace TEN::Renderer::Next
{
    constexpr auto GEOMETRY_REGION_ALIGNMENT = 256;

    // Atlas coordinates lie in the unit square; values outside it are clamped and counted.
    static unsigned short PackUV(float value, int& clampCount)
    {
        if (value < 0.0f || value > 1.0f)
            clampCount++;

        return (unsigned short)std::lround(std::clamp(value, 0.0f, 1.0f) * USHRT_MAX);
    }

    template <typename TVertex>
    static TVertex PackVertex(const Vertex& vertex, int& clampCount)
    {
        auto packed = TVertex{};
        packed.Position = vertex.Position;
        packed.Normal = vertex.Normal;
        packed.UV[0] = PackUV(vertex.UV.x, clampCount);
        packed.UV[1] = PackUV(vertex.UV.y, clampCount);
        packed.Color = vertex.Color;
        packed.Tangent = vertex.Tangent;
        packed.FaceNormal = vertex.FaceNormal;
        packed.Effects = vertex.Effects;
        packed.AnimationFrameOffsetIndexHash = vertex.AnimationFrameOffsetIndexHash;
        return packed;
    }

    static GpuObjectVertex PackObjectVertex(const Vertex& vertex, int& clampCount)
    {
        auto packed = PackVertex<GpuObjectVertex>(vertex, clampCount);
        memcpy(packed.BoneIndex, vertex.BoneIndex.data(), sizeof(packed.BoneIndex));
        memcpy(packed.BoneWeight, vertex.BoneWeight.data(), sizeof(packed.BoneWeight));
        return packed;
    }

    static std::vector<GpuObjectVertex> PackObjectVertices(const std::vector<Vertex>& vertices, int& clampCount)
    {
        auto packed = std::vector<GpuObjectVertex>{};
        packed.reserve(vertices.size());
        for (const auto& vertex : vertices)
            packed.push_back(PackObjectVertex(vertex, clampCount));

        return packed;
    }

    template <typename TVertex>
    static void AppendVertexBytes(std::vector<unsigned char>& data, const std::vector<TVertex>& vertices)
    {
        const auto* bytes = (const unsigned char*)vertices.data();
        data.insert(data.end(), bytes, bytes + (vertices.size() * sizeof(TVertex)));
    }

    void NextRenderer::SetGeometrySection(MeshSource source, int regionOffset, int baseVertex, int vertexCount, const std::vector<int>& indices)
    {
        auto& section = _geometrySections[(int)source];
        section.RegionOffset = regionOffset;
        section.BaseVertex = baseVertex;
        section.VertexCount = vertexCount;
        section.Indices = &indices;
    }

    // One GPU-only vertex buffer holds the room vertices (40 bytes) and, in a second region, the static and moveable
    // vertices (48 bytes); one index buffer holds the legacy indices, relative to their section. Vertices keep the
    // legacy order, so player skin and hair changes can be uploaded in place. Merging identical polygon corners was
    // measured below 1%: corners differ by UV, face normal, color and tangent.
    bool NextRenderer::CreateLevelGeometry(const LegacyLevelGeometry& geometry)
    {
        int clampCount = 0;
        auto data = std::vector<unsigned char>{};

        auto roomVertices = std::vector<GpuRoomVertex>{};
        roomVertices.reserve(geometry.RoomVertices->size());
        for (const auto& vertex : *geometry.RoomVertices)
            roomVertices.push_back(PackVertex<GpuRoomVertex>(vertex, clampCount));

        AppendVertexBytes(data, roomVertices);
        SetGeometrySection(MeshSource::Rooms, 0, 0, (int)roomVertices.size(), *geometry.RoomIndices);

        int objectRegionOffset = ((int)data.size() + GEOMETRY_REGION_ALIGNMENT - 1) & ~(GEOMETRY_REGION_ALIGNMENT - 1);
        data.resize(objectRegionOffset);

        int staticCount = (int)geometry.StaticVertices->size();
        AppendVertexBytes(data, PackObjectVertices(*geometry.StaticVertices, clampCount));
        AppendVertexBytes(data, PackObjectVertices(*geometry.MoveableVertices, clampCount));
        SetGeometrySection(MeshSource::Statics, objectRegionOffset, 0, staticCount, *geometry.StaticIndices);
        SetGeometrySection(MeshSource::Moveables, objectRegionOffset, staticCount, (int)geometry.MoveableVertices->size(), *geometry.MoveableIndices);

        if (clampCount > 0)
            TENLog("Next renderer: " + std::to_string(clampCount) + " texture coordinates outside the atlas were clamped.", LogLevel::Warning);

        auto indices = std::vector<int>{};
        for (auto& section : _geometrySections)
        {
            section.BaseIndex = (int)indices.size();
            indices.insert(indices.end(), section.Indices->begin(), section.Indices->end());
        }

        _geometry = {};
        int indexDataSize = (int)(indices.size() * sizeof(int));
        if (data.empty() || indexDataSize == 0)
            return false;

        _geometry.Vertices = _device->CreateBuffer(BufferDesc{ (int)data.size(), 0, BufferUsage::Vertex, "Level.Vertices" });
        _geometry.Indices = _device->CreateBuffer(BufferDesc{ indexDataSize, 0, BufferUsage::Index, "Level.Indices" });
        if (_geometry.Vertices == nullptr || _geometry.Indices == nullptr)
            return false;

        UploadBuffer(_geometry.Vertices.get(), data.data(), (int)data.size());
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

        int clampCount = 0;
        auto packed = PackObjectVertices(vertices, clampCount);

        auto lock = std::lock_guard<std::mutex>(_frameMutex);
        UploadBuffer(_geometry.Vertices.get(), packed.data(), (int)(packed.size() * sizeof(GpuObjectVertex)),
                     section.RegionOffset + (section.BaseVertex * (int)sizeof(GpuObjectVertex)));
    }

    // Binds the vertex region of a mesh source; object shaders also read the instance index stream.
    void NextRenderer::BindLevelGeometry(IRenderPass& renderPass, MeshSource source, bool hasInstanceStream)
    {
        BufferBinding vertexBindings[2] = { { _geometry.Vertices.get(), _geometrySections[(int)source].RegionOffset }, { _instanceIndexBuffer.get(), 0 } };
        renderPass.BindVertexBuffers(0, vertexBindings, hasInstanceStream ? 2 : 1);
        renderPass.BindIndexBuffer(BufferBinding{ _geometry.Indices.get(), 0 }, IndexSize::Bits32);
    }

    // Room buckets outside the view are skipped. Vertex effects move room vertices up to 128 units, hence the margin.
    bool NextRenderer::IsRoomBucketVisible(const RendererBucket& bucket) const
    {
        constexpr auto VERTEX_EFFECT_MARGIN = 128.0f;

        auto margin = Vector3(VERTEX_EFFECT_MARGIN);
        return _frustum.AABBInFrustum(bucket.BoundsMin - margin, bucket.BoundsMax + margin);
    }

    void NextRenderer::DrawBucket(IRenderPass& renderPass, MeshSource source, const RendererBucket& bucket, int instanceCount, int firstInstance)
    {
        const auto& section = _geometrySections[(int)source];

        _frameStats.DrawCalls++;
        renderPass.DrawIndexed(bucket.NumIndices, instanceCount, section.BaseIndex + bucket.StartIndex, section.BaseVertex, firstInstance);
    }
}
