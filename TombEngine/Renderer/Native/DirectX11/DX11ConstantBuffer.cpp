#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11ConstantBuffer.h"
#include "Renderer/Native/DirectX11/DX11ErrorHelper.h"
#include "Specific/trutils.h"

namespace TEN::Renderer::Native::DirectX11
{
	DX11ConstantBuffer::DX11ConstantBuffer(ID3D11Device* device, int size, std::wstring name)
	{
		_size = size;
		auto desc = D3D11_BUFFER_DESC{};
		desc.ByteWidth = size;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		throwIfFailed(device->CreateBuffer(&desc, nullptr, _buffer.GetAddressOf()), device,
			"CreateBuffer for ConstantBuffer (" + std::to_string(size) + " bytes):");
		_buffer->SetPrivateData(WKPDID_D3DDebugObjectName, 32, name.c_str());
	}

	void DX11ConstantBuffer::UpdateData(void* data, ID3D11DeviceContext* ctx) 
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
}

#endif