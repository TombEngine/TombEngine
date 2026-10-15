#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Math/Utils.h"
#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/Structures/RendererLine3D.h"
#include "Renderer/Structures/RendererTriangle3D.h"

using namespace TEN::Math;
using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Backend
{
    // Additive, depth tested and without culling.
    bool SceneRenderer::CreateDebugPrimitiveResources()
    {
        _debugVertexShader = LoadShader(*_device, _shaderDirectory, "DebugPrimitives", "VS");
        _debugPixelShader = LoadShader(*_device, _shaderDirectory, "DebugPrimitives", "PS");
        if (_debugVertexShader == nullptr || _debugPixelShader == nullptr)
            return false;

        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = _debugVertexShader.get();
        desc.PixelShader = _debugPixelShader.get();
        desc.Layout.NumBuffers = 1;
        desc.Layout.Buffers[0] = VertexBufferLayout{ 0, (int)sizeof(DebugVertex), VertexInputRate::Vertex };
        desc.Layout.Attributes[0] = VertexAttribute{ 0, 0, VertexElementFormat::Float3, (int)offsetof(DebugVertex, Position) };
        desc.Layout.Attributes[1] = VertexAttribute{ 1, 0, VertexElementFormat::UByte4Norm, (int)offsetof(DebugVertex, Color) };
        desc.Layout.NumAttributes = 2;
        desc.Rasterizer.Cull = RHI::CullMode::None;
        desc.DepthStencil.EnableDepthTest = true;
        desc.DepthStencil.EnableDepthWrite = false;
        desc.DepthFormat = PixelFormat::D32Float;
        desc.NumColorTargets = 1;
        desc.ColorTargets[0].Format = _device->GetSwapchainFormat();
        desc.ColorTargets[0].Blend = GetBlendState(BlendMode::Additive);

        desc.Primitive = RHI::PrimitiveType::LineList;
        desc.Name = "DebugLines";
        _debugLinePipeline = _device->CreateGraphicsPipeline(desc);

        desc.Primitive = RHI::PrimitiveType::TriangleList;
        desc.Name = "DebugTriangles";
        _debugTrianglePipeline = _device->CreateGraphicsPipeline(desc);

        return (_debugLinePipeline != nullptr && _debugTrianglePipeline != nullptr);
    }

    // Lines first, then triangles, in one frame vertex buffer. Collision mesh pages send hundreds of thousands of
    // primitives, so the vertices are written in place.
    void SceneRenderer::CollectDebugPrimitives(const FrameEnvironment& environment)
    {
        int lineCount = (environment.DebugLines != nullptr) ? (int)environment.DebugLines->size() : 0;
        int triangleCount = (environment.DebugTriangles != nullptr) ? (int)environment.DebugTriangles->size() : 0;
        _debugLineVertexCount = lineCount * 2;
        _debugTriangleVertexCount = triangleCount * 3;

        _debugVertexData.resize((_debugLineVertexCount + _debugTriangleVertexCount) * sizeof(DebugVertex));
        auto* vertex = (DebugVertex*)_debugVertexData.data();

        for (int i = 0; i < lineCount; i++)
        {
            const auto& line = (*environment.DebugLines)[i];
            unsigned int color = VectorColorToRGBA(line.Color);
            *vertex++ = DebugVertex{ line.Origin, color };
            *vertex++ = DebugVertex{ line.Target, color };
        }

        for (int i = 0; i < triangleCount; i++)
        {
            const auto& triangle = (*environment.DebugTriangles)[i];
            unsigned int color = VectorColorToRGBA(triangle.Color);
            for (const auto& position : triangle.Vertices)
                *vertex++ = DebugVertex{ position, color };
        }
    }

    void SceneRenderer::DrawDebugPrimitives(ICommandBuffer& commandBuffer, IRenderPass& renderPass)
    {
        if (_debugLineVertexCount == 0 && _debugTriangleVertexCount == 0)
            return;

        commandBuffer.PushDebugGroup("Debug primitives");
        commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 0, &_frameVertexUniforms, sizeof(_frameVertexUniforms));

        auto vertexBinding = BufferBinding{ _debugVertexBuffer.Buffer.get(), 0 };
        if (_debugLineVertexCount > 0)
        {
            renderPass.BindPipeline(_debugLinePipeline.get());
            renderPass.BindVertexBuffers(0, &vertexBinding, 1);
            CountDraw(DrawCategory::Debug, 0);
            renderPass.Draw(_debugLineVertexCount, 1, 0, 0);
        }

        if (_debugTriangleVertexCount > 0)
        {
            renderPass.BindPipeline(_debugTrianglePipeline.get());
            renderPass.BindVertexBuffers(0, &vertexBinding, 1);
            CountDraw(DrawCategory::Debug, _debugTriangleVertexCount / 3);
            renderPass.Draw(_debugTriangleVertexCount, 1, _debugLineVertexCount, 0);
        }

        commandBuffer.PopDebugGroup();
    }
}
