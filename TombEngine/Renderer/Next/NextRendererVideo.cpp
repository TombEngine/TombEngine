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
            frames.push_back(GpuAnimatedFrame{ uv[0], uv[1], uv[2], uv[3] });
        }
    }

    // The background video frame decoded by the legacy video handler is uploaded once per frame, cycling the
    // texture so frames still in flight keep the previous one.
    void NextRenderer::UploadVideoFrame(const unsigned char* frame, int width, int height)
    {
        _isVideoActive = false;
        if (frame == nullptr || width <= 0 || height <= 0)
            return;

        int size = width * height * 4;
        if (_videoTexture == nullptr || _videoTexture->GetDesc().Width != width || _videoTexture->GetDesc().Height != height)
        {
            auto desc = TextureDesc{};
            desc.Width = width;
            desc.Height = height;
            desc.Format = PixelFormat::BGRA8Unorm;
            desc.Name = "Video";
            _videoTexture = _device->CreateTexture(desc);

            desc.Type = TextureType::Texture2DArray;
            desc.Name = "Video.Array";
            _videoArrayTexture = _device->CreateTexture(desc);
            if (_videoTexture == nullptr || _videoArrayTexture == nullptr)
                return;
        }

        // Sprites sample the frame as a texture, animated buckets as a one layer array in place of their page.
        auto region = TextureRegion{};
        region.Width = width;
        region.Height = height;
        region.Texture = _videoTexture.get();
        _uploadRing.UploadToTexture(region, frame, size);
        region.Texture = _videoArrayTexture.get();
        _uploadRing.UploadToTexture(region, frame, size);
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
