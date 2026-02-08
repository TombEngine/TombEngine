#pragma once
#include <d3d11.h>
#include <vector>
#include <wrl/client.h>
#include "Renderer/RendererUtils.h"
#include "Renderer/Graphics/VRAMTracker.h"
#include "Game/Debug/Debug.h"
#include "Specific/fast_vector.h"

namespace TEN::Renderer::Graphics
{
	using namespace TEN::Renderer::Utils;

	class IndexBuffer
	{
	private:
		int _numIndices;
		int _vramSize = 0;

	public:
		Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;

		IndexBuffer() {};
		IndexBuffer(IndexBuffer&& other) noexcept : _numIndices(other._numIndices), _vramSize(other._vramSize), Buffer(std::move(other.Buffer))
		{
			other._vramSize = 0;
		}

		IndexBuffer& operator=(IndexBuffer&& other) noexcept
		{
			if (this != &other)
			{
				if (_vramSize > 0)
					VRAMTracker::Get().Remove(VRAMCategory::IndexBuffer, _vramSize);

				_numIndices = other._numIndices;
				Buffer = std::move(other.Buffer);
				_vramSize = other._vramSize;
				other._vramSize = 0;
			}
			return *this;
		}

		IndexBuffer(const IndexBuffer&) = delete;
		IndexBuffer& operator=(const IndexBuffer&) = delete;

		~IndexBuffer()
		{
			if (_vramSize > 0)
				VRAMTracker::Get().Remove(VRAMCategory::IndexBuffer, _vramSize);
		}

		IndexBuffer(ID3D11Device* device, int numIndices, int* indices)
		{
			D3D11_BUFFER_DESC desc = {};

			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.ByteWidth = sizeof(int) * (numIndices > 0 ? numIndices : 1);
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			if (numIndices > 0)
			{
				D3D11_SUBRESOURCE_DATA initData = {};
				initData.pSysMem = indices;
				initData.SysMemPitch = sizeof(int) * numIndices;

				throwIfFailed(device->CreateBuffer(&desc, &initData, &Buffer));
			}
			else
			{
				throwIfFailed(device->CreateBuffer(&desc, nullptr, &Buffer));
			}

			_numIndices = numIndices;
			_vramSize = desc.ByteWidth;
			VRAMTracker::Get().Add(VRAMCategory::IndexBuffer, _vramSize);
		}

		IndexBuffer(ID3D11Device* device, int numIndices, fast_vector<int> indices)
		{
			D3D11_BUFFER_DESC desc = {};

			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.ByteWidth = sizeof(int) * (numIndices > 0 ? numIndices : 1);
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			if (numIndices > 0)
			{
				D3D11_SUBRESOURCE_DATA initData = {};
				initData.pSysMem = indices.data();
				initData.SysMemPitch = sizeof(int) * numIndices;

				throwIfFailed(device->CreateBuffer(&desc, &initData, &Buffer));
			}
			else
			{
				throwIfFailed(device->CreateBuffer(&desc, nullptr, &Buffer));
			}

			_numIndices = numIndices;
			_vramSize = desc.ByteWidth;
			VRAMTracker::Get().Add(VRAMCategory::IndexBuffer, _vramSize);
		}

		bool Update(ID3D11DeviceContext* context, std::vector<int>& data, int startIndex, int count)
		{
			//TENLog("VertexBuffer::Update NumVertices: " + std::to_string(data.size()));

			D3D11_MAPPED_SUBRESOURCE mappedResource;
			HRESULT res = context->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			if (SUCCEEDED(res)) {
				void* dataPtr = (mappedResource.pData);
				memcpy(dataPtr, &data[startIndex], count * sizeof(int));
				context->Unmap(Buffer.Get(), 0);
				return true;
			}
			else
			{
				TENLog("Could not update constant buffer! " + std::to_string(res), LogLevel::Error);
				return false;
			}
		}

		bool Update(ID3D11DeviceContext* context, fast_vector<int>& data, int startIndex, int count)
		{
			//TENLog("VertexBuffer::Update NumVertices: " + std::to_string(data.size()));

			D3D11_MAPPED_SUBRESOURCE mappedResource;
			HRESULT res = context->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			if (SUCCEEDED(res)) {
				void* dataPtr = (mappedResource.pData);
				memcpy(dataPtr, &data[startIndex], count * sizeof(int));
				context->Unmap(Buffer.Get(), 0);
				return true;
			}
			else
			{
				TENLog("Could not update constant buffer! " + std::to_string(res), LogLevel::Error);
				return false;
			}
		}
	};
}
