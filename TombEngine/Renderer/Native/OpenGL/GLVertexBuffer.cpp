#include "framework.h"

#ifdef HAS_OPENGL

#include "Renderer/Native/OpenGL/GLVertexBuffer.h"
#include "Renderer/Native/OpenGL/GLUtils.h"
#include "Renderer/Native/OpenGL/GLErrorHelper.h"

namespace TEN::Renderer::Native::OpenGL
{
	GLVertexBuffer::GLVertexBuffer(int numVertices, int stride, void* vertices)
	{
		_stride = stride;
		_numVertices = numVertices;

		int bufferSize = stride * (numVertices > 0 ? numVertices : 1);

		glCreateBuffers(1, &_vbo);
		glNamedBufferData(_vbo, bufferSize,
			(numVertices > 0 && vertices) ? vertices : nullptr,
			GL_DYNAMIC_DRAW);

		ThrowIfGLError("CreateVertexBuffer (" + std::to_string(numVertices) +
			" vertices x " + std::to_string(stride) + " stride)");

		_vram = VRAMAllocation(VRAMCategory::VertexBuffer, bufferSize,
			"VertexBuffer allocated: " + std::to_string(numVertices) + " vertices x " +
			std::to_string(stride) + " stride (" + BytesToMBString(bufferSize) + " MB)");
	}

	GLVertexBuffer::~GLVertexBuffer()
	{
		if (_vbo)
			glDeleteBuffers(1, &_vbo);
	}

	bool GLVertexBuffer::Update(void* data, int startVertex, int count)
	{
		void* ptr = glMapNamedBufferRange(_vbo, 0, (GLsizeiptr)count * _stride,
			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

		if (ptr)
		{
			auto* src = static_cast<std::byte*>(data) + (size_t)startVertex * _stride;
			std::memcpy(ptr, src, (size_t)count * _stride);
			glUnmapNamedBuffer(_vbo);
			return true;
		}

		TENLog("Could not update vertex buffer!", LogLevel::Error);
		return false;
	}
}

#endif
