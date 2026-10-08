#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/Next/GpuTypes.h"
#include "Renderer/RenderView.h"
#include "Renderer/RHI/ShaderLibrary.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Next
{
    constexpr auto SSAO_RADIUS   = 64.0f;
    constexpr auto SSAO_BIAS     = 4.0f;
    constexpr auto SSAO_EXPONENT = 2.0f;

    // Same layout as SsaoUniforms in Shaders/RHI/SSAO.hlsl.
    struct SsaoUniforms
    {
        Matrix  Projection        = Matrix::Identity;
        Matrix  InverseProjection = Matrix::Identity;
        Vector2 TargetSize        = Vector2::Zero;
        Vector2 TexelSize         = Vector2::Zero;
        float   Radius            = SSAO_RADIUS;
        float   Bias              = SSAO_BIAS;
        float   Padding[2]        = {};
        Vector4 Kernel[SSAO_KERNEL_SIZE];
    };

    float NextRenderer::GetAmbientOcclusionExponent() const
    {
        return _isAmbientOcclusionActive ? SSAO_EXPONENT : 0.0f;
    }

    // Kernel and rotation noise are generated as in the legacy CreateSSAONoiseTexture, with the same seed.
    bool NextRenderer::CreateSSAOResources()
    {
        _ssaoVertexShader = LoadShader(*_device, _shaderDirectory, "SSAO", "VS");
        _ssaoPixelShader = LoadShader(*_device, _shaderDirectory, "SSAO", "PS");
        _ssaoBlurHorizontalShader = LoadShader(*_device, _shaderDirectory, "SSAO", "PSBlurHorizontal");
        _ssaoBlurVerticalShader = LoadShader(*_device, _shaderDirectory, "SSAO", "PSBlurVertical");
        _roomsPrepassPixelShader = LoadShader(*_device, _shaderDirectory, "Rooms", "PSDepthNormal");
        _objectsPrepassPixelShader = LoadShader(*_device, _shaderDirectory, "Objects", "PSDepthNormal");
        if (_ssaoVertexShader == nullptr || _ssaoPixelShader == nullptr || _ssaoBlurHorizontalShader == nullptr ||
            _ssaoBlurVerticalShader == nullptr || _roomsPrepassPixelShader == nullptr || _objectsPrepassPixelShader == nullptr)
        {
            return false;
        }

        auto createFullscreenPipeline = [&](IShader* pixelShader, const char* name)
        {
            auto desc = GraphicsPipelineDesc{};
            desc.VertexShader = _ssaoVertexShader.get();
            desc.PixelShader = pixelShader;
            desc.Rasterizer.Cull = RHI::CullMode::None;
            desc.DepthStencil.EnableDepthTest = false;
            desc.DepthStencil.EnableDepthWrite = false;
            desc.NumColorTargets = 1;
            desc.ColorTargets[0].Format = PixelFormat::R8Unorm;
            desc.Name = name;
            return _device->CreateGraphicsPipeline(desc);
        };

        _ssaoPipeline = createFullscreenPipeline(_ssaoPixelShader.get(), "SSAO");
        _ssaoBlurHorizontalPipeline = createFullscreenPipeline(_ssaoBlurHorizontalShader.get(), "SSAO.BlurHorizontal");
        _ssaoBlurVerticalPipeline = createFullscreenPipeline(_ssaoBlurVerticalShader.get(), "SSAO.BlurVertical");
        if (_ssaoPipeline == nullptr || _ssaoBlurHorizontalPipeline == nullptr || _ssaoBlurVerticalPipeline == nullptr)
            return false;

        auto generator = std::default_random_engine{};
        auto randomFloats = std::uniform_real_distribution<float>(0.0f, 1.0f);
        for (int i = 0; i < SSAO_KERNEL_SIZE; i++)
        {
            auto sample = Vector4(randomFloats(generator) * 2.0f - 1.0f, randomFloats(generator) * 2.0f - 1.0f, randomFloats(generator), 1.0f);
            sample.Normalize();
            sample *= randomFloats(generator);

            float scale = (float)i / (float)SSAO_KERNEL_SIZE;
            sample *= 0.1f + 0.9f * scale * scale;
            sample.w = 1.0f;
            _ssaoKernel[i] = sample;
        }

        Vector4 noise[16];
        for (auto& rotation : noise)
            rotation = Vector4(randomFloats(generator) * 2.0f - 1.0f, randomFloats(generator) * 2.0f - 1.0f, 0.0f, 1.0f);

        auto noiseDesc = TextureDesc{};
        noiseDesc.Width = 4;
        noiseDesc.Height = 4;
        noiseDesc.Format = PixelFormat::RGBA32Float;
        noiseDesc.Name = "SSAO.Noise";
        _ssaoNoiseTexture = _device->CreateTexture(noiseDesc);
        if (_ssaoNoiseTexture == nullptr)
            return false;

        UploadTexture(_ssaoNoiseTexture.get(), noise, (int)sizeof(noise));

        auto samplerDesc = SamplerDesc{};
        samplerDesc.MinFilter = samplerDesc.MagFilter = samplerDesc.MipFilter = Filter::Nearest;
        samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = AddressMode::ClampToEdge;
        samplerDesc.Name = "PointClamp";
        _pointClampSampler = _device->CreateSampler(samplerDesc);

        samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = AddressMode::Repeat;
        samplerDesc.Name = "PointWrap";
        _pointWrapSampler = _device->CreateSampler(samplerDesc);

        return (_pointClampSampler != nullptr && _pointWrapSampler != nullptr);
    }

    bool NextRenderer::EnsureSSAOTargets(int width, int height)
    {
        auto normalDesc = TextureDesc{};
        normalDesc.Width = width;
        normalDesc.Height = height;
        normalDesc.Format = PixelFormat::RGBA8Unorm;
        normalDesc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        normalDesc.Name = "Prepass.Normals";
        _normalTarget = _device->CreateTexture(normalDesc);

        // Occlusion is low frequency: half resolution, upsampled by bilinear filtering in the scene pass.
        auto ssaoDesc = TextureDesc{};
        ssaoDesc.Width = std::max(width / 2, 1);
        ssaoDesc.Height = std::max(height / 2, 1);
        ssaoDesc.Format = PixelFormat::R8Unorm;
        ssaoDesc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        ssaoDesc.Name = "SSAO.Occlusion";
        _ssaoTarget = _device->CreateTexture(ssaoDesc);

        ssaoDesc.Name = "SSAO.Blur";
        _ssaoBlurTarget = _device->CreateTexture(ssaoDesc);

        return (_normalTarget != nullptr && _ssaoTarget != nullptr && _ssaoBlurTarget != nullptr);
    }

    // Opaque geometry writes depth and view space normals; the scene pass reuses the depth.
    void NextRenderer::RenderDepthPrepass(ICommandBuffer& commandBuffer, const RenderView& view)
    {
        auto normalTarget = ColorTargetInfo{};
        normalTarget.Texture = _normalTarget.get();

        auto depthTarget = DepthTargetInfo{};
        depthTarget.Texture = _depthTarget.get();

        commandBuffer.PushDebugGroup("Depth and normals prepass");
        auto* renderPass = commandBuffer.BeginRenderPass(&normalTarget, 1, &depthTarget);
        DrawRooms(commandBuffer, *renderPass, view, ScenePass::Prepass);
        DrawObjects(commandBuffer, *renderPass, ScenePass::Prepass);
        commandBuffer.EndRenderPass();
        commandBuffer.PopDebugGroup();
    }

    void NextRenderer::RenderFullscreenPass(ICommandBuffer& commandBuffer, IGraphicsPipeline* pipeline, ITexture* target, ITexture* source, ISampler* sourceSampler, const char* name)
    {
        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = target;
        colorTarget.Load = LoadOp::DontCare;

        commandBuffer.PushDebugGroup(name);
        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        renderPass->BindPipeline(pipeline);

        TextureSamplerBinding bindings[3] =
        {
            { source, sourceSampler },
            { _normalTarget.get(), _pointClampSampler.get() },
            { _ssaoNoiseTexture.get(), _pointWrapSampler.get() }
        };
        renderPass->BindPixelSamplers(0, bindings, 3);

        _frameStats.DrawCalls++;
        renderPass->Draw(3, 1, 0, 0);
        commandBuffer.EndRenderPass();
        commandBuffer.PopDebugGroup();
    }

    // Occlusion at half resolution, then a separable bilateral blur. The result ends in _ssaoTarget.
    void NextRenderer::RenderSSAO(ICommandBuffer& commandBuffer, const Matrix& projection)
    {
        const auto& targetDesc = _ssaoTarget->GetDesc();

        auto uniforms = SsaoUniforms{};
        uniforms.Projection = projection;
        uniforms.InverseProjection = projection.Invert();
        uniforms.TargetSize = Vector2((float)targetDesc.Width, (float)targetDesc.Height);
        uniforms.TexelSize = Vector2(1.0f / targetDesc.Width, 1.0f / targetDesc.Height);
        memcpy(uniforms.Kernel, _ssaoKernel, sizeof(_ssaoKernel));
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &uniforms, sizeof(uniforms));

        commandBuffer.PushDebugGroup("SSAO");
        RenderFullscreenPass(commandBuffer, _ssaoPipeline.get(), _ssaoTarget.get(), _depthTarget.get(), _pointClampSampler.get(), "Occlusion");
        RenderFullscreenPass(commandBuffer, _ssaoBlurHorizontalPipeline.get(), _ssaoBlurTarget.get(), _ssaoTarget.get(), _pointClampSampler.get(), "Blur horizontal");
        RenderFullscreenPass(commandBuffer, _ssaoBlurVerticalPipeline.get(), _ssaoTarget.get(), _ssaoBlurTarget.get(), _pointClampSampler.get(), "Blur vertical");
        commandBuffer.PopDebugGroup();
    }
}
