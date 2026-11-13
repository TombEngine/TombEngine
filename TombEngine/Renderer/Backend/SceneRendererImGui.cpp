// ImGui debug overlay (not a frame pass): drawn last by FinishFrame, after the 2D overlay and the menu post-process,
// directly into the window resolution final target with the overlay alpha blend pipeline. The ImGui draw lists come
// from the UiFrame built by the frontend and are uploaded with the other overlay data.

#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include <imgui.h>

#include "Renderer/UI/ImGuiIntegration.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Backend
{
    static_assert(sizeof(ImDrawVert) == 20 && sizeof(ImDrawIdx) == 2, "ImGui vertices must match the overlay vertex layout.");

    // The font atlas built by ImGui, uploaded on first use and again when the overlay scale rebuilds it.
    bool SceneRenderer::EnsureImGuiFontTexture()
    {
        if (_imGuiFontTexture != nullptr && _imGuiFontGeneration == ImGuiGetFontGeneration())
            return true;

        _imGuiFontGeneration = ImGuiGetFontGeneration();

        unsigned char* pixels = nullptr;
        int width = 0;
        int height = 0;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        if (pixels == nullptr)
            return false;

        auto desc = TextureDesc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = PixelFormat::RGBA8Unorm;
        desc.Name = "ImGui.Font";
        _imGuiFontTexture = _device->CreateTexture(desc);
        if (_imGuiFontTexture == nullptr)
            return false;

        UploadTexture(_imGuiFontTexture.get(), pixels, width * height * 4);
        return true;
    }

    // Draw lists are concatenated into one vertex and one index buffer.
    void SceneRenderer::UploadImGuiData(const UiFrame& ui)
    {
        _imGuiVertexData.clear();
        _imGuiIndexData.clear();
        if (ui.DebugDrawData == nullptr || ui.DebugDrawData->TotalVtxCount == 0)
            return;

        for (int i = 0; i < ui.DebugDrawData->CmdListsCount; i++)
        {
            const auto* list = ui.DebugDrawData->CmdLists[i];
            const auto* vertices = (const unsigned char*)list->VtxBuffer.Data;
            const auto* indices = (const unsigned char*)list->IdxBuffer.Data;
            _imGuiVertexData.insert(_imGuiVertexData.end(), vertices, vertices + list->VtxBuffer.Size * sizeof(ImDrawVert));
            _imGuiIndexData.insert(_imGuiIndexData.end(), indices, indices + list->IdxBuffer.Size * sizeof(ImDrawIdx));
        }

        UploadFrameStorage(_imGuiVertexBuffer, _imGuiVertexData, (int)sizeof(ImDrawVert), BufferUsage::Vertex, "Frame.ImGuiVertices");
        UploadFrameStorage(_imGuiIndexBuffer, _imGuiIndexData, (int)sizeof(ImDrawIdx), BufferUsage::Index, "Frame.ImGuiIndices");
    }

    // Same pipeline as the 2D layer with straight alpha. ImGui coordinates follow its display size, so they are
    // scaled to the frame target.
    void SceneRenderer::DrawImGui(ICommandBuffer& commandBuffer, const UiFrame& ui)
    {
        const auto* drawData = ui.DebugDrawData;
        if (drawData == nullptr || _imGuiVertexData.empty() || drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f || !EnsureImGuiFontTexture())
            return;

        const auto& desc = _finalTarget->GetDesc();
        auto scale = Vector2(desc.Width / drawData->DisplaySize.x, desc.Height / drawData->DisplaySize.y);
        auto origin = Vector2(drawData->DisplayPos.x, drawData->DisplayPos.y);

        // The overlay vertex shader maps pixels with the inverse target size; ImGui pixels map with the display size.
        auto uniforms = OverlayUniforms{ Vector2(1.0f / drawData->DisplaySize.x, 1.0f / drawData->DisplaySize.y) };
        commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 0, &uniforms, sizeof(uniforms));

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _finalTarget.get();
        colorTarget.Load = LoadOp::Load;

        commandBuffer.PushDebugGroup("ImGui");
        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        renderPass->BindPipeline(GetOverlayPipeline(BlendMode::AlphaBlend));

        auto vertexBinding = BufferBinding{ _imGuiVertexBuffer.Buffer.get(), 0 };
        renderPass->BindVertexBuffers(0, &vertexBinding, 1);
        renderPass->BindIndexBuffer(BufferBinding{ _imGuiIndexBuffer.Buffer.get(), 0 }, IndexSize::Bits16);

        auto binding = TextureSamplerBinding{ _imGuiFontTexture.get(), _linearClampSampler.get() };
        renderPass->BindPixelSamplers(0, &binding, 1);

        // One indexed draw per command, clipped by its scissor rectangle in target pixels. Offsets advance per draw list
        // through the concatenated buffers.
        int vertexOffset = 0;
        int indexOffset = 0;
        for (int i = 0; i < drawData->CmdListsCount; i++)
        {
            const auto* list = drawData->CmdLists[i];
            for (const auto& command : list->CmdBuffer)
            {
                if (command.UserCallback != nullptr || command.ElemCount == 0)
                    continue;

                int left = std::max((int)((command.ClipRect.x - origin.x) * scale.x), 0);
                int top = std::max((int)((command.ClipRect.y - origin.y) * scale.y), 0);
                int right = std::min((int)((command.ClipRect.z - origin.x) * scale.x), desc.Width);
                int bottom = std::min((int)((command.ClipRect.w - origin.y) * scale.y), desc.Height);
                if (right <= left || bottom <= top)
                    continue;

                renderPass->SetScissor(ScissorRect{ left, top, right - left, bottom - top });

                CountDraw(DrawCategory::Overlay, (int)command.ElemCount / 3);
                renderPass->DrawIndexed((int)command.ElemCount, 1, indexOffset + (int)command.IdxOffset, vertexOffset + (int)command.VtxOffset, 0);
            }

            vertexOffset += list->VtxBuffer.Size;
            indexOffset += list->IdxBuffer.Size;
        }

        commandBuffer.EndRenderPass();
        commandBuffer.PopDebugGroup();
    }
}
