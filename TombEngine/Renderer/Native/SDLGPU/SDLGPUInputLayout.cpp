#include "framework.h"

#ifdef HAS_SDLGPU

#include "Renderer/Native/SDLGPU/SDLGPUInputLayout.h"
#include "Renderer/Native/SDLGPU/SDLGPUUtils.h"

namespace TEN::Renderer::Native::SDLGPU
{
	SDLGPUInputLayout::SDLGPUInputLayout(std::vector<RendererInputLayoutField> fields)
		: _fields(std::move(fields))
	{
		int offset = 0;
		for (int i = 0; i < (int)_fields.size(); ++i)
		{
			SDL_GPUVertexAttribute attr = {};
			attr.location = i;
			attr.buffer_slot = 0;
			attr.format = GetSDLGPUVertexFormat(_fields[i].Format);
			attr.offset = offset;

			_attributes.push_back(attr);
			offset += GetVertexFormatByteSize(_fields[i].Format);
		}

		_totalStride = offset;

		// Single vertex buffer binding.
		SDL_GPUVertexBufferDescription bufDesc = {};
		bufDesc.slot = 0;
		bufDesc.pitch = _totalStride;
		bufDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
		bufDesc.instance_step_rate = 0;
		_bufferDescriptions.push_back(bufDesc);
	}
}

#endif
