// Distortion and post-process passes, plus the shared fullscreen pipeline helpers. "Distortion" in FRAME_PASSES runs
// before the sky pass and accumulates distortion surfaces into the half resolution distortion target. "Post-process"
// runs after the glow pass: a single fullscreen pass at scene resolution that composes the scene target with
// distortion, glow, the DOF layers and lens flares and applies color scheme, fade, cinematic bars, tint and gamma.
// It writes the frame target directly, or through SMAA (SceneRendererSMAA.cpp) and the FSR 1 upscale source.

#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/Backend/GpuTypes.h"
#include "Renderer/Scene/RenderView.h"
#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/Structures/RendererBucket.h"
#include "Renderer/Structures/RendererRoom.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Backend
{
    // Same layout as PostProcessUniforms in Shaders/PostProcess.hlsl.
    struct PostProcessUniforms
    {
        Vector3      Tint                = Vector3(0.5f);
        float        Gamma               = 1.0f;
        unsigned int ColorScheme         = 0;
        float        ColorSchemeStrength = 0.0f;
        float        ScreenFade          = 1.0f;
        float        CinematicBarsHeight = 0.0f;
        float        ViewportHeight      = 1.0f;
        float        ViewportWidth       = 1.0f;
        float        NearPlane           = 1.0f;
        float        FarPlane            = 1.0f;
        unsigned int IsDistortionActive  = 0;
        float        Time                = 0.0f;
        unsigned int LensFlareCount      = 0;
        unsigned int IsGlowActive        = 0;
        unsigned int IsDofActive         = 0;
        unsigned int IsHdrActive         = 0;
        float        Exposure            = 1.0f;
        float        BloomIntensity      = 0.0f; // Divided by the bloom step count, whose sum the bloom target holds.
        Vector4      LensFlarePositions[LENS_FLARES_MAX] = {}; // Normalized device xy, depth.
        Vector4      LensFlareColors[LENS_FLARES_MAX]    = {};
    };

    static_assert(sizeof(PostProcessUniforms) == 336, "PostProcessUniforms must match PostProcess.hlsl.");

    // Opaque fullscreen triangle with one color target and no depth, the base of every screen space pipeline.
    GraphicsPipelineDesc SceneRenderer::GetFullscreenPipelineDesc(IShader* vertexShader, IShader* pixelShader, PixelFormat format, const char* name)
    {
        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = vertexShader;
        desc.PixelShader = pixelShader;
        desc.Rasterizer.Cull = RHI::CullMode::None;
        desc.DepthStencil.EnableDepthTest = false;
        desc.DepthStencil.EnableDepthWrite = false;
        desc.NumColorTargets = 1;
        desc.ColorTargets[0].Format = format;
        desc.Name = name;
        return desc;
    }

    std::unique_ptr<IGraphicsPipeline> SceneRenderer::CreateFullscreenPipeline(IShader* vertexShader, IShader* pixelShader, PixelFormat format, const char* name)
    {
        return _device->CreateGraphicsPipeline(GetFullscreenPipelineDesc(vertexShader, pixelShader, format, name));
    }

    // Post-process pipeline, which writes the swapchain format.
    bool SceneRenderer::CreatePostProcessResources()
    {
        _postProcessVertexShader = LoadShader(*_device, _shaderDirectory, "PostProcess", "VS");
        _postProcessPixelShader = LoadShader(*_device, _shaderDirectory, "PostProcess", "PS");
        if (_postProcessVertexShader == nullptr || _postProcessPixelShader == nullptr)
            return false;

        _postProcessPipeline = CreateFullscreenPipeline(_postProcessVertexShader.get(), _postProcessPixelShader.get(), _device->GetSwapchainFormat(), "PostProcess");
        return (_postProcessPipeline != nullptr);
    }

    // Enables the distortion pass: true when any collected sprite, object bucket or visible room bucket distorts.
    // Must run after the sprites and objects of the frame are collected.
    bool SceneRenderer::HasDistortionDraws(const RenderView& view) const
    {
        for (const auto& draw : _spriteDraws)
        {
            if (draw.BlendMode == BlendMode::Distortion)
                return true;
        }

        for (const auto& draw : _objectDraws)
        {
            if (draw.Bucket->BlendMode == BlendMode::Distortion)
                return true;
        }

        for (const auto* room : view.RoomsToDraw)
        {
            for (const auto& bucket : room->Buckets)
            {
                if (bucket.BlendMode == BlendMode::Distortion && bucket.NumIndices > 0)
                    return true;
            }
        }

        return false;
    }

    // Distortion surfaces and sprites accumulate their payload in the half resolution distortion target.
    void SceneRenderer::RenderDistortion(ICommandBuffer& commandBuffer)
    {
        const auto& view = *_frameView;

        // Slot 2 is shared with the screen space and sky passes: room and object shaders read the shadow uniforms.
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &_shadowUniforms, sizeof(_shadowUniforms));

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _distortionTarget.get();

        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        DrawRooms(commandBuffer, *renderPass, ScenePass::Distortion);
        DrawObjects(commandBuffer, *renderPass, ScenePass::Distortion);
        DrawSprites(commandBuffer, *renderPass, ScenePass::Distortion);
        DrawSprites(commandBuffer, *renderPass, ScenePass::Distortion, true);
        commandBuffer.EndRenderPass();
    }

    // Scene sized target in the frame format written by the post-process: the SMAA source and the upscale source, both
    // pooled.
    TextureDesc SceneRenderer::GetPostProcessTargetDesc() const
    {
        auto desc = _sceneTarget->GetDesc();
        desc.Format = _device->GetSwapchainFormat();
        desc.Name = "PostProcess";
        return desc;
    }

    // Single fullscreen pass from the scene target to the frame target, or to the upscale source below native scale.
    // With SMAA it writes the antialiasing source, which SMAA then resolves into that output.
    void SceneRenderer::RenderPostProcess(ICommandBuffer& commandBuffer)
    {
        const auto& environment = *_frameEnvironment;
        _isColorTargetSharpened = false;

        auto uniforms = PostProcessUniforms{};
        uniforms.Tint = environment.Tint;
        uniforms.Gamma = std::max(environment.Gamma, 0.01f);
        uniforms.ColorScheme = (unsigned int)environment.ColorScheme;
        uniforms.ColorSchemeStrength = (environment.ColorScheme != 0) ? environment.ColorSchemeStrength : 0.0f;
        uniforms.ScreenFade = environment.ScreenFade;
        uniforms.CinematicBarsHeight = environment.CinematicBarsHeight;
        uniforms.ViewportHeight = (float)_sceneTarget->GetDesc().Height;
        uniforms.ViewportWidth = (float)_sceneTarget->GetDesc().Width;
        uniforms.NearPlane = _nearPlane;
        uniforms.FarPlane = _farPlane;
        uniforms.IsDistortionActive = _hasDistortion ? 1 : 0;
        uniforms.Time = _time;
        uniforms.IsGlowActive = _hasVisibleEmissive ? 1 : 0;
        uniforms.IsDofActive = _isDofActive ? (unsigned int)GetDOFLayers(environment) : 0;
        uniforms.IsHdrActive = _isHdrActive ? 1 : 0;
        uniforms.Exposure = environment.Exposure;
        uniforms.BloomIntensity = _isBloomActive ? (environment.BloomIntensity / BLOOM_STEP_COUNT) : 0.0f;

        // Flares are projected here; the frontend collection already dropped those behind the camera.
        for (int i = 0; i < environment.LensFlareCount; i++)
        {
            auto clipPosition = Vector4::Transform(Vector4(environment.LensFlarePositions[i].x, environment.LensFlarePositions[i].y, environment.LensFlarePositions[i].z, 1.0f), _viewProjection);
            if (clipPosition.w <= EPSILON)
                continue;

            uniforms.LensFlarePositions[uniforms.LensFlareCount] = Vector4(clipPosition.x / clipPosition.w, clipPosition.y / clipPosition.w, clipPosition.z / clipPosition.w, 0.0f);
            uniforms.LensFlareColors[uniforms.LensFlareCount] = Vector4(environment.LensFlareColors[i].x, environment.LensFlareColors[i].y, environment.LensFlareColors[i].z, 0.0f);
            uniforms.LensFlareCount++;
        }
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &uniforms, sizeof(uniforms));

        // The temporal antialiasing already resolved the scene after the scene pass.
        bool isAntialiased = (environment.AntialiasingMode != 0 && environment.AntialiasingMode != ANTIALIASING_MODE_TEMPORAL);
        _upscaleSource = _isUpscaleActive ? _targetPool.Acquire(GetPostProcessTargetDesc()) : nullptr;
        auto* outputTarget = _isUpscaleActive ? _upscaleSource : _colorTarget.get();
        auto* antialiasingSource = isAntialiased ? _targetPool.Acquire(GetPostProcessTargetDesc()) : nullptr;

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = isAntialiased ? antialiasingSource : outputTarget;
        colorTarget.Load = LoadOp::DontCare;

        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        renderPass->BindPipeline(_postProcessPipeline.get());

        // The depth target is never sampled: occlusion tests of the distortion and lens flares read the prepass copy.
        TextureSamplerBinding bindings[7] =
        {
            { GetResolvedSceneTarget(), _linearClampSampler.get() },
            { _distortionTarget.get(), _linearClampSampler.get() },
            { _sceneDepthCopy.get(), _pointClampSampler.get() },
            { _glowTarget.get(), _linearClampSampler.get() },
            { (_dofTargets[0] != nullptr) ? _dofTargets[0].get() : _blackTexture.get(), _linearClampSampler.get() },
            { (_dofTargets[1] != nullptr) ? _dofTargets[1].get() : _blackTexture.get(), _linearClampSampler.get() },
            { _isBloomActive ? _bloomTarget : _blackTexture.get(), _linearClampSampler.get() }
        };
        renderPass->BindPixelSamplers(0, bindings, 7);

        CountDraw(DrawCategory::Fullscreen, 1);
        renderPass->Draw(3, 1, 0, 0);
        commandBuffer.EndRenderPass();

        if (_isBloomActive)
        {
            _targetPool.Release(_bloomTarget);
            _bloomTarget = nullptr;
        }

        if (isAntialiased)
        {
            RenderAntialiasing(commandBuffer, antialiasingSource, outputTarget);
            _targetPool.Release(antialiasingSource);
        }

        // The temporal antialiasing softens the image; at native scale RCAS sharpens it while the overlay draws it, as
        // it does for the FSR upscale.
        if (_isTaaActive && !_isUpscaleActive)
            _isColorTargetSharpened = true;
    }
}
