#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Game/effects/debris.h"
#include "Renderer/Next/GpuTypes.h"
#include "Renderer/Structures/RendererBucket.h"
#include "Renderer/Structures/RendererRoom.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;
using TEN::Renderer::Graphics::Vertices::Vertex;

namespace TEN::Renderer::Next
{
    // Same packing as the legacy Renderer::PackVector3: signed normalized bytes in RGBA order.
    static unsigned int PackNormal(Vector3 normal)
    {
        if (normal.Length() > EPSILON)
            normal.Normalize();

        auto toSignedByte = [](float value) { return (unsigned int)(unsigned char)(char)std::lround(std::clamp(value, -1.0f, 1.0f) * CHAR_MAX); };
        return toSignedByte(normal.x) | (toSignedByte(normal.y) << 8) | (toSignedByte(normal.z) << 16);
    }

    // Debris fragments are single triangles of shattered meshes. Their object space vertices go into a frame vertex
    // buffer with a per vertex instance index, so each instance carries the fragment transform and all fragments
    // sharing texture and blend mode become one draw.
    void NextRenderer::CollectDebris(float interpolationFactor)
    {
        _debrisVertexData.clear();
        _debrisInstanceData.clear();
        _debrisDraws.clear();

        auto fragments = std::vector<const DebrisFragment*>{};
        for (const auto& fragment : DebrisFragments)
        {
            if (fragment.active)
                fragments.push_back(&fragment);
        }

        if (fragments.empty())
            return;

        auto getKey = [](const DebrisFragment* fragment)
        {
            return std::tuple<bool, bool, int, int>(fragment->mesh.Animated, fragment->isStatic, fragment->mesh.tex, (int)fragment->mesh.blendMode);
        };
        std::sort(fragments.begin(), fragments.end(), [&](const DebrisFragment* first, const DebrisFragment* second) { return getKey(first) < getKey(second); });

        for (const auto* fragment : fragments)
        {
            // Like the legacy renderer, debris is lit by the room ambient only.
            const auto& room = (*_legacyGeometry.Rooms)[fragment->roomNumber];
            auto world = Matrix::Lerp(fragment->PrevTransform, fragment->Transform, interpolationFactor);
            unsigned int instance = (unsigned int)AddInstance(world, fragment->color, room.AmbientLight, fragment->lightMode, 0, 0, 0, SKINNING_NONE);

            int firstVertex = (int)(_debrisVertexData.size() / sizeof(Vertex));
            for (int i = 0; i < 3; i++)
            {
                auto vertex = Vertex{};
                vertex.Position = fragment->mesh.Positions[i];
                vertex.UV = fragment->mesh.TextureCoordinates[i];
                vertex.Normal = PackNormal(fragment->mesh.Normals[i]);
                vertex.Color = VectorColorToRGBA(fragment->mesh.Colors[i]);

                const auto* bytes = (const unsigned char*)&vertex;
                _debrisVertexData.insert(_debrisVertexData.end(), bytes, bytes + sizeof(vertex));

                const auto* indexBytes = (const unsigned char*)&instance;
                _debrisInstanceData.insert(_debrisInstanceData.end(), indexBytes, indexBytes + sizeof(instance));
            }

            const auto* pages = fragment->mesh.Animated ? &_animatedTextures : (fragment->isStatic ? &_staticTextures : &_moveableTextures);
            if (_debrisDraws.empty() || _debrisDraws.back().Pages != pages || _debrisDraws.back().Page != fragment->mesh.tex ||
                _debrisDraws.back().BlendMode != fragment->mesh.blendMode)
            {
                _debrisDraws.push_back(DebrisDraw{ pages, fragment->mesh.tex, fragment->mesh.blendMode, firstVertex, 0 });
            }

            _debrisDraws.back().VertexCount += 3;
        }
    }

    // Opaque and alpha tested debris in the opaque pass, additive debris in the additive pass, and the other
    // blend modes after the sorted faces, unsorted as in the legacy renderer.
    void NextRenderer::DrawDebris(ICommandBuffer& commandBuffer, IRenderPass& renderPass, DebrisPass pass)
    {
        if (_debrisDraws.empty())
            return;

        commandBuffer.PushDebugGroup("Debris");

        IBuffer* storageBuffers[4] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get(), _instanceBuffer.Buffer.get(), _boneBuffer.Buffer.get() };
        const IGraphicsPipeline* lastPipeline = nullptr;
        auto state = BucketBindState{};

        for (const auto& draw : _debrisDraws)
        {
            bool isOpaque = (draw.BlendMode == BlendMode::Opaque || draw.BlendMode == BlendMode::AlphaTest);
            bool isAdditive = (draw.BlendMode == BlendMode::Additive);
            if ((pass == DebrisPass::Opaque && !isOpaque) || (pass == DebrisPass::Additive && !isAdditive) ||
                (pass == DebrisPass::Blended && (isOpaque || isAdditive)))
            {
                continue;
            }

            auto* pipeline = GetScenePipeline(SceneShader::ObjectsDebris, draw.BlendMode);
            if (pipeline != lastPipeline)
            {
                renderPass.BindPipeline(pipeline);
                renderPass.BindVertexStorageBuffers(0, storageBuffers, 4);
                BindScenePixelStorage(renderPass);

                BufferBinding vertexBindings[2] = { { _debrisVertexBuffer.Buffer.get(), 0 }, { _debrisInstanceBuffer.Buffer.get(), 0 } };
                renderPass.BindVertexBuffers(0, vertexBindings, 2);
                lastPipeline = pipeline;
                state = BucketBindState{};
            }

            // Debris UVs are copied from the source polygon, so animated pages are bound without animation.
            auto bucket = RendererBucket{};
            bucket.Texture = draw.Page;
            bucket.Animated = false;
            bucket.BlendMode = draw.BlendMode;
            bucket.MaterialIndex = 0;
            BindBucket(commandBuffer, renderPass, bucket, *draw.Pages, draw.BlendMode, 0, BucketDrawInfo{}, state);

            _frameStats.DrawCalls++;
            renderPass.Draw(draw.VertexCount, 1, draw.FirstVertex, 0);
        }

        commandBuffer.PopDebugGroup();
    }
}
