#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/Structures/RendererBucket.h"
#include "Renderer/Structures/RendererRoom.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;
using TEN::Renderer::Structures::Vertex;

// Level geometry of the scene renderer: packing of the frontend vertices into the shared level vertex and index
// buffers at load, their binding and indexed bucket draws, and the frustum and portal culling of room buckets.

namespace TEN::Renderer::Backend
{
    // Byte alignment of the object vertex region, which is bound at an offset of the level vertex buffer.
    constexpr auto GEOMETRY_REGION_ALIGNMENT = 256;

    // Atlas coordinates lie in the unit square; values outside it are clamped and counted.
    static unsigned short PackUV(float value, int& clampCount)
    {
        if (value < 0.0f || value > 1.0f)
            clampCount++;

        return (unsigned short)std::lround(std::clamp(value, 0.0f, 1.0f) * USHRT_MAX);
    }

    // Fields shared by room and object vertices; normals, tangents and colors are already packed by the frontend.
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

    void SceneRenderer::SetGeometrySection(MeshSource source, int regionOffset, int baseVertex, int vertexCount, const std::vector<int>& indices)
    {
        auto& section = _geometrySections[(int)source];
        section.RegionOffset = regionOffset;
        section.BaseVertex = baseVertex;
        section.VertexCount = vertexCount;
        section.Indices = &indices;
    }

    // One GPU-only vertex buffer holds the room vertices (40 bytes) and, in a second region, the static and moveable
    // vertices (48 bytes); one index buffer holds the level indices, relative to their section. Vertices keep the
    // level order, so player skin and hair changes can be uploaded in place. Merging identical polygon corners was
    // measured below 1%: corners differ by UV, face normal, color and tangent.
    bool SceneRenderer::CreateLevelGeometry(const LevelGeometry& geometry)
    {
        int clampCount = 0;
        auto data = std::vector<unsigned char>{};

        auto roomVertices = std::vector<GpuRoomVertex>{};
        roomVertices.reserve(geometry.RoomVertices->size());
        for (const auto& vertex : *geometry.RoomVertices)
            roomVertices.push_back(PackVertex<GpuRoomVertex>(vertex, clampCount));

        AppendVertexBytes(data, roomVertices);
        SetGeometrySection(MeshSource::Rooms, 0, 0, (int)roomVertices.size(), *geometry.RoomIndices);

        // Statics and moveables share the object region; moveable indices are relative to the first moveable vertex.
        int objectRegionOffset = ((int)data.size() + GEOMETRY_REGION_ALIGNMENT - 1) & ~(GEOMETRY_REGION_ALIGNMENT - 1);
        data.resize(objectRegionOffset);

        int staticCount = (int)geometry.StaticVertices->size();
        AppendVertexBytes(data, PackObjectVertices(*geometry.StaticVertices, clampCount));
        AppendVertexBytes(data, PackObjectVertices(*geometry.MoveableVertices, clampCount));
        SetGeometrySection(MeshSource::Statics, objectRegionOffset, 0, staticCount, *geometry.StaticIndices);
        SetGeometrySection(MeshSource::Moveables, objectRegionOffset, staticCount, (int)geometry.MoveableVertices->size(), *geometry.MoveableIndices);

        if (clampCount > 0)
            TENLog("Scene renderer: " + std::to_string(clampCount) + " texture coordinates outside the atlas were clamped.", LogLevel::Warning);

        // Index buffer: statics, moveables and rooms, in MeshSource order.
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

    // Called when the frontend rewrites the moveable vertices of player skin joints and hair.
    void SceneRenderer::UpdateMoveableVertices(const std::vector<Vertex>& vertices)
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
    void SceneRenderer::BindLevelGeometry(IRenderPass& renderPass, MeshSource source, bool hasInstanceStream)
    {
        BufferBinding vertexBindings[2] = { { _geometry.Vertices.get(), _geometrySections[(int)source].RegionOffset }, { _instanceIndexBuffer.get(), 0 } };
        renderPass.BindVertexBuffers(0, vertexBindings, hasInstanceStream ? 2 : 1);
        renderPass.BindIndexBuffer(BufferBinding{ _geometry.Indices.get(), 0 }, IndexSize::Bits32);
    }

    // Room buckets outside the view are skipped. Vertex effects move room vertices up to 128 units, hence the margin.
    bool SceneRenderer::IsRoomBucketVisible(const RendererBucket& bucket) const
    {
        constexpr auto VERTEX_EFFECT_MARGIN = 128.0f;

        auto margin = Vector3(VERTEX_EFFECT_MARGIN);
        return _frustum.AABBInFrustum(bucket.BoundsMin - margin, bucket.BoundsMax + margin);
    }

    // Room buckets outside the portal rectangles the room is seen through are skipped. The box is projected with the
    // frontend camera, whose normalized device coordinates the rectangle uses; a box crossing the near plane is kept.
    bool SceneRenderer::IsRoomBucketInPortals(const RendererBucket& bucket, const RendererRoom& room, const Matrix& viewProjection) const
    {
        constexpr auto VERTEX_EFFECT_MARGIN = 128.0f;

        auto boundsMin = bucket.BoundsMin - Vector3(VERTEX_EFFECT_MARGIN);
        auto boundsMax = bucket.BoundsMax + Vector3(VERTEX_EFFECT_MARGIN);

        auto rectMin = Vector2(FLT_MAX);
        auto rectMax = Vector2(-FLT_MAX);
        for (int i = 0; i < 8; i++)
        {
            auto corner = Vector4((i & 1) ? boundsMax.x : boundsMin.x, (i & 2) ? boundsMax.y : boundsMin.y, (i & 4) ? boundsMax.z : boundsMin.z, 1.0f);
            auto clip = Vector4::Transform(corner, viewProjection);
            if (clip.w <= 0.0f)
                return true;

            auto point = Vector2(clip.x / clip.w, clip.y / clip.w);
            rectMin = Vector2::Min(rectMin, point);
            rectMax = Vector2::Max(rectMax, point);
        }

        return (rectMin.x <= room.ViewPort.z && rectMax.x >= room.ViewPort.x && rectMin.y <= room.ViewPort.w && rectMax.y >= room.ViewPort.y);
    }

    // Portal rectangle of a room. The frontend computes it in the normalized device coordinates of its own projection;
    // the target projection may have another aspect ratio, which scales the horizontal axis.
    ScissorRect SceneRenderer::GetRoomScissor(const RendererRoom& room, int targetWidth, int targetHeight) const
    {
        float left = (room.ViewPort.x * _roomClipScaleX + 1.0f) * 0.5f * targetWidth;
        float right = (room.ViewPort.z * _roomClipScaleX + 1.0f) * 0.5f * targetWidth;
        float top = (1.0f - room.ViewPort.w) * 0.5f * targetHeight;
        float bottom = (1.0f - room.ViewPort.y) * 0.5f * targetHeight;

        int x = std::clamp((int)std::floor(left), 0, targetWidth);
        int y = std::clamp((int)std::floor(top), 0, targetHeight);
        int width = std::clamp((int)std::ceil(right), 0, targetWidth) - x;
        int height = std::clamp((int)std::ceil(bottom), 0, targetHeight) - y;
        return ScissorRect{ x, y, std::max(width, 0), std::max(height, 0) };
    }

    // Indexed draw of a bucket from the level buffers bound by BindLevelGeometry, counted in the frame statistics.
    void SceneRenderer::DrawBucket(IRenderPass& renderPass, MeshSource source, const RendererBucket& bucket, int instanceCount, int firstInstance)
    {
        const auto& section = _geometrySections[(int)source];

        auto category = _isRenderingShadowMap ? DrawCategory::Shadows :
            ((source == MeshSource::Rooms) ? DrawCategory::Rooms : ((source == MeshSource::Statics) ? DrawCategory::Statics : DrawCategory::Moveables));
        CountDraw(category, bucket.NumIndices / 3 * instanceCount);
        _frameStats.InstancedDrawCalls += (instanceCount > 1) ? 1 : 0;
        _frameStats.DrawnInstances += instanceCount;
        renderPass.DrawIndexed(bucket.NumIndices, instanceCount, section.BaseIndex + bucket.StartIndex, section.BaseVertex, firstInstance);
    }
}
