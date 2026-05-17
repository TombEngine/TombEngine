#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11InputLayout.h"
#include "Renderer/Native/DirectX11/DX11ErrorHelper.h"
#include "Specific/trutils.h"

namespace TEN::Renderer::Native::DirectX11
{
	DX11InputLayout::DX11InputLayout(ID3D11Device* device, std::vector<RendererInputLayoutField> fields, DX11Shader* shader)
	{
		std::vector<D3D11_INPUT_ELEMENT_DESC> elements;

		for (auto field : fields)
		{
			D3D11_INPUT_ELEMENT_DESC element;

			element.SemanticIndex = field.Slot;
			element.SemanticName = field.Semantic;
			element.AlignedByteOffset = elements.size() == 0 ? 0 : D3D11_APPEND_ALIGNED_ELEMENT;
			element.InputSlot = 0;
			element.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			element.InstanceDataStepRate = 0;

			switch (field.Format)
			{
			case VertexInputFormat::VI_RGBA8_Unorm:
				element.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				break;

			case VertexInputFormat::VI_RG8_Unorm:
				element.Format = DXGI_FORMAT_R8G8_UNORM;
				break;

			case VertexInputFormat::VI_RGB32_Float:
				element.Format = DXGI_FORMAT_R32G32B32_FLOAT;
				break;

			case VertexInputFormat::VI_RGBA32_Float:
				element.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				break;

			case VertexInputFormat::VI_R32_Float:
				element.Format = DXGI_FORMAT_R32_FLOAT;
				break;

			case VertexInputFormat::VI_R32_Uint:
				element.Format = DXGI_FORMAT_R32_UINT;
				break;

			case VertexInputFormat::VI_RG32_Float:
				element.Format = DXGI_FORMAT_R32G32_FLOAT;
				break;

			case VertexInputFormat::VI_RGBA8_Snorm:
				element.Format = DXGI_FORMAT_R8G8B8A8_SNORM;
				break;

			case VertexInputFormat::VI_RGBA8_Uint:
				element.Format = DXGI_FORMAT_R8G8B8A8_UINT;
				break;

			default:
				continue;

			}

			elements.push_back(element);
		}

		throwIfFailed(device->CreateInputLayout(elements.data(), (int)elements.size(), shader->GetD3D10Blob()->GetBufferPointer(), shader->GetD3D10Blob()->GetBufferSize(), &_inputLayout));
	}
}

#endif