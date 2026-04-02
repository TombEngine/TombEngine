#pragma once
#include <string>
#include <winerror.h>
#include <wrl/client.h>
#include <d3d11.h>

namespace TEN::Renderer::Utils
{
	std::string GetHResultDescription(HRESULT hr);
	std::string GetDeviceDebugMessages(ID3D11Device* device, HRESULT originalHr);

	void throwIfFailed(const HRESULT& res);
	void throwIfFailed(const HRESULT& res, const std::string& info);
	void throwIfFailed(const HRESULT& res, const std::wstring& info);
	void throwIfFailed(const HRESULT& res, ID3D11Device* device, const std::string& info = {});

	std::wstring GetAssetPath(const wchar_t* fileName);
}
