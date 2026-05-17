#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11StructuredBuffer.h"
#include "Renderer/Native/DirectX11/DX11ErrorHelper.h"

namespace TEN::Renderer::Native::DirectX11
{
	DX11StructuredBuffer::DX11StructuredBuffer(ID3D11Device* device, int stride, int elementCount, std::wstring name)
	{
		_stride = stride;
		_elementCount = elementCount;

		auto desc = D3D11_BUFFER_DESC{};
		desc.ByteWidth = stride * elementCount;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = stride;

		throwIfFailed(device->CreateBuffer(&desc, nullptr, _buffer.GetAddressOf()), device,
			"CreateBuffer for StructuredBuffer (" + std::to_string(desc.ByteWidth) + " bytes):");
		_buffer->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)(name.size() * sizeof(wchar_t)), name.c_str());

		auto srvDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = elementCount;

		throwIfFailed(device->CreateShaderResourceView(_buffer.Get(), &srvDesc, _srv.GetAddressOf()), device,
			"CreateShaderResourceView for StructuredBuffer:");
	}

	void DX11StructuredBuffer::UpdateData(const void* data, int elementCount, ID3D11DeviceContext* ctx)
	{
		auto mappedResource = D3D11_MAPPED_SUBRESOURCE{};
		auto res = ctx->Map(_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (SUCCEEDED(res))
		{
			memcpy(mappedResource.pData, data, _stride * elementCount);
			ctx->Unmap(_buffer.Get(), 0);
		}
		else
		{
			TENLog("Could not update structured buffer.", LogLevel::Error);
		}
	}
}

#endif
