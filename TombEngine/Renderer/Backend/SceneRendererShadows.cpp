#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/Backend/GpuTypes.h"
#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/Structures/RendererItem.h"
#include "Renderer/Structures/RendererLight.h"
#include "Renderer/Structures/RendererMesh.h"
#include "Renderer/Structures/RendererObject.h"
#include "Specific/configuration.h"
#include "Specific/level.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Backend
{
    constexpr auto SHADOW_NEAR_PLANE = 16.0f;

    bool SceneRenderer::CreateShadowResources()
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
    bool SceneRenderer::EnsureShadowMap(int size)
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

    // Shadow caster rules: the object must cast shadows, and with the player-only setting only objects
    // marked for player shadows do.
    bool SceneRenderer::IsShadowCaster(int objectID) const
    {
        if (!_isShadowMapActive)
            return false;

        const auto& object = (*_levelGeometry.MoveableObjects)[objectID];
        if (!object.has_value() || object->ShadowType == ShadowMode::None)
            return false;

        return (!_isPlayerShadowOnly || object->ShadowType == ShadowMode::Player);
    }

    // Objects receive shadows only when they do not cast them.
    unsigned int SceneRenderer::GetShadowReceiverMask(int objectID) const
    {
        const auto& object = (*_levelGeometry.MoveableObjects)[objectID];
        return (object.has_value() && object->ShadowType == ShadowMode::None) ? SHADOWABLE_MASK : 0;
    }

    void SceneRenderer::MarkShadowCasterDraws(int firstDraw, const Vector4& bounds)
    {
        int firstShadowDraw = (int)_shadowDraws.size();
        _shadowDraws.insert(_shadowDraws.end(), _objectDraws.begin() + firstDraw, _objectDraws.end());
        for (int i = firstShadowDraw; i < (int)_shadowDraws.size(); i++)
            _shadowDraws[i].Bounds = bounds;
    }

    // Sphere enclosing the meshes of an item at its interpolated pose, for the culling of the shadow map faces.
    Vector4 SceneRenderer::GetItemBounds(const RendererItem& item, float margin) const
    {
        const auto& world = item.InterpolatedWorld;
        float scale = std::max({ world.Right().Length(), world.Up().Length(), world.Backward().Length() });

        auto bounds = BoundingSphere(world.Translation(), 0.0f);
        int meshCount = std::min((int)item.MeshIndex.size(), (int)BONE_COUNT_MAX);
        for (int i = 0; i < meshCount; i++)
        {
            const auto& sphere = (*_levelGeometry.Meshes)[item.MeshIndex[i]]->Sphere;
            auto meshSphere = BoundingSphere(Vector3::Transform(sphere.Center, item.InterpolatedAnimationTransforms[i] * world), sphere.Radius * scale);
            BoundingSphere::CreateMerged(bounds, bounds, meshSphere);
        }

        return Vector4(bounds.Center.x, bounds.Center.y, bounds.Center.z, bounds.Radius + margin);
    }

    // Cube face view projections as in the frontend, and the uniforms read by the scene shaders.
    void SceneRenderer::PrepareShadows(const FrameEnvironment& environment, float interpolationFactor)
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

        _shadowUniforms.ShadowLight = ToGpuLight(*light, interpolationFactor, 1.0f);
        _shadowUniforms.CastShadows = 1;
        _shadowUniforms.ShadowMapSize = _shadowMap->GetDesc().Width;

        auto position = _shadowUniforms.ShadowLight.Position;
        auto projection = Matrix::CreatePerspectiveFieldOfView(PI / 2.0f, 1.0f, SHADOW_NEAR_PLANE, light->Out);
        for (int face = 0; face < 6; face++)
        {
            auto view = Matrix::CreateLookAt(position, position + FORWARD_VECTORS[face] * BLOCK(10), UP_VECTORS[face]);
            _shadowUniforms.LightViewProjections[face] = view * projection;
            _shadowFaceFrustums[face].Update(view, projection);
        }
    }

    // Six depth only passes, one per cube face, with the instanced draws of the shadow casters.
    void SceneRenderer::RenderShadowMap(ICommandBuffer& commandBuffer)
    {
        if (!_shadowUniforms.CastShadows)
            return;

        const auto& cameraUniforms = _frameVertexUniforms;
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
            BindLevelGeometry(*renderPass, MeshSource::Moveables, true);

            auto state = BucketBindState{};
            for (const auto& draw : _shadowDraws)
            {
                const auto& bucket = *draw.Bucket;
                if (bucket.BlendMode != BlendMode::Opaque && bucket.BlendMode != BlendMode::AlphaTest)
                    continue;

                // Casters outside the face frustum, including those beyond the light range, are skipped.
                if (draw.Bounds.w >= 0.0f && !_shadowFaceFrustums[face].SphereInFrustum(Vector3(draw.Bounds), draw.Bounds.w))
                    continue;

                const auto& pages = (draw.Source == MeshSource::Statics) ? _staticTextures : _moveableTextures;
                BindBucket(commandBuffer, *renderPass, bucket, pages, bucket.BlendMode, 0, BucketDrawInfo{}, state);
                DrawBucket(*renderPass, draw.Source, bucket, draw.InstanceCount, draw.FirstInstance);
            }

            commandBuffer.EndRenderPass();
        }

        commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 0, &cameraUniforms, sizeof(cameraUniforms));
        _isRenderingShadowMap = false;
    }
}
