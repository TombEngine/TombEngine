#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11GpuReadbackBuffer.h"
#include "Renderer/Native/DirectX11/DX11Texture2D.h"
#include "Renderer/Native/DirectX11/DX11RenderTarget2D.h"

namespace TEN::Renderer::Native::DirectX11
{
	static int BytesPerPixel(DXGI_FORMAT format)
	{
		switch (format)
		{
		case DXGI_FORMAT_R8_UNORM:               return 1;
		case DXGI_FORMAT_R8G8_UNORM:             return 2;
		case DXGI_FORMAT_R16_FLOAT:              return 2;
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R11G11B10_FLOAT:        return 4;
		case DXGI_FORMAT_R16G16B16A16_FLOAT:     return 8;
		case DXGI_FORMAT_R32G32B32A32_FLOAT:     return 16;
		default:                                 return 0;
		}
	}

	DX11GpuReadbackBuffer::DX11GpuReadbackBuffer(ID3D11Device* device, ID3D11DeviceContext* context,
		int width, int height, DXGI_FORMAT format)
	{
		_device = device;
		_context = context;
		_byteSize = width * height * BytesPerPixel(format);

		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width            = width;
		desc.Height           = height;
		desc.MipLevels        = 1;
		desc.ArraySize        = 1;
		desc.Format           = format;
		desc.SampleDesc.Count = 1;
		desc.Usage            = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags   = D3D11_CPU_ACCESS_READ;

		for (int i = 0; i < 2; i++)
			device->CreateTexture2D(&desc, nullptr, _stagingTextures[i].GetAddressOf());
	}

	void DX11GpuReadbackBuffer::SubmitCopy(ITexture2D* src)
	{
		if (src == nullptr)
			return;

		ID3D11Texture2D* srcNative = nullptr;
		if (auto* rt = dynamic_cast<DX11RenderTarget2D*>(src))
			srcNative = rt->GetD3D11Texture();
		else if (auto* tex = dynamic_cast<DX11Texture2D*>(src))
			srcNative = tex->GetD3D11Texture();

		if (srcNative == nullptr)
			return;

		_context->CopyResource(_stagingTextures[_writeIndex].Get(), srcNative);
		_writeIndex = 1 - _writeIndex;

		if (_submitCount < 2)
			_submitCount++;
	}

	bool DX11GpuReadbackBuffer::TryRead(void* outBytes, int byteCount)
	{
		if (outBytes == nullptr || byteCount != _byteSize)
			return false;

		// Need at least 2 submits before the slot we read from contains valid,
		// fully-completed data (the slot one submit older than the most recent).
		if (_submitCount < 2)
			return false;

		// _writeIndex points to the slot that will be written next, i.e. the
		// older of the two slots — its GPU copy was submitted at least one
		// SubmitCopy ago, so the data is ready.
		ID3D11Texture2D* slot = _stagingTextures[_writeIndex].Get();

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		HRESULT hr = _context->Map(slot, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
		if (FAILED(hr))
			return false;

		memcpy(outBytes, mapped.pData, byteCount);
		_context->Unmap(slot, 0);
		return true;
	}
}

#endif
