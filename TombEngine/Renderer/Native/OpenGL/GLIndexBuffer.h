#pragma once

#ifdef HAS_OPENGL

#include <glad/glad.h>
#include "Renderer/Graphics/IIndexBuffer.h"
#include "Renderer/Graphics/VRAMAllocation.h"

namespace TEN::Renderer::Native::OpenGL
{
	using namespace TEN::Renderer::Graphics;

	class GLIndexBuffer final : public IIndexBuffer
	{
	private:
		GLuint         _ibo        = 0;
		int            _numIndices = 0;
		VRAMAllocation _vram;

	public:
		GLIndexBuffer() = default;
		~GLIndexBuffer();

		GLuint GetGLBuffer() const { return _ibo; }

		GLIndexBuffer(int numIndices, int* indices);
		bool Update(int* data, int startIndex, int count);
	};
}

#endif
