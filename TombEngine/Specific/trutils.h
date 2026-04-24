#pragma once

namespace TEN::Utils
{
	// Memory utilities
	float ToMegabytes(unsigned long long bytes);
	
	// String utilities

	std::string ConstructAssetDirectory(std::string customDirectory);
	std::string ReplaceNewLineSymbols(const std::string& string);

	std::string  ToUpper(std::string string);
	std::string  ToLower(std::string string);
	std::string  ToString(const std::wstring& wString);
	std::string  ToString(const wchar_t* wString);
	std::wstring ToWString(const std::string& string);
	std::wstring ToWString(const char* cString);
	std::string  Trim(std::string string);
	bool         StartsWith(const std::string& string, const char* pref);
	int          ToInt(const std::string& string, int fallback);
	float        ToFloat(const std::string& string, float fallback);
	bool         ToBool(const std::string& string, bool fallback);

	std::vector<std::wstring> SplitString(const std::wstring& string);
	std::vector<std::wstring> SplitWords(const std::wstring& input);

	int GetHash(const std::string& string);

	// 2D space utilities

	Vector2 GetAspectCorrect2DPosition(const Vector2& pos);
	Vector2 Convert2DPositionToNDC(const Vector2& pos);
	Vector2 ConvertNDCTo2DPosition(const Vector2& ndc);

	template <typename TElement>
	bool Contains(const std::vector<TElement>& vector, const TElement& element)
	{
		auto it = std::find(vector.begin(), vector.end(), element);
		return (it != vector.end());
	}

	template <typename TElement>
	void Erase(std::vector<TElement>& vector, unsigned int elementId)
	{
		vector.erase(vector.begin() + elementId);
	}

	// Miscellanea utilities
	#define SAFE_DELETE(x) if (x != nullptr) x.reset();
	#define ARGB_TO_UINT(a,r,g,b) \
		((D3DCOLOR)((((unsigned int)(a) & 0xFF) << 24) | \
					(((unsigned int)(r) & 0xFF) << 16) | \
					(((unsigned int)(g) & 0xFF) <<  8) | \
					(((unsigned int)(b) & 0xFF))))
}
