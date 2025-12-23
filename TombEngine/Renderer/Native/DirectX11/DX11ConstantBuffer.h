#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include <wrl/client.h>
#include "Game/Debug/Debug.h"
#include "Renderer/RendererUtils.h"
#include "Game/Debug/Debug.h"

namespace TEN::Renderer::Native::DirectX11
{
	using Microsoft::WRL::ComPtr;

	using namespace TEN::Renderer::Utils;
	using namespace TEN::Debug;

	class DX11ConstantBuffer : public IConstantBuffer
	{
	private:
		ComPtr<ID3D11Buffer> _buffer = {};
		int                  _size   = 0;

	public:
		DX11ConstantBuffer() = default;
		~DX11ConstantBuffer() = default;

		DX11ConstantBuffer(ID3D11Device* device, int size, std::wstring name)
		{
			_size = size;
			auto desc = D3D11_BUFFER_DESC{};
			desc.ByteWidth = size;
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			Utils::throwIfFailed(device->CreateBuffer(&desc, nullptr, _buffer.GetAddressOf()));
			_buffer->SetPrivateData(WKPDID_D3DDebugObjectName, 32, name.c_str());
		}

		ID3D11Buffer* GetD3D11Buffer()
		{
			return _buffer.Get();
		}

		void UpdateData(void* data, ID3D11DeviceContext* ctx)
		{
			auto mappedResource = D3D11_MAPPED_SUBRESOURCE{};
			auto res = ctx->Map(_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			if (SUCCEEDED(res))
			{
				void* dataPtr = (mappedResource.pData);
				memcpy(dataPtr, data, _size);
				ctx->Unmap(_buffer.Get(), 0);
			}
			else
			{
				TENLog("Could not update constant buffer.", LogLevel::Error);
			}
		}
	};
}

#endif
