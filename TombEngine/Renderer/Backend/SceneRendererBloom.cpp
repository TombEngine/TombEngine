// HDR mode and bloom. With HDR, the scene target is half float: the scene shaders clamp their colors to
// HDR_SCENE_COLOR_MAX instead of 1, the TAA resolve keeps a half float history, and "Bloom" in FRAME_PASSES, before the
// post-process, downsamples the bright part of the scene into BLOOM_STEP_COUNT pooled steps and adds each step back onto
// the next larger one (Shaders/Bloom.hlsl). The post-process adds the largest step and tonemaps the scene. Without HDR,
// the scene target, its pipelines and the shader results are those of the 8 bit renderer.

#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/RHI/ShaderLibrary.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Backend
{
    constexpr auto BLOOM_KNEE = 0.5f; // Soft transition below the threshold, relative to it.

    // Same layout as BloomUniforms in Shaders/Bloom.hlsl.
    struct BloomUniforms
    {
        Vector2 TexelSize = Vector2::Zero;
        float   Threshold = 1.0f;
        float   Knee      = 0.5f;
    };

    // Bloom steps are blurred, so their format favors bandwidth: R11G11B10 float where it can be a color target.
    bool SceneRenderer::CreateBloomResources()
    {
        _bloomVertexShader = LoadShader(*_device, _shaderDirectory, "Bloom", "VS");
        _bloomPrefilterShader = LoadShader(*_device, _shaderDirectory, "Bloom", "PSPrefilter");
        _bloomDownsampleShader = LoadShader(*_device, _shaderDirectory, "Bloom", "PSDownsample");
        _bloomUpsampleShader = LoadShader(*_device, _shaderDirectory, "Bloom", "PSUpsample");
        if (_bloomVertexShader == nullptr || _bloomPrefilterShader == nullptr || _bloomDownsampleShader == nullptr || _bloomUpsampleShader == nullptr)
            return false;

        unsigned int targetUsage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        _bloomFormat = _device->SupportsTextureFormat(PixelFormat::R11G11B10Float, TextureType::Texture2D, targetUsage) ?
            PixelFormat::R11G11B10Float : PixelFormat::RGBA16Float;
        _isHdrSupported = _device->SupportsTextureFormat(PixelFormat::RGBA16Float, TextureType::Texture2D, targetUsage | TextureUsage::ComputeStorageWrite);

        _bloomPrefilterPipeline = CreateFullscreenPipeline(_bloomVertexShader.get(), _bloomPrefilterShader.get(), _bloomFormat, "Bloom.Prefilter");
        _bloomDownsamplePipeline = CreateFullscreenPipeline(_bloomVertexShader.get(), _bloomDownsampleShader.get(), _bloomFormat, "Bloom.Downsample");

        auto upsampleDesc = GetFullscreenPipelineDesc(_bloomVertexShader.get(), _bloomUpsampleShader.get(), _bloomFormat, "Bloom.Upsample");
        auto& blend = upsampleDesc.ColorTargets[0].Blend;
        blend.EnableBlend = true;
        blend.SrcColor = blend.SrcAlpha = BlendFactor::One;
        blend.DstColor = blend.DstAlpha = BlendFactor::One;
        _bloomUpsamplePipeline = _device->CreateGraphicsPipeline(upsampleDesc);

        return (_bloomPrefilterPipeline != nullptr && _bloomDownsamplePipeline != nullptr && _bloomUpsamplePipeline != nullptr);
    }

    PixelFormat SceneRenderer::GetSceneColorFormat() const
    {
        return _isHdrActive ? PixelFormat::RGBA16Float : _device->GetSwapchainFormat();
    }

    // Creates a pipeline whose first color target is the scene target, in the scene color format, and keeps its
    // description for ApplyHdrMode.
    void SceneRenderer::CreateSceneColorPipeline(std::unique_ptr<IGraphicsPipeline>& pipeline, const GraphicsPipelineDesc& desc)
    {
        auto sceneDesc = desc;
        sceneDesc.ColorTargets[0].Format = GetSceneColorFormat();
        pipeline = _device->CreateGraphicsPipeline(sceneDesc);
        _sceneColorPipelines.push_back(SceneColorPipeline{ &pipeline, desc });
    }

    // A change of mode creates the scene color pipelines again and empties the scene pipeline caches; EnsureRenderTargets
    // then creates the scene targets in the new format. Devices without half float targets stay in the 8 bit mode.
    void SceneRenderer::ApplyHdrMode(bool isEnabled)
    {
        bool isActive = isEnabled && _isHdrSupported;
        if (isActive == _isHdrActive)
            return;

        _device->WaitIdle();
        _isHdrActive = isActive;
        for (auto& entry : _sceneColorPipelines)
        {
            auto desc = entry.Desc;
            desc.ColorTargets[0].Format = GetSceneColorFormat();
            *entry.Pipeline = _device->CreateGraphicsPipeline(desc);
            if (*entry.Pipeline == nullptr)
                TENLog("Scene renderer: cannot create pipeline " + desc.Name, LogLevel::Error);
        }

        ClearPipelineCaches();
        TENLog(_isHdrActive ? "Scene renderer: HDR enabled." : "Scene renderer: HDR disabled.", LogLevel::Info);
    }

    // Step 0 has half the scene resolution, each next step half the previous one.
    TextureDesc SceneRenderer::GetBloomStepDesc(int step) const
    {
        const auto& sceneDesc = _sceneTarget->GetDesc();
        auto desc = TextureDesc{};
        desc.Width = std::max(sceneDesc.Width >> (step + 1), 1);
        desc.Height = std::max(sceneDesc.Height >> (step + 1), 1);
        desc.Format = _bloomFormat;
        desc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        desc.Name = "Bloom";
        return desc;
    }

    // Downsamples the resolved scene through the steps, the first one keeping only the colors above the threshold, then
    // adds each step onto the next larger one. Step 0 stays acquired for the post-process, which releases it.
    void SceneRenderer::RenderBloom(ICommandBuffer& commandBuffer)
    {
        const auto& environment = *_frameEnvironment;

        ITexture* steps[BLOOM_STEP_COUNT] = {};
        for (int i = 0; i < BLOOM_STEP_COUNT; i++)
            steps[i] = _targetPool.Acquire(GetBloomStepDesc(i));

        auto uniforms = BloomUniforms{};
        uniforms.Threshold = environment.BloomThreshold;
        uniforms.Knee = std::max(environment.BloomThreshold * BLOOM_KNEE, EPSILON);

        auto drawStep = [&](IGraphicsPipeline* pipeline, ITexture* source, ITexture* target, LoadOp load, const char* name)
        {
            const auto& sourceDesc = source->GetDesc();
            uniforms.TexelSize = Vector2(1.0f / sourceDesc.Width, 1.0f / sourceDesc.Height);
            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &uniforms, sizeof(uniforms));

            auto binding = TextureSamplerBinding{ source, _linearClampSampler.get() };
            RenderFullscreenPass(commandBuffer, pipeline, target, &binding, 1, name, load);
        };

        drawStep(_bloomPrefilterPipeline.get(), GetResolvedSceneTarget(), steps[0], LoadOp::DontCare, "Prefilter");
        for (int i = 1; i < BLOOM_STEP_COUNT; i++)
            drawStep(_bloomDownsamplePipeline.get(), steps[i - 1], steps[i], LoadOp::DontCare, "Downsample");

        for (int i = BLOOM_STEP_COUNT - 1; i > 0; i--)
            drawStep(_bloomUpsamplePipeline.get(), steps[i], steps[i - 1], LoadOp::Load, "Upsample");

        for (int i = 1; i < BLOOM_STEP_COUNT; i++)
            _targetPool.Release(steps[i]);

        _bloomTarget = steps[0];
    }
}
