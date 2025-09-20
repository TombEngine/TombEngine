#pragma once

template<typename T>

std::vector<T> createVector(size_t initial) 
{
	std::vector<T> vec;
	vec.reserve(initial);
	return vec;
}
