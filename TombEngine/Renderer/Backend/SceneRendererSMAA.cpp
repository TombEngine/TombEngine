// Antialiasing resolve: FXAA or SMAA 1x (medium or ultra preset), called at the end of the post-process pass (and by
// the overlay composition) when an antialiasing mode is set. Reads an antialiasing source and writes the given target
// at its resolution: the frame target or the upscale source after the post-process, the final target for the overlay.
// SMAA uses pooled edge, weight and stencil targets and the precomputed area and search textures in SMAA/.

#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/Backend/SMAA/AreaTex.h"
#include "Renderer/Backend/SMAA/SearchTex.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Backend
{
    constexpr auto ANTIALIASING_MODE_LOW    = 1;
    constexpr auto ANTIALIASING_MODE_MEDIUM = 2;

    constexpr auto SMAA_EDGE_STENCIL = 1;

    // As in the original SMAA, edge detection marks the edge pixels in a stencil, and the blending weights, by far the
    // most expensive pass, are computed only there; the other pixels keep the cleared weights of zero.
    std::unique_ptr<IGraphicsPipeline> SceneRenderer::CreateSMAAPipeline(const SmaaPreset& preset, int pass, const char* name)
    {
        const PixelFormat formats[SMAA_PASS_COUNT] = { PixelFormat::RG8Unorm, PixelFormat::RGBA8Unorm, _device->GetSwapchainFormat() };

        auto desc = GetFullscreenPipelineDesc(preset.VertexShader.get(), preset.PixelShaders[pass].get(), formats[pass], name);
        if (_smaaStencilFormat != PixelFormat::Unknown && pass < 2)
        {
            desc.DepthFormat = _smaaStencilFormat;
            desc.DepthStencil.EnableStencilTest = true;
            desc.DepthStencil.StencilCompare = (pass == 0) ? CompareOp::Always : CompareOp::Equal;
            desc.DepthStencil.StencilPassOp = (pass == 0) ? StencilOp::Replace : StencilOp::Keep;
            desc.DepthStencil.StencilReference = SMAA_EDGE_STENCIL;
        }

        return _device->CreateGraphicsPipeline(desc);
    }

    // Picks a stencil format for the edge mask (none if unsupported, then weights run on every pixel), builds the
    // three pipelines of both presets and uploads the area and search lookup textures.
    bool SceneRenderer::CreateSMAAResources()
    {
        const char* shaderNames[SMAA_PRESET_COUNT] = { "SMAAMedium", "SMAAUltra" };
        const char* entryPoints[SMAA_PASS_COUNT] = { "PSEdgeDetection", "PSBlendingWeight", "PSNeighborhoodBlending" };

        _smaaStencilFormat = PixelFormat::Unknown;
        for (auto format : { PixelFormat::D24UnormS8Uint, PixelFormat::D32FloatS8Uint })
        {
            if (_device->SupportsTextureFormat(format, TextureType::Texture2D, TextureUsage::DepthStencilTarget))
            {
                _smaaStencilFormat = format;
                break;
            }
        }

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
                resources.Pipelines[pass] = CreateSMAAPipeline(resources, pass, name.c_str());
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

    // Descriptions of the intermediate SMAA targets for an output target, also used to reserve them in the pool.
    void SceneRenderer::GetSMAATargetDescs(const TextureDesc& targetDesc, TextureDesc& edgesDesc, TextureDesc& weightsDesc, TextureDesc& stencilDesc) const
    {
        edgesDesc = TextureDesc{};
        edgesDesc.Width = targetDesc.Width;
        edgesDesc.Height = targetDesc.Height;
        edgesDesc.Format = PixelFormat::RG8Unorm;
        edgesDesc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;

        weightsDesc = edgesDesc;
        weightsDesc.Format = PixelFormat::RGBA8Unorm;

        stencilDesc = edgesDesc;
        stencilDesc.Format = _smaaStencilFormat;
        stencilDesc.Usage = TextureUsage::DepthStencilTarget;
    }

    // FXAA for the low antialiasing mode, SMAA 1x with the medium or ultra preset for the medium and high modes,
    // from the post-processed source image to the target.
    void SceneRenderer::RenderAntialiasing(ICommandBuffer& commandBuffer, int antialiasingMode, ITexture* source, ITexture* target)
    {
        if (antialiasingMode == ANTIALIASING_MODE_LOW)
        {
            auto binding = TextureSamplerBinding{ source, _linearClampSampler.get() };
            RenderFullscreenPass(commandBuffer, _fxaaPipeline.get(), target, &binding, 1, "FXAA");
            return;
        }

        // SMAA: edge detection, blending weights, neighborhood blending.
        const auto& resources = _smaaPresets[(antialiasingMode == ANTIALIASING_MODE_MEDIUM) ? 0 : 1];
        const auto& desc = target->GetDesc();
        auto metrics = Vector4(1.0f / desc.Width, 1.0f / desc.Height, (float)desc.Width, (float)desc.Height);
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &metrics, sizeof(metrics));

        auto edgesDesc = TextureDesc{};
        auto weightsDesc = TextureDesc{};
        auto stencilDesc = TextureDesc{};
        GetSMAATargetDescs(desc, edgesDesc, weightsDesc, stencilDesc);

        auto* edgesTarget = _targetPool.Acquire(edgesDesc);
        auto* weightsTarget = _targetPool.Acquire(weightsDesc);
        auto* stencilTarget = (_smaaStencilFormat != PixelFormat::Unknown) ? _targetPool.Acquire(stencilDesc) : nullptr;

        auto edgeStencil = DepthTargetInfo{};
        edgeStencil.Texture = stencilTarget;
        edgeStencil.Store = StoreOp::Store;

        auto weightStencil = edgeStencil;
        weightStencil.Load = LoadOp::Load;
        weightStencil.Store = StoreOp::DontCare;

        // Edge detection discards texels without edges, so its target and the weights target are cleared.
        auto* linear = _linearClampSampler.get();
        TextureSamplerBinding edgeBindings[1] = { { source, _pointClampSampler.get() } };
        TextureSamplerBinding weightBindings[3] = { { edgesTarget, linear }, { _smaaAreaTexture.get(), linear }, { _smaaSearchTexture.get(), linear } };
        TextureSamplerBinding blendBindings[2] = { { source, linear }, { weightsTarget, linear } };

        commandBuffer.PushDebugGroup("SMAA");
        RenderFullscreenPass(commandBuffer, resources.Pipelines[0].get(), edgesTarget, edgeBindings, 1, "Edge detection", LoadOp::Clear,
            (stencilTarget != nullptr) ? &edgeStencil : nullptr);
        RenderFullscreenPass(commandBuffer, resources.Pipelines[1].get(), weightsTarget, weightBindings, 3, "Blending weights", LoadOp::Clear,
            (stencilTarget != nullptr) ? &weightStencil : nullptr);
        RenderFullscreenPass(commandBuffer, resources.Pipelines[2].get(), target, blendBindings, 2, "Neighborhood blending");
        _targetPool.Release(edgesTarget);
        _targetPool.Release(weightsTarget);
        if (stencilTarget != nullptr)
            _targetPool.Release(stencilTarget);
        commandBuffer.PopDebugGroup();
    }
}
