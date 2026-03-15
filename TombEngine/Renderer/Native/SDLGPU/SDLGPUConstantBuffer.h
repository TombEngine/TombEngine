#pragma once

#ifdef HAS_SDLGPU

#include <vector>
#include <string>
#include "Renderer/Graphics/IConstantBuffer.h"

namespace TEN::Renderer::Native::SDLGPU
{
	using namespace TEN::Renderer::Graphics;

	// SDL_GPU uses push uniforms, not GPU-side constant buffers.
	// This class holds the data on the CPU and pushes it before each draw call.
	class SDLGPUConstantBuffer final : public IConstantBuffer
	{
	private:
		std::vector<char> _data;
		std::string       _name;
		int               _size = 0;

	public:
		SDLGPUConstantBuffer() = default;
		~SDLGPUConstantBuffer() = default;

		SDLGPUConstantBuffer(int size, std::string name)
			: _size(size), _name(std::move(name)), _data(size, 0)
		{
		}

		int GetSize() const { return _size; }
		const std::string& GetName() const { return _name; }
		const void* GetData() const { return _data.data(); }
		void* GetMutableData() { return _data.data(); }

		void UpdateData(const void* data)
		{
			std::memcpy(_data.data(), data, _size);
		}
	};
}

#endif
