#include "framework.h"

#ifdef USE_OPENGL

#include "Renderer/Native/OpenGL/GLSpriteBatch.h"
#include "Renderer/Native/OpenGL/GLErrorHelper.h"

namespace TEN::Renderer::Native::OpenGL
{
	static const char* s_spriteBatchVS = R"(
		#version 450 core
		layout(location=0) in vec2 aPos;
		layout(location=1) in vec2 aUV;
		layout(location=2) in vec4 aColor;
		out vec2 vUV;
		out vec4 vColor;
		uniform mat4 uProjection;
		void main() {
			gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
			vUV = aUV;
			vColor = aColor;
		}
	)";

	static const char* s_spriteBatchFS = R"(
		#version 450 core
		in vec2 vUV;
		in vec4 vColor;
		layout(binding=0) uniform sampler2D uTexture;
		layout(location=0) out vec4 FragColor;
		void main() {
			FragColor = texture(uTexture, vUV) * vColor;
		}
	)";

	static GLuint CompileInlineShader(GLenum type, const char* src)
	{
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &src, nullptr);
		glCompileShader(shader);

		GLint ok = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
		if (!ok)
		{
			char log[512];
			glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
			TENLog(std::string("SpriteBatch shader compile error: ") + log, LogLevel::Error);
		}
		return shader;
	}

	GLSpriteBatch::GLSpriteBatch()
	{
		// Build shader program.
		GLuint vs = CompileInlineShader(GL_VERTEX_SHADER, s_spriteBatchVS);
		GLuint fs = CompileInlineShader(GL_FRAGMENT_SHADER, s_spriteBatchFS);
		_program = glCreateProgram();
		glAttachShader(_program, vs);
		glAttachShader(_program, fs);
		glLinkProgram(_program);
		glDeleteShader(vs);
		glDeleteShader(fs);

		// Create VAO and VBO.
		glCreateVertexArrays(1, &_vao);
		glCreateBuffers(1, &_vbo);
		glNamedBufferData(_vbo, 6 * 8 * sizeof(float) * 256, nullptr, GL_DYNAMIC_DRAW);

		// Vertex format: vec2 pos, vec2 uv, vec4 color = 8 floats.
		int stride = 8 * sizeof(float);
		glVertexArrayVertexBuffer(_vao, 0, _vbo, 0, stride);
		glEnableVertexArrayAttrib(_vao, 0);
		glEnableVertexArrayAttrib(_vao, 1);
		glEnableVertexArrayAttrib(_vao, 2);
		glVertexArrayAttribFormat(_vao, 0, 2, GL_FLOAT, GL_FALSE, 0);
		glVertexArrayAttribFormat(_vao, 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
		glVertexArrayAttribFormat(_vao, 2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float));
		glVertexArrayAttribBinding(_vao, 0, 0);
		glVertexArrayAttribBinding(_vao, 1, 0);
		glVertexArrayAttribBinding(_vao, 2, 0);

		// Create sampler.
		glCreateSamplers(1, &_sampler);
		glSamplerParameteri(_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glSamplerParameteri(_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	GLSpriteBatch::~GLSpriteBatch()
	{
		if (_program) glDeleteProgram(_program);
		if (_vao) glDeleteVertexArrays(1, &_vao);
		if (_vbo) glDeleteBuffers(1, &_vbo);
		if (_sampler) glDeleteSamplers(1, &_sampler);
	}

	GLuint GLSpriteBatch::GetTextureHandle(ITextureBase* texture)
	{
		if (auto* tex = dynamic_cast<GLTexture2D*>(texture))
			return tex->GetGLTexture();
		if (auto* rt = dynamic_cast<GLRenderTarget2D*>(texture))
			return rt->GetGLTexture();
		return 0;
	}

	void GLSpriteBatch::Begin(SpriteSortingMode sortingMode, BlendMode blendMode)
	{
		_queue.clear();
		_blendMode = blendMode;
	}

	void GLSpriteBatch::Draw(ITextureBase* texture, RendererRectangle area, Vector4 color)
	{
		SpriteBatchQuad quad;
		quad.TextureHandle = GetTextureHandle(texture);
		quad.Left = (float)area.Left;
		quad.Top = (float)area.Top;
		quad.Right = (float)area.Right;
		quad.Bottom = (float)area.Bottom;
		quad.Color = color;
		_queue.push_back(quad);
	}

	void GLSpriteBatch::Flush()
	{
		if (_queue.empty())
			return;

		// Build vertex data: 6 vertices per quad (2 triangles).
		std::vector<float> vertices;
		vertices.reserve(_queue.size() * 6 * 8);

		auto addVertex = [&](float x, float y, float u, float v, const Vector4& c)
		{
			vertices.push_back(x); vertices.push_back(y);
			vertices.push_back(u); vertices.push_back(v);
			vertices.push_back(c.x); vertices.push_back(c.y);
			vertices.push_back(c.z); vertices.push_back(c.w);
		};

		GLuint currentTex = 0;
		int quadStart = 0;

		// Sort by texture for batching.
		std::sort(_queue.begin(), _queue.end(),
			[](const SpriteBatchQuad& a, const SpriteBatchQuad& b) { return a.TextureHandle < b.TextureHandle; });

		// Save GL state that we modify.
		GLint prevVAO = 0;
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);

		// Activate SpriteBatch's own linked program (overrides pipeline).
		glUseProgram(_program);

		// Orthographic projection matching screen coordinates.
		float proj[16] = {};
		float L = 0, R = (float)_screenW, T = 0, B = (float)_screenH;
		proj[0] = 2.0f / (R - L);
		proj[5] = 2.0f / (T - B);
		proj[10] = -1.0f;
		proj[12] = -(R + L) / (R - L);
		proj[13] = -(T + B) / (T - B);
		proj[15] = 1.0f;
		glUniformMatrix4fv(glGetUniformLocation(_program, "uProjection"), 1, GL_FALSE, proj);

		glBindVertexArray(_vao);
		glBindSampler(0, _sampler);

		for (size_t i = 0; i < _queue.size(); i++)
		{
			const auto& q = _queue[i];
			addVertex(q.Left,  q.Top,    0, 0, q.Color);
			addVertex(q.Right, q.Top,    1, 0, q.Color);
			addVertex(q.Right, q.Bottom, 1, 1, q.Color);
			addVertex(q.Left,  q.Top,    0, 0, q.Color);
			addVertex(q.Right, q.Bottom, 1, 1, q.Color);
			addVertex(q.Left,  q.Bottom, 0, 1, q.Color);
		}

		glNamedBufferData(_vbo, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

		int vertexOffset = 0;
		currentTex = _queue[0].TextureHandle;

		for (size_t i = 0; i <= _queue.size(); i++)
		{
			bool flush = (i == _queue.size()) || (_queue[i].TextureHandle != currentTex);
			if (flush)
			{
				int count = ((int)i * 6) - vertexOffset;
				glBindTextureUnit(0, currentTex);
				glDrawArrays(GL_TRIANGLES, vertexOffset, count);

				if (i < _queue.size())
				{
					currentTex = _queue[i].TextureHandle;
					vertexOffset = (int)i * 6;
				}
			}
		}

		// Restore GL state: deactivate glUseProgram so the pipeline takes over again.
		glUseProgram(0);
		glBindSampler(0, 0);
		glBindVertexArray(prevVAO);
	}

	void GLSpriteBatch::End()
	{
		Flush();
	}
}

#endif
