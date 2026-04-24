#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11ErrorHelper.h"

#include <codecvt>
#include <iomanip>
#include <locale>
#include <sstream>
#include <wrl/client.h>

#include "Game/Debug/Debug.h"

using namespace TEN::Debug;

namespace TEN::Renderer::Native::DirectX11
{
	std::string GetHResultDescription(HRESULT hr)
	{
		switch (hr)
		{
		case E_OUTOFMEMORY:
			return "E_OUTOFMEMORY: Out of memory. The GPU or system does not have enough memory to allocate the resource.";
		case E_INVALIDARG:
			return "E_INVALIDARG: Invalid argument. One or more parameters passed to the function are invalid.";
		case E_FAIL:
			return "E_FAIL: Unspecified failure.";
		case E_NOINTERFACE:
			return "E_NOINTERFACE: The requested COM interface is not supported.";
		case DXGI_ERROR_DEVICE_REMOVED:
			return "DXGI_ERROR_DEVICE_REMOVED: The GPU device has been removed (driver crash or hardware failure).";
		case DXGI_ERROR_DEVICE_RESET:
			return "DXGI_ERROR_DEVICE_RESET: The GPU device has been reset (driver issue).";
		case DXGI_ERROR_DEVICE_HUNG:
			return "DXGI_ERROR_DEVICE_HUNG: The GPU device is hung (likely a long-running shader or driver bug).";
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
			return "DXGI_ERROR_DRIVER_INTERNAL_ERROR: Internal driver error.";
		case DXGI_ERROR_INVALID_CALL:
			return "DXGI_ERROR_INVALID_CALL: The method call is invalid (e.g. parameter mismatch or wrong calling order).";
		case DXGI_ERROR_WAS_STILL_DRAWING:
			return "DXGI_ERROR_WAS_STILL_DRAWING: The GPU is still busy with a previous operation.";
		case DXGI_ERROR_UNSUPPORTED:
			return "DXGI_ERROR_UNSUPPORTED: The requested functionality is not supported by the device or driver.";
		default:
		{
			auto oss = std::ostringstream();
			oss << "HRESULT 0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(8)
				<< (unsigned long)hr << ": " << std::system_category().message(hr);
			return oss.str();
		}
		}
	}

	std::string GetDeviceDebugMessages(ID3D11Device* device, HRESULT originalHr)
	{
		if (!device)
			return {};

		std::string result;

		// GetDeviceRemovedReason() works in release mode (no debug layer needed).
		if (originalHr == DXGI_ERROR_DEVICE_REMOVED)
		{
			HRESULT reason = device->GetDeviceRemovedReason();
			result += " DeviceRemovedReason: " + GetHResultDescription(reason);
		}

		// ID3D11InfoQueue is only available when the device was created with
		// D3D11_CREATE_DEVICE_DEBUG. The QueryInterface call will simply fail
		// in release builds, so this block is skipped silently.
		Microsoft::WRL::ComPtr<ID3D11InfoQueue> infoQueue;
		if (SUCCEEDED(device->QueryInterface(__uuidof(ID3D11InfoQueue), &infoQueue)) && infoQueue)
		{
			auto messageCount = infoQueue->GetNumStoredMessages();
			if (messageCount > 0)
			{
				result += " D3D11 debug messages:";
				auto maxMessages = std::min<unsigned long long>(messageCount, 5);

				for (auto i = messageCount - maxMessages; i < messageCount; i++)
				{
					SIZE_T messageLength = 0;
					infoQueue->GetMessage(i, nullptr, &messageLength);

					auto messageData = std::vector<unsigned char>(messageLength);
					auto* message = reinterpret_cast<D3D11_MESSAGE*>(messageData.data());
					infoQueue->GetMessage(i, message, &messageLength);

					result += "\n  - " + std::string(message->pDescription, message->DescriptionByteLength - 1);
				}

				infoQueue->ClearStoredMessages();
			}
		}

		return result;
	}

	void throwIfFailed(const HRESULT& res)
	{
		if (FAILED(res))
		{
			std::string message = std::system_category().message(res);
			TENLog(message, LogLevel::Error);
			throw std::runtime_error("An error occured!");
		}
	}

	void throwIfFailed(const HRESULT& res, const std::string& info)
	{
		if (FAILED(res))
		{
			std::string message = info + std::system_category().message(res);
			TENLog(message, LogLevel::Error);
			throw std::runtime_error("An error occured!");
		}
	}

	void throwIfFailed(const HRESULT& res, const std::wstring& info)
	{
		if (FAILED(res))
		{
			std::string message = (std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>{}.to_bytes(info)) + std::system_category().message(res);
			TENLog(message, LogLevel::Error);
			throw std::runtime_error("An error occured!");
		}
	}

	void throwIfFailed(const HRESULT& res, ID3D11Device* device, const std::string& context)
	{
		if (FAILED(res))
		{
			std::string message = context.empty()
				? GetHResultDescription(res)
				: context + " " + GetHResultDescription(res);

			auto debugMessages = GetDeviceDebugMessages(device, res);
			if (!debugMessages.empty())
				message += debugMessages;

			TENLog(message, LogLevel::Error);
			throw std::runtime_error(message);
		}
	}
}

#endif
