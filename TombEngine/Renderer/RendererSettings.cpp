#include "framework.h"
#include <filesystem>
#include <codecvt>

#include "Renderer/Renderer.h"
#include "Specific/trutils.h"
#include "Specific/winmain.h"

namespace TEN::Renderer 
{
	void Renderer::ChangeScreenResolution(int width, int height, bool windowed) 
	{
		// Unbind all render targets and flush the GPU so no resources are in flight.
		ID3D11RenderTargetView* nullViews[] = { nullptr };
		_context->OMSetRenderTargets(0, nullViews, NULL);
		_context->Flush();
		_context->ClearState();

		// Explicitly release _backBuffer BEFORE creating a new swap chain for the same HWND.
		// DXGI requires the old swap chain's back-buffer texture to have zero external references
		// before a new IDXGISwapChain can be created for the same window; failing to do so can
		// cause the new swap chain to be created in a partial/wrong-size state, which manifests
		// as the scene being rendered into only a quarter of the screen.
		_backBuffer = RenderTarget2D{};

		IDXGIOutput* output;
		Utils::throwIfFailed(_swapChain->GetContainingOutput(&output));

		DXGI_SWAP_CHAIN_DESC scd;
		Utils::throwIfFailed(_swapChain->GetDesc(&scd));

		unsigned int numModes = 1024;
		DXGI_MODE_DESC modes[1024];
		Utils::throwIfFailed(output->GetDisplayModeList(scd.BufferDesc.Format, 0, &numModes, modes));

		DXGI_MODE_DESC* mode = &modes[0];
		for (unsigned int i = 0; i < numModes; i++)
		{
			mode = &modes[i];
			if (mode->Width == width && mode->Height == height)
				break;
		}

		Utils::throwIfFailed( _swapChain->ResizeTarget(mode));

		_screenWidth = width;
		_screenHeight = height;
		_isWindowed = windowed;

		InitializeScreen(width, height, WindowsHandle, true);

		// Recreate resolution-dependent render targets that are NOT owned by InitializeScreen.
		// These were allocated at startup with the old screen dimensions and must be resized
		// so that subsequent draw passes use correctly-sized buffers.
		InitializeGodRays();          // recreates _godRayRenderTarget at new half-res
		ResizeVolumetricCloudTargets();   // recreates _cloudRenderTarget / _cloudOcclusionTarget (layer A)
		ResizeDualCloudTargets();         // recreates _cloudRenderTargetB / _cloudOcclusionTargetB (layer B)
	}

	std::string Renderer::GetDefaultAdapterName()
	{
		IDXGIFactory* dxgiFactory = NULL;
		Utils::throwIfFailed(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&dxgiFactory));

		IDXGIAdapter* dxgiAdapter = NULL;

		dxgiFactory->EnumAdapters(0, &dxgiAdapter);

		DXGI_ADAPTER_DESC adapterDesc = {};

		dxgiAdapter->GetDesc(&adapterDesc);
		dxgiFactory->Release();
		
		return TEN::Utils::ToString(adapterDesc.Description);
	}

	const AdapterInfo& Renderer::GetAdapterInfo() const
	{
		return _adapterInfo;
	}

	void Renderer::SetTextureOrDefault(Texture2D& texture, std::wstring path)
	{
		texture = Texture2D();

		if (std::filesystem::is_regular_file(path))
		{
			texture = Texture2D(_device.Get(), path);
		}
		else if (!path.empty()) // Loading default texture without path may be intentional.
		{
			std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
			TENLog("Texture file not found: " + converter.to_bytes(path), LogLevel::Warning);
		}
	}
}
