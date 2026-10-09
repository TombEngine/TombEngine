#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Math/Utils.h"
#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/Structures/RendererLine3D.h"
#include "Renderer/Structures/RendererTriangle3D.h"

using namespace TEN::Math;
using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Next
{
    // Additive, depth tested and without culling, as the legacy DrawLines3D and DrawTriangles3D.
    bool NextRenderer::CreateDebugPrimitiveResources()
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

    // Lines first, then triangles, in one frame vertex buffer.
    void NextRenderer::CollectDebugPrimitives(const FrameEnvironment& environment)
    {
        _debugVertexData.clear();
        _debugLineVertexCount = 0;
        _debugTriangleVertexCount = 0;

        auto addVertex = [this](const Vector3& position, unsigned int color)
        {
            auto vertex = DebugVertex{ position, color };
            const auto* bytes = (const unsigned char*)&vertex;
            _debugVertexData.insert(_debugVertexData.end(), bytes, bytes + sizeof(vertex));
        };

        if (environment.DebugLines != nullptr)
        {
            for (const auto& line : *environment.DebugLines)
            {
                unsigned int color = VectorColorToRGBA(line.Color);
                addVertex(line.Origin, color);
                addVertex(line.Target, color);
            }

            _debugLineVertexCount = (int)environment.DebugLines->size() * 2;
        }

        if (environment.DebugTriangles != nullptr)
        {
            for (const auto& triangle : *environment.DebugTriangles)
            {
                unsigned int color = VectorColorToRGBA(triangle.Color);
                for (const auto& vertex : triangle.Vertices)
                    addVertex(vertex, color);
            }

            _debugTriangleVertexCount = (int)environment.DebugTriangles->size() * 3;
        }
    }

    void NextRenderer::DrawDebugPrimitives(ICommandBuffer& commandBuffer, IRenderPass& renderPass)
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
            _frameStats.DrawCalls++;
            renderPass.Draw(_debugLineVertexCount, 1, 0, 0);
        }

        if (_debugTriangleVertexCount > 0)
        {
            renderPass.BindPipeline(_debugTrianglePipeline.get());
            renderPass.BindVertexBuffers(0, &vertexBinding, 1);
            _frameStats.DrawCalls++;
            renderPass.Draw(_debugTriangleVertexCount, 1, _debugLineVertexCount, 0);
        }

        commandBuffer.PopDebugGroup();
    }
}
