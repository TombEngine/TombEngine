#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Game/camera.h"
#include "Renderer/Next/GpuTypes.h"
#include "Renderer/RenderView.h"
#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/Structures/RendererSprite.h"
#include "Renderer/Structures/RendererSpriteToDraw.h"
#include "Specific/level.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Next
{
    // Same layout as SpriteUniforms in Sprites.hlsl.
    struct SpriteUniforms
    {
        unsigned int IsSoftParticle = 0;
        unsigned int RenderType     = 0;
        float        NearPlane      = 1.0f;
        float        FarPlane       = 1.0f;
        float        Time           = 0.0f;
        float        Padding[3]     = {};
    };

    // Corner order of the legacy sprite quads: top left, top right, bottom right, bottom left.
    constexpr unsigned int SPRITE_CORNER_INDICES[6] = { 0, 1, 3, 2, 3, 1 };

    static unsigned int PackSpriteColor(const Vector4& color)
    {
        auto toByte = [](float value) { return (unsigned int)(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f); };
        return toByte(color.x) | (toByte(color.y) << 8) | (toByte(color.z) << 16) | (toByte(color.w) << 24);
    }

    // Port of the legacy GetWorldMatrixForSprite; mirrors pass the reflected sprite position.
    static Matrix GetSpriteWorldMatrix(const RendererSpriteToDraw& sprite, const Vector3& position, const RenderView& view)
    {
        auto scaleMatrix = Matrix::CreateScale(sprite.Width * sprite.Scale, sprite.Height * sprite.Scale, sprite.Scale);

        switch (sprite.Type)
        {
        case SpriteType::Billboard:
        {
            auto cameraUp = Vector3(view.Camera.View._12, view.Camera.View._22, view.Camera.View._32);
            return scaleMatrix * Matrix::CreateRotationZ(sprite.Rotation) * Matrix::CreateBillboard(position, Camera.pos.ToVector3(), cameraUp);
        }

        case SpriteType::CustomBillboard:
        {
            auto quadForward = Vector3(0.0f, 0.0f, 1.0f);
            return scaleMatrix * Matrix::CreateConstrainedBillboard(position, Camera.pos.ToVector3(), sprite.ConstrainAxis, nullptr, &quadForward);
        }

        case SpriteType::LookAtBillboard:
        {
            auto rotation = Matrix::CreateRotationZ(sprite.Rotation) * Matrix::CreateLookAt(Vector3::Zero, sprite.LookAtAxis, Vector3::UnitZ);
            return scaleMatrix * rotation * Matrix::CreateTranslation(position);
        }

        default:
            return Matrix::Identity;
        }
    }

    bool NextRenderer::CreateSpriteResources()
    {
        _spritesVertexShader = LoadShader(*_device, _shaderDirectory, "Sprites", "VS");
        _spritesPixelShader = LoadShader(*_device, _shaderDirectory, "Sprites", "PS");
        if (_spritesVertexShader == nullptr || _spritesPixelShader == nullptr)
            return false;

        _spriteCornerBuffer = _device->CreateBuffer(BufferDesc{ (int)sizeof(SPRITE_CORNER_INDICES), 0, BufferUsage::Vertex, "Sprite.Corners" });
        if (_spriteCornerBuffer == nullptr)
            return false;

        UploadBuffer(_spriteCornerBuffer.get(), SPRITE_CORNER_INDICES, (int)sizeof(SPRITE_CORNER_INDICES));

        auto samplerDesc = SamplerDesc{};
        samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = AddressMode::ClampToEdge;
        samplerDesc.Name = "LinearClamp";
        _linearClampSampler = _device->CreateSampler(samplerDesc);
        return (_linearClampSampler != nullptr);
    }

    // Sprites reference legacy atlas pages; the page index comes from the level sprite table.
    ITexture* NextRenderer::GetSpriteTexture(const RendererSprite* sprite) const
    {
        if (sprite == nullptr || _legacyGeometry.Sprites == nullptr)
            return nullptr;

        // Particles with the video sprite ID show the background video, with whole frame UVs.
        if (sprite == _videoSprite)
            return _isVideoActive ? _videoTexture.get() : nullptr;

        int spriteIndex = (int)(sprite - _legacyGeometry.Sprites->data());
        if (spriteIndex < 0 || spriteIndex >= (int)g_Level.Sprites.size())
            return nullptr;

        int page = g_Level.Sprites[spriteIndex].tile;
        if (page < 0 || page >= (int)_spriteTextures.size())
            return nullptr;

        return _spriteTextures[page].ColorMap.get();
    }

    // As in the legacy renderer, a mirror reflects the corners of 3D sprites, and the position of billboards,
    // which still face the camera.
    bool NextRenderer::AddSprite(const RendererSpriteToDraw& sprite, const RenderView& view, const RendererMirror* mirror)
    {
        if ((int)(_spriteData.size() / sizeof(GpuSprite)) >= INSTANCE_INDEX_COUNT)
            return false;

        Vector3 corners[4];
        if (sprite.Type == SpriteType::ThreeD)
        {
            corners[0] = sprite.vtx1;
            corners[1] = sprite.vtx2;
            corners[2] = sprite.vtx3;
            corners[3] = sprite.vtx4;
            if (mirror != nullptr)
            {
                for (auto& corner : corners)
                    corner = Vector3::Transform(corner, mirror->ReflectionMatrix);
            }
        }
        else
        {
            auto position = (mirror != nullptr) ? Vector3::Transform(sprite.pos, mirror->ReflectionMatrix) : sprite.pos;
            auto world = GetSpriteWorldMatrix(sprite, position, view);
            corners[0] = Vector3::Transform(Vector3(-0.5f, 0.5f, 0.0f), world);
            corners[1] = Vector3::Transform(Vector3(0.5f, 0.5f, 0.0f), world);
            corners[2] = Vector3::Transform(Vector3(0.5f, -0.5f, 0.0f), world);
            corners[3] = Vector3::Transform(Vector3(-0.5f, -0.5f, 0.0f), world);
        }

        const Vector4* colors[4] = { &sprite.c1, &sprite.c2, &sprite.c3, &sprite.c4 };
        const auto* uv = sprite.Sprite->UV;

        auto gpuSprite = GpuSprite{};
        for (int i = 0; i < 4; i++)
            gpuSprite.Corners[i] = GpuSpriteCorner{ corners[i], PackSpriteColor(*colors[i]) };

        gpuSprite.UVX = Vector4(uv[0].x, uv[1].x, uv[2].x, uv[3].x);
        gpuSprite.UVY = Vector4(uv[0].y, uv[1].y, uv[2].y, uv[3].y);

        const auto* bytes = (const unsigned char*)&gpuSprite;
        _spriteData.insert(_spriteData.end(), bytes, bytes + sizeof(gpuSprite));
        _spriteSources.push_back(&sprite);
        return true;
    }

    // Sprites the legacy renderer does not sort (opaque, alpha test, additive). The legacy SortAndPrepareSprites
    // already ordered them by sprite and blend mode, so consecutive sprites merge into few instanced draws.
    void NextRenderer::CollectSprites(const RenderView& view)
    {
        _spriteData.clear();
        _spriteSources.clear();
        _spriteDraws.clear();

        for (const auto& sprite : view.SpritesToDraw)
        {
            if (sprite.BlendMode != BlendMode::Opaque && sprite.BlendMode != BlendMode::AlphaTest &&
                sprite.BlendMode != BlendMode::Additive && sprite.BlendMode != BlendMode::Distortion)
            {
                continue;
            }

            auto* texture = GetSpriteTexture(sprite.Sprite);
            if (texture == nullptr)
                continue;

            int spriteIndex = (int)(_spriteData.size() / sizeof(GpuSprite));
            if (!AddSprite(sprite, view))
                break;

            const auto* last = _spriteDraws.empty() ? nullptr : &_spriteDraws.back();
            if (last == nullptr || last->Texture != texture || last->BlendMode != sprite.BlendMode ||
                last->IsSoftParticle != sprite.SoftParticle || last->RenderType != (int)sprite.renderType)
            {
                _spriteDraws.push_back(SpriteDraw{ texture, sprite.BlendMode, spriteIndex, 0, sprite.SoftParticle, (int)sprite.renderType });
            }

            _spriteDraws.back().SpriteCount++;
        }
    }

    // Additive runs of the classic sky sprites; they never belong to mirrors or sorted draws.
    void NextRenderer::CollectSkySprites(const RenderView& view, const FrameEnvironment& environment)
    {
        _skySpriteDraws.clear();
        _skySunDraws.clear();

        if (environment.SkySprites != nullptr)
        {
            for (const auto& sprite : *environment.SkySprites)
            {
                if (!AddSkySprite(sprite, view, _skySpriteDraws))
                    break;
            }
        }

        if (environment.SkySunSprite != nullptr)
            AddSkySprite(*environment.SkySunSprite, view, _skySunDraws);
    }

    // Returns false when the sprite buffer is full.
    bool NextRenderer::AddSkySprite(const RendererSpriteToDraw& sprite, const RenderView& view, std::vector<SpriteDraw>& draws)
    {
        auto* texture = GetSpriteTexture(sprite.Sprite);
        if (texture == nullptr)
            return true;

        int spriteIndex = (int)(_spriteData.size() / sizeof(GpuSprite));
        if (!AddSprite(sprite, view))
            return false;

        if (draws.empty() || draws.back().Texture != texture)
            draws.push_back(SpriteDraw{ texture, BlendMode::Additive, spriteIndex, 0 });

        draws.back().SpriteCount++;
        return true;
    }

    void NextRenderer::DrawSpriteRun(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const SpriteDraw& draw,
                                     const IGraphicsPipeline*& lastPipeline, const ITexture*& lastTexture)
    {
        auto* pipeline = GetScenePipeline(SceneShader::Sprites, draw.BlendMode);
        if (pipeline != lastPipeline)
        {
            renderPass.BindPipeline(pipeline);

            IBuffer* spriteBuffer = _spriteBuffer.Buffer.get();
            renderPass.BindVertexStorageBuffers(0, &spriteBuffer, 1);

            BufferBinding vertexBindings[2] = { { _spriteCornerBuffer.get(), 0 }, { _instanceIndexBuffer.get(), 0 } };
            renderPass.BindVertexBuffers(0, vertexBindings, 2);

            lastPipeline = pipeline;
            lastTexture = nullptr;
        }

        // Opaque and alpha test share a pipeline, so the threshold is pushed with every run.
        auto pixelUniforms = DrawPixelUniforms{};
        pixelUniforms.AlphaThreshold = (draw.BlendMode == BlendMode::AlphaTest) ? ALPHA_TEST_THRESHOLD : 0.0f;
        pixelUniforms.BlendMode = (unsigned int)draw.BlendMode;
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 1, &pixelUniforms, sizeof(pixelUniforms));

        if (draw.Texture != lastTexture)
        {
            TextureSamplerBinding bindings[2] =
            {
                { draw.Texture, _linearClampSampler.get() },
                { _sceneDepthCopy.get(), _pointClampSampler.get() }
            };
            renderPass.BindPixelSamplers(0, bindings, 2);
            lastTexture = draw.Texture;
        }

        auto spriteUniforms = SpriteUniforms{ draw.IsSoftParticle ? 1u : 0u, (unsigned int)draw.RenderType, _nearPlane, _farPlane, _time };
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 3, &spriteUniforms, sizeof(spriteUniforms));

        _frameStats.DrawCalls++;
        renderPass.Draw(6, draw.SpriteCount, 0, draw.FirstSprite);
    }

    // Opaque and alpha test sprites belong to the legacy opaque pass, where they also write depth.
    void NextRenderer::DrawSprites(ICommandBuffer& commandBuffer, IRenderPass& renderPass, ScenePass pass, bool isMirrored)
    {
        constexpr const char* GROUP_NAMES[] = { "Sprites prepass", "Opaque sprites", "Sprites alpha edges", "Additive sprites", "Distortion sprites" };

        const auto& draws = isMirrored ? _mirrorSpriteDraws : _spriteDraws;
        if (draws.empty())
            return;

        commandBuffer.PushDebugGroup(GROUP_NAMES[(int)pass]);

        const IGraphicsPipeline* lastPipeline = nullptr;
        const ITexture* lastTexture = nullptr;
        for (const auto& draw : draws)
        {
            bool isOpaque = (draw.BlendMode == BlendMode::Opaque || draw.BlendMode == BlendMode::AlphaTest);
            if (isOpaque ? (pass == ScenePass::Opaque) : (GetPassBlendMode(pass) == draw.BlendMode))
                DrawSpriteRun(commandBuffer, renderPass, draw, lastPipeline, lastTexture);
        }

        commandBuffer.PopDebugGroup();
    }
}
