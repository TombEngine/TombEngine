#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/RHI/ShaderLibrary.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Next
{
    // Same layout as SkyCameraUniforms in Shaders/RHI/AtmosphericSky.hlsl.
    struct SkyCameraUniforms
    {
        Matrix InverseProjection = Matrix::Identity;
        Matrix InverseView       = Matrix::Identity;
    };

    bool NextRenderer::CreateAtmosphereResources()
    {
        _atmosphericSkyVertexShader = LoadShader(*_device, _shaderDirectory, "AtmosphericSky", "VS");
        _atmosphericSkyPixelShader = LoadShader(*_device, _shaderDirectory, "AtmosphericSky", "PSAtmosphericSky");
        _skyDiscPixelShader = LoadShader(*_device, _shaderDirectory, "AtmosphericSky", "PSSunMoonDisc");
        _auroraPixelShader = LoadShader(*_device, _shaderDirectory, "AtmosphericSky", "PSAurora");
        _skyCopyPixelShader = LoadShader(*_device, _shaderDirectory, "AtmosphericSky", "PSCopy");
        if (_atmosphericSkyVertexShader == nullptr || _atmosphericSkyPixelShader == nullptr || _skyDiscPixelShader == nullptr ||
            _auroraPixelShader == nullptr || _skyCopyPixelShader == nullptr)
        {
            return false;
        }

        // Drawn inside the sky pass, so the pipeline matches its targets; the dome ignores depth.
        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = _atmosphericSkyVertexShader.get();
        desc.PixelShader = _atmosphericSkyPixelShader.get();
        desc.Rasterizer.Cull = RHI::CullMode::None;
        desc.DepthStencil.EnableDepthTest = false;
        desc.DepthStencil.EnableDepthWrite = false;
        desc.DepthFormat = PixelFormat::D32Float;
        desc.NumColorTargets = 1;
        desc.ColorTargets[0].Format = _device->GetSwapchainFormat();
        desc.Name = "AtmosphericSky";
        _atmosphericSkyPipeline = _device->CreateGraphicsPipeline(desc);

        // Sun and moon discs and aurora add to the sky in passes without depth, after the clouds.
        desc.DepthFormat = PixelFormat::Unknown;
        desc.ColorTargets[0].Blend = GetBlendState(BlendMode::Additive);
        desc.PixelShader = _skyDiscPixelShader.get();
        desc.Name = "AtmosphericSky.SunMoonDisc";
        _skyDiscPipeline = _device->CreateGraphicsPipeline(desc);

        desc.PixelShader = _auroraPixelShader.get();
        desc.Name = "AtmosphericSky.Aurora";
        _auroraPipeline = _device->CreateGraphicsPipeline(desc);

        desc.PixelShader = _skyCopyPixelShader.get();
        desc.Name = "AtmosphericSky.AuroraUpscale";
        _auroraUpscalePipeline = _device->CreateGraphicsPipeline(desc);

        desc.PixelShader = _auroraPixelShader.get();
        desc.ColorTargets[0].Format = PixelFormat::RGBA16Float;
        desc.Name = "AtmosphericSky.AuroraHalfResolution";
        _auroraHalfPipeline = _device->CreateGraphicsPipeline(desc);

        return (_atmosphericSkyPipeline != nullptr && _skyDiscPipeline != nullptr && _auroraPipeline != nullptr &&
                _auroraUpscalePipeline != nullptr && _auroraHalfPipeline != nullptr);
    }

    void NextRenderer::PushSkyUniforms(ICommandBuffer& commandBuffer)
    {
        auto cameraUniforms = SkyCameraUniforms{ _projection.Invert(), _view.Invert() };
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, _atmosphericSkyUniforms, _atmosphericSkyUniformsSize);
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 3, &cameraUniforms, sizeof(cameraUniforms));
    }

    // Legacy DrawAtmosphericSkyDome: a fullscreen opaque background below the classic sky layers. The uniform block
    // is the one the legacy renderer computed for this frame.
    void NextRenderer::DrawAtmosphericSky(ICommandBuffer& commandBuffer, IRenderPass& renderPass)
    {
        if (_atmosphericSkyUniforms == nullptr || !_frameEnvironment->SkyEffects.IsDomeDrawn)
            return;

        PushSkyUniforms(commandBuffer);

        commandBuffer.PushDebugGroup("Atmospheric sky");
        renderPass.BindPipeline(_atmosphericSkyPipeline.get());
        CountDraw(DrawCategory::Sky, 1);
        renderPass.Draw(3, 1, 0, 0);
        commandBuffer.PopDebugGroup();
    }

    bool NextRenderer::HasSkyDiscsOrAurora() const
    {
        const auto& effects = _frameEnvironment->SkyEffects;
        return (_atmosphericSkyUniforms != nullptr && (effects.IsSunMoonDiscDrawn || effects.IsAuroraDrawn));
    }

    // Legacy DrawSunMoonDisc and DrawAurora: additive fullscreen passes after the clouds, which read the cloud coverage
    // from the alpha channel of a copy of the sky.
    void NextRenderer::DrawSkyDiscsAndAurora(ICommandBuffer& commandBuffer)
    {
        if (!HasSkyDiscsOrAurora())
            return;

        const auto& effects = _frameEnvironment->SkyEffects;
        auto* skyCopy = _targetPool.Acquire(_sceneTarget->GetDesc());
        if (skyCopy == nullptr)
            return;

        commandBuffer.PushDebugGroup("Sun and moon discs and aurora");
        commandBuffer.BlitTexture(_sceneTarget.get(), skyCopy);
        PushSkyUniforms(commandBuffer);

        auto coverageBinding = TextureSamplerBinding{ skyCopy, _linearWrapSampler.get() };
        ITexture* auroraTarget = nullptr;
        if (effects.IsAuroraDrawn && effects.IsAuroraHalfResolution)
            auroraTarget = DrawHalfResolutionAurora(commandBuffer, coverageBinding);

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _sceneTarget.get();
        colorTarget.Load = LoadOp::Load;
        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);

        if (effects.IsSunMoonDiscDrawn)
            DrawFullscreen(*renderPass, _skyDiscPipeline.get(), coverageBinding);

        if (effects.IsAuroraDrawn)
        {
            if (auroraTarget != nullptr)
                DrawFullscreen(*renderPass, _auroraUpscalePipeline.get(), TextureSamplerBinding{ auroraTarget, _linearClampSampler.get() });
            else
                DrawFullscreen(*renderPass, _auroraPipeline.get(), coverageBinding);
        }

        commandBuffer.EndRenderPass();
        commandBuffer.PopDebugGroup();

        if (auroraTarget != nullptr)
            _targetPool.Release(auroraTarget);

        _targetPool.Release(skyCopy);
    }

    // Low sky quality: the aurora goes to a cleared half resolution target, upscaled additively by the caller.
    ITexture* NextRenderer::DrawHalfResolutionAurora(ICommandBuffer& commandBuffer, const TextureSamplerBinding& coverageBinding)
    {
        const auto& sceneDesc = _sceneTarget->GetDesc();

        auto desc = TextureDesc{};
        desc.Width = std::max(sceneDesc.Width / 2, 1);
        desc.Height = std::max(sceneDesc.Height / 2, 1);
        desc.Format = PixelFormat::RGBA16Float;
        desc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        desc.Name = "Sky.AuroraHalfResolution";
        auto* target = _targetPool.Acquire(desc);
        if (target == nullptr)
            return nullptr;

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = target;
        colorTarget.Load = LoadOp::Clear;
        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        DrawFullscreen(*renderPass, _auroraHalfPipeline.get(), coverageBinding);
        commandBuffer.EndRenderPass();
        return target;
    }

    void NextRenderer::DrawFullscreen(IRenderPass& renderPass, IGraphicsPipeline* pipeline, const TextureSamplerBinding& binding)
    {
        renderPass.BindPipeline(pipeline);
        renderPass.BindPixelSamplers(0, &binding, 1);
        CountDraw(DrawCategory::Sky, 1);
        renderPass.Draw(3, 1, 0, 0);
    }
}
