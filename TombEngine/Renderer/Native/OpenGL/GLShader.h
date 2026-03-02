#pragma once

#ifdef HAS_OPENGL

#include <glad/glad.h>
#include "Renderer/Graphics/IShader.h"

namespace TEN::Renderer::Native::OpenGL
{
	using namespace TEN::Renderer::Graphics;

	class GLShader final : public IShader
	{
	private:
		GLuint _vsProgram = 0;
		GLuint _fsProgram = 0;
		GLuint _gsProgram = 0;

	public:
		GLShader() = default;
		~GLShader();

		GLuint GetVSProgram() const { return _vsProgram; }
		GLuint GetFSProgram() const { return _fsProgram; }
		GLuint GetGSProgram() const { return _gsProgram; }

		void SetVSProgram(GLuint program) { _vsProgram = program; }
		void SetFSProgram(GLuint program) { _fsProgram = program; }
		void SetGSProgram(GLuint program) { _gsProgram = program; }
	};
}

#endif
