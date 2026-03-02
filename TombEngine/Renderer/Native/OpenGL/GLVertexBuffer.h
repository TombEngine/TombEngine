#pragma once

#ifdef HAS_OPENGL

#include <glad/glad.h>
#include "Renderer/Graphics/IVertexBuffer.h"
#include "Renderer/Graphics/VRAMAllocation.h"

namespace TEN::Renderer::Native::OpenGL
{
	using namespace TEN::Renderer::Graphics;

	class GLVertexBuffer final : public IVertexBuffer
	{
	private:
		GLuint         _vbo         = 0;
		int            _numVertices = 0;
		int            _stride      = 0;
		VRAMAllocation _vram;

	public:
		GLVertexBuffer() = default;
		~GLVertexBuffer();

		GLuint GetGLBuffer() const { return _vbo; }
		int GetStride() const { return _stride; }

		GLVertexBuffer(int numVertices, int stride, void* vertices);
		bool Update(void* data, int startVertex, int count);
	};
}

#endif
