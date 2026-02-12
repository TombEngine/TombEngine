#include "framework.h"

#include <codecvt>
#include <d3dcompiler.h>
#include <locale>
#include <iostream>
#include <winerror.h>
#include <wrl/client.h>

#include "Renderer/Renderer.h"
#include "Renderer/Structures/RendererShader.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/trutils.h"

namespace TEN::Renderer::Utils
{
	void throwIfFailed(const HRESULT& res) 
	{
		if (FAILED(res))
		{
			std::string message = std::system_category().message(res);
			TENLog(message, LogLevel::Error);
			throw std::runtime_error("An error occured!");
		}
	}

	void throwIfFailed(const HRESULT& res, const std::string &info) 
	{
		if (FAILED(res))
		{
			std::string message = info + std::system_category().message(res);
			TENLog(message, LogLevel::Error);
			throw std::runtime_error("An error occured!");
		}
	}

	void throwIfFailed(const HRESULT& res, const std::wstring &info) 
	{
		if (FAILED(res))
		{
			std::string message = (std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>{}.to_bytes(info)) + std::system_category().message(res);
			TENLog(message, LogLevel::Error);
			throw std::runtime_error("An error occured!");
		}
	}

	std::wstring GetAssetPath(const wchar_t* fileName)
	{
		return TEN::Utils::ToWString(g_GameFlow->GetGameDir()) + fileName;
	}
}
