// Depth and normals prepass and ambient occlusion. "Depth and normals prepass" in FRAME_PASSES always runs after the
// shadow map: opaque rooms and objects write the scene depth, view space normals, emissive color, a sampled R32 depth
// copy and, while a temporal pass is active, motion vectors, all at scene resolution. "GTAO" (when enabled) computes
// ground truth ambient occlusion at half resolution from the depth copy and normals, accumulates it over frames with the
// motion vectors and blurs it into the occlusion target sampled by the scene pass (Shaders/GTAO.hlsl). Also hosts the
// shared RenderFullscreenPass helper.

#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/Backend/GpuTypes.h"
#include "Renderer/Scene/RenderView.h"
#include "Renderer/RHI/ShaderLibrary.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Backend
{
    constexpr auto AMBIENT_OCCLUSION_RADIUS            = 192.0f; // World units.
    constexpr auto AMBIENT_OCCLUSION_MAX_SCREEN_RADIUS = 0.1f;   // UV units.
    constexpr auto AMBIENT_OCCLUSION_EXPONENT          = 1.5f;
    constexpr auto AMBIENT_OCCLUSION_HISTORY_WEIGHT    = 0.85f;

    // Same layout as AmbientOcclusionUniforms in Shaders/GTAO.hlsl. Two-component vectors stay 8 byte aligned for the std140 layout of OpenGL.
    struct AmbientOcclusionUniforms
    {
        Matrix       Projection        = Matrix::Identity;
        Matrix       InverseProjection = Matrix::Identity;
        Vector2      TargetSize        = Vector2::Zero;
        Vector2      TexelSize         = Vector2::Zero;
        Vector2      DepthTexelSize    = Vector2::Zero;
        float        Radius            = AMBIENT_OCCLUSION_RADIUS;
        float        MaxScreenRadius   = AMBIENT_OCCLUSION_MAX_SCREEN_RADIUS;
        float        FrameNoise        = 0.0f;
        float        HistoryWeight     = AMBIENT_OCCLUSION_HISTORY_WEIGHT;
        unsigned int IsHistoryValid    = 0;
        float        Padding           = 0.0f;
    };

    // Exponent applied to the occlusion by the scene shaders; zero disables it when ambient occlusion is off.
    float SceneRenderer::GetAmbientOcclusionExponent() const
    {
        return _isAmbientOcclusionActive ? AMBIENT_OCCLUSION_EXPONENT : 0.0f;
    }

    // GTAO, temporal and blur pipelines, the prepass pixel shaders of rooms and objects, and the point sampler.
    bool SceneRenderer::CreateAmbientOcclusionResources()
    {
        _aoVertexShader = LoadShader(*_device, _shaderDirectory, "GTAO", "VS");
        _gtaoPixelShader = LoadShader(*_device, _shaderDirectory, "GTAO", "PS");
        _aoTemporalPixelShader = LoadShader(*_device, _shaderDirectory, "GTAO", "PSTemporal");
        _aoBlurHorizontalShader = LoadShader(*_device, _shaderDirectory, "GTAO", "PSBlurHorizontal");
        _aoBlurVerticalShader = LoadShader(*_device, _shaderDirectory, "GTAO", "PSBlurVertical");
        _roomsPrepassPixelShader = LoadShader(*_device, _shaderDirectory, "Rooms", "PSDepthNormal");
        _objectsPrepassPixelShader = LoadShader(*_device, _shaderDirectory, "Objects", "PSDepthNormal");
        if (_aoVertexShader == nullptr || _gtaoPixelShader == nullptr || _aoTemporalPixelShader == nullptr || _aoBlurHorizontalShader == nullptr ||
            _aoBlurVerticalShader == nullptr || _roomsPrepassPixelShader == nullptr || _objectsPrepassPixelShader == nullptr)
        {
            return false;
        }

        _gtaoPipeline = CreateFullscreenPipeline(_aoVertexShader.get(), _gtaoPixelShader.get(), PixelFormat::R16Float, "GTAO");
        _aoTemporalPipeline = CreateFullscreenPipeline(_aoVertexShader.get(), _aoTemporalPixelShader.get(), PixelFormat::R16Float, "GTAO.Temporal");
        _aoBlurHorizontalPipeline = CreateFullscreenPipeline(_aoVertexShader.get(), _aoBlurHorizontalShader.get(), PixelFormat::R8Unorm, "GTAO.BlurHorizontal");
        _aoBlurVerticalPipeline = CreateFullscreenPipeline(_aoVertexShader.get(), _aoBlurVerticalShader.get(), PixelFormat::R8Unorm, "GTAO.BlurVertical");
        if (_gtaoPipeline == nullptr || _aoTemporalPipeline == nullptr || _aoBlurHorizontalPipeline == nullptr || _aoBlurVerticalPipeline == nullptr)
            return false;

        auto samplerDesc = SamplerDesc{};
        samplerDesc.MinFilter = samplerDesc.MagFilter = samplerDesc.MipFilter = Filter::Nearest;
        samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = AddressMode::ClampToEdge;
        samplerDesc.Name = "PointClamp";
        _pointClampSampler = _device->CreateSampler(samplerDesc);
        return (_pointClampSampler != nullptr);
    }

    // Prepass normal and emissive targets at scene resolution, and the half resolution occlusion target with the two
    // half float histories of the temporal accumulation, which 8 bits would round to a standstill.
    bool SceneRenderer::EnsureAmbientOcclusionTargets(int width, int height)
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
        auto occlusionDesc = TextureDesc{};
        occlusionDesc.Width = std::max(width / 2, 1);
        occlusionDesc.Height = std::max(height / 2, 1);
        occlusionDesc.Format = PixelFormat::R8Unorm;
        occlusionDesc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        occlusionDesc.Name = "GTAO.Occlusion";
        _ambientOcclusionTarget = _device->CreateTexture(occlusionDesc);

        occlusionDesc.Format = PixelFormat::R16Float;
        for (int i = 0; i < AO_HISTORY_COUNT; i++)
        {
            occlusionDesc.Name = "GTAO.History[" + std::to_string(i) + "]";
            _aoHistory[i] = _device->CreateTexture(occlusionDesc);
            if (_aoHistory[i] == nullptr)
                return false;
        }

        _isAoHistoryValid = false;
        return (_normalTarget != nullptr && _emissiveTarget != nullptr && _ambientOcclusionTarget != nullptr);
    }

    // Opaque geometry writes depth, view space normals, emissive, a sampled copy of the depth and with temporal passes
    // the motion vectors; the scene pass reuses the depth.
    void SceneRenderer::RenderDepthPrepass(ICommandBuffer& commandBuffer)
    {
        ColorTargetInfo colorTargets[4] = {};
        colorTargets[0].Texture = _normalTarget.get();
        colorTargets[1].Texture = _emissiveTarget.get();
        colorTargets[2].Texture = _sceneDepthCopy.get();
        colorTargets[2].ClearColor[0] = 1.0f; // Far plane, as the cleared depth.
        colorTargets[3].Texture = _velocityTarget.get(); // Motion vectors of the temporal passes, zero where nothing moves.

        auto depthTarget = DepthTargetInfo{};
        depthTarget.Texture = _depthTarget.get();

        auto* renderPass = commandBuffer.BeginRenderPass(colorTargets, _isVelocityActive ? 4 : 3, &depthTarget);
        DrawRooms(commandBuffer, *renderPass, ScenePass::Prepass);
        DrawObjects(commandBuffer, *renderPass, ScenePass::Prepass);
        commandBuffer.EndRenderPass();
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

    // Occlusion of one slice per texel, temporal accumulation into the next history, then a separable bilateral blur
    // into _ambientOcclusionTarget. The slice angles change every frame with the motion frame counter.
    void SceneRenderer::RenderAmbientOcclusion(ICommandBuffer& commandBuffer)
    {
        constexpr auto FRAME_NOISE_STEPS = 16;

        const auto& targetDesc = _ambientOcclusionTarget->GetDesc();
        int nextHistory = (_aoHistoryIndex + 1) % AO_HISTORY_COUNT;

        auto uniforms = AmbientOcclusionUniforms{};
        uniforms.Projection = _projection;
        uniforms.InverseProjection = _projection.Invert();
        uniforms.TargetSize = Vector2((float)targetDesc.Width, (float)targetDesc.Height);
        uniforms.TexelSize = Vector2(1.0f / targetDesc.Width, 1.0f / targetDesc.Height);
        uniforms.FrameNoise = (float)(_motionFrame % FRAME_NOISE_STEPS) / FRAME_NOISE_STEPS;
        uniforms.IsHistoryValid = _isAoHistoryValid ? 1 : 0;
        uniforms.DepthTexelSize = Vector2(1.0f / _sceneDepthCopy->GetDesc().Width, 1.0f / _sceneDepthCopy->GetDesc().Height);
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &uniforms, sizeof(uniforms));

        // The raw occlusion of one slice exceeds 1 on slanted surfaces: half float until it is accumulated.
        auto rawDesc = targetDesc;
        rawDesc.Format = PixelFormat::R16Float;
        auto* rawTarget = _targetPool.Acquire(rawDesc);
        auto* blurTarget = _targetPool.Acquire(targetDesc);

        TextureSamplerBinding occlusionBindings[2] =
        {
            { _sceneDepthCopy.get(), _pointClampSampler.get() },
            { _normalTarget.get(), _pointClampSampler.get() }
        };
        RenderFullscreenPass(commandBuffer, _gtaoPipeline.get(), rawTarget, occlusionBindings, 2, "Occlusion");

        TextureSamplerBinding temporalBindings[4] =
        {
            { rawTarget, _pointClampSampler.get() },
            { _aoHistory[_aoHistoryIndex].get(), _linearClampSampler.get() },
            { _velocityTarget.get(), _pointClampSampler.get() },
            { _sceneDepthCopy.get(), _pointClampSampler.get() }
        };
        RenderFullscreenPass(commandBuffer, _aoTemporalPipeline.get(), _aoHistory[nextHistory].get(), temporalBindings, 4, "Temporal accumulation");

        auto horizontalBinding = TextureSamplerBinding{ _aoHistory[nextHistory].get(), _pointClampSampler.get() };
        RenderFullscreenPass(commandBuffer, _aoBlurHorizontalPipeline.get(), blurTarget, &horizontalBinding, 1, "Blur horizontal");

        auto verticalBinding = TextureSamplerBinding{ blurTarget, _pointClampSampler.get() };
        RenderFullscreenPass(commandBuffer, _aoBlurVerticalPipeline.get(), _ambientOcclusionTarget.get(), &verticalBinding, 1, "Blur vertical");

        _targetPool.Release(rawTarget);
        _targetPool.Release(blurTarget);
        _aoHistoryIndex = nextHistory;
        _isAoHistoryValid = true;
    }
}
