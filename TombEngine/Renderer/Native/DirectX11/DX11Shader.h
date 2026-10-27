#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include "Renderer/Graphics/IShader.h"
#include "Game/debug/debug.h"
#include <wrl/client.h>
#include <d3dcompiler.h>

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Graphics;
	using namespace TEN::Debug;

	using Microsoft::WRL::ComPtr;

	class DX11Shader final : public IShader
	{
	private:
		ComPtr<ID3D11VertexShader> _vertexShader = nullptr;
		ComPtr<ID3D11GeometryShader> _geometryShader = nullptr;
		ComPtr<ID3D11PixelShader> _pixelShader = nullptr;
		ComPtr<ID3D10Blob> _blob = nullptr;

	public:
		DX11Shader() = default;
		~DX11Shader() = default;

		ID3D11VertexShader* GetD3D11VertexShader() const { return _vertexShader.Get(); }
		ID3D11GeometryShader* GetD3D11GeometryShader() const { return _geometryShader.Get(); }
		ID3D11PixelShader* GetD3D11PixelShader() const { return _pixelShader.Get(); }
		ID3D10Blob* GetD3D10Blob() const { return _blob.Get(); }

		void SetD3D11VertexShader(ComPtr<ID3D11VertexShader> shader) { _vertexShader = shader; }
		void SetD3D11GeometryShader(ComPtr<ID3D11GeometryShader> shader) { _geometryShader = shader; }
		void SetD3D11PixelShader(ComPtr<ID3D11PixelShader> shader) { _pixelShader = shader; }
		void SetD3D10Blob(ComPtr<ID3D10Blob> blob) { _blob = blob; }
	};
}

#endif
