#pragma once

#ifdef HAS_SDLGPU

#include <vector>
#include <SDL3/SDL_gpu.h>
#include "Renderer/Graphics/IInputLayout.h"
#include "Renderer/Structures/RendererInputLayout.h"

namespace TEN::Renderer::Native::SDLGPU
{
	using namespace TEN::Renderer::Graphics;
	using namespace TEN::Renderer::Structures;

	class SDLGPUInputLayout final : public IInputLayout
	{
	private:
		std::vector<SDL_GPUVertexAttribute>       _attributes;
		std::vector<SDL_GPUVertexBufferDescription> _bufferDescriptions;
		std::vector<RendererInputLayoutField>       _fields;
		int _totalStride = 0;

	public:
		SDLGPUInputLayout() = default;
		~SDLGPUInputLayout() = default;

		SDLGPUInputLayout(std::vector<RendererInputLayoutField> fields);

		const std::vector<SDL_GPUVertexAttribute>& GetAttributes() const { return _attributes; }
		const std::vector<SDL_GPUVertexBufferDescription>& GetBufferDescriptions() const { return _bufferDescriptions; }
		const std::vector<RendererInputLayoutField>& GetFields() const { return _fields; }
		int GetTotalStride() const { return _totalStride; }
	};
}

#endif
