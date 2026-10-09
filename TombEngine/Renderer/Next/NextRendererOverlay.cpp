#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/Structures/RendererBucket.h"
#include "Renderer/Structures/RendererMesh.h"
#include "Renderer/Structures/RendererSprite.h"
#include "Specific/trutils.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Next
{
    // Same layout as OverlayUniforms in Shaders/RHI/Overlay.hlsl.
    struct OverlayUniforms
    {
        Vector2 InverseTargetSize = Vector2::Zero;
        Vector2 Padding           = Vector2::Zero;
    };

    // Legacy 2D and 3D space object draws blend alpha tested buckets, and every bucket of a fading object.
    static BlendMode GetOverlayObjectBlendMode(BlendMode blendMode, float alpha)
    {
        if (blendMode == BlendMode::AlphaTest)
            return BlendMode::AlphaBlend;

        if (alpha < ALPHA_BLEND_THRESHOLD && (blendMode == BlendMode::Opaque || blendMode == BlendMode::FastAlphaBlend))
            return BlendMode::AlphaBlend;

        return blendMode;
    }

    static unsigned int PackOverlayColor(const Vector4& color)
    {
        auto toByte = [](float value) { return (unsigned int)std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f); };
        return toByte(color.x) | (toByte(color.y) << 8) | (toByte(color.z) << 16) | (toByte(color.w) << 24);
    }

    // DirectXTK .spritefont: "DXTKfont", glyphs (character, subrect, offsets, advance), line spacing, default
    // character and one texture, as written by MakeSpriteFont and read by the legacy DX11SpriteFont.
    bool NextRenderer::LoadFont(const std::string& path)
    {
        auto file = std::ifstream(path, std::ios::binary);
        auto data = std::vector<unsigned char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        auto read = [&](size_t offset, void* destination, size_t size)
        {
            if (offset + size > data.size())
                return false;

            memcpy(destination, data.data() + offset, size);
            return true;
        };

        unsigned int glyphCount = 0;
        if (data.size() < 12 || memcmp(data.data(), "DXTKfont", 8) != 0 || !read(8, &glyphCount, 4))
        {
            TENLog("Next renderer: invalid font " + path, LogLevel::Error);
            return false;
        }

        _font = FontData{};
        size_t offset = 12;
        for (unsigned int i = 0; i < glyphCount; i++, offset += sizeof(FontGlyph))
        {
            auto glyph = FontGlyph{};
            if (!read(offset, &glyph, sizeof(glyph)))
                return false;

            _font.Glyphs[glyph.Character] = glyph;
        }

        unsigned int header[7] = {};
        if (!read(offset, header, sizeof(header)))
            return false;

        memcpy(&_font.LineSpacing, &header[0], sizeof(float));
        _font.DefaultCharacter = header[1];
        int width = (int)header[2];
        int height = (int)header[3];
        unsigned int dxgiFormat = header[4];
        int textureSize = (int)(header[5] * header[6]);
        offset += sizeof(header);

        constexpr auto DXGI_FORMAT_RGBA8 = 28u;
        constexpr auto DXGI_FORMAT_BC2 = 74u;
        constexpr auto DXGI_FORMAT_BC3 = 77u;

        auto desc = TextureDesc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = (dxgiFormat == DXGI_FORMAT_BC2) ? PixelFormat::BC2Unorm : ((dxgiFormat == DXGI_FORMAT_BC3) ? PixelFormat::BC3Unorm : PixelFormat::RGBA8Unorm);
        desc.Name = "Font";
        if ((dxgiFormat != DXGI_FORMAT_RGBA8 && dxgiFormat != DXGI_FORMAT_BC2 && dxgiFormat != DXGI_FORMAT_BC3) || offset + textureSize > data.size())
        {
            TENLog("Next renderer: unsupported font texture in " + path, LogLevel::Error);
            return false;
        }

        _font.Texture = _device->CreateTexture(desc);
        if (_font.Texture == nullptr)
            return false;

        UploadTexture(_font.Texture.get(), data.data() + offset, textureSize);
        return true;
    }

    bool NextRenderer::CreateOverlayResources(const std::string& fontPath)
    {
        _overlayVertexShader = LoadShader(*_device, _shaderDirectory, "Overlay", "VS");
        _overlayPixelShader = LoadShader(*_device, _shaderDirectory, "Overlay", "PS");
        _overlayBarPixelShader = LoadShader(*_device, _shaderDirectory, "Overlay", "PSBar");
        if (_overlayVertexShader == nullptr || _overlayPixelShader == nullptr || _overlayBarPixelShader == nullptr)
            return false;

        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = _overlayVertexShader.get();
        desc.PixelShader = _overlayPixelShader.get();
        desc.Layout.NumBuffers = 1;
        desc.Layout.Buffers[0] = VertexBufferLayout{ 0, (int)sizeof(OverlayVertex), VertexInputRate::Vertex };
        desc.Layout.Attributes[0] = VertexAttribute{ 0, 0, VertexElementFormat::Float2, (int)offsetof(OverlayVertex, Position) };
        desc.Layout.Attributes[1] = VertexAttribute{ 1, 0, VertexElementFormat::Float2, (int)offsetof(OverlayVertex, UV) };
        desc.Layout.Attributes[2] = VertexAttribute{ 2, 0, VertexElementFormat::UByte4Norm, (int)offsetof(OverlayVertex, Color) };
        desc.Layout.NumAttributes = 3;
        desc.Rasterizer.Cull = RHI::CullMode::None;
        desc.DepthStencil.EnableDepthTest = false;
        desc.DepthStencil.EnableDepthWrite = false;
        desc.NumColorTargets = 1;
        desc.ColorTargets[0].Format = _device->GetSwapchainFormat();

        // Colors are premultiplied, as with the legacy sprite batch.
        auto& blend = desc.ColorTargets[0].Blend;
        blend.EnableBlend = true;
        blend.SrcColor = blend.SrcAlpha = BlendFactor::One;
        blend.DstColor = blend.DstAlpha = BlendFactor::OneMinusSrcAlpha;
        desc.Name = "Overlay";
        _overlayPipeline = _device->CreateGraphicsPipeline(desc);
        _overlayPipelineDesc = desc;

        // Bars are opaque, as in the legacy renderer.
        desc.PixelShader = _overlayBarPixelShader.get();
        desc.ColorTargets[0].Blend = BlendState{};
        desc.Name = "Overlay.Bar";
        _overlayBarPipeline = _device->CreateGraphicsPipeline(desc);

        return (_overlayPipeline != nullptr && _overlayBarPipeline != nullptr && LoadFont(fontPath));
    }

    // Overlay pipelines share the layout and shaders of the text pipeline and differ by legacy blend mode.
    IGraphicsPipeline* NextRenderer::GetOverlayPipeline(BlendMode blendMode)
    {
        if (blendMode == BlendMode::PremultipliedAlphaBlend)
            return _overlayPipeline.get();

        auto& pipeline = _overlayPipelines[std::clamp((int)blendMode, 0, 15)];
        if (pipeline == nullptr)
        {
            auto desc = _overlayPipelineDesc;
            desc.ColorTargets[0].Blend = GetBlendState(blendMode);
            desc.Name = "Overlay.BlendMode" + std::to_string((int)blendMode);
            pipeline = _device->CreateGraphicsPipeline(desc);
        }

        return pipeline.get();
    }

    // Quad from four corners in target pixels, merged into the previous run when texture and blend mode match.
    void NextRenderer::AddOverlayCorners(ITexture* texture, const Vector2* corners, const Vector2* uvs, const Vector4& color, BlendMode blendMode)
    {
        unsigned int packedColor = PackOverlayColor(color);
        int firstVertex = (int)(_overlayVertexData.size() / sizeof(OverlayVertex));
        const int order[6] = { 0, 1, 2, 0, 2, 3 };
        for (int index : order)
        {
            auto vertex = OverlayVertex{ corners[index], uvs[index], packedColor };
            const auto* bytes = (const unsigned char*)&vertex;
            _overlayVertexData.insert(_overlayVertexData.end(), bytes, bytes + sizeof(OverlayVertex));
        }

        const auto* last = _overlayDraws.empty() ? nullptr : &_overlayDraws.back();
        if (last == nullptr || last->Texture != texture || last->IsBar || last->BlendMode != blendMode)
        {
            auto draw = OverlayDraw{ texture, firstVertex, 0 };
            draw.BlendMode = blendMode;
            _overlayDraws.push_back(draw);
        }

        _overlayDraws.back().VertexCount += 6;
    }

    void NextRenderer::AddOverlayQuad(ITexture* texture, const Vector2& position, const Vector2& size, const Vector2& uvMin, const Vector2& uvMax, const Vector4& color,
                                      BlendMode blendMode)
    {
        Vector2 corners[4] = { position, Vector2(position.x + size.x, position.y), position + size, Vector2(position.x, position.y + size.y) };
        Vector2 uvs[4] = { uvMin, Vector2(uvMax.x, uvMin.y), uvMax, Vector2(uvMin.x, uvMax.y) };
        AddOverlayCorners(texture, corners, uvs, color, blendMode);
    }

    // Display sprite of the legacy DrawDisplaySprites, from the 800x600 display space.
    void NextRenderer::AddOverlaySprite(const OverlaySprite& sprite)
    {
        constexpr auto DISPLAY_WIDTH = 800.0f;
        constexpr auto DISPLAY_HEIGHT = 600.0f;

        auto* texture = GetSpriteTexture(sprite.Sprite);
        if (texture == nullptr)
            return;

        const auto& desc = _finalTarget->GetDesc();
        auto displayToTarget = Vector2(desc.Width / DISPLAY_WIDTH, desc.Height / DISPLAY_HEIGHT);
        Vector2 corners[4] = {};
        for (int i = 0; i < 4; i++)
            corners[i] = sprite.Corners[i] * displayToTarget;

        AddOverlayCorners(texture, corners, sprite.Sprite->UV, sprite.Color, sprite.BlendMode);
    }

    // Legacy DrawOverlays: additive lightning flash, then the binocular or laser sight sprite fitted to the screen
    // with the red aiming point of the laser sight.
    void NextRenderer::AddScreenOverlays(const UiFrame& environment)
    {
        const auto& desc = _finalTarget->GetDesc();
        auto targetSize = Vector2((float)desc.Width, (float)desc.Height);

        if (environment.FlashColor != Vector3::Zero)
        {
            auto color = Vector4(environment.FlashColor.x, environment.FlashColor.y, environment.FlashColor.z, 1.0f);
            AddOverlayQuad(_whiteTexture.get(), Vector2::Zero, targetSize, Vector2::Zero, Vector2::One, color, BlendMode::Additive);
        }

        auto* opticTexture = GetSpriteTexture(environment.OpticSprite);
        if (opticTexture == nullptr)
            return;

        // Legacy DrawFullScreenSprite fits the laser sight sprite to the screen aspect; the binocular one is stretched.
        const auto& sprite = *environment.OpticSprite;
        auto uvStart = Vector2::Zero;
        auto uvEnd = Vector2::One;
        if (environment.IsLaserSight)
        {
            float screenAspect = targetSize.x / targetSize.y;
            float imageAspect = sprite.Width / (float)sprite.Height;
            float difference = (screenAspect > imageAspect) ? ((screenAspect - imageAspect) / screenAspect / 2.0f) : ((imageAspect - screenAspect) / imageAspect / 2.0f);
            auto& start = (screenAspect > imageAspect) ? uvStart.y : uvStart.x;
            auto& end = (screenAspect > imageAspect) ? uvEnd.y : uvEnd.x;
            start += difference;
            end -= difference;
        }

        auto scale = Vector2(sprite.Width / (float)opticTexture->GetDesc().Width, sprite.Height / (float)opticTexture->GetDesc().Height);
        AddOverlayQuad(opticTexture, Vector2::Zero, targetSize, uvStart * scale + sprite.UV[0], uvEnd * scale + sprite.UV[0], Vector4::One, BlendMode::AlphaBlend);

        // Aiming point: 4 by 4 pixels at the screen center, as the legacy NDC quad of half extent 4 / screen size.
        if (environment.IsLaserSight)
            AddOverlayQuad(_whiteTexture.get(), targetSize / 2.0f - Vector2(2.0f), Vector2(4.0f), Vector2::Zero, Vector2::One, Vector4(1.0f, 0.0f, 0.0f, 1.0f), BlendMode::Opaque);
    }

    // Glyph layout of DirectXTK SpriteFont::DrawString without rotation: offsets accumulate per glyph, whitespace
    // only advances and new lines restart at the line spacing.
    void NextRenderer::AddOverlayText(const OverlayText& text, const Vector2& scale)
    {
        const auto& font = _font;
        if (font.Texture == nullptr)
            return;

        float inverseWidth = 1.0f / font.Texture->GetDesc().Width;
        float inverseHeight = 1.0f / font.Texture->GetDesc().Height;

        float x = 0.0f;
        float y = 0.0f;
        for (wchar_t character : TEN::Utils::ToWString(text.Text))
        {
            if (character == L'\r')
                continue;

            if (character == L'\n')
            {
                x = 0.0f;
                y += font.LineSpacing;
                continue;
            }

            auto it = font.Glyphs.find((unsigned int)character);
            if (it == font.Glyphs.end())
                it = font.Glyphs.find(font.DefaultCharacter);

            if (it == font.Glyphs.end())
                continue;

            const auto& glyph = it->second;
            x = std::max(x + glyph.XOffset, 0.0f);

            float width = (float)(glyph.Right - glyph.Left);
            float height = (float)(glyph.Bottom - glyph.Top);
            if (!iswspace(character) || width > 1.0f || height > 1.0f)
            {
                auto position = Vector2(text.Position.x * scale.x + x * text.Scale * scale.y, (text.Position.y + (y + glyph.YOffset) * text.Scale) * scale.y);
                auto size = Vector2(width, height) * text.Scale * scale.y;
                AddOverlayQuad(font.Texture.get(), position, size, Vector2(glyph.Left * inverseWidth, glyph.Top * inverseHeight),
                    Vector2(glyph.Right * inverseWidth, glyph.Bottom * inverseHeight), text.Color);
            }

            x += width + glyph.XAdvance;
        }
    }

    // Legacy DrawBar: a 9 slice border from the border sprite and the inner sprite clipped at the percentage. The
    // display space layout of RendererHudBar and GetHudBarTransform is rebuilt here for this target's aspect.
    void NextRenderer::AddOverlayBar(const OverlayBar& bar)
    {
        constexpr auto DISPLAY_WIDTH = 800.0f;
        constexpr auto DISPLAY_HEIGHT = 600.0f;
        constexpr auto DISPLAY_ASPECT = DISPLAY_WIDTH / DISPLAY_HEIGHT;
        constexpr auto BORDER_SIZE = 1.0f;

        auto* borderTexture = GetSpriteTexture(bar.Border);
        auto* innerTexture = GetSpriteTexture(bar.Inner);
        if (borderTexture == nullptr || innerTexture == nullptr)
            return;

        const auto& desc = _finalTarget->GetDesc();
        float aspect = (float)desc.Width / (float)desc.Height;
        auto scale = Vector2::One;
        auto sourcePivot = bar.Position;
        auto targetPivot = bar.Position;
        if (aspect >= 1.0f)
        {
            scale.y = aspect / DISPLAY_ASPECT;
        }
        else
        {
            scale.x = DISPLAY_ASPECT / aspect;
            sourcePivot = Vector2(bar.Position.x + bar.Size.x * 0.5f, bar.Position.y);
            targetPivot = bar.VerticalPosition;
        }

        auto toTarget = [&](float x, float y)
        {
            auto point = (Vector2(x, y) - sourcePivot) * scale + targetPivot;
            return Vector2(point.x * desc.Width / DISPLAY_WIDTH, point.y * desc.Height / DISPLAY_HEIGHT);
        };

        auto getSpriteUniforms = [](const RendererSprite& sprite, ITexture& texture, bool isInner)
        {
            auto uniforms = OverlayBarUniforms{};
            uniforms.BarStartUV = sprite.UV[0];
            uniforms.BarScale = Vector2(sprite.Width / (float)texture.GetDesc().Width, sprite.Height / (float)texture.GetDesc().Height);
            uniforms.IsInner = isInner ? 1 : 0;
            return uniforms;
        };

        auto addQuad = [&](ITexture* texture, const OverlayBarUniforms& uniforms, const Vector2& min, const Vector2& max, const Vector2& uvMin, const Vector2& uvMax)
        {
            int firstVertex = (int)(_overlayVertexData.size() / sizeof(OverlayVertex));
            auto topLeft = toTarget(min.x, min.y);
            AddOverlayQuad(texture, topLeft, toTarget(max.x, max.y) - topLeft, uvMin, uvMax, Vector4::One);

            // Each bar part is its own draw with its uniforms.
            _overlayDraws.back() = OverlayDraw{ texture, firstVertex, 6, true, uniforms };
        };

        // Border: 9 slices with the UV split of RendererHudBar.
        auto border = Vector2(BORDER_SIZE * DISPLAY_ASPECT, BORDER_SIZE);
        float xs[4] = { bar.Position.x - border.x, bar.Position.x, bar.Position.x + bar.Size.x, bar.Position.x + bar.Size.x + border.x };
        float ys[4] = { bar.Position.y - border.y, bar.Position.y, bar.Position.y + bar.Size.y, bar.Position.y + bar.Size.y + border.y };
        const float uvs[4] = { 0.0f, 0.25f, 0.75f, 1.0f };

        auto borderUniforms = getSpriteUniforms(*bar.Border, *borderTexture, false);
        for (int row = 0; row < 3; row++)
        {
            for (int column = 0; column < 3; column++)
            {
                addQuad(borderTexture, borderUniforms, Vector2(xs[column], ys[row]), Vector2(xs[column + 1], ys[row + 1]),
                    Vector2(uvs[column], uvs[row]), Vector2(uvs[column + 1], uvs[row + 1]));
            }
        }

        // Inner bar, clipped in the shader at the percentage.
        auto innerUniforms = getSpriteUniforms(*bar.Inner, *innerTexture, true);
        innerUniforms.Percent = bar.Percent;
        innerUniforms.PoisonFactor = bar.IsPoisoned ? (std::sin((bar.Frame % 30) / 30.0f * PI_MUL_2) * 0.5f + 0.5f) : 0.0f;
        addQuad(innerTexture, innerUniforms, bar.Position, bar.Position + bar.Size, Vector2::Zero, Vector2::One);
    }

    // Image files of the legacy 2D layer are decoded on first use and kept for the session.
    void NextRenderer::LoadOverlayImages(const UiFrame& ui)
    {
        for (const auto& image : ui.Images)
        {
            if (image.Path == OVERLAY_VIDEO_PATH || _overlayImages.find(image.Path) != _overlayImages.end())
                continue;

            auto file = std::ifstream(image.Path, std::ios::binary);
            auto data = std::vector<unsigned char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            // No mips: averaging straight alpha texels brightens transparent edges, unlike the legacy sprite batch.
            auto texture = CreateLevelTexture(data, "Overlay.Image", false);
            if (texture == nullptr)
                TENLog("Next renderer: cannot load image " + image.Path, LogLevel::Warning);

            _overlayImages[image.Path] = std::move(texture);
        }
    }

    ITexture* NextRenderer::GetOverlayImageTexture(const std::string& path) const
    {
        if (path == OVERLAY_VIDEO_PATH)
            return (_isVideoActive ? _videoTexture.get() : nullptr);

        auto it = _overlayImages.find(path);
        return ((it != _overlayImages.end()) ? it->second.get() : nullptr);
    }

    // Presented frame: the last scene faded as the legacy menu backgrounds, then the 2D layer in the legacy draw
    // order. Legacy string coordinates are in its own screen resolution.
    void NextRenderer::CollectOverlay(const UiFrame& ui)
    {
        _overlayVertexData.clear();
        _overlayDraws.clear();
        _overlayObjectDraws.clear();
        _overlayInstanceData.clear();
        _overlayBoneData.clear();

        const auto& desc = _finalTarget->GetDesc();
        auto targetSize = Vector2((float)desc.Width, (float)desc.Height);
        if (_hasScene && ui.BackgroundFade > 0.0f)
        {
            auto color = Vector4(ui.BackgroundFade, ui.BackgroundFade, ui.BackgroundFade, 1.0f);
            AddOverlayQuad(_colorTarget.get(), Vector2::Zero, targetSize, Vector2::Zero, Vector2::One, color, BlendMode::Opaque);
        }

        if (ui.ScreenWidth <= 0 || ui.ScreenHeight <= 0)
            return;

        auto textScale = Vector2(targetSize.x / ui.ScreenWidth, targetSize.y / ui.ScreenHeight);
        for (const auto& layer : ui.Layers)
        {
            int end = layer.First + layer.Count;
            switch (layer.Type)
            {
            case OverlayLayerType::Texts:
                for (int i = layer.First; i < end && i < (int)ui.Texts.size(); i++)
                    AddOverlayText(ui.Texts[i], textScale);

                break;

            case OverlayLayerType::Bars:
                for (int i = layer.First; i < end && i < (int)ui.Bars.size(); i++)
                    AddOverlayBar(ui.Bars[i]);

                break;

            case OverlayLayerType::Sprites:
                for (int i = layer.First; i < end && i < (int)ui.Sprites.size(); i++)
                    AddOverlaySprite(ui.Sprites[i]);

                break;

            case OverlayLayerType::Images:
                for (int i = layer.First; i < end && i < (int)ui.Images.size(); i++)
                {
                    const auto& image = ui.Images[i];
                    auto* texture = GetOverlayImageTexture(image.Path);
                    if (texture != nullptr)
                        AddOverlayQuad(texture, image.Min * targetSize, (image.Max - image.Min) * targetSize, image.UVMin, image.UVMax, image.Color, image.BlendMode);
                }

                break;

            case OverlayLayerType::ScreenOverlays:
                AddScreenOverlays(ui);
                break;

            case OverlayLayerType::Objects:
                AddOverlayObjects(ui, layer);
                break;
            }
        }
    }

    // Objects of one layer share a depth buffer cleared before them, as the legacy depth clear before menus and
    // display items. Buckets follow the legacy order: per mesh, static buckets before animated ones.
    void NextRenderer::AddOverlayObjects(const UiFrame& ui, const OverlayLayer& layer)
    {
        if (!_isLevelLoaded)
            return;

        int firstDraw = (int)_overlayObjectDraws.size();
        int end = std::min(layer.First + layer.Count, (int)ui.Objects.size());
        for (int objectIndex = layer.First; objectIndex < end; objectIndex++)
        {
            const auto& object = ui.Objects[objectIndex];
            int meshEnd = std::min(object.FirstMesh + object.MeshCount, (int)ui.ObjectMeshes.size());
            for (int meshIndex = object.FirstMesh; meshIndex < meshEnd; meshIndex++)
            {
                const auto& mesh = ui.ObjectMeshes[meshIndex];
                if (mesh.Mesh == nullptr)
                    continue;

                int boneOffset = (int)(_overlayBoneData.size() / sizeof(Matrix));
                int boneCount = std::clamp((int)ui.ObjectBones.size() - mesh.FirstBone, 0, mesh.BoneCount);
                if (boneCount > 0)
                {
                    const auto* bones = (const unsigned char*)(ui.ObjectBones.data() + mesh.FirstBone);
                    _overlayBoneData.insert(_overlayBoneData.end(), bones, bones + (boneCount * sizeof(Matrix)));
                }

                auto instance = GpuObjectInstance{};
                instance.World = mesh.World;
                instance.Color = object.Color;
                instance.AmbientLight = object.AmbientLight;
                instance.Skinning = ((unsigned int)boneOffset & 0xFFFFFF) | ((unsigned int)((boneCount > 0) ? SKINNING_FULL : SKINNING_NONE) << 24);

                int instanceIndex = (int)(_overlayInstanceData.size() / sizeof(GpuObjectInstance));
                const auto* bytes = (const unsigned char*)&instance;
                _overlayInstanceData.insert(_overlayInstanceData.end(), bytes, bytes + sizeof(instance));

                for (int animated = 0; animated < 2; animated++)
                {
                    for (const auto& bucket : mesh.Mesh->Buckets)
                    {
                        if (bucket.Animated != (animated == 1) || bucket.NumIndices <= 0)
                            continue;

                        bool isAlphaTested = object.IsAlphaTested && (bucket.BlendMode == BlendMode::AlphaTest);
                        _overlayObjectDraws.push_back(OverlayObjectDraw{ &bucket, instanceIndex, objectIndex, GetOverlayObjectBlendMode(bucket.BlendMode, object.Color.w), isAlphaTested });
                    }
                }
            }
        }

        int drawCount = (int)_overlayObjectDraws.size() - firstDraw;
        if (drawCount == 0)
            return;

        auto draw = OverlayDraw{};
        draw.FirstVertex = firstDraw;
        draw.VertexCount = drawCount;
        draw.IsObjects = true;
        _overlayDraws.push_back(draw);
    }

    // Object draws read the material, light and decal buffers of the scene; frames without a scene fill them here.
    void NextRenderer::UploadOverlayData(ICopyPass& copyPass, bool hasScene)
    {
        UploadFrameStorage(copyPass, _overlayVertexBuffer, _overlayVertexData, (int)sizeof(OverlayVertex), BufferUsage::Vertex, "Frame.OverlayVertices");
        if (_overlayObjectDraws.empty())
            return;

        UploadFrameStorage(copyPass, _overlayInstanceBuffer, _overlayInstanceData, (int)sizeof(GpuObjectInstance), BufferUsage::GraphicsStorageRead, "Frame.OverlayInstances");
        UploadFrameStorage(copyPass, _overlayBoneBuffer, _overlayBoneData, (int)sizeof(Matrix), BufferUsage::GraphicsStorageRead, "Frame.OverlayBones");
        if (hasScene)
            return;

        CollectMaterials(0.0f);
        UploadFrameStorage(copyPass, _materialBuffer, _materialData, (int)sizeof(GpuMaterial), BufferUsage::GraphicsStorageRead, "Frame.Materials");

        // Created empty if no scene was rendered yet.
        auto noData = std::vector<unsigned char>{};
        if (_lightBuffer.Buffer == nullptr)
            UploadFrameStorage(copyPass, _lightBuffer, noData, (int)sizeof(GpuLight), BufferUsage::GraphicsStorageRead, "Frame.Lights");

        if (_decalBuffer.Buffer == nullptr)
            UploadFrameStorage(copyPass, _decalBuffer, noData, (int)sizeof(GpuDecal), BufferUsage::GraphicsStorageRead, "Frame.Decals");
    }

    // Quad runs up to the next object range, in one pass; returns the index of the first draw not drawn.
    int NextRenderer::DrawOverlayQuads(ICommandBuffer& commandBuffer, int firstDraw, bool isFirstPass)
    {
        // Object passes overwrite vertex slot 0.
        const auto& desc = _finalTarget->GetDesc();
        auto uniforms = OverlayUniforms{ Vector2(1.0f / desc.Width, 1.0f / desc.Height) };
        commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 0, &uniforms, sizeof(uniforms));

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _finalTarget.get();
        colorTarget.Load = isFirstPass ? LoadOp::Clear : LoadOp::Load;
        colorTarget.ClearColor[3] = 1.0f;

        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        auto vertexBinding = BufferBinding{ _overlayVertexBuffer.Buffer.get(), 0 };

        const IGraphicsPipeline* lastPipeline = nullptr;
        int drawIndex = firstDraw;
        for (; drawIndex < (int)_overlayDraws.size(); drawIndex++)
        {
            const auto& draw = _overlayDraws[drawIndex];
            if (draw.IsObjects)
                break;

            auto* pipeline = draw.IsBar ? _overlayBarPipeline.get() : GetOverlayPipeline(draw.BlendMode);
            if (pipeline != lastPipeline)
            {
                renderPass->BindPipeline(pipeline);
                renderPass->BindVertexBuffers(0, &vertexBinding, 1);
                lastPipeline = pipeline;
            }

            if (draw.IsBar)
                commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 0, &draw.Bar, sizeof(draw.Bar));

            auto binding = TextureSamplerBinding{ draw.Texture, _linearClampSampler.get() };
            renderPass->BindPixelSamplers(0, &binding, 1);

            _frameStats.DrawCalls++;
            renderPass->Draw(draw.VertexCount, 1, draw.FirstVertex, 0);
        }

        commandBuffer.EndRenderPass();
        return drawIndex;
    }

    // Object range in its own pass over the final target, with the scene depth target reused as a cleared depth buffer.
    void NextRenderer::DrawOverlayObjects(ICommandBuffer& commandBuffer, const UiFrame& ui, const OverlayDraw& draw, bool isFirstPass)
    {
        const auto& desc = _finalTarget->GetDesc();

        // Every uniform slot of the object shaders holds data before the first draw, as in RenderFrame.
        auto drawVertexUniforms = DrawVertexUniforms{ NO_VALUE };
        auto skyVertexUniforms = SkyVertexUniforms{};
        auto fogBulbUniforms = FogBulbUniforms{};
        auto drawPixelUniforms = DrawPixelUniforms{};
        auto framePixelUniforms = FramePixelUniforms{};
        framePixelUniforms.CameraDirection = -Vector3::UnitZ;
        framePixelUniforms.Gamma = ui.Gamma;
        framePixelUniforms.InverseViewportSize = Vector2(1.0f / desc.Width, 1.0f / desc.Height);
        commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 1, &drawVertexUniforms, sizeof(drawVertexUniforms));
        commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 2, &skyVertexUniforms, sizeof(skyVertexUniforms));
        commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 3, &fogBulbUniforms, sizeof(fogBulbUniforms));
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 0, &framePixelUniforms, sizeof(framePixelUniforms));
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 1, &drawPixelUniforms, sizeof(drawPixelUniforms));
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &_shadowUniforms, sizeof(_shadowUniforms));

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _finalTarget.get();
        colorTarget.Load = isFirstPass ? LoadOp::Clear : LoadOp::Load;
        colorTarget.ClearColor[3] = 1.0f;

        auto depthTarget = DepthTargetInfo{};
        depthTarget.Texture = _depthTarget.get();
        depthTarget.Load = LoadOp::Clear;
        depthTarget.Store = StoreOp::DontCare;
        depthTarget.ClearDepth = 1.0f;

        commandBuffer.PushDebugGroup("Overlay objects");
        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, &depthTarget);

        IBuffer* storageBuffers[4] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get(), _overlayInstanceBuffer.Buffer.get(), _overlayBoneBuffer.Buffer.get() };
        BufferBinding vertexBindings[2] = { { _moveableGeometry.Vertices.get(), 0 }, { _instanceIndexBuffer.get(), 0 } };

        const IGraphicsPipeline* lastPipeline = nullptr;
        int lastObject = NO_VALUE;
        auto state = BucketBindState{};

        for (int i = draw.FirstVertex; i < draw.FirstVertex + draw.VertexCount; i++)
        {
            const auto& objectDraw = _overlayObjectDraws[i];
            const auto& object = ui.Objects[objectDraw.Object];

            auto* pipeline = GetScenePipeline(SceneShader::Inventory, objectDraw.BlendMode);
            if (pipeline == nullptr)
                continue;

            if (pipeline != lastPipeline)
            {
                renderPass->BindPipeline(pipeline);
                renderPass->BindVertexStorageBuffers(0, storageBuffers, 4);
                BindScenePixelStorage(*renderPass);
                renderPass->BindVertexBuffers(0, vertexBindings, 2);
                renderPass->BindIndexBuffer(BufferBinding{ _moveableGeometry.Indices.get(), 0 }, IndexSize::Bits32);
                lastPipeline = pipeline;
                state = BucketBindState{};
            }

            if (objectDraw.Object != lastObject)
            {
                auto frameVertexUniforms = FrameVertexUniforms{};
                frameVertexUniforms.ViewProjection = object.ViewProjection;
                frameVertexUniforms.Frame = object.Frame;
                frameVertexUniforms.InterpolatedFrame = object.InterpolatedFrame;
                commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 0, &frameVertexUniforms, sizeof(frameVertexUniforms));
                lastObject = objectDraw.Object;
            }

            auto drawInfo = BucketDrawInfo{};
            drawInfo.IsAlphaTestDisabled = !objectDraw.IsAlphaTested;
            BindBucket(commandBuffer, *renderPass, *objectDraw.Bucket, _moveableTextures, objectDraw.BlendMode, 0, drawInfo, state);

            _frameStats.DrawCalls++;
            renderPass->DrawIndexed(objectDraw.Bucket->NumIndices, 1, objectDraw.Bucket->StartIndex, 0, objectDraw.Instance);
        }

        commandBuffer.EndRenderPass();
        commandBuffer.PopDebugGroup();
    }

    // Quad runs and object ranges in the legacy order; the first pass clears the final target.
    void NextRenderer::RenderOverlay(ICommandBuffer& commandBuffer, const UiFrame& ui)
    {
        commandBuffer.PushDebugGroup("Overlay");

        int drawIndex = 0;
        bool isFirstPass = true;
        do
        {
            if (drawIndex < (int)_overlayDraws.size() && _overlayDraws[drawIndex].IsObjects)
            {
                DrawOverlayObjects(commandBuffer, ui, _overlayDraws[drawIndex], isFirstPass);
                drawIndex++;
            }
            else
            {
                drawIndex = DrawOverlayQuads(commandBuffer, drawIndex, isFirstPass);
            }

            isFirstPass = false;
        }
        while (drawIndex < (int)_overlayDraws.size());

        commandBuffer.PopDebugGroup();
    }
}
