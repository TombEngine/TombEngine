#pragma once

#ifdef USE_OPENGL

#include <glad/glad.h>
#include "Renderer/Graphics/IPrimitiveBatch.h"

namespace TEN::Renderer::Native::OpenGL
{
	using namespace TEN::Renderer::Graphics;

	class GLPrimitiveBatch final : public IPrimitiveBatch
	{
	private:
		GLuint _vao = 0;
		GLuint _vbo = 0;

		std::vector<Vertex> _vertices;
		GLenum _primitiveType = GL_TRIANGLES;

		static constexpr int MAX_BATCH_VERTICES = 65536;

	public:
		GLPrimitiveBatch();
		~GLPrimitiveBatch();

		void Begin() override;
		void End() override;
		void DrawLine(Vertex const& v1, Vertex const& v2) override;
		void DrawTriangle(Vertex const& v1, Vertex const& v2, Vertex const& v3) override;
		void DrawQuad(Vertex const& v1, Vertex const& v2, Vertex const& v3, Vertex const& v4) override;

	private:
		void Flush();
	};
}

#endif
