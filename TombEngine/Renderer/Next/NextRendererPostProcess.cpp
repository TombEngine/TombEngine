#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/Next/GpuTypes.h"
#include "Renderer/RHI/ShaderLibrary.h"

using namespace TEN::Renderer::RHI;

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
        float        Padding[3]          = {};
    };

    static_assert(sizeof(PostProcessUniforms) == 48, "PostProcessUniforms must match PostProcess.hlsl.");

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
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &uniforms, sizeof(uniforms));

        bool isAntialiased = (environment.AntialiasingMode != 0);

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = isAntialiased ? _antialiasingSource.get() : _colorTarget.get();
        colorTarget.Load = LoadOp::DontCare;

        commandBuffer.PushDebugGroup("Post-process");
        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        renderPass->BindPipeline(_postProcessPipeline.get());

        auto binding = TextureSamplerBinding{ _sceneTarget.get(), _pointClampSampler.get() };
        renderPass->BindPixelSamplers(0, &binding, 1);

        _frameStats.DrawCalls++;
        renderPass->Draw(3, 1, 0, 0);
        commandBuffer.EndRenderPass();

        if (isAntialiased)
        {
            colorTarget.Texture = _colorTarget.get();
            renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
            renderPass->BindPipeline(_fxaaPipeline.get());

            binding = TextureSamplerBinding{ _antialiasingSource.get(), _linearClampSampler.get() };
            renderPass->BindPixelSamplers(0, &binding, 1);

            _frameStats.DrawCalls++;
            renderPass->Draw(3, 1, 0, 0);
            commandBuffer.EndRenderPass();
        }

        commandBuffer.PopDebugGroup();
    }
}
