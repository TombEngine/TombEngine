#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11ConstantBuffer.h"
#include "Renderer/Native/DirectX11/DX11ErrorHelper.h"
#include "Specific/trutils.h"

namespace TEN::Renderer::Native::DirectX11
{
	DX11ConstantBuffer::DX11ConstantBuffer(ID3D11Device* device, int size, std::string name)
	{
		_size = size;
		auto desc = D3D11_BUFFER_DESC{};
		desc.ByteWidth = size;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		throwIfFailed(device->CreateBuffer(&desc, nullptr, _buffer.GetAddressOf()), device,
			"CreateBuffer for ConstantBuffer (" + std::to_string(size) + " bytes):");
		_buffer->SetPrivateData(WKPDID_D3DDebugObjectName, (unsigned int)name.size(), name.c_str());
	}

	void DX11ConstantBuffer::UpdateData(void* data, ID3D11DeviceContext* ctx, int size)
	{
		// A negative size means "upload everything". Callers that only fill a prefix of the
		// buffer pass its byte count instead: WRITE_DISCARD hands back a renamed allocation
		// whose contents are undefined anyway, so leaving the tail unwritten is legal as long
		// as no shader reads it. This matters for large buffers such as CObjectsBuffer, where
		// a single-instance draw would otherwise memcpy all 64 KB.
		int uploadSize = (size < 0) ? _size : std::min(size, _size);

		auto mappedResource = D3D11_MAPPED_SUBRESOURCE{};
		auto res = ctx->Map(_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (SUCCEEDED(res))
		{
			void* dataPtr = (mappedResource.pData);
			memcpy(dataPtr, data, uploadSize);
			ctx->Unmap(_buffer.Get(), 0);
		}
		else
		{
			TENLog("Could not update constant buffer.", LogLevel::Error);
		}
	}
}

#endif