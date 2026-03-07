#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

#include "Renderer/RendererUtils.h"
#include "Renderer/Graphics/Vertices/Vertex.h"
#include "Renderer/Graphics/VRAMTracker.h"
#include "Specific/fast_vector.h"

using namespace TEN::Renderer::Graphics::Vertices;

namespace TEN::Renderer::Graphics
{
	using namespace TEN::Renderer::Utils;

	using Microsoft::WRL::ComPtr;

	template <typename CVertex>
	class VertexBuffer
	{
	private:
		int _numVertices;
		int _vramSize = 0;

	public:
		ComPtr<ID3D11Buffer> Buffer;

		VertexBuffer() {};
		VertexBuffer(VertexBuffer&& other) noexcept : _numVertices(other._numVertices), _vramSize(other._vramSize), Buffer(std::move(other.Buffer))
		{
			other._vramSize = 0;
		}

		VertexBuffer& operator=(VertexBuffer&& other) noexcept
		{
			if (this != &other)
			{
				if (_vramSize > 0)
					VRAMTracker::Get().Remove(VRAMCategory::VertexBuffer, _vramSize);

				_numVertices = other._numVertices;
				Buffer = std::move(other.Buffer);
				_vramSize = other._vramSize;
				other._vramSize = 0;
			}
			return *this;
		}

		VertexBuffer(const VertexBuffer&) = delete;
		VertexBuffer& operator=(const VertexBuffer&) = delete;

		~VertexBuffer()
		{
			if (_vramSize > 0)
				VRAMTracker::Get().Remove(VRAMCategory::VertexBuffer, _vramSize);
		}

		template <typename CVertex>
		VertexBuffer(ID3D11Device* device, int numVertices, std::vector<CVertex> vertices)
		{
			D3D11_BUFFER_DESC desc = {};

			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.ByteWidth = sizeof(CVertex) * (numVertices > 0 ? numVertices : 1);
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			if (vertices.size() != 0)
			{
				D3D11_SUBRESOURCE_DATA initData = {};
				initData.pSysMem = vertices.data();
				initData.SysMemPitch = sizeof(CVertex) * numVertices;

				throwIfFailed(device->CreateBuffer(&desc, &initData, &Buffer));
			}
			else
			{
				throwIfFailed(device->CreateBuffer(&desc, nullptr, &Buffer));
			}

			_numVertices = numVertices;
			_vramSize = desc.ByteWidth;
			VRAMTracker::Get().Add(VRAMCategory::VertexBuffer, _vramSize);
		}

		template <typename CVertex>
		VertexBuffer(ID3D11Device* device, int numVertices, fast_vector<CVertex> vertices)
		{
			D3D11_BUFFER_DESC desc = {};

			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.ByteWidth = sizeof(CVertex) * (numVertices > 0 ? numVertices : 1);
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			if (vertices.size() != 0)
			{
				D3D11_SUBRESOURCE_DATA initData = {};
				initData.pSysMem = vertices.data();
				initData.SysMemPitch = sizeof(CVertex) * numVertices;

				throwIfFailed(device->CreateBuffer(&desc, &initData, &Buffer));
			}
			else
			{
				throwIfFailed(device->CreateBuffer(&desc, nullptr, &Buffer));
			}

			_numVertices = numVertices;
			_vramSize = desc.ByteWidth;
			VRAMTracker::Get().Add(VRAMCategory::VertexBuffer, _vramSize);
		}

		template <typename CVertex>
		VertexBuffer(ID3D11Device* device, int numVertices, CVertex* vertices)
		{
			D3D11_BUFFER_DESC desc = {};

			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.ByteWidth = sizeof(CVertex) * (numVertices > 0 ? numVertices : 1);
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			if (numVertices != 0)
			{
				D3D11_SUBRESOURCE_DATA initData = {};
				initData.pSysMem = vertices;
				initData.SysMemPitch = sizeof(CVertex) * numVertices;

				throwIfFailed(device->CreateBuffer(&desc, &initData, &Buffer));
			}
			else
			{
				throwIfFailed(device->CreateBuffer(&desc, nullptr, &Buffer));
			}

			_numVertices = numVertices;
			_vramSize = desc.ByteWidth;
			VRAMTracker::Get().Add(VRAMCategory::VertexBuffer, _vramSize);
		}

		template <typename CVertex>
		bool Update(ID3D11DeviceContext* context, CVertex* data, int startVertex, int count)
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			HRESULT res = context->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

			if (SUCCEEDED(res))
			{
				void* dataPtr = (mappedResource.pData);
				memcpy(dataPtr, &data[startVertex], count * sizeof(CVertex));
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
