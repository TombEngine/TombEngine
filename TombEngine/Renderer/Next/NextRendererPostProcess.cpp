#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/Next/GpuTypes.h"
#include "Renderer/RenderView.h"
#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/Structures/RendererBucket.h"
#include "Renderer/Structures/RendererRoom.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Next
{
    // Same layout as PostProcessUniforms in Shaders/RHI/PostProcess.hlsl.
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
        float        Padding[3]          = {};
        Vector4      LensFlarePositions[LENS_FLARES_MAX] = {}; // Normalized device xy, depth.
        Vector4      LensFlareColors[LENS_FLARES_MAX]    = {};
    };

    static_assert(sizeof(PostProcessUniforms) == 336, "PostProcessUniforms must match PostProcess.hlsl.");

    std::unique_ptr<IGraphicsPipeline> NextRenderer::CreateFullscreenPipeline(IShader* vertexShader, IShader* pixelShader, PixelFormat format, const char* name)
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
        return _device->CreateGraphicsPipeline(desc);
    }

    bool NextRenderer::CreatePostProcessResources()
    {
        _postProcessVertexShader = LoadShader(*_device, _shaderDirectory, "PostProcess", "VS");
        _postProcessPixelShader = LoadShader(*_device, _shaderDirectory, "PostProcess", "PS");
        if (_postProcessVertexShader == nullptr || _postProcessPixelShader == nullptr)
            return false;

        _postProcessPipeline = CreateFullscreenPipeline(_postProcessVertexShader.get(), _postProcessPixelShader.get(), _device->GetSwapchainFormat(), "PostProcess");

        _depthCopyPixelShader = LoadShader(*_device, _shaderDirectory, "PostProcess", "PSDepthCopy");
        if (_depthCopyPixelShader == nullptr)
            return false;

        _depthCopyPipeline = CreateFullscreenPipeline(_postProcessVertexShader.get(), _depthCopyPixelShader.get(), PixelFormat::R32Float, "DepthCopy");

        _fxaaPixelShader = LoadShader(*_device, _shaderDirectory, "PostProcess", "PSFXAA");
        if (_fxaaPixelShader == nullptr)
            return false;

        _fxaaPipeline = CreateFullscreenPipeline(_postProcessVertexShader.get(), _fxaaPixelShader.get(), _device->GetSwapchainFormat(), "FXAA");
        return (_postProcessPipeline != nullptr && _depthCopyPipeline != nullptr && _fxaaPipeline != nullptr);
    }

    bool NextRenderer::HasDistortionDraws(const RenderView& view) const
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
    void NextRenderer::RenderDistortion(ICommandBuffer& commandBuffer, const RenderView& view)
    {
        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _distortionTarget.get();

        commandBuffer.PushDebugGroup("Distortion");
        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        DrawRooms(commandBuffer, *renderPass, view, ScenePass::Distortion);
        DrawObjects(commandBuffer, *renderPass, ScenePass::Distortion);
        DrawSprites(commandBuffer, *renderPass, ScenePass::Distortion);
        DrawSprites(commandBuffer, *renderPass, ScenePass::Distortion, true);
        commandBuffer.EndRenderPass();
        commandBuffer.PopDebugGroup();
    }

    // The scene pass keeps the prepass depth as its target, so effects reading the depth (soft particles) use a copy.
    void NextRenderer::CopySceneDepth(ICommandBuffer& commandBuffer)
    {
        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _sceneDepthCopy.get();
        colorTarget.Load = LoadOp::DontCare;

        commandBuffer.PushDebugGroup("Depth copy");
        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        renderPass->BindPipeline(_depthCopyPipeline.get());

        auto binding = TextureSamplerBinding{ _depthTarget.get(), _pointClampSampler.get() };
        renderPass->BindPixelSamplers(0, &binding, 1);

        _frameStats.DrawCalls++;
        renderPass->Draw(3, 1, 0, 0);
        commandBuffer.EndRenderPass();
        commandBuffer.PopDebugGroup();
    }

    // Single fullscreen pass from the scene target to the frame target.
    void NextRenderer::RenderPostProcess(ICommandBuffer& commandBuffer, const FrameEnvironment& environment)
    {
        auto uniforms = PostProcessUniforms{};
        uniforms.Tint = environment.Tint;
        uniforms.Gamma = std::max(environment.Gamma, 0.01f);
        uniforms.ColorScheme = (unsigned int)environment.ColorScheme;
        uniforms.ColorSchemeStrength = (environment.ColorScheme != 0) ? environment.ColorSchemeStrength : 0.0f;
        uniforms.ScreenFade = environment.ScreenFade;
        uniforms.CinematicBarsHeight = environment.CinematicBarsHeight;
        uniforms.ViewportHeight = (float)_colorTarget->GetDesc().Height;
        uniforms.ViewportWidth = (float)_colorTarget->GetDesc().Width;
        uniforms.NearPlane = _nearPlane;
        uniforms.FarPlane = _farPlane;
        uniforms.IsDistortionActive = _hasDistortion ? 1 : 0;
        uniforms.Time = _time;
        uniforms.IsGlowActive = _hasEmissiveMaps ? 1 : 0;
        uniforms.IsDofActive = _isDofActive ? 1 : 0;

        // Flares are projected here; the legacy collection already dropped those behind the camera.
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

        bool isAntialiased = (environment.AntialiasingMode != 0);

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = isAntialiased ? _antialiasingSource.get() : _colorTarget.get();
        colorTarget.Load = LoadOp::DontCare;

        commandBuffer.PushDebugGroup("Post-process");
        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        renderPass->BindPipeline(_postProcessPipeline.get());

        TextureSamplerBinding bindings[6] =
        {
            { _sceneTarget.get(), _linearClampSampler.get() },
            { _distortionTarget.get(), _linearClampSampler.get() },
            { _sceneDepthCopy.get(), _pointClampSampler.get() },
            { _glowTargets[0].get(), _linearClampSampler.get() },
            { _dofTargets[0].get(), _linearClampSampler.get() },
            { _dofTargets[1].get(), _linearClampSampler.get() }
        };
        renderPass->BindPixelSamplers(0, bindings, 6);

        _frameStats.DrawCalls++;
        renderPass->Draw(3, 1, 0, 0);
        commandBuffer.EndRenderPass();

        commandBuffer.PopDebugGroup();

        if (isAntialiased)
            RenderAntialiasing(commandBuffer, environment.AntialiasingMode, _colorTarget.get());
    }
}
