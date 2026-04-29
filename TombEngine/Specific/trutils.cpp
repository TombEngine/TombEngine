#include "framework.h"

#include <codecvt>

#include "Renderer/Renderer.h"
#include "Renderer/RendererEnums.h"
#include "Specific/trutils.h"

using TEN::Renderer::g_Renderer;

namespace TEN::Utils
{
	float ToMegabytes(unsigned long long bytes)
	{
		return (float)bytes / (1024.0f * 1024.0f);
	}

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
			// Use .EXE path if any error is encountered.
			return std::string{};
		}

		// Use .EXE path if no any assets were found.
		return std::string{};
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

	std::string Trim(std::string string)
	{
		auto isNotSpace = [](unsigned char ch)
		{
			return !std::isspace(ch);
		};

		auto left = std::find_if(string.begin(), string.end(), isNotSpace);
		string.erase(string.begin(), left);

		auto right = std::find_if(string.rbegin(), string.rend(), isNotSpace).base();
		string.erase(right, string.end());

		return string;
	}

	bool StartsWith(const std::string& string, const char* pref)
	{
		return string.rfind(pref, 0) == 0;
	}

	int ToInt(const std::string& string, int fallback)
	{
		try
		{
			return std::stoi(string);
		}
		catch (...)
		{
			return fallback;
		}
	}

	float ToFloat(const std::string& string, float fallback)
	{
		try
		{
			return std::stof(string);
		}
		catch (...)
		{
			return fallback;
		}
	}

	bool ToBool(const std::string& string, bool fallback)
	{
		if (string == "1" || string == "true" || string == "True" || string == "TRUE")
			return true;

		if (string == "0" || string == "false" || string == "False" || string == "FALSE")
			return false;

		return fallback;
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
	
	std::vector<std::wstring> SplitWords(const std::wstring& input)
	{
		std::vector<std::wstring> words;
		std::wstringstream stream(input);
		std::wstring word;

		while (stream >> word)
			words.push_back(word);

		return words;
	}

	int GetHash(const std::string& string)
	{
		if (string.empty())
			return 0;

		unsigned int hash = 2166136261u;
		for (char c : string)
		{
			hash ^= static_cast<unsigned char>(c);
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

}
