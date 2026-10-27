#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include "Renderer/Graphics/IInputLayout.h"
#include "Renderer/Structures/RendererInputLayout.h"
#include "Renderer/Native/DirectX11/DX11Shader.h"
#include <wrl/client.h>

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Structures;
	using namespace TEN::Renderer::Graphics;

	using Microsoft::WRL::ComPtr;

	class DX11InputLayout final : public IInputLayout
	{
	private:
		ComPtr<ID3D11InputLayout> _inputLayout;

	public:
		DX11InputLayout() = default;
		~DX11InputLayout() = default;

		ID3D11InputLayout* GetD3D11InputLayout() const { return _inputLayout.Get(); }

		DX11InputLayout(ID3D11Device* device, std::vector<RendererInputLayoutField> fields, DX11Shader* shader);
	};
}

#endif
