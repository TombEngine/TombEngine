// Depth and normals prepass and SSAO. "Depth and normals prepass" in FRAME_PASSES always runs after the shadow map: opaque
// rooms and objects write the scene depth, view space normals, emissive color and a sampled R32 depth copy, all at scene
// resolution. "SSAO" (when enabled) reads the depth copy and normals and writes a blurred half resolution occlusion
// target sampled by the scene pass. Also hosts the shared RenderFullscreenPass helpers.

#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/Backend/GpuTypes.h"
#include "Renderer/Scene/RenderView.h"
#include "Renderer/RHI/ShaderLibrary.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Backend
{
    constexpr auto SSAO_RADIUS   = 64.0f;
    constexpr auto SSAO_BIAS     = 4.0f;
    constexpr auto SSAO_EXPONENT = 2.0f;

    // Same layout as SsaoUniforms in Shaders/SSAO.hlsl.
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

    // Exponent applied to the occlusion by the scene shaders; zero disables it when SSAO is off.
    float SceneRenderer::GetAmbientOcclusionExponent() const
    {
        return _isAmbientOcclusionActive ? SSAO_EXPONENT : 0.0f;
    }

    // SSAO and blur pipelines, the prepass pixel shaders of rooms and objects, and the point samplers.
    // Kernel and rotation noise are generated from a fixed seed.
    bool SceneRenderer::CreateSSAOResources()
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

        _ssaoPipeline = CreateFullscreenPipeline(_ssaoVertexShader.get(), _ssaoPixelShader.get(), PixelFormat::R8Unorm, "SSAO");
        _ssaoBlurHorizontalPipeline = CreateFullscreenPipeline(_ssaoVertexShader.get(), _ssaoBlurHorizontalShader.get(), PixelFormat::R8Unorm, "SSAO.BlurHorizontal");
        _ssaoBlurVerticalPipeline = CreateFullscreenPipeline(_ssaoVertexShader.get(), _ssaoBlurVerticalShader.get(), PixelFormat::R8Unorm, "SSAO.BlurVertical");
        if (_ssaoPipeline == nullptr || _ssaoBlurHorizontalPipeline == nullptr || _ssaoBlurVerticalPipeline == nullptr)
            return false;

        // Hemisphere kernel, with samples concentrated near the origin.
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

        // 4x4 tiled rotation noise.
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

    // Prepass normal and emissive targets at scene resolution and the half resolution occlusion target.
    bool SceneRenderer::EnsureSSAOTargets(int width, int height)
    {
        auto normalDesc = TextureDesc{};
        normalDesc.Width = width;
        normalDesc.Height = height;
        normalDesc.Format = PixelFormat::RGBA8Unorm;
        normalDesc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        normalDesc.Name = "Prepass.Normals";
        _normalTarget = _device->CreateTexture(normalDesc);

        normalDesc.Name = "Prepass.Emissive";
        _emissiveTarget = _device->CreateTexture(normalDesc);

        // Occlusion is low frequency: half resolution, upsampled by bilinear filtering in the scene pass.
        auto ssaoDesc = TextureDesc{};
        ssaoDesc.Width = std::max(width / 2, 1);
        ssaoDesc.Height = std::max(height / 2, 1);
        ssaoDesc.Format = PixelFormat::R8Unorm;
        ssaoDesc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        ssaoDesc.Name = "SSAO.Occlusion";
        _ssaoTarget = _device->CreateTexture(ssaoDesc);

        return (_normalTarget != nullptr && _emissiveTarget != nullptr && _ssaoTarget != nullptr);
    }

    // Opaque geometry writes depth, view space normals, emissive and a sampled copy of the depth; the scene pass reuses
    // the depth.
    void SceneRenderer::RenderDepthPrepass(ICommandBuffer& commandBuffer)
    {
        const auto& view = *_frameView;

        ColorTargetInfo colorTargets[3] = {};
        colorTargets[0].Texture = _normalTarget.get();
        colorTargets[1].Texture = _emissiveTarget.get();
        colorTargets[2].Texture = _sceneDepthCopy.get();
        colorTargets[2].ClearColor[0] = 1.0f; // Far plane, as the cleared depth.

        auto depthTarget = DepthTargetInfo{};
        depthTarget.Texture = _depthTarget.get();

        auto* renderPass = commandBuffer.BeginRenderPass(colorTargets, 3, &depthTarget);
        DrawRooms(commandBuffer, *renderPass, ScenePass::Prepass);
        DrawObjects(commandBuffer, *renderPass, ScenePass::Prepass);
        commandBuffer.EndRenderPass();
    }

    // SSAO variant: binds the source, the prepass normals and the rotation noise in slots 0 to 2.
    void SceneRenderer::RenderFullscreenPass(ICommandBuffer& commandBuffer, IGraphicsPipeline* pipeline, ITexture* target, ITexture* source, ISampler* sourceSampler, const char* name)
    {
        TextureSamplerBinding bindings[3] =
        {
            { source, sourceSampler },
            { _normalTarget.get(), _pointClampSampler.get() },
            { _ssaoNoiseTexture.get(), _pointWrapSampler.get() }
        };
        RenderFullscreenPass(commandBuffer, pipeline, target, bindings, 3, name);
    }

    // Draws a fullscreen triangle in its own render pass on one color target, with an optional depth or stencil target.
    void SceneRenderer::RenderFullscreenPass(ICommandBuffer& commandBuffer, IGraphicsPipeline* pipeline, ITexture* target, const TextureSamplerBinding* bindings, int bindingCount,
                                            const char* name, LoadOp load, const DepthTargetInfo* depthTarget)
    {
        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = target;
        colorTarget.Load = load;

        commandBuffer.PushDebugGroup(name);
        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, depthTarget);
        renderPass->BindPipeline(pipeline);
        renderPass->BindPixelSamplers(0, bindings, bindingCount);

        CountDraw(DrawCategory::Fullscreen, 1);
        renderPass->Draw(3, 1, 0, 0);
        commandBuffer.EndRenderPass();
        commandBuffer.PopDebugGroup();
    }

    // Occlusion at half resolution, then a separable bilateral blur. The result ends in _ssaoTarget.
    void SceneRenderer::RenderSSAO(ICommandBuffer& commandBuffer)
    {
        const auto& projection = _projection;
        const auto& targetDesc = _ssaoTarget->GetDesc();

        auto uniforms = SsaoUniforms{};
        uniforms.Projection = projection;
        uniforms.InverseProjection = projection.Invert();
        uniforms.TargetSize = Vector2((float)targetDesc.Width, (float)targetDesc.Height);
        uniforms.TexelSize = Vector2(1.0f / targetDesc.Width, 1.0f / targetDesc.Height);
        memcpy(uniforms.Kernel, _ssaoKernel, sizeof(_ssaoKernel));
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &uniforms, sizeof(uniforms));

        auto* blurTarget = _targetPool.Acquire(targetDesc);
        RenderFullscreenPass(commandBuffer, _ssaoPipeline.get(), _ssaoTarget.get(), _sceneDepthCopy.get(), _pointClampSampler.get(), "Occlusion");
        RenderFullscreenPass(commandBuffer, _ssaoBlurHorizontalPipeline.get(), blurTarget, _ssaoTarget.get(), _pointClampSampler.get(), "Blur horizontal");
        RenderFullscreenPass(commandBuffer, _ssaoBlurVerticalPipeline.get(), _ssaoTarget.get(), blurTarget, _pointClampSampler.get(), "Blur vertical");
        _targetPool.Release(blurTarget);
    }
}
