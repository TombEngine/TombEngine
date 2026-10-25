#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/RHI/ShaderLibrary.h"

using namespace TEN::Renderer::RHI;

// AMD FidelityFX Super Resolution 1. Below native scale, the scene renders at a lower resolution, then EASU upscales
// the post-processed and antialiased scene to the window resolution and RCAS sharpens it. The 2D layer stays native.
// EASU is the last entry of FRAME_PASSES ("Upscale"), from the upscale source into the window sized color target;
// RCAS runs later, when the overlay composition (FinishFrame) draws the color target.

namespace TEN::Renderer::Backend
{
    // Sharpness of RCAS in stops below its maximum.
    constexpr auto RCAS_SHARPNESS = 0.2f;

    // Same layout as FsrUniforms in Shaders/FSR.hlsl.
    struct FsrUniforms
    {
        unsigned int EasuConstants[4][4] = {};
        unsigned int RcasConstants[4]    = {};
    };

    // FSR constants are passed as raw float bits in unsigned integers.
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

    static FsrUniforms GetFsrUniforms(const ITexture& source, const ITexture& target)
    {
        const auto& sourceDesc = source.GetDesc();
        const auto& targetDesc = target.GetDesc();
        return GetFsrUniforms(sourceDesc.Width, sourceDesc.Height, targetDesc.Width, targetDesc.Height);
    }

    // Reuses the post-process vertex shader, so it must run after CreatePostProcessResources.
    bool SceneRenderer::CreateUpscaleResources()
    {
        _easuPixelShader = LoadShader(*_device, _shaderDirectory, "FSR", "PSEasu");
        _rcasPixelShader = LoadShader(*_device, _shaderDirectory, "FSR", "PSRcas");
        if (_easuPixelShader == nullptr || _rcasPixelShader == nullptr)
            return false;

        // The RCAS pipeline is an overlay pipeline, created with the overlay resources.
        _easuPipeline = CreateFullscreenPipeline(_postProcessVertexShader.get(), _easuPixelShader.get(), _device->GetSwapchainFormat(), "FSR.EASU");
        return (_easuPipeline != nullptr);
    }

    // Scene resolution along one axis for the frontend render scale, clamped to 25-100% of the window size.
    int SceneRenderer::GetRenderSize(int windowSize) const
    {
        float scale = std::clamp(_renderScale, 0.25f, 1.0f);
        return std::max((int)std::round(windowSize * scale), 1);
    }

    // EASU pass from the scene resolution upscale source to the window resolution color target.
    void SceneRenderer::RenderUpscale(ICommandBuffer& commandBuffer)
    {
        auto uniforms = GetFsrUniforms(*_upscaleSource, *_colorTarget);
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &uniforms, sizeof(uniforms));

        // EASU gathers the source with bilinear coordinates. RCAS runs when the overlay draws the result.
        auto easuBinding = TextureSamplerBinding{ _upscaleSource, _linearClampSampler.get() };
        RenderFullscreenPass(commandBuffer, _easuPipeline.get(), _colorTarget.get(), &easuBinding, 1, "EASU");
        _targetPool.Release(_upscaleSource);
        _upscaleSource = nullptr;
        _isColorTargetSharpened = true;
    }

    // The FSR shader declares slots 0 to 2, and the overlay command buffer may hold no data in the lower ones yet;
    // the overlay shaders push their own data into slot 0 before using it.
    void SceneRenderer::PushRcasUniforms(ICommandBuffer& commandBuffer)
    {
        // At native scale RCAS sharpens the temporal antialiasing; its constants do not depend on the source size.
        auto uniforms = GetFsrUniforms(*_colorTarget, *_colorTarget);
        for (int slot = 0; slot <= 2; slot++)
            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, slot, &uniforms, sizeof(uniforms));
    }

    // Overlay objects and the menu post-process draw at window resolution. At native scale they share the scene
    // targets; below it, they use pooled targets of the window size, which the pool keeps across frames. The menu
    // post-process acquires its antialiasing source itself.
    void SceneRenderer::AcquireOverlayTargets()
    {
        _overlayTargets = OverlayTargets{ _emissiveTarget.get(), _depthTarget.get(), _glowTarget.get(), nullptr };
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
    }

    // Returns the window sized overlay targets to the pool; at native scale they were the scene targets themselves.
    void SceneRenderer::ReleaseOverlayTargets()
    {
        if (_isUpscaleActive)
        {
            _targetPool.Release(_overlayTargets.Emissive);
            _targetPool.Release(_overlayTargets.Depth);
            _targetPool.Release(_overlayTargets.Glow);
        }

        if (_overlayTargets.AntialiasingSource != nullptr)
            _targetPool.Release(_overlayTargets.AntialiasingSource);

        _overlayTargets = {};
    }
}
