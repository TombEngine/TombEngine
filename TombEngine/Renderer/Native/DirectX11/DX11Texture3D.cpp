#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11Texture3D.h"
#include "Renderer/Native/DirectX11/DX11Utils.h"
#include "Renderer/Native/DirectX11/DX11ErrorHelper.h"

namespace TEN::Renderer::Native::DirectX11
{
	DX11Texture3D::DX11Texture3D(ID3D11Device* device, int width, int height, int depth,
		DXGI_FORMAT format, const void* data)
	{
		_width  = width;
		_height = height;
		_depth  = depth;

		D3D11_TEXTURE3D_DESC desc = {};
		desc.Width     = width;
		desc.Height    = height;
		desc.Depth     = depth;
		desc.MipLevels = 1;
		desc.Format    = format;
		desc.Usage     = (data != nullptr) ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		int bpp = GetBytesPerPixel(format);

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem          = data;
		initData.SysMemPitch      = bpp * width;
		initData.SysMemSlicePitch = bpp * width * height;

		throwIfFailed(
			device->CreateTexture3D(&desc, (data != nullptr) ? &initData : nullptr, _texture.GetAddressOf()),
			"DX11Texture3D: CreateTexture3D failed");

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format                    = format;
		srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels       = 1;
		srvDesc.Texture3D.MostDetailedMip = 0;

		throwIfFailed(
			device->CreateShaderResourceView(_texture.Get(), &srvDesc, _shaderResourceView.GetAddressOf()),
			"DX11Texture3D: CreateShaderResourceView failed");
	}
}

#endif
