#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/RHI/ShaderLibrary.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Next
{
    // Same layout as DofUniforms in Shaders/RHI/DOF.hlsl.
    struct DofUniforms
    {
        float        FocusDistance = 0.0f;
        float        FocusRange    = 0.0f;
        float        Strength      = 0.0f;
        unsigned int Mode          = 0;
        Vector2      TexelSize     = Vector2::Zero;
        Vector2      FullTexelSize = Vector2::Zero;
        float        NearPlane     = 1.0f;
        float        FarPlane      = 1.0f;
        float        Padding[2]    = {};
    };

    static_assert(sizeof(DofUniforms) == 48, "DofUniforms must match DOF.hlsl.");

    bool NextRenderer::CreateDOFResources()
    {
        const char* entryPoints[DOF_PASS_COUNT] = { "PSDownsample", "PSFarBlur", "PSNearDilate", "PSNearBlur" };

        _dofVertexShader = LoadShader(*_device, _shaderDirectory, "DOF", "VS");
        if (_dofVertexShader == nullptr)
            return false;

        for (int i = 0; i < DOF_PASS_COUNT; i++)
        {
            _dofPixelShaders[i] = LoadShader(*_device, _shaderDirectory, "DOF", entryPoints[i]);
            if (_dofPixelShaders[i] == nullptr)
                return false;

            _dofPipelines[i] = CreateFullscreenPipeline(_dofVertexShader.get(), _dofPixelShaders[i].get(), PixelFormat::RGBA8Unorm, (std::string("DOF.") + entryPoints[i]).c_str());
            if (_dofPipelines[i] == nullptr)
                return false;
        }

        return true;
    }

    bool NextRenderer::EnsureDOFTargets(int width, int height)
    {
        auto desc = TextureDesc{};
        desc.Width = std::max(width / 2, 1);
        desc.Height = std::max(height / 2, 1);
        desc.Format = PixelFormat::RGBA8Unorm;
        desc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;

        for (int i = 0; i < DOF_TARGET_COUNT; i++)
        {
            desc.Name = "DOF[" + std::to_string(i) + "]";
            _dofTargets[i] = _device->CreateTexture(desc);
            if (_dofTargets[i] == nullptr)
                return false;
        }

        return true;
    }

    bool NextRenderer::IsDOFActive(const FrameEnvironment& environment)
    {
        return (environment.DofMode != 0 && environment.DofStrength > EPSILON);
    }

    // Same passes as the legacy ApplyDOF. The near blur ends in _dofTargets[0], the far blur in _dofTargets[1].
    void NextRenderer::RenderDOF(ICommandBuffer& commandBuffer)
    {
        const auto& environment = *_frameEnvironment;
        const auto& targetDesc = _dofTargets[0]->GetDesc();
        const auto& sceneDesc = _sceneTarget->GetDesc();

        auto uniforms = DofUniforms{};
        uniforms.FocusDistance = environment.DofDistance;
        uniforms.FocusRange = environment.DofRange;
        uniforms.Strength = environment.DofStrength;
        uniforms.Mode = (unsigned int)environment.DofMode;
        uniforms.TexelSize = Vector2(1.0f / targetDesc.Width, 1.0f / targetDesc.Height);
        uniforms.FullTexelSize = Vector2(1.0f / sceneDesc.Width, 1.0f / sceneDesc.Height);
        uniforms.NearPlane = _nearPlane;
        uniforms.FarPlane = _farPlane;
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &uniforms, sizeof(uniforms));

        auto drawPass = [&](int pass, ITexture* target, ITexture* source, const char* name)
        {
            TextureSamplerBinding bindings[2] =
            {
                { source, _linearClampSampler.get() },
                { _sceneDepthCopy.get(), _pointClampSampler.get() }
            };
            RenderFullscreenPass(commandBuffer, _dofPipelines[pass].get(), target, bindings, (pass == 0) ? 2 : 1, name);
        };

        auto* dilatedTarget = _targetPool.Acquire(targetDesc);
        drawPass(0, _dofTargets[0].get(), _sceneTarget.get(), "Downsample");
        drawPass(1, _dofTargets[1].get(), _dofTargets[0].get(), "Far blur");
        drawPass(2, dilatedTarget, _dofTargets[0].get(), "Near dilate");
        drawPass(3, _dofTargets[0].get(), dilatedTarget, "Near blur");
        _targetPool.Release(dilatedTarget);
    }
}
