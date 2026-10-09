#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/RHI/ShaderLibrary.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Next
{
    constexpr auto GLOW_DOWNSCALE_FACTOR = 4;

    // Same layout as GlowUniforms in Shaders/RHI/Glow.hlsl.
    struct GlowUniforms
    {
        Vector2 TexelSize = Vector2::Zero;
        Vector2 Direction = Vector2::Zero;
    };

    bool NextRenderer::CreateGlowResources()
    {
        _glowVertexShader = LoadShader(*_device, _shaderDirectory, "Glow", "VS");
        _glowDownscaleShader = LoadShader(*_device, _shaderDirectory, "Glow", "PSDownscale");
        _glowBlurShader = LoadShader(*_device, _shaderDirectory, "Glow", "PSBlur");
        if (_glowVertexShader == nullptr || _glowDownscaleShader == nullptr || _glowBlurShader == nullptr)
            return false;

        _glowDownscalePipeline = CreateFullscreenPipeline(_glowVertexShader.get(), _glowDownscaleShader.get(), PixelFormat::RGBA8Unorm, "Glow.Downscale");
        _glowBlurPipeline = CreateFullscreenPipeline(_glowVertexShader.get(), _glowBlurShader.get(), PixelFormat::RGBA8Unorm, "Glow.Blur");
        return (_glowDownscalePipeline != nullptr && _glowBlurPipeline != nullptr);
    }

    bool NextRenderer::EnsureGlowTargets(int width, int height)
    {
        auto desc = TextureDesc{};
        desc.Width = std::max(width / GLOW_DOWNSCALE_FACTOR, 1);
        desc.Height = std::max(height / GLOW_DOWNSCALE_FACTOR, 1);
        desc.Format = PixelFormat::RGBA8Unorm;
        desc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;

        for (int i = 0; i < 2; i++)
        {
            desc.Name = "Glow[" + std::to_string(i) + "]";
            _glowTargets[i] = _device->CreateTexture(desc);
            if (_glowTargets[i] == nullptr)
                return false;
        }

        desc.Width = std::max(width / 2, 1);
        desc.Height = std::max(height / 2, 1);
        desc.Name = "Reflections";
        _reflectionTarget = _device->CreateTexture(desc);
        return (_reflectionTarget != nullptr);
    }

    // Legacy 9 tap downscale filter, also used for the half resolution scene copy sampled by reflective materials.
    void NextRenderer::DownscaleTexture(ICommandBuffer& commandBuffer, ITexture* source, ITexture* target, const char* name)
    {
        const auto& targetDesc = target->GetDesc();

        auto uniforms = GlowUniforms{};
        uniforms.TexelSize = Vector2(1.0f / targetDesc.Width, 1.0f / targetDesc.Height);
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &uniforms, sizeof(uniforms));

        auto binding = TextureSamplerBinding{ source, _linearClampSampler.get() };
        RenderFullscreenPass(commandBuffer, _glowDownscalePipeline.get(), target, &binding, 1, name);
    }

    // Emissive prepass output, downscaled and blurred into _glowTargets[0]. Levels without emissive maps skip it.
    void NextRenderer::RenderGlow(ICommandBuffer& commandBuffer)
    {
        const auto& targetDesc = _glowTargets[0]->GetDesc();

        auto uniforms = GlowUniforms{};
        uniforms.TexelSize = Vector2(1.0f / targetDesc.Width, 1.0f / targetDesc.Height);

        auto drawPass = [&](IGraphicsPipeline* pipeline, ITexture* target, ITexture* source, const Vector2& direction, const char* name)
        {
            uniforms.Direction = direction;
            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &uniforms, sizeof(uniforms));

            auto binding = TextureSamplerBinding{ source, _linearClampSampler.get() };
            RenderFullscreenPass(commandBuffer, pipeline, target, &binding, 1, name);
        };

        commandBuffer.PushDebugGroup("Glow");
        DownscaleTexture(commandBuffer, _emissiveTarget.get(), _glowTargets[0].get(), "Downscale");
        drawPass(_glowBlurPipeline.get(), _glowTargets[1].get(), _glowTargets[0].get(), Vector2::UnitX, "Blur horizontal");
        drawPass(_glowBlurPipeline.get(), _glowTargets[0].get(), _glowTargets[1].get(), Vector2::UnitY, "Blur vertical");
        commandBuffer.PopDebugGroup();
    }
}
