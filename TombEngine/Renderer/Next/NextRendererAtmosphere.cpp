#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/RHI/ShaderLibrary.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Next
{
    // Same layout as SkyCameraUniforms in Shaders/RHI/AtmosphericSky.hlsl.
    struct SkyCameraUniforms
    {
        Matrix InverseProjection = Matrix::Identity;
        Matrix InverseView       = Matrix::Identity;
    };

    bool NextRenderer::CreateAtmosphereResources()
    {
        _atmosphericSkyVertexShader = LoadShader(*_device, _shaderDirectory, "AtmosphericSky", "VS");
        _atmosphericSkyPixelShader = LoadShader(*_device, _shaderDirectory, "AtmosphericSky", "PSAtmosphericSky");
        if (_atmosphericSkyVertexShader == nullptr || _atmosphericSkyPixelShader == nullptr)
            return false;

        // Drawn inside the scene pass, so the pipeline matches its targets; the dome ignores depth.
        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = _atmosphericSkyVertexShader.get();
        desc.PixelShader = _atmosphericSkyPixelShader.get();
        desc.Rasterizer.Cull = RHI::CullMode::None;
        desc.DepthStencil.EnableDepthTest = false;
        desc.DepthStencil.EnableDepthWrite = false;
        desc.DepthFormat = PixelFormat::D32Float;
        desc.NumColorTargets = 1;
        desc.ColorTargets[0].Format = _device->GetSwapchainFormat();
        desc.Name = "AtmosphericSky";
        _atmosphericSkyPipeline = _device->CreateGraphicsPipeline(desc);
        return (_atmosphericSkyPipeline != nullptr);
    }

    // Legacy DrawAtmosphericSkyDome: a fullscreen opaque background below the classic sky layers. The uniform block
    // is the one the legacy renderer computed for this frame.
    void NextRenderer::DrawAtmosphericSky(ICommandBuffer& commandBuffer, IRenderPass& renderPass)
    {
        if (_atmosphericSkyUniforms == nullptr)
            return;

        auto cameraUniforms = SkyCameraUniforms{ _projection.Invert(), _view.Invert() };
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, _atmosphericSkyUniforms, _atmosphericSkyUniformsSize);
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 3, &cameraUniforms, sizeof(cameraUniforms));

        commandBuffer.PushDebugGroup("Atmospheric sky");
        renderPass.BindPipeline(_atmosphericSkyPipeline.get());
        _frameStats.DrawCalls++;
        renderPass.Draw(3, 1, 0, 0);
        commandBuffer.PopDebugGroup();
    }
}
