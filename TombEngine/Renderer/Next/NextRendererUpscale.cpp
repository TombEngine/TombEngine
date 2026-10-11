#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/RHI/ShaderLibrary.h"

using namespace TEN::Renderer::RHI;

// AMD FidelityFX Super Resolution 1. Below native scale, the scene renders at a lower resolution, then EASU upscales
// the post-processed and antialiased scene to the window resolution and RCAS sharpens it. The 2D layer stays native.

namespace TEN::Renderer::Next
{
    // Sharpness of RCAS in stops below its maximum.
    constexpr auto RCAS_SHARPNESS = 0.2f;

    // Same layout as FsrUniforms in Shaders/RHI/FSR.hlsl.
    struct FsrUniforms
    {
        unsigned int EasuConstants[4][4] = {};
        unsigned int RcasConstants[4]    = {};
    };

    static unsigned int GetFloatBits(float value)
    {
        unsigned int bits = 0;
        memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    // Port of FsrEasuCon and FsrRcasCon of ffx_fsr1.h. The input image fills its texture.
    static FsrUniforms GetFsrUniforms(int inputWidth, int inputHeight, int outputWidth, int outputHeight)
    {
        float texelWidth = 1.0f / inputWidth;
        float texelHeight = 1.0f / inputHeight;
        float scaleX = (float)inputWidth / outputWidth;
        float scaleY = (float)inputHeight / outputHeight;

        auto uniforms = FsrUniforms{};
        auto& constants = uniforms.EasuConstants;
        constants[0][0] = GetFloatBits(scaleX);
        constants[0][1] = GetFloatBits(scaleY);
        constants[0][2] = GetFloatBits(0.5f * scaleX - 0.5f);
        constants[0][3] = GetFloatBits(0.5f * scaleY - 0.5f);
        constants[1][0] = GetFloatBits(texelWidth);
        constants[1][1] = GetFloatBits(texelHeight);
        constants[1][2] = GetFloatBits(texelWidth);
        constants[1][3] = GetFloatBits(-texelHeight);
        constants[2][0] = GetFloatBits(-texelWidth);
        constants[2][1] = GetFloatBits(2.0f * texelHeight);
        constants[2][2] = GetFloatBits(texelWidth);
        constants[2][3] = GetFloatBits(2.0f * texelHeight);
        constants[3][0] = GetFloatBits(0.0f);
        constants[3][1] = GetFloatBits(4.0f * texelHeight);

        uniforms.RcasConstants[0] = GetFloatBits(std::exp2(-RCAS_SHARPNESS));
        return uniforms;
    }

    bool NextRenderer::CreateUpscaleResources()
    {
        _easuPixelShader = LoadShader(*_device, _shaderDirectory, "FSR", "PSEasu");
        _rcasPixelShader = LoadShader(*_device, _shaderDirectory, "FSR", "PSRcas");
        if (_easuPixelShader == nullptr || _rcasPixelShader == nullptr)
            return false;

        _easuPipeline = CreateFullscreenPipeline(_postProcessVertexShader.get(), _easuPixelShader.get(), _device->GetSwapchainFormat(), "FSR.EASU");
        _rcasPipeline = CreateFullscreenPipeline(_postProcessVertexShader.get(), _rcasPixelShader.get(), _device->GetSwapchainFormat(), "FSR.RCAS");
        return (_easuPipeline != nullptr && _rcasPipeline != nullptr);
    }

    int NextRenderer::GetRenderSize(int windowSize) const
    {
        float scale = std::clamp(_renderScale, 0.25f, 1.0f);
        return std::max((int)std::round(windowSize * scale), 1);
    }

    void NextRenderer::RenderUpscale(ICommandBuffer& commandBuffer)
    {
        const auto& sourceDesc = _upscaleSource->GetDesc();
        const auto& targetDesc = _colorTarget->GetDesc();
        auto uniforms = GetFsrUniforms(sourceDesc.Width, sourceDesc.Height, targetDesc.Width, targetDesc.Height);
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &uniforms, sizeof(uniforms));

        auto upscaledDesc = TextureDesc{};
        upscaledDesc.Width = targetDesc.Width;
        upscaledDesc.Height = targetDesc.Height;
        upscaledDesc.Format = targetDesc.Format;
        upscaledDesc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        upscaledDesc.Name = "FSR.Upscaled";
        auto* upscaledTarget = _targetPool.Acquire(upscaledDesc);

        // EASU gathers the source with bilinear coordinates; RCAS loads texels.
        auto easuBinding = TextureSamplerBinding{ _upscaleSource.get(), _linearClampSampler.get() };
        auto rcasBinding = TextureSamplerBinding{ upscaledTarget, _pointClampSampler.get() };
        RenderFullscreenPass(commandBuffer, _easuPipeline.get(), upscaledTarget, &easuBinding, 1, "EASU");
        RenderFullscreenPass(commandBuffer, _rcasPipeline.get(), _colorTarget.get(), &rcasBinding, 1, "RCAS");
        _targetPool.Release(upscaledTarget);
    }
    // Overlay objects and the menu post-process draw at window resolution. At native scale they share the scene
    // targets; below it, they use pooled targets of the window size, which the pool keeps across frames.
    void NextRenderer::AcquireOverlayTargets()
    {
        _overlayTargets = OverlayTargets{ _emissiveTarget.get(), _depthTarget.get(), _glowTarget.get(), _antialiasingSource.get() };
        if (!_isUpscaleActive)
            return;

        const auto& frameDesc = _finalTarget->GetDesc();
        auto getWindowDesc = [&](const ITexture* sceneTexture)
        {
            auto desc = sceneTexture->GetDesc();
            desc.Width = frameDesc.Width;
            desc.Height = frameDesc.Height;
            return desc;
        };

        _overlayTargets.Emissive = _targetPool.Acquire(getWindowDesc(_emissiveTarget.get()));
        _overlayTargets.Depth = _targetPool.Acquire(getWindowDesc(_depthTarget.get()));
        _overlayTargets.Glow = _targetPool.Acquire(GetGlowTargetDesc(frameDesc.Width, frameDesc.Height));
        _overlayTargets.AntialiasingSource = _targetPool.Acquire(getWindowDesc(_antialiasingSource.get()));
    }

    void NextRenderer::ReleaseOverlayTargets()
    {
        if (_isUpscaleActive)
        {
            _targetPool.Release(_overlayTargets.Emissive);
            _targetPool.Release(_overlayTargets.Depth);
            _targetPool.Release(_overlayTargets.Glow);
            _targetPool.Release(_overlayTargets.AntialiasingSource);
        }

        _overlayTargets = {};
    }
}
