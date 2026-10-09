#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/SMAA/AreaTex.h"
#include "Renderer/SMAA/SearchTex.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Next
{
    constexpr auto ANTIALIASING_MODE_LOW    = 1;
    constexpr auto ANTIALIASING_MODE_MEDIUM = 2;

    bool NextRenderer::CreateSMAAResources()
    {
        const char* shaderNames[SMAA_PRESET_COUNT] = { "SMAAMedium", "SMAAUltra" };
        const char* entryPoints[SMAA_PASS_COUNT] = { "PSEdgeDetection", "PSBlendingWeight", "PSNeighborhoodBlending" };
        const PixelFormat formats[SMAA_PASS_COUNT] = { PixelFormat::RG8Unorm, PixelFormat::RGBA8Unorm, _device->GetSwapchainFormat() };

        for (int preset = 0; preset < SMAA_PRESET_COUNT; preset++)
        {
            auto& resources = _smaaPresets[preset];
            resources.VertexShader = LoadShader(*_device, _shaderDirectory, shaderNames[preset], "VS");
            if (resources.VertexShader == nullptr)
                return false;

            for (int pass = 0; pass < SMAA_PASS_COUNT; pass++)
            {
                resources.PixelShaders[pass] = LoadShader(*_device, _shaderDirectory, shaderNames[preset], entryPoints[pass]);
                if (resources.PixelShaders[pass] == nullptr)
                    return false;

                auto name = std::string(shaderNames[preset]) + "." + entryPoints[pass];
                resources.Pipelines[pass] = CreateFullscreenPipeline(resources.VertexShader.get(), resources.PixelShaders[pass].get(), formats[pass], name.c_str());
                if (resources.Pipelines[pass] == nullptr)
                    return false;
            }
        }

        auto desc = TextureDesc{};
        desc.Width = AREATEX_WIDTH;
        desc.Height = AREATEX_HEIGHT;
        desc.Format = PixelFormat::RG8Unorm;
        desc.Name = "SMAA.Area";
        _smaaAreaTexture = _device->CreateTexture(desc);

        desc.Width = SEARCHTEX_WIDTH;
        desc.Height = SEARCHTEX_HEIGHT;
        desc.Format = PixelFormat::R8Unorm;
        desc.Name = "SMAA.Search";
        _smaaSearchTexture = _device->CreateTexture(desc);
        if (_smaaAreaTexture == nullptr || _smaaSearchTexture == nullptr)
            return false;

        UploadTexture(_smaaAreaTexture.get(), areaTexBytes, (int)sizeof(areaTexBytes));
        UploadTexture(_smaaSearchTexture.get(), searchTexBytes, (int)sizeof(searchTexBytes));
        return true;
    }

    // FXAA for the low antialiasing mode, SMAA 1x with the medium or ultra preset for the medium and high modes,
    // from the post-processed image to the target.
    void NextRenderer::RenderAntialiasing(ICommandBuffer& commandBuffer, int antialiasingMode, ITexture* target)
    {
        if (antialiasingMode == ANTIALIASING_MODE_LOW)
        {
            auto binding = TextureSamplerBinding{ _antialiasingSource.get(), _linearClampSampler.get() };
            RenderFullscreenPass(commandBuffer, _fxaaPipeline.get(), target, &binding, 1, "FXAA");
            return;
        }

        const auto& resources = _smaaPresets[(antialiasingMode == ANTIALIASING_MODE_MEDIUM) ? 0 : 1];
        const auto& desc = target->GetDesc();
        auto metrics = Vector4(1.0f / desc.Width, 1.0f / desc.Height, (float)desc.Width, (float)desc.Height);
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &metrics, sizeof(metrics));

        auto edgesDesc = TextureDesc{};
        edgesDesc.Width = desc.Width;
        edgesDesc.Height = desc.Height;
        edgesDesc.Format = PixelFormat::RG8Unorm;
        edgesDesc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;

        auto weightsDesc = edgesDesc;
        weightsDesc.Format = PixelFormat::RGBA8Unorm;

        auto* edgesTarget = _targetPool.Acquire(edgesDesc);
        auto* weightsTarget = _targetPool.Acquire(weightsDesc);

        // Edge detection discards texels without edges, so its target and the weights target are cleared.
        auto* linear = _linearClampSampler.get();
        TextureSamplerBinding edgeBindings[1] = { { _antialiasingSource.get(), _pointClampSampler.get() } };
        TextureSamplerBinding weightBindings[3] = { { edgesTarget, linear }, { _smaaAreaTexture.get(), linear }, { _smaaSearchTexture.get(), linear } };
        TextureSamplerBinding blendBindings[2] = { { _antialiasingSource.get(), linear }, { weightsTarget, linear } };

        commandBuffer.PushDebugGroup("SMAA");
        RenderFullscreenPass(commandBuffer, resources.Pipelines[0].get(), edgesTarget, edgeBindings, 1, "Edge detection", LoadOp::Clear);
        RenderFullscreenPass(commandBuffer, resources.Pipelines[1].get(), weightsTarget, weightBindings, 3, "Blending weights", LoadOp::Clear);
        RenderFullscreenPass(commandBuffer, resources.Pipelines[2].get(), target, blendBindings, 2, "Neighborhood blending");
        _targetPool.Release(edgesTarget);
        _targetPool.Release(weightsTarget);
        commandBuffer.PopDebugGroup();
    }
}
