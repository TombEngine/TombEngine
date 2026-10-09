#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/Next/GpuTypes.h"
#include "Renderer/Structures/RendererAnimatedTexture.h"
#include "Renderer/Structures/RendererAnimatedTextureSet.h"
#include "Renderer/Structures/RendererBucket.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Next
{
    // Legacy SetupAnimatedTextures shows the whole video frame on a video set's face through its normalized UVs.
    // Each video set gets a single frame set with those UVs, used while a background video plays.
    void NextRenderer::AddVideoSets(const LegacyLevelGeometry& geometry, std::vector<GpuAnimatedFrame>& frames, std::vector<GpuAnimatedSet>& sets)
    {
        _videoSets.clear();
        for (int setIndex = 0; setIndex < (int)geometry.AnimatedTextureSets->size(); setIndex++)
        {
            const auto& set = (*geometry.AnimatedTextureSets)[setIndex];
            if (set.Type != AnimatedTextureType::Video || set.Textures.empty())
                continue;

            auto videoSet = GpuAnimatedSet{};
            videoSet.FirstFrame = (unsigned int)frames.size();
            videoSet.NumFrames = 1;
            videoSet.Fps = 1;

            _videoSets[setIndex] = (int)sets.size();
            sets.push_back(videoSet);

            const auto* uv = set.Textures[0].NormalizedUV;
            frames.push_back(GpuAnimatedFrame{ { uv[0], uv[1], uv[2], uv[3] } });
        }
    }

    // The background video frame decoded by the legacy video handler is uploaded once per frame, cycling the
    // texture so frames still in flight keep the previous one.
    void NextRenderer::UploadVideoFrame(ICopyPass& copyPass, const FrameEnvironment& environment)
    {
        _isVideoActive = false;
        if (environment.VideoFrame == nullptr || environment.VideoWidth <= 0 || environment.VideoHeight <= 0)
            return;

        int size = environment.VideoWidth * environment.VideoHeight * 4;
        if (_videoTexture == nullptr || _videoTexture->GetDesc().Width != environment.VideoWidth || _videoTexture->GetDesc().Height != environment.VideoHeight)
        {
            auto desc = TextureDesc{};
            desc.Width = environment.VideoWidth;
            desc.Height = environment.VideoHeight;
            desc.Format = PixelFormat::BGRA8Unorm;
            desc.Name = "Video";
            _videoTexture = _device->CreateTexture(desc);
            _videoTransferBuffer = _device->CreateTransferBuffer(TransferBufferUsage::Upload, size);
            if (_videoTexture == nullptr || _videoTransferBuffer == nullptr)
                return;
        }

        memcpy(_device->MapTransferBuffer(_videoTransferBuffer.get(), true), environment.VideoFrame, size);
        _device->UnmapTransferBuffer(_videoTransferBuffer.get());

        auto region = TextureRegion{};
        region.Texture = _videoTexture.get();
        region.Width = environment.VideoWidth;
        region.Height = environment.VideoHeight;
        copyPass.UploadToTexture(TextureTransferInfo{ _videoTransferBuffer.get(), 0, 0, 0 }, region, true);
        _isVideoActive = true;
    }

    // Video set of an animated bucket while a background video plays, NO_VALUE otherwise.
    int NextRenderer::GetVideoSet(const RendererBucket& bucket) const
    {
        if (!_isVideoActive || !bucket.Animated)
            return NO_VALUE;

        auto it = _videoSets.find(bucket.Texture);
        return (it != _videoSets.end()) ? it->second : NO_VALUE;
    }
}
