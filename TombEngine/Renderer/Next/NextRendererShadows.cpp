#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/Next/GpuTypes.h"
#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/Structures/RendererItem.h"
#include "Renderer/Structures/RendererLight.h"
#include "Renderer/Structures/RendererObject.h"
#include "Specific/configuration.h"
#include "Specific/level.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Next
{
    constexpr auto SHADOW_NEAR_PLANE = 16.0f;

    bool NextRenderer::CreateShadowResources()
    {
        _objectsShadowPixelShader = LoadShader(*_device, _shaderDirectory, "Objects", "PSShadow");
        if (_objectsShadowPixelShader == nullptr)
            return false;

        // Bound in place of the shadow map while the shadow map itself is the render target.
        auto dummyDesc = TextureDesc{};
        dummyDesc.Type = TextureType::Texture2DArray;
        dummyDesc.DepthOrLayers = 6;
        dummyDesc.Format = PixelFormat::R32Float;
        dummyDesc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        dummyDesc.Name = "Shadow.Dummy";
        _dummyShadowMap = _device->CreateTexture(dummyDesc);

        return (_dummyShadowMap != nullptr && EnsureShadowMap(GameConfiguration::DEFAULT_SHADOW_MAP_SIZE));
    }

    // One R32 float layer per cube face, plus a depth buffer reused by every face. SDL GPU does not allow depth
    // array targets, so the depth is written as color and compared in the shader. Recreated when the size changes.
    bool NextRenderer::EnsureShadowMap(int size)
    {
        size = std::max(size, 64);
        if (_shadowMap != nullptr && _shadowMap->GetDesc().Width == size)
            return true;

        auto desc = TextureDesc{};
        desc.Type = TextureType::Texture2DArray;
        desc.Width = size;
        desc.Height = size;
        desc.DepthOrLayers = 6;
        desc.Format = PixelFormat::R32Float;
        desc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        desc.Name = "Shadow.CubeMap";
        _shadowMap = _device->CreateTexture(desc);

        auto depthDesc = TextureDesc{};
        depthDesc.Width = size;
        depthDesc.Height = size;
        depthDesc.Format = PixelFormat::D32Float;
        depthDesc.Usage = TextureUsage::DepthStencilTarget;
        depthDesc.Name = "Shadow.Depth";
        _shadowDepth = _device->CreateTexture(depthDesc);

        return (_shadowMap != nullptr && _shadowDepth != nullptr);
    }

    // Legacy RenderShadowMap rules: the object must cast shadows, and with the player-only setting only objects
    // marked for player shadows do.
    bool NextRenderer::IsShadowCaster(int objectID) const
    {
        if (!_isShadowMapActive)
            return false;

        const auto& object = (*_legacyGeometry.MoveableObjects)[objectID];
        if (!object.has_value() || object->ShadowType == ShadowMode::None)
            return false;

        return (!_isPlayerShadowOnly || object->ShadowType == ShadowMode::Player);
    }

    // Objects receive shadows only when they do not cast them.
    unsigned int NextRenderer::GetShadowReceiverMask(int objectID) const
    {
        const auto& object = (*_legacyGeometry.MoveableObjects)[objectID];
        return (object.has_value() && object->ShadowType == ShadowMode::None) ? SHADOWABLE_MASK : 0;
    }

    void NextRenderer::MarkShadowCasterDraws(int firstDraw)
    {
        _shadowDraws.insert(_shadowDraws.end(), _objectDraws.begin() + firstDraw, _objectDraws.end());
    }

    // Cube face view projections as in the legacy renderer, and the uniforms read by the scene shaders.
    void NextRenderer::PrepareShadows(const FrameEnvironment& environment, float interpolationFactor)
    {
        static constexpr Vector3 FORWARD_VECTORS[6] =
        {
            Vector3(-1.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f),
            Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), Vector3(0.0f, 0.0f, -1.0f)
        };
        static constexpr Vector3 UP_VECTORS[6] =
        {
            Vector3(0.0f, -1.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f), Vector3(0.0f, 0.0f, -1.0f),
            Vector3(0.0f, 0.0f, 1.0f), Vector3(0.0f, -1.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f)
        };

        _shadowUniforms = GpuShadowUniforms{};
        _shadowUniforms.SphereCount = std::min(environment.ShadowSphereCount, SHADOW_SPHERES_MAX);
        memcpy(_shadowUniforms.Spheres, environment.ShadowSpheres, sizeof(Vector4) * _shadowUniforms.SphereCount);

        const auto* light = environment.ShadowLight;
        if (light == nullptr || (light->Type != LightType::Point && light->Type != LightType::Spot) || _shadowDraws.empty() ||
            !EnsureShadowMap(environment.ShadowMapSize))
        {
            return;
        }

        _shadowUniforms.Light = ToGpuLight(*light, interpolationFactor, 1.0f);
        _shadowUniforms.CastShadows = 1;
        _shadowUniforms.ShadowMapSize = _shadowMap->GetDesc().Width;

        auto position = _shadowUniforms.Light.Position;
        auto projection = Matrix::CreatePerspectiveFieldOfView(PI / 2.0f, 1.0f, SHADOW_NEAR_PLANE, light->Out);
        for (int face = 0; face < 6; face++)
        {
            auto view = Matrix::CreateLookAt(position, position + FORWARD_VECTORS[face] * BLOCK(10), UP_VECTORS[face]);
            _shadowUniforms.LightViewProjections[face] = view * projection;
        }
    }

    // Six depth only passes, one per cube face, with the instanced draws of the shadow casters.
    void NextRenderer::RenderShadowMap(ICommandBuffer& commandBuffer, const FrameVertexUniforms& cameraUniforms)
    {
        if (!_shadowUniforms.CastShadows)
            return;

        commandBuffer.PushDebugGroup("Shadow map");
        _isRenderingShadowMap = true;

        auto* pipeline = GetScenePipeline(SceneShader::ObjectsShadow, BlendMode::Opaque);
        IBuffer* storageBuffers[4] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get(), _instanceBuffer.Buffer.get(), _boneBuffer.Buffer.get() };

        for (int face = 0; face < 6; face++)
        {
            auto faceUniforms = cameraUniforms;
            faceUniforms.ViewProjection = _shadowUniforms.LightViewProjections[face];
            commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 0, &faceUniforms, sizeof(faceUniforms));

            auto colorTarget = ColorTargetInfo{};
            colorTarget.Texture = _shadowMap.get();
            colorTarget.Layer = face;
            colorTarget.ClearColor[0] = 1.0f;

            auto depthTarget = DepthTargetInfo{};
            depthTarget.Texture = _shadowDepth.get();
            depthTarget.Store = StoreOp::DontCare;

            auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, &depthTarget);
            renderPass->BindPipeline(pipeline);
            renderPass->BindVertexStorageBuffers(0, storageBuffers, 4);
            BindScenePixelStorage(*renderPass);

            auto lastSource = (MeshSource)-1;
            auto state = BucketBindState{};
            for (const auto& draw : _shadowDraws)
            {
                const auto& bucket = *draw.Bucket;
                if (bucket.BlendMode != BlendMode::Opaque && bucket.BlendMode != BlendMode::AlphaTest)
                    continue;

                if (draw.Source != lastSource)
                {
                    const auto& geometry = (draw.Source == MeshSource::Statics) ? _staticGeometry : _moveableGeometry;
                    BufferBinding vertexBindings[2] = { { geometry.Vertices.get(), 0 }, { _instanceIndexBuffer.get(), 0 } };
                    renderPass->BindVertexBuffers(0, vertexBindings, 2);
                    renderPass->BindIndexBuffer(BufferBinding{ geometry.Indices.get(), 0 }, IndexSize::Bits32);
                    lastSource = draw.Source;
                }

                const auto& pages = (draw.Source == MeshSource::Statics) ? _staticTextures : _moveableTextures;
                BindBucket(commandBuffer, *renderPass, bucket, pages, bucket.BlendMode, 0, BucketDrawInfo{}, state);

                _frameStats.DrawCalls++;
                renderPass->DrawIndexed(bucket.NumIndices, draw.InstanceCount, bucket.StartIndex, 0, draw.FirstInstance);
            }

            commandBuffer.EndRenderPass();
        }

        commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 0, &cameraUniforms, sizeof(cameraUniforms));
        _isRenderingShadowMap = false;
        commandBuffer.PopDebugGroup();
    }
}
