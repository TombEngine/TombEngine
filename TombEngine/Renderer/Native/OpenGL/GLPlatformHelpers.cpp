#include "framework.h"

#ifdef HAS_OPENGL

#include "Renderer/Native/OpenGL/GLPlatformHelpers.h"

#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_platform_defines.h>

#ifdef SDL_PLATFORM_WIN32
#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")
#endif

namespace TEN::Renderer::Native::OpenGL
{
	// Custom GL function loader that keeps opengl32.dll loaded permanently.
	// gladLoadGL()'s built-in loader calls FreeLibrary(opengl32.dll) after loading,
	// which can invalidate the ICD dispatch table on Windows.
#ifdef SDL_PLATFORM_WIN32
	static void* PersistentGLLoader(const char* name)
	{
		static HMODULE opengl32 = LoadLibraryW(L"opengl32.dll");
		static auto wglGetProc = (PROC(WINAPI*)(LPCSTR))GetProcAddress(opengl32, "wglGetProcAddress");

		void* result = nullptr;
		if (wglGetProc)
			result = (void*)wglGetProc(name);
		if (!result && opengl32)
			result = (void*)GetProcAddress(opengl32, name);
		return result;
	}
#endif

	GLLoadProc GetPlatformGLLoader()
	{
#ifdef SDL_PLATFORM_WIN32
		return PersistentGLLoader;
#else
		return (GLLoadProc)SDL_GL_GetProcAddress;
#endif
	}

	TEN::Renderer::Graphics::AdapterInfo GetPlatformAdapterInfo()
	{
		using namespace TEN::Renderer::Graphics;

		AdapterInfo info = {};

		const char* renderer = (const char*)glGetString(GL_RENDERER);
		info.Name = renderer ? std::string(renderer) : "Unknown OpenGL Renderer";

#ifdef SDL_PLATFORM_WIN32
		// DXGI gives accurate adapter info for all vendors (NVIDIA, AMD, Intel).
		IDXGIFactory* factory = nullptr;
		if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory)))
		{
			IDXGIAdapter* adapter = nullptr;
			if (SUCCEEDED(factory->EnumAdapters(0, &adapter)))
			{
				DXGI_ADAPTER_DESC desc = {};
				adapter->GetDesc(&desc);
				info.VendorId = desc.VendorId;
				info.DeviceId = desc.DeviceId;
				info.SubSysId = desc.SubSysId;
				info.Revision = desc.Revision;
				info.DedicatedVideoMemory = desc.DedicatedVideoMemory;
				info.DedicatedSystemMemory = desc.DedicatedSystemMemory;
				info.SharedSystemMemory = desc.SharedSystemMemory;
				adapter->Release();
			}
			factory->Release();
		}
#else
		// Cross-platform fallback: detect vendor from GL_VENDOR string.
		const char* vendor = (const char*)glGetString(GL_VENDOR);
		if (vendor)
		{
			std::string v(vendor);
			if (v.find("NVIDIA") != std::string::npos)
				info.VendorId = 0x10DE;
			else if (v.find("ATI") != std::string::npos || v.find("AMD") != std::string::npos)
				info.VendorId = 0x1002;
			else if (v.find("Intel") != std::string::npos)
				info.VendorId = 0x8086;
		}

		// NVIDIA: GL_NVX_gpu_memory_info -- total dedicated VRAM in KB.
		if (GLAD_GL_NVX_gpu_memory_info)
		{
			GLint totalKB = 0;
			glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &totalKB);
			info.DedicatedVideoMemory = (int)((size_t)totalKB * 1024);
		}
		// AMD: GL_ATI_meminfo -- only reports free memory (total free KB as best estimate).
		else if (GLAD_GL_ATI_meminfo)
		{
			GLint memInfo[4] = {};
			glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, memInfo);
			info.DedicatedVideoMemory = (int)((size_t)memInfo[0] * 1024);
		}
		// Intel: no GL extension for VRAM -- stays at 0.
#endif

		return info;
	}
}

#endif
