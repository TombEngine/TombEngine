#include "framework.h"

#ifdef USE_OPENGL

#include "Renderer/Native/OpenGL/GLPrimitiveBatch.h"
#include "Renderer/Native/OpenGL/GLErrorHelper.h"

namespace TEN::Renderer::Native::OpenGL
{
	GLPrimitiveBatch::GLPrimitiveBatch()
	{
		glCreateVertexArrays(1, &_vao);
		glCreateBuffers(1, &_vbo);
		glNamedBufferData(_vbo, MAX_BATCH_VERTICES * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);

		// Bind the VBO to the VAO at binding point 0.
		glVertexArrayVertexBuffer(_vao, 0, _vbo, 0, sizeof(Vertex));

		// The input layout is set by GLGraphicsDevice::SetInputLayout, which configures
		// the main VAO. For PrimitiveBatch, we rely on the currently active input layout
		// and just provide vertex data. The VAO setup here is minimal — the actual attribute
		// configuration is done when the main input layout is applied.

		_vertices.reserve(MAX_BATCH_VERTICES);
	}

	GLPrimitiveBatch::~GLPrimitiveBatch()
	{
		if (_vao) glDeleteVertexArrays(1, &_vao);
		if (_vbo) glDeleteBuffers(1, &_vbo);
	}

	void GLPrimitiveBatch::Begin()
	{
		_vertices.clear();
	}

	void GLPrimitiveBatch::DrawLine(Vertex const& v1, Vertex const& v2)
	{
		_primitiveType = GL_LINES;
		_vertices.push_back(v1);
		_vertices.push_back(v2);
	}

	void GLPrimitiveBatch::DrawTriangle(Vertex const& v1, Vertex const& v2, Vertex const& v3)
	{
		_primitiveType = GL_TRIANGLES;
		_vertices.push_back(v1);
		_vertices.push_back(v2);
		_vertices.push_back(v3);
	}

	void GLPrimitiveBatch::DrawQuad(Vertex const& v1, Vertex const& v2, Vertex const& v3, Vertex const& v4)
	{
		_primitiveType = GL_TRIANGLES;
		_vertices.push_back(v1);
		_vertices.push_back(v2);
		_vertices.push_back(v3);
		_vertices.push_back(v1);
		_vertices.push_back(v3);
		_vertices.push_back(v4);
	}

	void GLPrimitiveBatch::Flush()
	{
		if (_vertices.empty())
			return;

		// Save current VAO, bind ours for drawing, then restore.
		GLint prevVAO = 0;
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);

		glNamedBufferSubData(_vbo, 0, _vertices.size() * sizeof(Vertex), _vertices.data());
		glBindVertexArray(_vao);
		glDrawArrays(_primitiveType, 0, (GLsizei)_vertices.size());

		glBindVertexArray(prevVAO);
	}

	void GLPrimitiveBatch::End()
	{
		Flush();
	}
}

#endif
