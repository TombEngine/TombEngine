#pragma once

namespace TEN::Utils
{
	// String utilities

	std::string ConstructAssetDirectory(std::string customDirectory);
	std::string ReplaceNewLineSymbols(const std::string& string);

	std::string	 ToUpper(std::string string);
	std::string	 ToLower(std::string string);
	std::string	 ToString(const std::wstring& wString);
	std::string	 ToString(const wchar_t* wString);
	std::wstring ToWString(const std::string& string);
	std::wstring ToWString(const char* cString);

	std::vector<std::wstring> SplitString(const std::wstring& string);

	int GetHash(const std::string& string);

	// 2D space utilities

	Vector2 GetAspectCorrect2DPosition(const Vector2& pos);
	Vector2 Convert2DPositionToNDC(const Vector2& pos);
	Vector2 ConvertNDCTo2DPosition(const Vector2& ndc);

	std::vector<unsigned short> GetProductOrFileVersion(bool productVersion);

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
}
