// Temporal antialiasing and motion vectors. While TAA (or another temporal pass) is active, the prepass also writes
// screen space motion vectors (_velocityTarget), using the unjittered view projection of this frame and of the previous
// one and the previous transforms of moving items, cached here by item number. With TAA, the projection of the frame
// is jittered by a sub-pixel Halton offset, and the "TAA" pass of FRAME_PASSES, after the scene pass, blends the scene
// with the reprojected history of the previous frames into the next history (Shaders/TAA.hlsl), which the later passes
// read as the scene. The motion vectors have the render resolution, the history the window resolution: below native
// scale the resolve upscales temporally, in place of the FSR 1 upscale pass.

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
    constexpr auto TAA_JITTER_SAMPLES     = 8;        // At native scale; more below it, so the samples cover every window texel.
    constexpr auto TAA_JITTER_SAMPLES_MAX = 32;
    constexpr auto TAA_CURRENT_WEIGHT     = 0.1f;
    constexpr auto TAA_CAMERA_CUT_DISTANCE = BLOCK(2); // A camera jump longer than this per frame invalidates the history.
    constexpr auto TAA_GROUP_SIZE          = 16;       // Threads per group side, as GROUP_SIZE in Shaders/TAA.hlsl.

    // Same layout as TemporalUniforms in Shaders/TAA.hlsl.
    struct TemporalUniforms
    {
        Matrix       ReprojectionMatrix = Matrix::Identity;
        Vector2      InputSize          = Vector2::Zero;
        Vector2      OutputSize         = Vector2::Zero;
        Vector2      InputTexelSize     = Vector2::Zero;
        Vector2      OutputTexelSize    = Vector2::Zero;
        Vector2      SampleOffset       = Vector2::Zero;
        float        CurrentWeight      = TAA_CURRENT_WEIGHT;
        unsigned int IsHistoryValid     = 0;
        unsigned int FrameIndex         = 0;
        unsigned int Padding[3]         = {};
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

    // Prepass shaders with motion vectors and the resolve compute pipeline, which writes the history.
    bool SceneRenderer::CreateTemporalResources()
    {
        _roomsVelocityVertexShader = LoadShader(*_device, _shaderDirectory, "Rooms", "VSVelocity");
        _objectsVelocityVertexShader = LoadShader(*_device, _shaderDirectory, "Objects", "VSVelocity");
        _roomsVelocityPixelShader = LoadShader(*_device, _shaderDirectory, "Rooms", "PSDepthNormalVelocity");
        _objectsVelocityPixelShader = LoadShader(*_device, _shaderDirectory, "Objects", "PSDepthNormalVelocity");
        _taaShader = LoadShader(*_device, _shaderDirectory, "TAA", "CS");
        _taauShader = LoadShader(*_device, _shaderDirectory, "TAA", "CS", "TAA_UPSCALE");
        if (_roomsVelocityVertexShader == nullptr || _objectsVelocityVertexShader == nullptr || _roomsVelocityPixelShader == nullptr ||
            _objectsVelocityPixelShader == nullptr || _taaShader == nullptr || _taauShader == nullptr)
        {
            return false;
        }

        _taaPipeline = _device->CreateComputePipeline(ComputePipelineDesc{ _taaShader.get(), "TAA" });
        _taauPipeline = _device->CreateComputePipeline(ComputePipelineDesc{ _taauShader.get(), "TAA.Upscale" });
        return (_taaPipeline != nullptr && _taauPipeline != nullptr);
    }

    // Motion vectors at the render resolution.
    bool SceneRenderer::EnsureTemporalTargets(int width, int height)
    {
        auto desc = TextureDesc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = PixelFormat::RG16Float;
        desc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        desc.Name = "Prepass.Velocity";
        _velocityTarget = _device->CreateTexture(desc);
        return (_velocityTarget != nullptr);
    }

    // The two history targets the resolve alternates between, at the window resolution, so they outlive render scale
    // changes. 8 bits per channel, as the scene; the resolve rounds them stochastically so slow blends converge.
    bool SceneRenderer::EnsureTemporalHistory(int width, int height)
    {
        auto desc = TextureDesc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = PixelFormat::RGBA8Unorm;
        desc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler | TextureUsage::ComputeStorageWrite; // Cleared as targets.
        for (int i = 0; i < TAA_HISTORY_COUNT; i++)
        {
            desc.Name = "TAA.History[" + std::to_string(i) + "]";
            _taaHistory[i] = _device->CreateTexture(desc);
            if (_taaHistory[i] == nullptr)
                return false;
        }

        _isTaaHistoryValid = false;
        return true;
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
        _isSpatialUpscaleActive = (_isUpscaleActive && !_isTaaActive);
        _prevCameraPosition = view.Camera.WorldPosition;
        _unjitteredViewProjection = view.Camera.View * projection;
        _motionFrame++;

        if (!_isTaaActive)
            return;

        // Offsets of one pixel at most, centred on the pixel: clip x and y move by the offset times w (z * _34). Below native
        // scale the sequence is longer by the texel area ratio, as each render texel covers several window texels.
        const auto& sceneDesc = _sceneTarget->GetDesc();
        float areaRatio = (float)(_colorTarget->GetDesc().Width * _colorTarget->GetDesc().Height) / (float)(sceneDesc.Width * sceneDesc.Height);
        int sampleCount = std::clamp((int)std::ceil(TAA_JITTER_SAMPLES * areaRatio), TAA_JITTER_SAMPLES, TAA_JITTER_SAMPLES_MAX);
        int sample = (_motionFrame % sampleCount) + 1;
        auto jitter = Vector2(GetHaltonValue(sample, 2) - 0.5f, GetHaltonValue(sample, 3) - 0.5f);
        projection._31 += (jitter.x * 2.0f / (float)sceneDesc.Width) * projection._34;
        projection._32 += (jitter.y * 2.0f / (float)sceneDesc.Height) * projection._34;

        // The image moves right and up by the jitter, so a render texel holds the unjittered scene left of and below its center.
        _taaSampleOffset = Vector2(-jitter.x, jitter.y);
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

    // The scene the passes after the scene pass read: the antialiased history with TAA, else the scene target.
    ITexture* SceneRenderer::GetResolvedSceneTarget() const
    {
        return _isTaaActive ? _taaHistory[_taaHistoryIndex].get() : _sceneTarget.get();
    }

    // Resolves the jittered scene and the history into the next history, which then stands for the scene: one 8 bit
    // write per window texel, without a copy of the scene target, in groups of TAA_GROUP_SIZE squared window texels.
    void SceneRenderer::RenderTemporalAntialiasing(ICommandBuffer& commandBuffer)
    {
        const auto& sceneDesc = _sceneTarget->GetDesc();
        const auto& historyDesc = _taaHistory[0]->GetDesc();
        int nextHistory = (_taaHistoryIndex + 1) % TAA_HISTORY_COUNT;

        auto uniforms = TemporalUniforms{};
        uniforms.ReprojectionMatrix = _unjitteredViewProjection.Invert() * GetPrevViewProjection();
        uniforms.InputSize = Vector2((float)sceneDesc.Width, (float)sceneDesc.Height);
        uniforms.OutputSize = Vector2((float)historyDesc.Width, (float)historyDesc.Height);
        uniforms.InputTexelSize = Vector2(1.0f / sceneDesc.Width, 1.0f / sceneDesc.Height);
        uniforms.OutputTexelSize = Vector2(1.0f / historyDesc.Width, 1.0f / historyDesc.Height);
        uniforms.SampleOffset = _taaSampleOffset;
        uniforms.IsHistoryValid = _isTaaHistoryValid ? 1 : 0;
        uniforms.FrameIndex = (unsigned int)_motionFrame;
        commandBuffer.PushUniforms(RHI::ShaderStage::Compute, 0, &uniforms, sizeof(uniforms));

        auto passDesc = ComputePassDesc{};
        passDesc.RWTextures[0] = _taaHistory[nextHistory].get();
        passDesc.NumRWTextures = 1;

        auto* computePass = commandBuffer.BeginComputePass(passDesc);
        computePass->BindPipeline(_isUpscaleActive ? _taauPipeline.get() : _taaPipeline.get());

        TextureSamplerBinding bindings[4] =
        {
            { _sceneTarget.get(), _pointClampSampler.get() },
            { _taaHistory[_taaHistoryIndex].get(), _linearClampSampler.get() },
            { _velocityTarget.get(), _pointClampSampler.get() },
            { _sceneDepthCopy.get(), _pointClampSampler.get() }
        };
        computePass->BindSamplers(0, bindings, 4);

        CountDraw(DrawCategory::Fullscreen, 1);
        computePass->Dispatch((historyDesc.Width + TAA_GROUP_SIZE - 1) / TAA_GROUP_SIZE, (historyDesc.Height + TAA_GROUP_SIZE - 1) / TAA_GROUP_SIZE, 1);
        commandBuffer.EndComputePass();

        _taaHistoryIndex = nextHistory;
        _isTaaHistoryValid = true;
    }
}
