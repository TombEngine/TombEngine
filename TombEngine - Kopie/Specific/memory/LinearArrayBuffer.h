#pragma once
#include <array>

// A fixed size vector-like class

namespace TEN::Memory 
{
	template<typename T, size_t Sz>

	class LinearArrayBuffer 
	{
	private:
		std::array<T, Sz> buffer;
		size_t numElements;

	public:
		LinearArrayBuffer() : numElements(0) {}

		void push_back(const T& obj) 
		{
			buffer[numElements] = obj;
			numElements = numElements + 1 >= Sz-1 ? Sz-1 : numElements + 1;
		}

		void push_back(const T&& obj) 
		{
			buffer[numElements] = obj;
			numElements = numElements+1>=Sz-1 ? Sz-1 : numElements+1;

		}

		auto begin() 
		{
			return buffer.begin();
		}

		auto end() 
		{
			return buffer.begin() + numElements;
		}

		auto cbegin() 
		{
			return buffer.cbegin();
		}

		auto cend() 
		{
			return buffer.cbegin() + numElements;
		}

		auto clear() 
		{
			numElements = 0;
		}

		T& operator[](size_t index) 
		{
			return buffer[index];
		}

		auto size() 
		{
			return numElements;
		}

		auto max_Size() 
		{
			return buffer.max_size();
		}

		auto capacity() 
		{
			return buffer.max_size();
		}
	};
}
