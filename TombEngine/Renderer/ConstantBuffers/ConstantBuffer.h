#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "Game/Debug/Debug.h"
#include "Renderer/RendererUtils.h"

namespace TEN::Renderer::ConstantBuffers
{
	template <typename CBuff>
	class ConstantBuffer
	{
		ComPtr<ID3D11Buffer> buffer;

	public:
		ConstantBuffer() = default;
		ConstantBuffer(ID3D11Device* device)
		{
			auto desc = D3D11_BUFFER_DESC{};
			desc.ByteWidth = sizeof(CBuff);
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			Utils::throwIfFailed(device->CreateBuffer(&desc, nullptr, buffer.GetAddressOf()));
			buffer->SetPrivateData(WKPDID_D3DDebugObjectName, 32, typeid(CBuff).name());
		}

		ID3D11Buffer** get()
		{
			return buffer.GetAddressOf();
		}

		void UpdateData(CBuff& data, ID3D11DeviceContext* ctx)
		{
			auto mappedResource = D3D11_MAPPED_SUBRESOURCE{};
			auto res = ctx->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			if (SUCCEEDED(res))
			{
				void* dataPtr = (mappedResource.pData);
				memcpy(dataPtr, &data, sizeof(CBuff));
				ctx->Unmap(buffer.Get(), 0);
			}
			else
			{
				TENLog("Could not update constant buffer.", LogLevel::Error);
			}
		}
	};
}
