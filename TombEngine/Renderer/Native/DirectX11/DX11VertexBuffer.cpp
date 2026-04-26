#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11VertexBuffer.h"
#include "Renderer/Native/DirectX11/DX11ErrorHelper.h"
#include "Renderer/Native/DirectX11/DX11Utils.h"
#include "Specific/trutils.h"

namespace TEN::Renderer::Native::DirectX11
{
	DX11VertexBuffer::DX11VertexBuffer(ID3D11Device* device, int numVertices, int stride, void* vertices)
	{
		_stride = stride;

		D3D11_BUFFER_DESC desc = {};

		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.ByteWidth = _stride * (numVertices > 0 ? numVertices : 1);
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		auto contextStr = "CreateBuffer for VertexBuffer (" + std::to_string(numVertices) +
			" vertices x " + std::to_string(stride) + " stride, " +
			BytesToMBString(desc.ByteWidth) + " MB):";

		if (numVertices != 0)
		{
			D3D11_SUBRESOURCE_DATA initData = {};
			initData.pSysMem = vertices;
			initData.SysMemPitch = _stride * numVertices;

			throwIfFailed(device->CreateBuffer(&desc, &initData, &_buffer), device, contextStr);
		}
		else
		{
			throwIfFailed(device->CreateBuffer(&desc, nullptr, &_buffer), device, contextStr);
		}

		_numVertices = numVertices;

		int vramSize = desc.ByteWidth;
		_vram = VRAMAllocation(VRAMCategory::VertexBuffer, vramSize,
			"VertexBuffer allocated: " + std::to_string(numVertices) + " vertices x " +
			std::to_string(stride) + " stride (" +
			BytesToMBString(vramSize) + " MB)");
	}

	bool DX11VertexBuffer::Update(ID3D11DeviceContext* context, void* data, int startVertex, int count)
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		HRESULT res = context->Map(_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

		if (SUCCEEDED(res))
		{
			void* dataPtr = (mappedResource.pData);
			auto* src = (unsigned char*)data + startVertex * _stride;
			std::memcpy(dataPtr, src, (size_t)count * _stride);
			context->Unmap(_buffer.Get(), 0);
			return true;
		}
		else
		{
			TENLog("Could not update vertex buffer! " + std::to_string(res), LogLevel::Error);
			return false;
		}
	}
}

#endif
