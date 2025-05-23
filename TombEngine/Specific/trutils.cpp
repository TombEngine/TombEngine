#include "framework.h"

#include <codecvt>
#include <filesystem>

#include "Renderer/Renderer.h"
#include "Renderer/RendererEnums.h"
#include "Specific/trutils.h"

using TEN::Renderer::g_Renderer;

namespace TEN::Utils
{
	std::string ConstructAssetDirectory(std::string customDirectory)
	{
		static const int searchDepth = 2;
		static const std::string upDir = "../";
		static const std::string testPath = "Scripts/Gameflow.lua";

		if (!customDirectory.empty())
		{
			// Replace all backslashes with forward slashes.
			std::replace(customDirectory.begin(), customDirectory.end(), '\\', '/');

			// Add trailing slash if missing.
			if (customDirectory.back() != '/')
				customDirectory += '/';
		}

		// Wrap directory depth searching into try-catch block to avoid crashes if we get too
		// shallow directory level (e.g. if user have placed executable in a disk root folder).

		try
		{
			// First, search custom directory, if exists, only then try own (empty) subdirectory.

			for (int useCustomSubdirectory = 1; useCustomSubdirectory >= 0; useCustomSubdirectory--)
			{
				// Quickly exit if no custom directory specified.

				if (useCustomSubdirectory && customDirectory.empty())
					continue;

				for (int depth = 0; depth < searchDepth + 1; depth++)
				{
					auto result = useCustomSubdirectory ? customDirectory : std::string{};
					bool isAbsolute = useCustomSubdirectory && std::filesystem::path(result).is_absolute();

					if (isAbsolute)
					{
						// Custom directory may be specified as absolute. In such case, it makes no sense
						// to search for assets on extra depth levels, since user never would expect that.

						if (depth > 0)
							break;
					}
					else
					{
						// Add upward directory levels, according to current depth.

						for (int level = 0; level < depth; level++)
							result = upDir + result;
					}

					// Look if provided test path / file exists in current folder. If it is,
					// it means this is a valid asset folder.

					auto testDir = result + (useCustomSubdirectory ? "/" : "") + testPath;
					if (std::filesystem::is_regular_file(testDir))
						return result;
				}
			}
		}
		catch (std::exception ex)
		{
			return std::string{}; // Use exe path if any error is encountered.
		}

		return std::string{}; // Use exe path if no any assets were found.
	}

	std::string ToUpper(std::string string)
	{
		std::transform(string.begin(), string.end(), string.begin(), [](unsigned char c) { return std::toupper(c); });
		return string;
	}
	
	std::string ToLower(std::string string)
	{
		std::transform(string.begin(), string.end(), string.begin(), [](unsigned char c) { return std::tolower(c); });
		return string;
	}

	std::string ToString(const std::wstring& wString)
	{
		return ToString(wString.c_str());
	}

	std::string ToString(const wchar_t* wString)
	{
        auto converter = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>();
		return converter.to_bytes(std::wstring(wString));
	}

    std::wstring ToWString(const std::string& string)
    {
        auto cString = string.c_str();
        int size = MultiByteToWideChar(CP_UTF8, 0, cString, (int)string.size(), nullptr, 0);
        auto wString = std::wstring(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, cString, (int)strlen(cString), &wString[0], size);
        return wString;
    }

	std::wstring ToWString(const char* cString)
	{
		wchar_t buffer[UCHAR_MAX];
		std::mbstowcs(buffer, cString, UCHAR_MAX);
		return std::wstring(buffer);
	}

	std::string ReplaceNewLineSymbols(const std::string& string)
	{
		auto result = string;
		std::string::size_type index = 0;

		while ((index = result.find("\\n", index)) != std::string::npos) 
		{
			result.replace(index, 2, "\n");
			++index;
		}

		return result;
	}

	std::vector<std::wstring> SplitString(const std::wstring& string)
	{
		auto strings = std::vector<std::wstring>{};

		// Exit early if string is single line.
		if (string.find(L'\n') == std::wstring::npos)
		{
			strings.push_back(string);
			return strings;
		}

		std::wstring::size_type pos = 0;
		std::wstring::size_type prev = 0;
		while ((pos = string.find(L'\n', prev)) != std::string::npos)
		{
			strings.push_back(string.substr(prev, pos - prev));
			prev = pos + 1;
		}

		strings.push_back(string.substr(prev));
		return strings;
	}

	int GetHash(const std::string& string)
	{
		if (string.empty())
			return 0;

		uint32_t hash = 2166136261u;
		for (char c : string)
		{
			hash ^= static_cast<uint8_t>(c);
			hash *= 16777619u;
		}

		return static_cast<int>(hash);
	}

    Vector2 GetAspectCorrect2DPosition(const Vector2& pos)
    {
       constexpr auto DISPLAY_SPACE_ASPECT = DISPLAY_SPACE_RES.x / DISPLAY_SPACE_RES.y;

        auto screenRes = g_Renderer.GetScreenResolution().ToVector2();
        float screenResAspect = screenRes.x / screenRes.y;
        float aspectDelta = screenResAspect - DISPLAY_SPACE_ASPECT;

		auto correctedPos = pos;
        if (aspectDelta > EPSILON)
        {
			correctedPos.x *= 1.0f - (aspectDelta / 2);
        }
        else if (aspectDelta < -EPSILON)
        {
			correctedPos.y *= 1.0f - (aspectDelta / 2);
        }

        return correctedPos;
    }

    Vector2 Convert2DPositionToNDC(const Vector2& pos)
    {
        return Vector2(
            ((pos.x * 2) / DISPLAY_SPACE_RES.x) - 1.0f,
            1.0f - ((pos.y * 2) / DISPLAY_SPACE_RES.y));
    }

    Vector2 ConvertNDCTo2DPosition(const Vector2& ndc)
    {
        return Vector2(
            ((ndc.x + 1.0f) * DISPLAY_SPACE_RES.x) / 2,
            ((1.0f - ndc.y) * DISPLAY_SPACE_RES.y) / 2);
    }

	std::vector<unsigned short> GetProductOrFileVersion(bool productVersion)
	{
		wchar_t fileName[UCHAR_MAX] = {};

		if (!GetModuleFileNameW(nullptr, fileName, UCHAR_MAX))
		{
			TENLog("Can't get current assembly filename", LogLevel::Error);
			return {};
		}

		DWORD dummy;
		DWORD size = GetFileVersionInfoSizeW(fileName, &dummy);

		if (size == 0)
		{
			TENLog("GetFileVersionInfoSizeW failed", LogLevel::Error);
			return {};
		}

		std::unique_ptr<unsigned char[]> buffer(new unsigned char[size]);

		// Load version info.
		if (!GetFileVersionInfoW(fileName, 0, size, buffer.get()))
		{
			TENLog("GetFileVersionInfoW failed", LogLevel::Error);
			return {};
		}

		VS_FIXEDFILEINFO* info;
		unsigned int infoSize;

		if (!VerQueryValueW(buffer.get(), L"\\", (void**)&info, &infoSize))
		{
			TENLog("VerQueryValueW failed", LogLevel::Error);
			return {};
		}

		if (infoSize != sizeof(VS_FIXEDFILEINFO))
		{
			TENLog("VerQueryValueW returned wrong size for VS_FIXEDFILEINFO", LogLevel::Error);
			return {};
		}

		if (productVersion)
		{
			return
			{
				HIWORD(info->dwProductVersionMS),
				LOWORD(info->dwProductVersionMS),
				HIWORD(info->dwProductVersionLS),
				LOWORD(info->dwProductVersionLS)
			};
		}
		else
		{
			return
			{
				HIWORD(info->dwFileVersionMS),
				LOWORD(info->dwFileVersionMS),
				HIWORD(info->dwFileVersionLS),
				LOWORD(info->dwFileVersionLS)
			};
		}
	}
}
