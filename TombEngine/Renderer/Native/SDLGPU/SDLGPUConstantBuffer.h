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
		int               _size = 0;      // Allocated capacity.
		int               _pushSize = 0;  // Actual bytes to push (may be < _size).

	public:
		SDLGPUConstantBuffer() = default;
		~SDLGPUConstantBuffer() = default;

		SDLGPUConstantBuffer(int size, std::string name)
			: _size(size), _pushSize(size), _name(std::move(name)), _data(size, 0)
		{
		}

		int GetSize() const { return _pushSize; }
		int GetCapacity() const { return _size; }
		const std::string& GetName() const { return _name; }
		const void* GetData() const { return _data.data(); }
		void* GetMutableData() { return _data.data(); }

		// Set the number of bytes actually used (for variable-size CBs like instanced statics).
		void SetPushSize(int size) { _pushSize = std::min(size, _size); }

		void UpdateData(const void* data)
		{
			std::memcpy(_data.data(), data, _size);
		}
	};
}

#endif
