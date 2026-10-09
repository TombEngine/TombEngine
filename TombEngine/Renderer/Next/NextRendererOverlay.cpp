#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/RHI/ShaderLibrary.h"
#include "Specific/trutils.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Next
{
    // Same layout as OverlayUniforms in Shaders/RHI/Overlay.hlsl.
    struct OverlayUniforms
    {
        Vector2 InverseTargetSize = Vector2::Zero;
        Vector2 Padding           = Vector2::Zero;
    };

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
        if (_overlayVertexShader == nullptr || _overlayPixelShader == nullptr)
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

        return (_overlayPipeline != nullptr && LoadFont(fontPath));
    }

    // Quad in target pixels with texture coordinates, merged into the previous run when the texture matches.
    void NextRenderer::AddOverlayQuad(ITexture* texture, const Vector2& position, const Vector2& size, const Vector2& uvMin, const Vector2& uvMax, const Vector4& color)
    {
        unsigned int packedColor = PackOverlayColor(color);
        OverlayVertex corners[4] =
        {
            { position, uvMin, packedColor },
            { Vector2(position.x + size.x, position.y), Vector2(uvMax.x, uvMin.y), packedColor },
            { position + size, uvMax, packedColor },
            { Vector2(position.x, position.y + size.y), Vector2(uvMin.x, uvMax.y), packedColor }
        };

        int firstVertex = (int)(_overlayVertexData.size() / sizeof(OverlayVertex));
        const int order[6] = { 0, 1, 2, 0, 2, 3 };
        for (int index : order)
        {
            const auto* bytes = (const unsigned char*)&corners[index];
            _overlayVertexData.insert(_overlayVertexData.end(), bytes, bytes + sizeof(OverlayVertex));
        }

        if (_overlayDraws.empty() || _overlayDraws.back().Texture != texture)
            _overlayDraws.push_back(OverlayDraw{ texture, firstVertex, 0 });

        _overlayDraws.back().VertexCount += 6;
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

    // 2D layer of the frame. Legacy coordinates are in its own screen resolution and are scaled to this target.
    void NextRenderer::CollectOverlay(const FrameEnvironment& environment)
    {
        _overlayVertexData.clear();
        _overlayDraws.clear();

        if (environment.Texts == nullptr || environment.ScreenWidth <= 0 || environment.ScreenHeight <= 0 || _colorTarget == nullptr)
            return;

        const auto& desc = _colorTarget->GetDesc();
        auto scale = Vector2((float)desc.Width / environment.ScreenWidth, (float)desc.Height / environment.ScreenHeight);
        for (const auto& text : *environment.Texts)
            AddOverlayText(text, scale);
    }

    void NextRenderer::RenderOverlay(ICommandBuffer& commandBuffer)
    {
        if (_overlayDraws.empty())
            return;

        const auto& desc = _colorTarget->GetDesc();
        auto uniforms = OverlayUniforms{ Vector2(1.0f / desc.Width, 1.0f / desc.Height) };
        commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 0, &uniforms, sizeof(uniforms));

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _colorTarget.get();
        colorTarget.Load = LoadOp::Load;

        commandBuffer.PushDebugGroup("Overlay");
        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        renderPass->BindPipeline(_overlayPipeline.get());

        auto vertexBinding = BufferBinding{ _overlayVertexBuffer.Buffer.get(), 0 };
        renderPass->BindVertexBuffers(0, &vertexBinding, 1);

        for (const auto& draw : _overlayDraws)
        {
            auto binding = TextureSamplerBinding{ draw.Texture, _linearClampSampler.get() };
            renderPass->BindPixelSamplers(0, &binding, 1);

            _frameStats.DrawCalls++;
            renderPass->Draw(draw.VertexCount, 1, draw.FirstVertex, 0);
        }

        commandBuffer.EndRenderPass();
        commandBuffer.PopDebugGroup();
    }
}
