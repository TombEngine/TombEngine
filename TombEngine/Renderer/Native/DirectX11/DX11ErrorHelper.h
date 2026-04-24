#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include <string>

namespace TEN::Renderer::Native::DirectX11
{
	std::string GetHResultDescription(HRESULT hr);
	std::string GetDeviceDebugMessages(ID3D11Device* device, HRESULT originalHr);

	void throwIfFailed(const HRESULT& res);
	void throwIfFailed(const HRESULT& res, const std::string& info);
	void throwIfFailed(const HRESULT& res, const std::wstring& info);
	void throwIfFailed(const HRESULT& res, ID3D11Device* device, const std::string& context);
}

#endif
