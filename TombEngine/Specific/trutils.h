#pragma once

namespace TEN::Utils
{
	// Memory utilities
	float ToMegabytes(unsigned long long bytes);
	
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
	std::vector<std::wstring> SplitWords(const std::wstring& input);

	int GetHash(const std::string& string);

	// 2D space utilities

	std::optional<Vector2> Get2DPosition(const Vector3& pos);
	Vector2				   GetAspectCorrect2DPosition(const Vector2& pos);
	Vector2				   Convert2DPositionToNDC(const Vector2& pos);
	Vector2				   ConvertNDCTo2DPosition(const Vector2& ndc);

	std::wstring GetBinaryPath(bool includeExeName);
	std::vector<unsigned short> GetProductOrFileVersion(bool productVersion);

	template<typename T>
	std::vector<T> RemoveDuplicates(const std::vector<T>& vector)
	{
		auto uniqueElements = std::unordered_set<T>{};
		auto newVector = std::vector<T>{};

		for (const auto& element : vector)
		{
			if (uniqueElements.find(element) == uniqueElements.end())
			{
				uniqueElements.insert(element);
				newVector.push_back(element);
			}
		}

		return newVector;
	}

	template <typename TContainer, typename TElement>
	bool Contains(const TContainer& cont, const TElement& element)
	{
		auto it = std::find(cont.begin(), cont.end(), element);
		return (it != cont.end());
	}

	template <typename T>
	void Erase(std::vector<T>& vector, unsigned int elementId)
	{
		vector.erase(vector.begin() + elementId);
	}
}
