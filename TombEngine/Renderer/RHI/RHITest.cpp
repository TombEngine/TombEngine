#include "framework.h"
#include "Renderer/RHI/RHITest.h"

#include "Renderer/RHI/RHIDevice.h"
#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/RHI/TextureCapture.h"

namespace TEN::Renderer::RHI
{
    constexpr auto TEST_INSTANCE_COUNT = 64;
    constexpr auto TEST_TEXTURE_SIZE   = 64;

    struct TestVertex
    {
        float Position[3];
        float UV[2];
    };

    struct TestInstance
    {
        float        Offset[4];
        unsigned int Index;
    };

    struct TestVertexUniforms
    {
        Matrix Transform;
        float  Time;
        float  Padding[3];
    };

    struct TestPixelUniforms
    {
        float Fade[4];
    };

    struct TestResources
    {
        std::unique_ptr<IShader>           VertexShader   = nullptr;
        std::unique_ptr<IShader>           PixelShader    = nullptr;
        std::unique_ptr<IGraphicsPipeline> Pipeline       = nullptr;
        std::unique_ptr<IBuffer>           VertexBuffer   = nullptr;
        std::unique_ptr<IBuffer>           IndexBuffer    = nullptr;
        std::unique_ptr<IBuffer>           InstanceBuffer = nullptr;
        std::unique_ptr<IBuffer>           TintBuffer     = nullptr;
        std::unique_ptr<ITexture>          Texture        = nullptr;
        std::unique_ptr<ISampler>          Sampler        = nullptr;
        std::unique_ptr<ITexture>          ColorTarget    = nullptr;
        std::unique_ptr<ITexture>          DepthTarget    = nullptr;
    };

    static const char* GetBackendName(Backend backend)
    {
        switch (backend)
        {
        case Backend::DirectX11:  return "DirectX11";
        case Backend::Vulkan:     return "Vulkan";
        case Backend::Direct3D12: return "Direct3D12";
        default:                  return "None";
        }
    }

    static bool CreateTestPipeline(IDevice& device, TestResources& resources, const std::string& shaderDirectory)
    {
        resources.VertexShader = LoadShader(device, shaderDirectory, "RHITest", "VS");
        resources.PixelShader = LoadShader(device, shaderDirectory, "RHITest", "PS");
        if (resources.VertexShader == nullptr || resources.PixelShader == nullptr)
            return false;

        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = resources.VertexShader.get();
        desc.PixelShader = resources.PixelShader.get();
        desc.Layout.NumBuffers = 2;
        desc.Layout.Buffers[0] = VertexBufferLayout{ 0, (int)sizeof(TestVertex), VertexInputRate::Vertex };
        desc.Layout.Buffers[1] = VertexBufferLayout{ 1, (int)sizeof(TestInstance), VertexInputRate::Instance };
        desc.Layout.NumAttributes = 4;
        desc.Layout.Attributes[0] = VertexAttribute{ 0, 0, VertexElementFormat::Float3, 0 };
        desc.Layout.Attributes[1] = VertexAttribute{ 1, 0, VertexElementFormat::Float2, 12 };
        desc.Layout.Attributes[2] = VertexAttribute{ 2, 1, VertexElementFormat::Float4, 0 };
        desc.Layout.Attributes[3] = VertexAttribute{ 3, 1, VertexElementFormat::UInt, 16 };
        desc.Rasterizer.Cull = CullMode::None;
        desc.DepthStencil.EnableDepthTest = true;
        desc.DepthStencil.EnableDepthWrite = true;
        desc.DepthStencil.DepthCompare = CompareOp::LessOrEqual;
        desc.NumColorTargets = 1;
        desc.ColorTargets[0].Format = device.GetSwapchainFormat();
        desc.ColorTargets[0].Blend.EnableBlend = true;
        desc.ColorTargets[0].Blend.SrcColor = BlendFactor::SrcAlpha;
        desc.ColorTargets[0].Blend.DstColor = BlendFactor::OneMinusSrcAlpha;
        desc.ColorTargets[0].Blend.SrcAlpha = BlendFactor::One;
        desc.ColorTargets[0].Blend.DstAlpha = BlendFactor::OneMinusSrcAlpha;
        desc.DepthFormat = PixelFormat::D32Float;
        desc.Name = "RHITest.Pipeline";

        resources.Pipeline = device.CreateGraphicsPipeline(desc);
        return resources.Pipeline != nullptr;
    }

    static bool CreateTestResources(IDevice& device, TestResources& resources, int width, int height)
    {
        resources.VertexBuffer = device.CreateBuffer(BufferDesc{ (int)sizeof(TestVertex) * 4, 0, BufferUsage::Vertex, "RHITest.Vertices" });
        resources.IndexBuffer = device.CreateBuffer(BufferDesc{ (int)sizeof(unsigned short) * 6, 0, BufferUsage::Index, "RHITest.Indices" });
        resources.InstanceBuffer = device.CreateBuffer(BufferDesc{ (int)sizeof(TestInstance) * TEST_INSTANCE_COUNT, 0, BufferUsage::Vertex, "RHITest.Instances" });
        resources.TintBuffer = device.CreateBuffer(BufferDesc{ (int)sizeof(Vector4) * TEST_INSTANCE_COUNT, (int)sizeof(Vector4), BufferUsage::GraphicsStorageRead, "RHITest.Tints" });

        auto textureDesc = TextureDesc{};
        textureDesc.Width = TEST_TEXTURE_SIZE;
        textureDesc.Height = TEST_TEXTURE_SIZE;
        textureDesc.Format = PixelFormat::RGBA8Unorm;
        textureDesc.Name = "RHITest.Checker";
        resources.Texture = device.CreateTexture(textureDesc);

        auto colorDesc = TextureDesc{};
        colorDesc.Width = width;
        colorDesc.Height = height;
        colorDesc.Format = device.GetSwapchainFormat();
        colorDesc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        colorDesc.Name = "RHITest.Color";
        resources.ColorTarget = device.CreateTexture(colorDesc);

        auto depthDesc = TextureDesc{};
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.Format = PixelFormat::D32Float;
        depthDesc.Usage = TextureUsage::DepthStencilTarget;
        depthDesc.Name = "RHITest.Depth";
        resources.DepthTarget = device.CreateTexture(depthDesc);

        auto samplerDesc = SamplerDesc{};
        samplerDesc.MinFilter = samplerDesc.MagFilter = Filter::Nearest;
        samplerDesc.Name = "RHITest.Sampler";
        resources.Sampler = device.CreateSampler(samplerDesc);

        return resources.VertexBuffer && resources.IndexBuffer && resources.InstanceBuffer && resources.TintBuffer &&
               resources.Texture && resources.ColorTarget && resources.DepthTarget && resources.Sampler;
    }

    static void UploadTestData(IDevice& device, TestResources& resources)
    {
        TestVertex vertices[4] =
        {
            { { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
            { { -1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f } },
            { {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f } },
            { {  1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } }
        };
        unsigned short indices[6] = { 0, 1, 2, 0, 2, 3 };

        TestInstance instances[TEST_INSTANCE_COUNT];
        Vector4 tints[TEST_INSTANCE_COUNT];
        for (int i = 0; i < TEST_INSTANCE_COUNT; i++)
        {
            int x = i % 8;
            int y = i / 8;
            instances[i] = TestInstance{ { -0.875f + x * 0.25f, -0.875f + y * 0.25f, 0.1f + (i % 5) * 0.15f, 0.1f }, (unsigned int)i };
            tints[i] = Vector4(x / 7.0f, y / 7.0f, 1.0f - (x / 7.0f), 0.85f);
        }

        unsigned int texels[TEST_TEXTURE_SIZE * TEST_TEXTURE_SIZE];
        for (int y = 0; y < TEST_TEXTURE_SIZE; y++)
        {
            for (int x = 0; x < TEST_TEXTURE_SIZE; x++)
                texels[y * TEST_TEXTURE_SIZE + x] = (((x / 8) + (y / 8)) & 1) ? 0xFFFFFFFF : 0xFF404040;
        }

        int vertexOffset = 0;
        int indexOffset = vertexOffset + (int)sizeof(vertices);
        int instanceOffset = indexOffset + (int)sizeof(indices);
        int tintOffset = instanceOffset + (int)sizeof(instances);
        int texelOffset = tintOffset + (int)sizeof(tints);
        int totalSize = texelOffset + (int)sizeof(texels);

        auto transferBuffer = device.CreateTransferBuffer(TransferBufferUsage::Upload, totalSize);
        auto* data = (unsigned char*)device.MapTransferBuffer(transferBuffer.get(), false);
        memcpy(data + vertexOffset, vertices, sizeof(vertices));
        memcpy(data + indexOffset, indices, sizeof(indices));
        memcpy(data + instanceOffset, instances, sizeof(instances));
        memcpy(data + tintOffset, tints, sizeof(tints));
        memcpy(data + texelOffset, texels, sizeof(texels));
        device.UnmapTransferBuffer(transferBuffer.get());

        auto* commandBuffer = device.AcquireCommandBuffer();
        commandBuffer->PushDebugGroup("Upload");
        auto* copyPass = commandBuffer->BeginCopyPass();
        copyPass->UploadToBuffer(transferBuffer.get(), vertexOffset, resources.VertexBuffer.get(), 0, (int)sizeof(vertices), false);
        copyPass->UploadToBuffer(transferBuffer.get(), indexOffset, resources.IndexBuffer.get(), 0, (int)sizeof(indices), false);
        copyPass->UploadToBuffer(transferBuffer.get(), instanceOffset, resources.InstanceBuffer.get(), 0, (int)sizeof(instances), false);
        copyPass->UploadToBuffer(transferBuffer.get(), tintOffset, resources.TintBuffer.get(), 0, (int)sizeof(tints), false);

        auto region = TextureRegion{};
        region.Texture = resources.Texture.get();
        region.Width = TEST_TEXTURE_SIZE;
        region.Height = TEST_TEXTURE_SIZE;
        copyPass->UploadToTexture(TextureTransferInfo{ transferBuffer.get(), texelOffset, 0, 0 }, region, false);

        commandBuffer->EndCopyPass();
        commandBuffer->PopDebugGroup();
        commandBuffer->Submit();
        device.WaitIdle();
    }

    static void DrawTestScene(ICommandBuffer& commandBuffer, TestResources& resources, ITexture* target, float time, float aspect)
    {
        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = target;
        colorTarget.ClearColor[0] = 0.1f;
        colorTarget.ClearColor[1] = 0.12f;
        colorTarget.ClearColor[2] = 0.18f;
        colorTarget.ClearColor[3] = 1.0f;

        auto depthTarget = DepthTargetInfo{};
        depthTarget.Texture = resources.DepthTarget.get();
        depthTarget.Store = StoreOp::DontCare;

        auto vertexUniforms = TestVertexUniforms{};
        vertexUniforms.Transform = Matrix::CreateScale(1.0f / aspect, 1.0f, 1.0f);
        vertexUniforms.Time = time;
        commandBuffer.PushUniforms(ShaderStage::Vertex, 0, &vertexUniforms, sizeof(vertexUniforms));

        auto pixelUniforms = TestPixelUniforms{ { 1.0f, 1.0f, 1.0f, 1.0f } };
        commandBuffer.PushUniforms(ShaderStage::Pixel, 0, &pixelUniforms, sizeof(pixelUniforms));

        commandBuffer.PushDebugGroup("Instanced quads");
        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, &depthTarget);
        renderPass->BindPipeline(resources.Pipeline.get());

        BufferBinding vertexBuffers[2] = { { resources.VertexBuffer.get(), 0 }, { resources.InstanceBuffer.get(), 0 } };
        renderPass->BindVertexBuffers(0, vertexBuffers, 2);
        renderPass->BindIndexBuffer(BufferBinding{ resources.IndexBuffer.get(), 0 }, IndexSize::Bits16);

        IBuffer* storageBuffers[1] = { resources.TintBuffer.get() };
        renderPass->BindVertexStorageBuffers(0, storageBuffers, 1);

        auto textureBinding = TextureSamplerBinding{ resources.Texture.get(), resources.Sampler.get() };
        renderPass->BindPixelSamplers(0, &textureBinding, 1);

        renderPass->DrawIndexed(6, TEST_INSTANCE_COUNT, 0, 0, 0);
        commandBuffer.EndRenderPass();
        commandBuffer.PopDebugGroup();
    }

    bool RunRHITest(SDL_Window* window, Backend backend, const std::string& gameDirectory, int frameCount)
    {
        auto device = CreateDevice(backend, window, true);
        if (device == nullptr)
            return false;

        if (device->GetBackend() != backend)
        {
            TENLog(std::string("RHI test: requested backend ") + GetBackendName(backend) + " unavailable, got " + GetBackendName(device->GetBackend()), LogLevel::Warning);
            backend = device->GetBackend();
        }

        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);

        auto resources = TestResources{};
        auto shaderDirectory = gameDirectory + "Shaders/RHI/Bin";
        if (!CreateTestPipeline(*device, resources, shaderDirectory) || !CreateTestResources(*device, resources, width, height))
        {
            TENLog("RHI test: resource creation failed.", LogLevel::Error);
            return false;
        }

        UploadTestData(*device, resources);

        float aspect = (float)width / (float)height;
        auto startTime = std::chrono::steady_clock::now();

        for (int frame = 0; frame < frameCount; frame++)
        {
            SDL_PumpEvents();

            // Fixed time step, so every backend renders the same image on the last frame.
            float time = frame * (1.0f / 60.0f);

            auto* commandBuffer = device->AcquireCommandBuffer();
            commandBuffer->PushDebugGroup("RHITest frame");

            if (frame == (frameCount - 1))
                DrawTestScene(*commandBuffer, resources, resources.ColorTarget.get(), time, aspect);

            auto* swapchainTexture = commandBuffer->AcquireSwapchainTexture();
            if (swapchainTexture != nullptr && swapchainTexture->GetDesc().Width == width && swapchainTexture->GetDesc().Height == height)
                DrawTestScene(*commandBuffer, resources, swapchainTexture, time, aspect);

            commandBuffer->PopDebugGroup();
            commandBuffer->Submit();
        }

        device->WaitIdle();
        float elapsed = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - startTime).count();

        auto path = gameDirectory + "Logs/RHITest_" + GetBackendName(backend) + ".png";
        bool saved = SaveTextureToPng(*device, resources.ColorTarget.get(), path);
        TENLog(fmt::format("RHI test on {} ({}): {} frames in {:.1f} ms, screenshot {} {}.",
            GetBackendName(backend), device->GetDeviceName(), frameCount, elapsed, path, saved ? "saved" : "FAILED"), LogLevel::Info);

        return saved;
    }
}
