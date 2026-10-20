#include "framework.h"
#include "Renderer/RHI/RHIDevice.h"

#include "Renderer/RHI/DirectX11/DX11Device.h"
#include "Renderer/RHI/OpenGL/GLDevice.h"
#include "Renderer/RHI/SDLGPU/SDLGPUDevice.h"

// Backend independent format helpers and the device factory, which picks the first backend that initializes.

namespace TEN::Renderer::RHI
{
    int GetFormatBlockSize(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::R8Unorm:
            return 1;

        case PixelFormat::RG8Unorm:
        case PixelFormat::R16Float:
        case PixelFormat::D16Unorm:
            return 2;

        case PixelFormat::RGBA8Unorm:
        case PixelFormat::RGBA8UnormSrgb:
        case PixelFormat::BGRA8Unorm:
        case PixelFormat::BGRA8UnormSrgb:
        case PixelFormat::RG16Float:
        case PixelFormat::R32Float:
        case PixelFormat::R11G11B10Float:
        case PixelFormat::R32Uint:
        case PixelFormat::D24UnormS8Uint:
        case PixelFormat::D32Float:
            return 4;

        case PixelFormat::RGBA16Float:
        case PixelFormat::D32FloatS8Uint:
        case PixelFormat::RG32Float:
        case PixelFormat::BC1Unorm:
        case PixelFormat::BC4Unorm:
            return 8;

        case PixelFormat::RGBA32Float:
        case PixelFormat::BC2Unorm:
        case PixelFormat::BC3Unorm:
        case PixelFormat::BC5Unorm:
        case PixelFormat::BC7Unorm:
            return 16;

        default:
            return 0;
        }
    }

    int GetFormatBlockDimension(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::BC1Unorm:
        case PixelFormat::BC2Unorm:
        case PixelFormat::BC3Unorm:
        case PixelFormat::BC4Unorm:
        case PixelFormat::BC5Unorm:
        case PixelFormat::BC7Unorm:
            return 4;

        default:
            return 1;
        }
    }

    bool IsDepthFormat(PixelFormat format)
    {
        return (format == PixelFormat::D16Unorm || format == PixelFormat::D24UnormS8Uint || format == PixelFormat::D32Float || format == PixelFormat::D32FloatS8Uint);
    }

    bool HasStencil(PixelFormat format)
    {
        return (format == PixelFormat::D24UnormS8Uint || format == PixelFormat::D32FloatS8Uint);
    }

    int GetTextureDataSize(PixelFormat format, int width, int height, int depth)
    {
        int blockDimension = GetFormatBlockDimension(format);
        int blocksX = (width + blockDimension - 1) / blockDimension;
        int blocksY = (height + blockDimension - 1) / blockDimension;
        return blocksX * blocksY * depth * GetFormatBlockSize(format);
    }

    std::unique_ptr<IDevice> CreateDevice(GraphicsApi preferredBackend, SDL_Window* window, bool debugMode, bool preferLowPower)
    {
        // Try the requested backend first, then fall back in order of preference: Vulkan, D3D12, DX11, OpenGL.
        GraphicsApi order[] = { preferredBackend, GraphicsApi::Vulkan, GraphicsApi::Direct3D12, GraphicsApi::DirectX11, GraphicsApi::OpenGL };

        for (auto backend : order)
        {
            std::unique_ptr<IDevice> device = nullptr;

            switch (backend)
            {
            case GraphicsApi::Vulkan:
                device = SDLGPU::CreateSDLGPUDevice(window, "vulkan", debugMode, preferLowPower);
                break;

            case GraphicsApi::Direct3D12:
                device = SDLGPU::CreateSDLGPUDevice(window, "direct3d12", debugMode, preferLowPower);
                break;

            case GraphicsApi::DirectX11:
                device = DirectX11::CreateDX11Device(window, debugMode, preferLowPower);
                break;

            case GraphicsApi::OpenGL:
                device = OpenGL::CreateGLDevice(window, debugMode);
                break;

            default:
                break;
            }

            if (device != nullptr)
                return device;
        }

        TENLog("No graphics backend could be initialized.", LogLevel::Error);
        return nullptr;
    }

    // Only OpenGL needs a flag and context attributes before the window exists; the other backends attach later.
    unsigned int GetWindowFlags(GraphicsApi backend)
    {
        return (backend == GraphicsApi::OpenGL) ? OpenGL::PrepareGLWindow() : 0;
    }
}
