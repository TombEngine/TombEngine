// Sky pass ("Sky" in FRAME_PASSES, after the distortion pass and before the scene pass): clears the full resolution
// scene target to the fog color and, when a sky room is visible, draws the sky background, the volumetric cloud
// layers, the sun and moon discs, the aurora, the horizon meshes and the god rays. The scene pass then loads the
// result. Clouds raymarch at the resolution chosen by the frontend, god rays at half resolution. The dust storm,
// drawn later inside the scene pass, also lives here.

#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/Scene/RenderView.h"
#include "Renderer/RHI/ShaderLibrary.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Backend
{
    // Same layout as CloudCameraUniforms in Shaders/VolumetricClouds.hlsl.
    struct CloudCameraUniforms
    {
        Matrix  InverseView       = Matrix::Identity;
        Matrix  InverseProjection = Matrix::Identity;
        Vector4 CameraPosition    = Vector4::Zero;
    };

    // Raymarch, composite and sun occlusion pipelines, plus the 1x1 occlusion targets read back for the lens flares.
    bool SceneRenderer::CreateCloudResources()
    {
        _cloudVertexShader = LoadShader(*_device, _shaderDirectory, "VolumetricClouds", "VS");
        _cloudRaymarchPixelShader = LoadShader(*_device, _shaderDirectory, "VolumetricClouds", "PS");
        _cloudCompositePixelShader = LoadShader(*_device, _shaderDirectory, "VolumetricClouds", "PSCloudComposite");
        _cloudOcclusionPixelShader = LoadShader(*_device, _shaderDirectory, "VolumetricClouds", "PSCloudOcclusion");
        if (_cloudVertexShader == nullptr || _cloudRaymarchPixelShader == nullptr || _cloudCompositePixelShader == nullptr ||
            _cloudOcclusionPixelShader == nullptr)
        {
            return false;
        }

        // Both passes are opaque fullscreen triangles: the composite shader blends with its copy of the scene.
        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = _cloudVertexShader.get();
        desc.PixelShader = _cloudRaymarchPixelShader.get();
        desc.Rasterizer.Cull = RHI::CullMode::None;
        desc.DepthStencil.EnableDepthTest = false;
        desc.DepthStencil.EnableDepthWrite = false;
        desc.NumColorTargets = 1;
        desc.ColorTargets[0].Format = PixelFormat::RGBA16Float;
        desc.Name = "Clouds.Raymarch";
        _cloudRaymarchPipeline = _device->CreateGraphicsPipeline(desc);

        desc.PixelShader = _cloudCompositePixelShader.get();
        desc.ColorTargets[0].Format = GetSceneColorFormat();
        desc.Name = "Clouds.Composite";
        CreateSceneColorPipeline(_cloudCompositePipeline, desc);

        desc.PixelShader = _cloudOcclusionPixelShader.get();
        desc.ColorTargets[0].Format = PixelFormat::R32Float;
        desc.Name = "Clouds.Occlusion";
        _cloudOcclusionPipeline = _device->CreateGraphicsPipeline(desc);

        auto targetDesc = TextureDesc{};
        targetDesc.Width = 1;
        targetDesc.Height = 1;
        targetDesc.Format = PixelFormat::R32Float;
        targetDesc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        for (int layer = 0; layer < CLOUD_LAYER_COUNT; layer++)
        {
            targetDesc.Name = "Clouds.Occlusion" + std::to_string(layer);
            _cloudOcclusionTargets[layer] = _device->CreateTexture(targetDesc);
            if (_cloudOcclusionTargets[layer] == nullptr)
                return false;
        }

        // Cloud noise tiles.
        auto samplerDesc = SamplerDesc{};
        samplerDesc.Name = "LinearWrap";
        _linearWrapSampler = _device->CreateSampler(samplerDesc);

        return (_cloudRaymarchPipeline != nullptr && _cloudCompositePipeline != nullptr && _cloudOcclusionPipeline != nullptr &&
                _linearWrapSampler != nullptr);
    }

    // The noise is generated once by the frontend; it is uploaded on the first frame that provides it.
    void SceneRenderer::EnsureCloudNoise(const CloudNoiseData& noise)
    {
        if (_cloudNoise3D != nullptr || noise.Noise3D == nullptr || noise.Worley2D == nullptr)
            return;

        auto desc = TextureDesc{};
        desc.Type = TextureType::Texture3D;
        desc.Width = desc.Height = desc.DepthOrLayers = noise.Noise3DSize;
        desc.Format = PixelFormat::RGBA8Unorm;
        desc.Name = "Clouds.Noise3D";
        auto noise3D = _device->CreateTexture(desc);

        desc.Type = TextureType::Texture2D;
        desc.Width = desc.Height = noise.Worley2DSize;
        desc.DepthOrLayers = 1;
        desc.Format = PixelFormat::RG8Unorm;
        desc.Name = "Clouds.Worley2D";
        auto worley2D = _device->CreateTexture(desc);

        if (noise3D == nullptr || worley2D == nullptr)
            return;

        int noise3DSize = noise.Noise3DSize;
        UploadTexture(noise3D.get(), noise.Noise3D, noise3DSize * noise3DSize * noise3DSize * 4);
        UploadTexture(worley2D.get(), noise.Worley2D, noise.Worley2DSize * noise.Worley2DSize * 2);
        _cloudNoise3D = std::move(noise3D);
        _cloudWorley2D = std::move(worley2D);
    }

    // Cloud layer targets and their histories, at the cloud resolution of the frontend uniforms. New targets are
    // cleared.
    bool SceneRenderer::EnsureCloudTargets(ICommandBuffer& commandBuffer, int width, int height)
    {
        if (_cloudTargets[0] != nullptr && _cloudTargets[0]->GetDesc().Width == width && _cloudTargets[0]->GetDesc().Height == height)
            return true;

        auto desc = TextureDesc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = PixelFormat::RGBA16Float;
        desc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        for (int layer = 0; layer < CLOUD_LAYER_COUNT; layer++)
        {
            desc.Name = "Clouds.Layer" + std::to_string(layer);
            _cloudTargets[layer] = _device->CreateTexture(desc);
            desc.Name += ".History";
            _cloudHistoryTargets[layer] = _device->CreateTexture(desc);
            if (_cloudTargets[layer] == nullptr || _cloudHistoryTargets[layer] == nullptr)
                return false;

            ClearColorTarget(commandBuffer, _cloudTargets[layer].get());
            ClearColorTarget(commandBuffer, _cloudHistoryTargets[layer].get());
        }

        return true;
    }

    // Clears a color target with an empty render pass.
    void SceneRenderer::ClearColorTarget(ICommandBuffer& commandBuffer, ITexture* texture)
    {
        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = texture;
        colorTarget.Load = LoadOp::Clear;
        commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        commandBuffer.EndRenderPass();
    }

    // True when the frontend queued a cloud draw for the given side of the horizon meshes and the noise is uploaded.
    bool SceneRenderer::HasCloudDraws(bool isAfterHorizons) const
    {
        const auto* draws = _frameEnvironment->CloudDraws;
        if (draws == nullptr || _cloudNoise3D == nullptr)
            return false;

        for (const auto& draw : *draws)
        {
            if (draw.IsAfterHorizons == isAfterHorizons)
                return true;
        }

        return false;
    }

    // Replays the frontend cloud draws before or after the horizon meshes, outside of the sky render pass.
    void SceneRenderer::DrawCloudLayers(ICommandBuffer& commandBuffer, bool isAfterHorizons)
    {
        const auto& cameraPosition = _frameEnvironment->CameraPosition;
        auto cameraUniforms = CloudCameraUniforms{ _view.Invert(), _projection.Invert(), Vector4(cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0f) };

        commandBuffer.PushDebugGroup("Clouds");
        for (const auto& draw : *_frameEnvironment->CloudDraws)
        {
            if (draw.IsAfterHorizons != isAfterHorizons || draw.Layer < 0 || draw.Layer >= CLOUD_LAYER_COUNT)
                continue;

            // Cleared layers carry no uniforms; their targets only exist once a layer was drawn.
            if (draw.IsCleared)
            {
                if (_cloudTargets[draw.Layer] != nullptr)
                    ClearColorTarget(commandBuffer, _cloudTargets[draw.Layer].get());

                continue;
            }

            int width = std::max((int)draw.Uniforms.CloudRenderSize.x, 1);
            int height = std::max((int)draw.Uniforms.CloudRenderSize.y, 1);
            if (!EnsureCloudTargets(commandBuffer, width, height))
                break;

            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &draw.Uniforms, sizeof(draw.Uniforms));
            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 3, &cameraUniforms, sizeof(cameraUniforms));

            if (draw.IsHistoryReset)
                ClearColorTarget(commandBuffer, _cloudHistoryTargets[draw.Layer].get());

            if (draw.IsRaymarched)
                RaymarchCloudLayer(commandBuffer, draw);

            if (draw.IsOcclusionQueried)
                QueryCloudOcclusion(commandBuffer, draw);

            CompositeCloudLayer(commandBuffer, draw.Layer);
        }

        commandBuffer.PopDebugGroup();
    }

    // Pass 1: the current target becomes the history of the temporal reprojection, then the layer is raymarched.
    void SceneRenderer::RaymarchCloudLayer(ICommandBuffer& commandBuffer, const CloudLayerDraw& draw)
    {
        auto* target = _cloudTargets[draw.Layer].get();
        auto* history = _cloudHistoryTargets[draw.Layer].get();
        if (draw.IsHistoryUsed)
            commandBuffer.BlitTexture(target, history);

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = target;
        colorTarget.Load = LoadOp::Clear;

        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        renderPass->BindPipeline(_cloudRaymarchPipeline.get());

        TextureSamplerBinding bindings[3] =
        {
            { history, _linearClampSampler.get() },
            { _cloudNoise3D.get(), _linearWrapSampler.get() },
            { _cloudWorley2D.get(), _linearWrapSampler.get() }
        };
        renderPass->BindPixelSamplers(0, bindings, 3);

        CountDraw(DrawCategory::Sky, 1);
        renderPass->Draw(3, 1, 0, 0);
        commandBuffer.EndRenderPass();
    }

    // Sun transmittance through the layer, from rays toward the sun and the layer coverage around it on screen. The value
    // is read back without stalling and reaches the lens flares some frames later.
    void SceneRenderer::QueryCloudOcclusion(ICommandBuffer& commandBuffer, const CloudLayerDraw& draw)
    {
        auto* target = _cloudOcclusionTargets[draw.Layer].get();

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = target;
        colorTarget.Load = LoadOp::Clear;

        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        renderPass->BindPipeline(_cloudOcclusionPipeline.get());

        TextureSamplerBinding bindings[3] =
        {
            { _cloudTargets[draw.Layer].get(), _linearClampSampler.get() },
            { _cloudNoise3D.get(), _linearWrapSampler.get() },
            { _cloudWorley2D.get(), _linearWrapSampler.get() }
        };
        renderPass->BindPixelSamplers(0, bindings, 3);

        CountDraw(DrawCategory::Sky, 1);
        renderPass->Draw(3, 1, 0, 0);
        commandBuffer.EndRenderPass();

        int layer = draw.Layer;
        _readback.Record(*_device, *commandBuffer.BeginCopyPass(), target, [this, layer](const TextureDesc& desc, const unsigned char* data)
        {
            _cloudTransmittances[layer] = std::clamp(*(const float*)data, 0.0f, 1.0f);
            _isCloudTransmittanceRead[layer] = true;
        });
        commandBuffer.EndCopyPass();
    }

    // Returns true once per completed readback of the layer transmittance.
    bool SceneRenderer::ReadCloudTransmittance(int layer, float& transmittance)
    {
        if (layer < 0 || layer >= CLOUD_LAYER_COUNT || !_isCloudTransmittanceRead[layer])
            return false;

        transmittance = _cloudTransmittances[layer];
        _isCloudTransmittanceRead[layer] = false;
        return true;
    }

    // Pass 2: the composite shader blends the layer with a copy of the scene and writes the coverage to alpha.
    void SceneRenderer::CompositeCloudLayer(ICommandBuffer& commandBuffer, int layer)
    {
        auto* sceneCopy = _targetPool.Acquire(_sceneTarget->GetDesc());
        if (sceneCopy == nullptr)
            return;

        commandBuffer.BlitTexture(_sceneTarget.get(), sceneCopy);

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _sceneTarget.get();
        colorTarget.Load = LoadOp::Load;

        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        renderPass->BindPipeline(_cloudCompositePipeline.get());

        TextureSamplerBinding bindings[2] =
        {
            { _cloudTargets[layer].get(), _linearClampSampler.get() },
            { sceneCopy, _linearClampSampler.get() }
        };
        renderPass->BindPixelSamplers(0, bindings, 2);

        CountDraw(DrawCategory::Sky, 1);
        renderPass->Draw(3, 1, 0, 0);
        commandBuffer.EndRenderPass();
        _targetPool.Release(sceneCopy);
    }

    // Half resolution radial blur pipeline and the additive composite pipeline of the god rays.
    bool SceneRenderer::CreateGodRayResources()
    {
        _godRayVertexShader = LoadShader(*_device, _shaderDirectory, "GodRay", "VS");
        _godRayPixelShader = LoadShader(*_device, _shaderDirectory, "GodRay", "PSGodRay");
        _godRayCompositePixelShader = LoadShader(*_device, _shaderDirectory, "GodRay", "PSGodRayComposite");
        if (_godRayVertexShader == nullptr || _godRayPixelShader == nullptr || _godRayCompositePixelShader == nullptr)
            return false;

        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = _godRayVertexShader.get();
        desc.PixelShader = _godRayPixelShader.get();
        desc.Rasterizer.Cull = RHI::CullMode::None;
        desc.DepthStencil.EnableDepthTest = false;
        desc.DepthStencil.EnableDepthWrite = false;
        desc.NumColorTargets = 1;
        desc.ColorTargets[0].Format = PixelFormat::R11G11B10Float;
        desc.Name = "GodRays.Blur";
        _godRayPipeline = _device->CreateGraphicsPipeline(desc);

        desc.PixelShader = _godRayCompositePixelShader.get();
        desc.ColorTargets[0].Format = GetSceneColorFormat();
        desc.ColorTargets[0].Blend = GetBlendState(BlendMode::Additive);
        desc.Name = "GodRays.Composite";
        CreateSceneColorPipeline(_godRayCompositePipeline, desc);

        return (_godRayPipeline != nullptr && _godRayCompositePipeline != nullptr);
    }

    // God rays: a horizon silhouette and a radial blur at half resolution, then an additive composite. The blur
    // reads both cloud layers, the silhouette and a copy of the sky.
    void SceneRenderer::DrawGodRays(ICommandBuffer& commandBuffer)
    {
        const auto& godRays = _frameEnvironment->GodRays;
        if (!godRays.IsDrawn)
            return;

        // The underwater shafts draw without cloud layers, which then read as transparent.
        const auto& sceneDesc = _sceneTarget->GetDesc();
        if (_cloudTargets[0] == nullptr && !EnsureCloudTargets(commandBuffer, std::max(sceneDesc.Width / 4, 1), std::max(sceneDesc.Height / 4, 1)))
            return;

        auto desc = TextureDesc{};
        desc.Width = std::max(sceneDesc.Width / 2, 1);
        desc.Height = std::max(sceneDesc.Height / 2, 1);
        desc.Format = PixelFormat::R11G11B10Float;
        desc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        desc.Name = "Sky.HorizonMask";
        auto* maskTarget = _targetPool.Acquire(desc);
        desc.Name = "Sky.GodRays";
        auto* rayTarget = _targetPool.Acquire(desc);
        auto* sceneCopy = _targetPool.Acquire(sceneDesc);
        if (maskTarget != nullptr && rayTarget != nullptr && sceneCopy != nullptr)
        {
            commandBuffer.PushDebugGroup("God rays");
            commandBuffer.BlitTexture(_sceneTarget.get(), sceneCopy);

            // Horizon silhouette.
            auto colorTarget = ColorTargetInfo{};
            colorTarget.Texture = maskTarget;
            auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
            DrawHorizonMeshes(commandBuffer, *renderPass, *_frameView, _frameEnvironment->InterpolationFactor, 0, true);
            commandBuffer.EndRenderPass();

            // Radial blur.
            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &godRays.Uniforms, sizeof(godRays.Uniforms));
            colorTarget.Texture = rayTarget;
            renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
            renderPass->BindPipeline(_godRayPipeline.get());

            TextureSamplerBinding bindings[4] =
            {
                { _cloudTargets[0].get(), _linearClampSampler.get() },
                { _cloudTargets[1].get(), _linearClampSampler.get() },
                { maskTarget, _linearClampSampler.get() },
                { sceneCopy, _linearClampSampler.get() }
            };
            renderPass->BindPixelSamplers(0, bindings, 4);
            CountDraw(DrawCategory::Sky, 1);
            renderPass->Draw(3, 1, 0, 0);
            commandBuffer.EndRenderPass();

            // Additive composite.
            colorTarget.Texture = _sceneTarget.get();
            colorTarget.Load = LoadOp::Load;
            renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
            DrawFullscreen(*renderPass, _godRayCompositePipeline.get(), TextureSamplerBinding{ rayTarget, _linearClampSampler.get() });
            commandBuffer.EndRenderPass();
            commandBuffer.PopDebugGroup();
        }

        _targetPool.Release(maskTarget);
        _targetPool.Release(rayTarget);
        _targetPool.Release(sceneCopy);
    }

    bool SceneRenderer::CreateDustStormResources()
    {
        _dustStormVertexShader = LoadShader(*_device, _shaderDirectory, "DustStorm", "VS");
        _dustStormPixelShader = LoadShader(*_device, _shaderDirectory, "DustStorm", "PSDustStorm");
        if (_dustStormVertexShader == nullptr || _dustStormPixelShader == nullptr)
            return false;

        // Drawn inside the scene pass, without depth test.
        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = _dustStormVertexShader.get();
        desc.PixelShader = _dustStormPixelShader.get();
        desc.Rasterizer.Cull = RHI::CullMode::None;
        desc.DepthStencil.EnableDepthTest = false;
        desc.DepthStencil.EnableDepthWrite = false;
        desc.DepthFormat = PixelFormat::D32Float;
        desc.NumColorTargets = 1;
        desc.ColorTargets[0].Format = GetSceneColorFormat();
        desc.ColorTargets[0].Blend = GetBlendState(BlendMode::AlphaBlend);
        desc.Name = "DustStorm";
        CreateSceneColorPipeline(_dustStormPipeline, desc);
        return (_dustStormPipeline != nullptr);
    }

    // Dust storm: a fullscreen raymarch over the transparent geometry, clamped by the prepass depth and
    // limited to outdoor rooms by the outdoor mask in the alpha of the prepass normals.
    void SceneRenderer::DrawDustStorm(ICommandBuffer& commandBuffer, IRenderPass& renderPass)
    {
        const auto& dustStorm = _frameEnvironment->DustStorm;
        if (!dustStorm.IsDrawn)
            return;

        commandBuffer.PushDebugGroup("Dust storm");
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &dustStorm.Uniforms, sizeof(dustStorm.Uniforms));
        renderPass.BindPipeline(_dustStormPipeline.get());

        TextureSamplerBinding bindings[2] =
        {
            { _sceneDepthCopy.get(), _pointClampSampler.get() },
            { _normalTarget.get(), _pointClampSampler.get() }
        };
        renderPass.BindPixelSamplers(0, bindings, 2);
        CountDraw(DrawCategory::Sky, 1);
        renderPass.Draw(3, 1, 0, 0);

        // Later scene draws read the shadow uniforms from the same slot.
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &_shadowUniforms, sizeof(_shadowUniforms));
        commandBuffer.PopDebugGroup();
    }

    // Scene target with the depth of the prepass, which the sky pipelines test but never write, so the sky covers only
    // the texels left empty by the scene geometry.
    IRenderPass* SceneRenderer::BeginSkyRenderPass(ICommandBuffer& commandBuffer, LoadOp load)
    {
        const auto& fogColor = _frameEnvironment->FogColor;

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _sceneTarget.get();
        colorTarget.Load = load;
        colorTarget.ClearColor[0] = fogColor.x;
        colorTarget.ClearColor[1] = fogColor.y;
        colorTarget.ClearColor[2] = fogColor.z;
        colorTarget.ClearColor[3] = 1.0f;

        auto depthTarget = DepthTargetInfo{};
        depthTarget.Texture = _depthTarget.get();
        depthTarget.Load = LoadOp::Load;

        return commandBuffer.BeginRenderPass(&colorTarget, 1, &depthTarget);
    }

    // Sky of the view before the scene pass, which loads the result.
    // Cloud layers composite over the sky background and, for the bleed overlay, over the horizon meshes.
    void SceneRenderer::RenderSkyPass(ICommandBuffer& commandBuffer)
    {
        const auto& view = *_frameView;

        auto* renderPass = BeginSkyRenderPass(commandBuffer, LoadOp::Clear);
        if (!IsSkyVisible(view))
        {
            commandBuffer.EndRenderPass();
            return;
        }

        DrawSkyBackground(commandBuffer, *renderPass, view, 0);
        if (HasCloudDraws(false) || HasSkyDiscsOrAurora())
        {
            commandBuffer.EndRenderPass();
            if (HasCloudDraws(false))
                DrawCloudLayers(commandBuffer, false);

            DrawSkyDiscsAndAurora(commandBuffer);
            renderPass = BeginSkyRenderPass(commandBuffer, LoadOp::Load);
        }

        DrawSkyHorizons(commandBuffer, *renderPass, view, _frameEnvironment->InterpolationFactor, 0);
        commandBuffer.EndRenderPass();

        if (HasCloudDraws(true))
            DrawCloudLayers(commandBuffer, true);

        DrawGodRays(commandBuffer);
    }
}
