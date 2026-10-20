// Temporal antialiasing and motion vectors. While TAA (or another temporal pass) is active, the prepass also writes
// screen space motion vectors (_velocityTarget), using the unjittered view projection of this frame and of the previous
// one and the previous transforms of moving items, cached here by item number. With TAA, the projection of the frame
// is jittered by a sub-pixel Halton offset, and the "TAA" pass of FRAME_PASSES, after the scene pass, blends the scene
// with the reprojected history of the previous frames (Shaders/TAA.hlsl). All targets have the render resolution.

#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/Backend/GpuTypes.h"
#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/Scene/RenderView.h"
#include "Renderer/Structures/RendererItem.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Backend
{
    constexpr auto TAA_JITTER_SAMPLES     = 8;
    constexpr auto TAA_CURRENT_WEIGHT     = 0.1f;
    constexpr auto TAA_CAMERA_CUT_DISTANCE = BLOCK(2); // A camera jump longer than this per frame invalidates the history.

    // Same layout as TemporalUniforms in Shaders/TAA.hlsl.
    struct TemporalUniforms
    {
        Matrix       ReprojectionMatrix = Matrix::Identity;
        Vector2      TexelSize          = Vector2::Zero;
        float        CurrentWeight      = TAA_CURRENT_WEIGHT;
        unsigned int IsHistoryValid     = 0;
    };

    // Radical inverse of an index in a base: the low discrepancy points of the Halton sequence, in [0, 1).
    static float GetHaltonValue(int index, int base)
    {
        float result = 0.0f;
        float fraction = 1.0f / (float)base;
        while (index > 0)
        {
            result += (float)(index % base) * fraction;
            index /= base;
            fraction /= (float)base;
        }

        return result;
    }

    // Prepass shaders with motion vectors and the resolve pipeline, which writes the scene and the history.
    bool SceneRenderer::CreateTemporalResources()
    {
        _roomsVelocityVertexShader = LoadShader(*_device, _shaderDirectory, "Rooms", "VSVelocity");
        _objectsVelocityVertexShader = LoadShader(*_device, _shaderDirectory, "Objects", "VSVelocity");
        _roomsVelocityPixelShader = LoadShader(*_device, _shaderDirectory, "Rooms", "PSDepthNormalVelocity");
        _objectsVelocityPixelShader = LoadShader(*_device, _shaderDirectory, "Objects", "PSDepthNormalVelocity");
        _taaVertexShader = LoadShader(*_device, _shaderDirectory, "TAA", "VS");
        _taaPixelShader = LoadShader(*_device, _shaderDirectory, "TAA", "PS");
        if (_roomsVelocityVertexShader == nullptr || _objectsVelocityVertexShader == nullptr || _roomsVelocityPixelShader == nullptr ||
            _objectsVelocityPixelShader == nullptr || _taaVertexShader == nullptr || _taaPixelShader == nullptr)
        {
            return false;
        }

        auto desc = GetFullscreenPipelineDesc(_taaVertexShader.get(), _taaPixelShader.get(), _device->GetSwapchainFormat(), "TAA");
        desc.NumColorTargets = 2;
        desc.ColorTargets[1].Format = PixelFormat::RGBA16Float;
        _taaPipeline = _device->CreateGraphicsPipeline(desc);
        return (_taaPipeline != nullptr);
    }

    // Motion vectors and the two history targets the resolve alternates between. The history is half float, so the
    // blend of many frames does not band.
    bool SceneRenderer::EnsureTemporalTargets(int width, int height)
    {
        auto desc = TextureDesc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = PixelFormat::RG16Float;
        desc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        desc.Name = "Prepass.Velocity";
        _velocityTarget = _device->CreateTexture(desc);

        desc.Format = PixelFormat::RGBA16Float;
        for (int i = 0; i < TAA_HISTORY_COUNT; i++)
        {
            desc.Name = "TAA.History[" + std::to_string(i) + "]";
            _taaHistory[i] = _device->CreateTexture(desc);
            if (_taaHistory[i] == nullptr)
                return false;
        }

        _isTaaHistoryValid = false;
        return (_velocityTarget != nullptr);
    }

    // Decides the temporal passes of the frame and offsets the projection by the sub-pixel jitter of the frame when TAA
    // is active. The camera position detects cuts, after which the history is discarded.
    void SceneRenderer::PrepareTemporalFrame(const RenderView& view, const FrameEnvironment& environment, Matrix& projection)
    {
        bool isTaaActive = (environment.AntialiasingMode == ANTIALIASING_MODE_TEMPORAL);
        bool isCameraCut = (Vector3::Distance(view.Camera.WorldPosition, _prevCameraPosition) > TAA_CAMERA_CUT_DISTANCE);
        if (isTaaActive != _isTaaActive || isCameraCut)
            _isTaaHistoryValid = false;

        // The ambient occlusion accumulates over frames too, through the motion vectors or the camera motion.
        if (isCameraCut)
            _isAoHistoryValid = false;

        // Motion vectors cost prepass bandwidth: only TAA needs them per texel.
        _isTaaActive = isTaaActive;
        _isVelocityActive = _isTaaActive;
        _prevCameraPosition = view.Camera.WorldPosition;
        _unjitteredViewProjection = view.Camera.View * projection;
        _motionFrame++;

        if (!_isTaaActive)
            return;

        // Offsets of one pixel at most, centred on the pixel: clip x and y move by the offset times w (z * _34).
        int sample = (_motionFrame % TAA_JITTER_SAMPLES) + 1;
        auto jitter = Vector2(GetHaltonValue(sample, 2) - 0.5f, GetHaltonValue(sample, 3) - 0.5f);
        const auto& sceneDesc = _sceneTarget->GetDesc();
        projection._31 += (jitter.x * 2.0f / (float)sceneDesc.Width) * projection._34;
        projection._32 += (jitter.y * 2.0f / (float)sceneDesc.Height) * projection._34;
    }

    // View projection of the previous frame for the motion vectors: this frame's without a usable history.
    Matrix SceneRenderer::GetPrevViewProjection() const
    {
        return _hasPrevViewProjection ? _prevViewProjection : _unjitteredViewProjection;
    }

    // Keeps the camera of the frame for the motion vectors of the next one.
    void SceneRenderer::FinishTemporalFrame()
    {
        _prevViewProjection = _unjitteredViewProjection;
        _hasPrevViewProjection = true;
    }

    // Transforms of an item in the previous frame, or null when the item was not drawn in it.
    const SceneRenderer::PreviousItemTransforms* SceneRenderer::GetPreviousItemTransforms(int itemNumber) const
    {
        auto it = _previousItemTransforms.find(itemNumber);
        if (it == _previousItemTransforms.end() || it->second.Frame != (_motionFrame - 1))
            return nullptr;

        return &it->second;
    }

    // Stores the interpolated transforms drawn this frame, the previous transforms of the next frame.
    void SceneRenderer::StoreItemTransforms(const RendererItem& item)
    {
        auto& transforms = _previousItemTransforms[item.ItemNumber];
        transforms.World = item.InterpolatedWorld;
        memcpy(transforms.Bones, item.InterpolatedAnimationTransforms, sizeof(transforms.Bones));
        transforms.Frame = _motionFrame;
    }

    // Previous transform of an instance; a bone offset of NO_VALUE keeps the bones of the current frame.
    void SceneRenderer::SetInstanceMotion(int instance, const Matrix& prevWorld, int prevBoneOffset)
    {
        auto& gpuInstance = ((GpuObjectInstance*)_instanceData.data())[instance];
        gpuInstance.PrevWorld = prevWorld;
        if (prevBoneOffset != NO_VALUE)
            gpuInstance.PrevSkinning = ((unsigned int)prevBoneOffset & 0xFFFFFF) | (gpuInstance.Skinning & 0xFF000000);
    }

    // Copies the scene, then blends the copy with the history into the scene target and the next history.
    void SceneRenderer::RenderTemporalAntialiasing(ICommandBuffer& commandBuffer)
    {
        const auto& sceneDesc = _sceneTarget->GetDesc();
        auto* current = CopySceneTarget(commandBuffer);
        int nextHistory = (_taaHistoryIndex + 1) % TAA_HISTORY_COUNT;

        auto uniforms = TemporalUniforms{};
        uniforms.ReprojectionMatrix = _unjitteredViewProjection.Invert() * GetPrevViewProjection();
        uniforms.TexelSize = Vector2(1.0f / sceneDesc.Width, 1.0f / sceneDesc.Height);
        uniforms.IsHistoryValid = _isTaaHistoryValid ? 1 : 0;
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &uniforms, sizeof(uniforms));

        ColorTargetInfo colorTargets[2] = {};
        colorTargets[0].Texture = _sceneTarget.get();
        colorTargets[0].Load = LoadOp::DontCare;
        colorTargets[1].Texture = _taaHistory[nextHistory].get();
        colorTargets[1].Load = LoadOp::DontCare;

        auto* renderPass = commandBuffer.BeginRenderPass(colorTargets, 2, nullptr);
        renderPass->BindPipeline(_taaPipeline.get());

        TextureSamplerBinding bindings[4] =
        {
            { current, _pointClampSampler.get() },
            { _taaHistory[_taaHistoryIndex].get(), _linearClampSampler.get() },
            { _velocityTarget.get(), _pointClampSampler.get() },
            { _sceneDepthCopy.get(), _pointClampSampler.get() }
        };
        renderPass->BindPixelSamplers(0, bindings, 4);

        CountDraw(DrawCategory::Fullscreen, 1);
        renderPass->Draw(3, 1, 0, 0);
        commandBuffer.EndRenderPass();

        _targetPool.Release(current);
        _taaHistoryIndex = nextHistory;
        _isTaaHistoryValid = true;

        // The scene pass and the screen space passes after it read the slot 2 uniforms of their own.
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &_shadowUniforms, sizeof(_shadowUniforms));
    }
}
