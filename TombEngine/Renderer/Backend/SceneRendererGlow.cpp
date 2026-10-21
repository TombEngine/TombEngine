// Glow and reflection source passes. "Glow" in FRAME_PASSES (only when an emissive material is visible) downscales the
// emissive target written by the depth and normals prepass to quarter resolution and blurs it into _glowTarget, which
// the post-process adds to the scene. "Reflections" in FRAME_PASSES (only with reflective materials) downscales the
// scene target into the half resolution _reflectionTarget, sampled by reflective materials on the next frame.
// The overlay reuses the same blur for its own emissive target.

#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/RHI/ShaderLibrary.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Backend
{
    constexpr auto GLOW_DOWNSCALE_FACTOR = 4;

    // Same layout as GlowUniforms in Shaders/Glow.hlsl.
    struct GlowUniforms
    {
        Vector2 TexelSize = Vector2::Zero;
        Vector2 Direction = Vector2::Zero;
    };

    // Downscale and blur write RGBA8 targets; the combine pipeline writes the swapchain format for the overlay.
    bool SceneRenderer::CreateGlowResources()
    {
        _glowVertexShader = LoadShader(*_device, _shaderDirectory, "Glow", "VS");
        _glowDownscaleShader = LoadShader(*_device, _shaderDirectory, "Glow", "PSDownscale");
        _glowBlurShader = LoadShader(*_device, _shaderDirectory, "Glow", "PSBlur");
        _glowCombineShader = LoadShader(*_device, _shaderDirectory, "Glow", "PSCombine");
        if (_glowVertexShader == nullptr || _glowDownscaleShader == nullptr || _glowBlurShader == nullptr || _glowCombineShader == nullptr)
            return false;

        _glowDownscalePipeline = CreateFullscreenPipeline(_glowVertexShader.get(), _glowDownscaleShader.get(), PixelFormat::RGBA8Unorm, "Glow.Downscale");
        _glowBlurPipeline = CreateFullscreenPipeline(_glowVertexShader.get(), _glowBlurShader.get(), PixelFormat::RGBA8Unorm, "Glow.Blur");
        _glowCombinePipeline = CreateFullscreenPipeline(_glowVertexShader.get(), _glowCombineShader.get(), _device->GetSwapchainFormat(), "Glow.Combine");
        return (_glowDownscalePipeline != nullptr && _glowBlurPipeline != nullptr && _glowCombinePipeline != nullptr);
    }

    // Quarter resolution of the given size; also used for the glow target of the overlay.
    TextureDesc SceneRenderer::GetGlowTargetDesc(int width, int height) const
    {
        auto desc = TextureDesc{};
        desc.Width = std::max(width / GLOW_DOWNSCALE_FACTOR, 1);
        desc.Height = std::max(height / GLOW_DOWNSCALE_FACTOR, 1);
        desc.Format = PixelFormat::RGBA8Unorm;
        desc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        desc.Name = "Glow";
        return desc;
    }

    // Creates the quarter resolution glow target and the half resolution reflection target for the scene resolution.
    bool SceneRenderer::EnsureGlowTargets(int width, int height)
    {
        auto desc = GetGlowTargetDesc(width, height);
        _glowTarget = _device->CreateTexture(desc);
        if (_glowTarget == nullptr)
            return false;

        desc.Width = std::max(width / 2, 1);
        desc.Height = std::max(height / 2, 1);
        desc.Name = "Reflections";
        _reflectionTarget = _device->CreateTexture(desc);
        return (_reflectionTarget != nullptr);
    }

    // 9 tap downscale filter, also used for the half resolution scene copy sampled by reflective materials.
    void SceneRenderer::DownscaleTexture(ICommandBuffer& commandBuffer, ITexture* source, ITexture* target, const char* name)
    {
        const auto& targetDesc = target->GetDesc();

        auto uniforms = GlowUniforms{};
        uniforms.TexelSize = Vector2(1.0f / targetDesc.Width, 1.0f / targetDesc.Height);
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &uniforms, sizeof(uniforms));

        auto binding = TextureSamplerBinding{ source, _linearClampSampler.get() };
        RenderFullscreenPass(commandBuffer, _glowDownscalePipeline.get(), target, &binding, 1, name);
    }

    // Emissive prepass output, downscaled and blurred into _glowTarget. Levels without emissive maps skip it.
    void SceneRenderer::RenderGlow(ICommandBuffer& commandBuffer)
    {
        BlurEmissive(commandBuffer, _emissiveTarget.get(), _glowTarget.get());
    }

    // Downscales the emissive texture into the glow target, then applies a separable blur (horizontal into a pooled
    // target, vertical back into the glow target). Three fullscreen passes at the glow resolution.
    void SceneRenderer::BlurEmissive(ICommandBuffer& commandBuffer, ITexture* emissive, ITexture* glow)
    {
        const auto& targetDesc = glow->GetDesc();

        auto uniforms = GlowUniforms{};
        uniforms.TexelSize = Vector2(1.0f / targetDesc.Width, 1.0f / targetDesc.Height);

        auto drawPass = [&](IGraphicsPipeline* pipeline, ITexture* target, ITexture* source, const Vector2& direction, const char* name)
        {
            uniforms.Direction = direction;
            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &uniforms, sizeof(uniforms));

            auto binding = TextureSamplerBinding{ source, _linearClampSampler.get() };
            RenderFullscreenPass(commandBuffer, pipeline, target, &binding, 1, name);
        };

        auto* blurTarget = _targetPool.Acquire(targetDesc);
        DownscaleTexture(commandBuffer, emissive, glow, "Downscale");
        drawPass(_glowBlurPipeline.get(), blurTarget, glow, Vector2::UnitX, "Blur horizontal");
        drawPass(_glowBlurPipeline.get(), glow, blurTarget, Vector2::UnitY, "Blur vertical");
        _targetPool.Release(blurTarget);
    }

    // Reflective materials sample a half resolution copy of the scene, read by the next frame.
    void SceneRenderer::CopyReflectionSource(ICommandBuffer& commandBuffer)
    {
        DownscaleTexture(commandBuffer, GetResolvedSceneTarget(), _reflectionTarget.get(), "Downscale");
    }
}
