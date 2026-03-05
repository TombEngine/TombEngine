#include "framework.h"

#ifdef HAS_OPENGL

#include "Renderer/Native/OpenGL/GLGraphicsDevice.h"
#include "Renderer/Native/OpenGL/GLPlatformHelpers.h"
#include "Renderer/Native/OpenGL/GLUtils.h"
#include "Renderer/Native/OpenGL/GLErrorHelper.h"
#include "Specific/EngineMain.h"
#include "Specific/configuration.h"
#include "Specific/trutils.h"

#include <stb_image_write.h>

extern GameConfiguration g_Configuration;

using namespace TEN::Renderer::Graphics;

namespace TEN::Renderer::Native::OpenGL
{
	// ========================================================================
	// Helpers
	// ========================================================================

	GLuint GLGraphicsDevice::GetTextureHandle(ITextureBase* texture)
	{
		if (auto* tex = dynamic_cast<GLTexture2D*>(texture))
			return tex->GetGLTexture();
		if (auto* rt = dynamic_cast<GLRenderTarget2D*>(texture))
			return rt->GetGLTexture();
		return 0;
	}

	GLuint GLGraphicsDevice::GetOrCreateFBO(
		GLuint colorTex, GLuint depthTex,
		bool colorIsArray, int colorArrayIndex,
		bool depthIsArray, int depthArrayIndex)
	{
		// Default framebuffer (backbuffer).
		if (colorTex == 0 && depthTex == 0)
			return 0;

		uint64_t key = ((uint64_t)colorTex << 32) | (uint64_t)depthTex;
		// Include array indices in key for array textures.
		if (colorIsArray || depthIsArray)
			key ^= ((uint64_t)(colorArrayIndex + 1) << 48) | ((uint64_t)(depthArrayIndex + 1) << 16);

		auto it = _fboCache.find(key);
		if (it != _fboCache.end())
			return it->second;

		GLuint fbo;
		glCreateFramebuffers(1, &fbo);

		if (colorTex != 0)
		{
			if (colorIsArray)
				glNamedFramebufferTextureLayer(fbo, GL_COLOR_ATTACHMENT0, colorTex, 0, colorArrayIndex);
			else
				glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, colorTex, 0);
		}
		else
		{
			glNamedFramebufferDrawBuffer(fbo, GL_NONE);
			glNamedFramebufferReadBuffer(fbo, GL_NONE);
		}

		if (depthTex != 0)
		{
			if (depthIsArray)
				glNamedFramebufferTextureLayer(fbo, GL_DEPTH_ATTACHMENT, depthTex, 0, depthArrayIndex);
			else
				glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, depthTex, 0);
		}

		GLenum status = glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			TENLog("Incomplete FBO: status=" + std::to_string(status), LogLevel::Error);
		}

		_fboCache[key] = fbo;
		return fbo;
	}

	std::string GLGraphicsDevice::ReadShaderFile(const std::wstring& path)
	{
		auto fspath = std::filesystem::path(path);
		std::ifstream file(fspath);
		if (!file)
			throw std::runtime_error("Cannot open shader file: " + TEN::Utils::ToString(path));

		std::stringstream ss;
		ss << file.rdbuf();
		return ss.str();
	}

	std::string GLGraphicsDevice::ResolveIncludes(const std::string& source, const std::wstring& directory, std::set<std::string>& alreadyIncluded)
	{
		std::string result;
		std::istringstream stream(source);
		std::string line;

		while (std::getline(stream, line))
		{
			// Check for #include "filename" pattern.
			auto includePos = line.find("#include");
			if (includePos != std::string::npos)
			{
				auto quoteStart = line.find('"', includePos);
				auto quoteEnd = line.find('"', quoteStart + 1);

				if (quoteStart != std::string::npos && quoteEnd != std::string::npos)
				{
					auto includeFile = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

					// Skip if already included (handles #ifndef guards at source level too).
					if (alreadyIncluded.count(includeFile))
					{
						result += "// (already included: " + includeFile + ")\n";
						continue;
					}

					alreadyIncluded.insert(includeFile);

					// Read and recursively resolve the included file.
					auto includePath = directory + std::wstring(includeFile.begin(), includeFile.end());
					try
					{
						auto includedSource = ReadShaderFile(includePath);
						result += ResolveIncludes(includedSource, directory, alreadyIncluded);
					}
					catch (const std::exception& e)
					{
						TENLog("Failed to resolve shader #include \"" + includeFile + "\": " + e.what(), LogLevel::Error);
						result += "// ERROR: failed to include " + includeFile + "\n";
					}

					continue;
				}
			}

			result += line + "\n";
		}

		return result;
	}

	GLuint GLGraphicsDevice::CompileSeparableProgram(GLenum shaderType, const std::string& source, const std::string& label)
	{
		const char* src = source.c_str();
		GLuint program = glCreateShaderProgramv(shaderType, 1, &src);

		GLint linkStatus = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);

		if (!linkStatus)
		{
			GLint logLen = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
			std::string log(logLen, '\0');
			glGetProgramInfoLog(program, logLen, nullptr, log.data());
			TENLog("Shader link error [" + label + "]: " + log, LogLevel::Error);
			glDeleteProgram(program);

			// Drain GL errors left by the failed compilation so they don't leak to later calls.
			DrainGLErrors();
			return 0;
		}

		return program;
	}

	// ========================================================================
	// Device Lifecycle
	// ========================================================================

	void GLGraphicsDevice::CreateDevice()
	{
		TENLog("OpenGL 4.5 renderer", LogLevel::Info);

		// Set GL attributes before context creation.
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

		if constexpr (DEBUG_BUILD)
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0); // We manage our own depth buffers.
	}

	void GLGraphicsDevice::Initialize()
	{
		auto window = g_Platform->GetSDL3Window();

		_glContext = SDL_GL_CreateContext(window);
		if (!_glContext)
			throw std::runtime_error(std::string("Failed to create OpenGL context: ") + SDL_GetError());

		if (!gladLoadGLLoader((GLADloadproc)GetPlatformGLLoader()))
			throw std::runtime_error("Failed to initialize GLAD");

		// Verify critical GL 1.0 function pointers were loaded.
		if (!glad_glEnable)
			TENLog("GLAD WARNING: glEnable is NULL after glad init!", LogLevel::Error);
		if (!glad_glDisable)
			TENLog("GLAD WARNING: glDisable is NULL after glad init!", LogLevel::Error);
		if (!glad_glClear)
			TENLog("GLAD WARNING: glClear is NULL after glad init!", LogLevel::Error);

		TENLog(std::string("OpenGL version: ") + (const char*)glGetString(GL_VERSION), LogLevel::Info);
		TENLog(std::string("OpenGL renderer: ") + (const char*)glGetString(GL_RENDERER), LogLevel::Info);

		if constexpr (DEBUG_BUILD)
			InitGLDebugOutput();

		// Match DX11 coordinate conventions.
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
		glFrontFace(GL_CW);
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

		// Create default VAO (required for core profile).
		glCreateVertexArrays(1, &_defaultVAO);
		glBindVertexArray(_defaultVAO);

		// Create separable shader pipeline.
		glCreateProgramPipelines(1, &_pipeline);
		glBindProgramPipeline(_pipeline);

		// Create sampler objects.
		auto createSampler = [](GLenum minFilter, GLenum magFilter, GLenum wrapMode, bool comparison = false) -> GLuint
		{
			GLuint sampler;
			glCreateSamplers(1, &sampler);
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, minFilter);
			glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, magFilter);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, wrapMode);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, wrapMode);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, wrapMode);
			if (comparison)
			{
				glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
				glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
			}
			return sampler;
		};

		_samplers[SamplerStateRegister::PointWrap] = createSampler(GL_NEAREST, GL_NEAREST, GL_REPEAT);
		_samplers[SamplerStateRegister::LinearWrap] = createSampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT);
		_samplers[SamplerStateRegister::LinearClamp] = createSampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
		_samplers[SamplerStateRegister::ShadowMap] = createSampler(GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, true);

		// Anisotropic samplers (GL_EXT_texture_filter_anisotropic).
		float maxAniso = 1.0f;
		if (GLAD_GL_EXT_texture_filter_anisotropic)
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
		float aniso = std::min(maxAniso, 16.0f);

		_samplers[SamplerStateRegister::AnisotropicWrap] = createSampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_REPEAT);
		glSamplerParameterf(_samplers[SamplerStateRegister::AnisotropicWrap], GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);

		_samplers[SamplerStateRegister::AnisotropicClamp] = createSampler(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE);
		glSamplerParameterf(_samplers[SamplerStateRegister::AnisotropicClamp], GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);

		// Enable scissor test (always on in DX11 rasterizer states).
		glEnable(GL_SCISSOR_TEST);

		ThrowIfGLError("Initialize");
	}

	void GLGraphicsDevice::ReleaseContext()
	{
		if (!SDL_GL_MakeCurrent(g_Platform->GetSDL3Window(), nullptr))
			TENLog(std::string("ReleaseContext failed: ") + SDL_GetError(), LogLevel::Error);
		else
			TENLog("GL context released from current thread.", LogLevel::Info);
	}

	void GLGraphicsDevice::BindContext()
	{
		if (!SDL_GL_MakeCurrent(g_Platform->GetSDL3Window(), _glContext))
		{
			TENLog(std::string("BindContext failed: ") + SDL_GetError(), LogLevel::Error);
			return;
		}

		TENLog("GL context bound to current thread.", LogLevel::Info);

		// Reload GL function pointers on the new thread.
		if (!gladLoadGLLoader((GLADloadproc)GetPlatformGLLoader()))
			TENLog("Failed to reload GLAD on game thread!", LogLevel::Error);
		else
			TENLog("GLAD reloaded on game thread.", LogLevel::Info);
	}

	GLGraphicsDevice::~GLGraphicsDevice()
	{
		// Clean up FBO cache.
		for (auto& [key, fbo] : _fboCache)
		{
			if (fbo) glDeleteFramebuffers(1, &fbo);
		}

		// Clean up samplers.
		for (auto& [reg, sampler] : _samplers)
		{
			if (sampler) glDeleteSamplers(1, &sampler);
		}

		if (_pipeline) glDeleteProgramPipelines(1, &_pipeline);
		if (_defaultVAO) glDeleteVertexArrays(1, &_defaultVAO);
		if (_glContext) SDL_GL_DestroyContext(_glContext);
	}

	std::unique_ptr<IRenderSurface2D> GLGraphicsDevice::InitializeSwapChain(int width, int height)
	{
		_screenWidth = width;
		_screenHeight = height;

		// Invalidate the FBO cache: old render target textures have been deleted
		// by InitializeScreen and OpenGL may reuse the same texture IDs.
		// Stale cached FBOs would reference deleted textures.
		for (auto& [key, fbo] : _fboCache)
		{
			if (fbo)
				glDeleteFramebuffers(1, &fbo);
		}
		_fboCache.clear();
		_backbufferFBO = 0;

		if (!g_Configuration.EnableHighFramerate)
			_refreshRate = 30;
		else
		{
			auto* mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
			_refreshRate = mode ? (int)mode->refresh_rate : 60;
			if (_refreshRate == 0) _refreshRate = 60;
		}

		SDL_GL_SetSwapInterval(1);

		// Backbuffer render target (real color texture, blitted to FBO 0 in Present).
		auto backbufferRT = std::make_unique<GLRenderTarget2D>(width, height);
		auto backbufferDepth = std::make_unique<GLDepthTarget>(width, height, GL_DEPTH_COMPONENT32F);

		// Eagerly create and cache the backbuffer FBO so Present() can blit from it.
		_backbufferFBO = GetOrCreateFBO(
			backbufferRT->GetGLTexture(), backbufferDepth->GetGLTexture(),
			false, 0, false, 0);

		return std::make_unique<IRenderSurface2D>(std::move(backbufferRT), std::move(backbufferDepth));
	}

	void GLGraphicsDevice::ResizeSwapChain(int width, int height)
	{
		_screenWidth = width;
		_screenHeight = height;
	}

	void GLGraphicsDevice::Present()
	{
		// Blit from the backbuffer FBO (real color texture) to FBO 0 (default framebuffer).
		if (_backbufferFBO != 0)
		{
			// Disable scissor test — it clips the blit destination.
			glDisable(GL_SCISSOR_TEST);

			glNamedFramebufferReadBuffer(_backbufferFBO, GL_COLOR_ATTACHMENT0);

			// Query actual drawable size in pixels (may differ from logical window size due to HiDPI scaling).
			int drawW = _screenWidth;
			int drawH = _screenHeight;
			auto* window = g_Platform->GetSDL3Window();
			if (window)
				SDL_GetWindowSizeInPixels(window, &drawW, &drawH);

			glBlitNamedFramebuffer(
				_backbufferFBO, 0,
				0, 0, _screenWidth, _screenHeight,
				0, 0, drawW, drawH,
				GL_COLOR_BUFFER_BIT, GL_NEAREST);

			glEnable(GL_SCISSOR_TEST);

			GLenum err = glGetError();
			if (err != GL_NO_ERROR)
				TENLog("Present blit GL error: 0x" + std::to_string(err), LogLevel::Error);
		}

		SDL_GL_SwapWindow(g_Platform->GetSDL3Window());
	}

	void GLGraphicsDevice::ClearState()
	{
		glBindVertexArray(_defaultVAO);
		glBindProgramPipeline(_pipeline);
		glUseProgram(0);
	}

	void GLGraphicsDevice::ClearDefaultFramebuffer()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		SDL_GL_SwapWindow(g_Platform->GetSDL3Window());
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void GLGraphicsDevice::Flush()
	{
		glFlush();
	}

	// ========================================================================
	// Vertex/Index Buffers
	// ========================================================================

	std::unique_ptr<IVertexBuffer> GLGraphicsDevice::CreateVertexBuffer(int numVertices, int vertexSize, void* data)
	{
		return std::make_unique<GLVertexBuffer>(numVertices, vertexSize, data);
	}

	void GLGraphicsDevice::UpdateVertexBuffer(IVertexBuffer* vertexBuffer, int startVertex, int count, void* data)
	{
		auto* glVB = static_cast<GLVertexBuffer*>(vertexBuffer);
		glVB->Update(data, startVertex, count);
	}

	// Map DX11 semantic name + index to the fixed GLSL vertex attribute location
	// as defined in VertexInput.glsl.
	static GLuint SemanticToLocation(const char* semantic, int slot)
	{
		if (strcmp(semantic, "POSITION") == 0 && slot == 0)      return 0;
		if (strcmp(semantic, "NORMAL") == 0 && slot == 0)        return 1;
		if (strcmp(semantic, "TEXCOORD") == 0 && slot == 0)      return 2;
		if (strcmp(semantic, "COLOR") == 0 && slot == 0)         return 3;
		if (strcmp(semantic, "TANGENT") == 0 && slot == 0)       return 4;
		if (strcmp(semantic, "NORMAL") == 0 && slot == 1)        return 5;  // FaceNormal
		if (strcmp(semantic, "BONEINDICES") == 0 && slot == 0)   return 6;
		if (strcmp(semantic, "BONEWEIGHTS") == 0 && slot == 0)   return 7;
		if (strcmp(semantic, "EFFECTS") == 0 && slot == 0)       return 8;
		if (strcmp(semantic, "EFFECTS") == 0 && slot == 1)       return 9;
		return 0;
	}

	void GLGraphicsDevice::BindVertexBuffer(IVertexBuffer* vertexBuffer)
	{
		auto* glVB = static_cast<GLVertexBuffer*>(vertexBuffer);
		// Bind VBO to binding point 0 on the current VAO.
		int stride = glVB->GetStride();
		glVertexArrayVertexBuffer(_defaultVAO, 0, glVB->GetGLBuffer(), 0, stride);

		// Set all attribute bindings to use binding point 0, using semantic-based locations.
		if (_currentInputLayout)
		{
			const auto& fields = _currentInputLayout->GetFields();
			for (GLuint i = 0; i < (GLuint)fields.size(); i++)
			{
				GLuint location = SemanticToLocation(fields[i].Semantic, fields[i].Slot);
				glVertexArrayAttribBinding(_defaultVAO, location, 0);
			}
		}
	}

	std::unique_ptr<IIndexBuffer> GLGraphicsDevice::CreateIndexBuffer(int numIndices, int* data)
	{
		return std::make_unique<GLIndexBuffer>(numIndices, data);
	}

	void GLGraphicsDevice::UpdateIndexBuffer(IIndexBuffer* indexBuffer, int numIndices, int startIndex, int* data)
	{
		auto* glIB = static_cast<GLIndexBuffer*>(indexBuffer);
		glIB->Update(data, startIndex, numIndices);
	}

	void GLGraphicsDevice::BindIndexBuffer(IIndexBuffer* indexBuffer)
	{
		auto* glIB = static_cast<GLIndexBuffer*>(indexBuffer);
		glVertexArrayElementBuffer(_defaultVAO, glIB->GetGLBuffer());
	}

	// ========================================================================
	// Render Surfaces
	// ========================================================================

	std::unique_ptr<IRenderSurface2D> GLGraphicsDevice::CreateRenderSurface2D(int width, int height, SurfaceFormat colorFormat, bool isTypeless, DepthFormat depthFormat)
	{
		auto fmt = GetGLFormat(colorFormat);
		auto rt = std::make_unique<GLRenderTarget2D>(width, height, fmt.InternalFormat, isTypeless);

		std::unique_ptr<IDepthTarget> depth = nullptr;
		if (depthFormat != DepthFormat::None)
			depth = std::make_unique<GLDepthTarget>(width, height, GetGLDepthFormat(depthFormat));

		return std::make_unique<IRenderSurface2D>(std::move(rt), std::move(depth));
	}

	std::unique_ptr<IRenderSurface2D> GLGraphicsDevice::CreateRenderSurface2D(int width, int height, int arraySize, SurfaceFormat colorFormat, DepthFormat depthFormat)
	{
		auto fmt = GetGLFormat(colorFormat);
		auto rt = std::make_unique<GLRenderTarget2D>(width, height, arraySize, fmt.InternalFormat);

		std::unique_ptr<IDepthTarget> depth = nullptr;
		if (depthFormat != DepthFormat::None)
			depth = std::make_unique<GLDepthTarget>(width, height, arraySize, GetGLDepthFormat(depthFormat));

		return std::make_unique<IRenderSurface2D>(std::move(rt), std::move(depth));
	}

	std::unique_ptr<IRenderSurface2D> GLGraphicsDevice::CreateRenderSurface2D(IRenderSurface2D* parentRenderTarget, SurfaceFormat colorFormat)
	{
		auto* parentRT = static_cast<GLRenderTarget2D*>(parentRenderTarget->GetRenderTarget());
		auto fmt = GetGLFormat(colorFormat);

		auto rt = std::make_unique<GLRenderTarget2D>(
			parentRT->GetGLTexture(), parentRT->GetWidth(), parentRT->GetHeight(), fmt.InternalFormat);

		return std::make_unique<IRenderSurface2D>(std::move(rt), nullptr);
	}

	IRenderTargetCube* GLGraphicsDevice::CreateRenderTargetCube(int size, SurfaceFormat colorFormat)
	{
		return nullptr; // Not implemented yet.
	}

	// ========================================================================
	// Textures
	// ========================================================================

	std::unique_ptr<ITexture2D> GLGraphicsDevice::CreateTexture2D(int width, int height, SurfaceFormat format, void* data)
	{
		auto fmt = GetGLFormat(format);
		return std::make_unique<GLTexture2D>(width, height, fmt.InternalFormat, fmt.Format, fmt.Type, data);
	}

	std::unique_ptr<ITexture2D> GLGraphicsDevice::CreateTexture2DFromFile(const std::string fileName)
	{
		return std::make_unique<GLTexture2D>(fileName);
	}

	std::unique_ptr<ITexture2D> GLGraphicsDevice::CreateTexture2DFromFileInMemory(int dataSize, unsigned char* data)
	{
		return std::make_unique<GLTexture2D>(dataSize, data);
	}

	void GLGraphicsDevice::UpdateTexture2D(ITexture2D* texture, std::vector<char> data)
	{
		auto* glTex = static_cast<GLTexture2D*>(texture);
		int width = texture->GetWidth();
		int height = texture->GetHeight();

		glTextureSubImage2D(glTex->GetGLTexture(), 0, 0, 0, width, height,
			GL_RGBA, GL_UNSIGNED_BYTE, data.data());
	}

	// ========================================================================
	// State Management
	// ========================================================================

	void GLGraphicsDevice::SetBlendMode(BlendMode blendMode)
	{
		switch (blendMode)
		{
		case BlendMode::Opaque:
		case BlendMode::AlphaTest:
			glDisable(GL_BLEND);
			break;

		case BlendMode::AlphaBlend:
		case BlendMode::FastAlphaBlend:
			glEnable(GL_BLEND);
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glBlendEquation(GL_FUNC_ADD);
			break;

		case BlendMode::Additive:
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			glBlendEquation(GL_FUNC_ADD);
			break;

		case BlendMode::Subtractive:
			glEnable(GL_BLEND);
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
			break;

		case BlendMode::Screen:
			glEnable(GL_BLEND);
			glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_COLOR, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			glBlendEquation(GL_FUNC_ADD);
			break;

		case BlendMode::Lighten:
			glEnable(GL_BLEND);
			glBlendFuncSeparate(GL_ONE, GL_ONE, GL_SRC_ALPHA, GL_DST_ALPHA);
			glBlendEquationSeparate(GL_MAX, GL_FUNC_ADD);
			break;

		case BlendMode::Exclude:
			glEnable(GL_BLEND);
			glBlendFuncSeparate(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_ONE, GL_ZERO);
			glBlendEquationSeparate(GL_FUNC_SUBTRACT, GL_FUNC_ADD);
			break;
		}
	}

	void GLGraphicsDevice::SetDepthState(DepthState depthState)
	{
		switch (depthState)
		{
		case DepthState::Write:
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LEQUAL);
			glDepthMask(GL_TRUE);
			break;

		case DepthState::Read:
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LEQUAL);
			glDepthMask(GL_FALSE);
			break;

		case DepthState::None:
			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_FALSE);
			break;
		}
	}

	void GLGraphicsDevice::SetCullMode(CullMode cullMode)
	{
		switch (cullMode)
		{
		case CullMode::None:
			glDisable(GL_CULL_FACE);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			break;

		case CullMode::Clockwise:
			glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			break;

		case CullMode::CounterClockwise:
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			break;

		case CullMode::Wireframe:
			glDisable(GL_CULL_FACE);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			break;
		}
	}

	void GLGraphicsDevice::SetScissor(RendererRectangle rectangle)
	{
		int y = _screenHeight - rectangle.Bottom;
		glScissor(rectangle.Left, y, rectangle.Right - rectangle.Left, rectangle.Bottom - rectangle.Top);
	}

	void GLGraphicsDevice::SetScissor(RendererViewport viewport)
	{
		int y = _screenHeight - viewport.Height;
		glScissor(viewport.X, y, viewport.Width - viewport.X, viewport.Height - viewport.Y);
	}

	void GLGraphicsDevice::SetViewport(RendererViewport viewport)
	{
		glViewport(viewport.X, viewport.Y, viewport.Width, viewport.Height);
		glDepthRangef(viewport.MinDepth, viewport.MaxDepth);

		// Set matching scissor to avoid clipping issues.
		glScissor(viewport.X, viewport.Y, viewport.Width, viewport.Height);
	}

	void GLGraphicsDevice::SetPrimitiveType(PrimitiveType primitiveType)
	{
		switch (primitiveType)
		{
		case PrimitiveType::TriangleList:
			_primitiveType = GL_TRIANGLES;
			break;
		case PrimitiveType::TriangleStrip:
			_primitiveType = GL_TRIANGLE_STRIP;
			break;
		case PrimitiveType::LineList:
			_primitiveType = GL_LINES;
			break;
		}
	}

	// ========================================================================
	// Texture/Sampler Binding
	// ========================================================================

	void GLGraphicsDevice::BindTexture(TextureRegister registerType, ITextureBase* texture, SamplerStateRegister samplerType)
	{
		GLuint unit = (GLuint)registerType;
		GLuint texHandle = GetTextureHandle(texture);

		glBindTextureUnit(unit, texHandle);

		auto it = _samplers.find(samplerType);
		if (it != _samplers.end())
			glBindSampler(unit, it->second);
	}

	// ========================================================================
	// Constant Buffers
	// ========================================================================

	std::unique_ptr<IConstantBuffer> GLGraphicsDevice::CreateConstantBuffer(int size, std::wstring name)
	{
		return std::make_unique<GLConstantBuffer>(size, name);
	}

	void GLGraphicsDevice::UpdateConstantBuffer(IConstantBuffer* constantBuffer, void* data)
	{
		auto* glCB = static_cast<GLConstantBuffer*>(constantBuffer);
		glCB->UpdateData(data);
	}

	void GLGraphicsDevice::BindConstantBuffer(ShaderStage shaderStage, ConstantBufferRegister constantBufferType, IConstantBuffer* buffer)
	{
		auto* glCB = static_cast<GLConstantBuffer*>(buffer);
		GLuint bindingIndex = static_cast<GLuint>(constantBufferType);
		glBindBufferBase(GL_UNIFORM_BUFFER, bindingIndex, glCB->GetGLBuffer());
	}

	// ========================================================================
	// Draw Calls
	// ========================================================================

	void GLGraphicsDevice::DrawIndexedTriangles(int count, int baseIndex, int baseVertex)
	{
		glBindVertexArray(_defaultVAO);
		glDrawElementsBaseVertex(_primitiveType, count, GL_UNSIGNED_INT,
			(void*)(intptr_t)(baseIndex * sizeof(GLuint)), baseVertex);
	}

	void GLGraphicsDevice::DrawIndexedInstancedTriangles(int count, int instances, int baseIndex, int baseVertex)
	{
		glBindVertexArray(_defaultVAO);
		glDrawElementsInstancedBaseVertex(_primitiveType, count, GL_UNSIGNED_INT,
			(void*)(intptr_t)(baseIndex * sizeof(GLuint)), instances, baseVertex);
	}

	void GLGraphicsDevice::DrawInstancedTriangles(int count, int instances, int baseVertex)
	{
		glBindVertexArray(_defaultVAO);
		glDrawArraysInstancedBaseInstance(_primitiveType, baseVertex, count, instances, 0);
	}

	void GLGraphicsDevice::DrawTriangles(int count, int baseVertex)
	{
		glBindVertexArray(_defaultVAO);
		glDrawArrays(_primitiveType, baseVertex, count);
	}

	// ========================================================================
	// Render Target Clearing
	// ========================================================================

	void GLGraphicsDevice::ClearRenderTarget2D(IRenderTarget2D* renderTarget, Vector4 clearColor)
	{
		auto* glRT = static_cast<GLRenderTarget2D*>(renderTarget);

		// DX11 ClearRenderTargetView ignores scissor/color mask — replicate that behavior.
		glDisable(GL_SCISSOR_TEST);

		GLuint fbo = GetOrCreateFBO(glRT->GetGLTexture(), 0, false, 0, false, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glClearBufferfv(GL_COLOR, 0, &clearColor.x);
		glBindFramebuffer(GL_FRAMEBUFFER, _currentFBO);

		glEnable(GL_SCISSOR_TEST);
	}

	void GLGraphicsDevice::ClearRenderTarget2D(IRenderTarget2D* renderTarget, int arrayIndex, Vector4 clearColor)
	{
		auto* glRT = static_cast<GLRenderTarget2D*>(renderTarget);

		glDisable(GL_SCISSOR_TEST);

		GLuint fbo = GetOrCreateFBO(glRT->GetGLTexture(), 0, glRT->IsArray(), arrayIndex, false, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glClearBufferfv(GL_COLOR, 0, &clearColor.x);
		glBindFramebuffer(GL_FRAMEBUFFER, _currentFBO);

		glEnable(GL_SCISSOR_TEST);
	}

	void GLGraphicsDevice::ClearDepthStencil(IDepthTarget* depthTarget, DepthStencilClearFlags clearFlags, float depth, unsigned char stencil)
	{
		auto* glDT = static_cast<GLDepthTarget*>(depthTarget);

		// DX11 ClearDepthStencilView ignores scissor and depth write mask.
		glDisable(GL_SCISSOR_TEST);
		GLboolean prevDepthMask = GL_TRUE;
		glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
		glDepthMask(GL_TRUE);

		GLuint fbo = GetOrCreateFBO(0, glDT->GetGLTexture(), false, 0, false, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		if (clearFlags == DepthStencilClearFlags::Depth || clearFlags == DepthStencilClearFlags::DepthAndStencil)
			glClearBufferfv(GL_DEPTH, 0, &depth);
		if (clearFlags == DepthStencilClearFlags::Stencil || clearFlags == DepthStencilClearFlags::DepthAndStencil)
		{
			GLint s = stencil;
			glClearBufferiv(GL_STENCIL, 0, &s);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, _currentFBO);
		glDepthMask(prevDepthMask);
		glEnable(GL_SCISSOR_TEST);
	}

	void GLGraphicsDevice::ClearDepthStencil(IDepthTarget* depthTarget, int arrayIndex, DepthStencilClearFlags clearFlags, float depth, unsigned char stencil)
	{
		auto* glDT = static_cast<GLDepthTarget*>(depthTarget);

		glDisable(GL_SCISSOR_TEST);
		GLboolean prevDepthMask = GL_TRUE;
		glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
		glDepthMask(GL_TRUE);

		GLuint fbo = GetOrCreateFBO(0, glDT->GetGLTexture(), false, 0, glDT->IsArray(), arrayIndex);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);

		if (clearFlags == DepthStencilClearFlags::Depth || clearFlags == DepthStencilClearFlags::DepthAndStencil)
			glClearBufferfv(GL_DEPTH, 0, &depth);
		if (clearFlags == DepthStencilClearFlags::Stencil || clearFlags == DepthStencilClearFlags::DepthAndStencil)
		{
			GLint s = stencil;
			glClearBufferiv(GL_STENCIL, 0, &s);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, _currentFBO);
		glDepthMask(prevDepthMask);
		glEnable(GL_SCISSOR_TEST);
	}

	// ========================================================================
	// Render Target Binding
	// ========================================================================

	void GLGraphicsDevice::BindRenderTarget(IRenderTarget2D* renderTarget, IDepthTarget* depthTarget)
	{
		auto* glRT = static_cast<GLRenderTarget2D*>(renderTarget);
		GLuint colorTex = glRT ? glRT->GetGLTexture() : 0;
		GLuint depthTex = 0;
		bool depthIsArray = false;

		if (depthTarget)
		{
			auto* glDT = static_cast<GLDepthTarget*>(depthTarget);
			depthTex = glDT->GetGLTexture();
			depthIsArray = glDT->IsArray();
		}

		bool colorIsArray = glRT && glRT->IsArray();

		_currentFBO = GetOrCreateFBO(colorTex, depthTex, colorIsArray, 0, depthIsArray, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, _currentFBO);

		if (_currentFBO != 0 && colorTex != 0)
		{
			GLenum bufs[] = { GL_COLOR_ATTACHMENT0 };
			glNamedFramebufferDrawBuffers(_currentFBO, 1, bufs);
		}

		// Reset scissor to full render target size (DX11 does this implicitly).
		int w = glRT ? glRT->GetWidth() : _screenWidth;
		int h = glRT ? glRT->GetHeight() : _screenHeight;
		glScissor(0, 0, w, h);
	}

	void GLGraphicsDevice::BindRenderTarget(IRenderTargetBinding renderTarget, IDepthTargetBinding depthTarget)
	{
		auto* glRT = static_cast<GLRenderTarget2D*>(renderTarget.RenderTarget);
		GLuint colorTex = glRT ? glRT->GetGLTexture() : 0;
		GLuint depthTex = 0;
		bool depthIsArray = false;
		int depthArrayIndex = depthTarget.ArrayIndex;

		if (depthTarget.DepthTarget)
		{
			auto* glDT = static_cast<GLDepthTarget*>(depthTarget.DepthTarget);
			depthTex = glDT->GetGLTexture();
			depthIsArray = glDT->IsArray();
		}

		bool colorIsArray = glRT && glRT->IsArray();

		_currentFBO = GetOrCreateFBO(colorTex, depthTex, colorIsArray, renderTarget.ArrayIndex, depthIsArray, depthArrayIndex);
		glBindFramebuffer(GL_FRAMEBUFFER, _currentFBO);

		// Reset scissor to full render target size (DX11 does this implicitly).
		int w = glRT ? glRT->GetWidth() : _screenWidth;
		int h = glRT ? glRT->GetHeight() : _screenHeight;
		glScissor(0, 0, w, h);
	}

	void GLGraphicsDevice::BindRenderTargets(std::vector<IRenderTarget2D*> renderTargets, IDepthTarget* depthTarget)
	{
		if (renderTargets.empty())
			return;

		// For MRT, we need a unique FBO. Use first color + depth as cache key, and attach the rest.
		auto* firstRT = static_cast<GLRenderTarget2D*>(renderTargets[0]);
		GLuint depthTex = 0;
		if (depthTarget)
		{
			auto* glDT = static_cast<GLDepthTarget*>(depthTarget);
			depthTex = glDT->GetGLTexture();
		}

		// Create a dedicated MRT FBO.
		GLuint fbo;
		glCreateFramebuffers(1, &fbo);

		for (int i = 0; i < (int)renderTargets.size(); i++)
		{
			auto* rt = static_cast<GLRenderTarget2D*>(renderTargets[i]);
			if (rt->IsArray())
				glNamedFramebufferTextureLayer(fbo, GL_COLOR_ATTACHMENT0 + i, rt->GetGLTexture(), 0, 0);
			else
				glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0 + i, rt->GetGLTexture(), 0);
		}

		if (depthTex)
			glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, depthTex, 0);

		std::vector<GLenum> drawBuffers;
		for (int i = 0; i < (int)renderTargets.size(); i++)
			drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + i);
		glNamedFramebufferDrawBuffers(fbo, (GLsizei)drawBuffers.size(), drawBuffers.data());

		_currentFBO = fbo;
		glBindFramebuffer(GL_FRAMEBUFFER, _currentFBO);

		// Reset scissor to full render target size (DX11 does this implicitly).
		glScissor(0, 0, firstRT->GetWidth(), firstRT->GetHeight());
	}

	void GLGraphicsDevice::BindRenderTargets(std::vector<IRenderTargetBinding> renderTargets, IDepthTargetBinding depthTarget)
	{
		if (renderTargets.empty())
			return;

		GLuint depthTex = 0;
		bool depthIsArray = false;
		if (depthTarget.DepthTarget)
		{
			auto* glDT = static_cast<GLDepthTarget*>(depthTarget.DepthTarget);
			depthTex = glDT->GetGLTexture();
			depthIsArray = glDT->IsArray();
		}

		GLuint fbo;
		glCreateFramebuffers(1, &fbo);

		for (int i = 0; i < (int)renderTargets.size(); i++)
		{
			auto* rt = static_cast<GLRenderTarget2D*>(renderTargets[i].RenderTarget);
			if (rt->IsArray())
				glNamedFramebufferTextureLayer(fbo, GL_COLOR_ATTACHMENT0 + i, rt->GetGLTexture(), 0, renderTargets[i].ArrayIndex);
			else
				glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0 + i, rt->GetGLTexture(), 0);
		}

		if (depthTex)
		{
			if (depthIsArray)
				glNamedFramebufferTextureLayer(fbo, GL_DEPTH_ATTACHMENT, depthTex, 0, depthTarget.ArrayIndex);
			else
				glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, depthTex, 0);
		}

		std::vector<GLenum> drawBuffers;
		for (int i = 0; i < (int)renderTargets.size(); i++)
			drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + i);
		glNamedFramebufferDrawBuffers(fbo, (GLsizei)drawBuffers.size(), drawBuffers.data());

		_currentFBO = fbo;
		glBindFramebuffer(GL_FRAMEBUFFER, _currentFBO);

		// Reset scissor to full render target size (DX11 does this implicitly).
		auto* firstMRT = static_cast<GLRenderTarget2D*>(renderTargets[0].RenderTarget);
		glScissor(0, 0, firstMRT->GetWidth(), firstMRT->GetHeight());
	}

	void GLGraphicsDevice::UnbindAllRenderTargets()
	{
		_currentFBO = 0;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// ========================================================================
	// Input Layout
	// ========================================================================

	std::unique_ptr<IInputLayout> GLGraphicsDevice::CreateInputLayout(std::vector<RendererInputLayoutField> fields, IShader* shader)
	{
		return std::make_unique<GLInputLayout>(std::move(fields));
	}

	void GLGraphicsDevice::SetInputLayout(IInputLayout* inputLayout)
	{
		_currentInputLayout = static_cast<GLInputLayout*>(inputLayout);

		if (_currentInputLayout)
		{
			// Apply vertex attribute format to the default VAO.
			// Use semantic-based location mapping to match GLSL layout(location) qualifiers.
			int offset = 0;
			const auto& fields = _currentInputLayout->GetFields();
			std::vector<GLuint> usedLocations;

			for (GLuint i = 0; i < (GLuint)fields.size(); i++)
			{
				const auto& field = fields[i];
				GLuint location = SemanticToLocation(field.Semantic, field.Slot);
				int components = GetVertexAttribComponentCount(field.Format);
				GLenum type = GetVertexAttribGLType(field.Format);
				int byteSize = GetVertexAttribByteSize(field.Format);

				glEnableVertexArrayAttrib(_defaultVAO, location);

				if (IsIntegerFormat(field.Format))
					glVertexArrayAttribIFormat(_defaultVAO, location, components, type, offset);
				else if (IsNormalizedFormat(field.Format))
					glVertexArrayAttribFormat(_defaultVAO, location, components, type, GL_TRUE, offset);
				else
					glVertexArrayAttribFormat(_defaultVAO, location, components, type, GL_FALSE, offset);

				glVertexArrayAttribBinding(_defaultVAO, location, 0);
				usedLocations.push_back(location);
				offset += byteSize;
			}

			// Disable all attributes that are not used by this layout.
			for (GLuint loc = 0; loc < 16; loc++)
			{
				if (std::find(usedLocations.begin(), usedLocations.end(), loc) == usedLocations.end())
					glDisableVertexArrayAttrib(_defaultVAO, loc);
			}
		}
	}

	// ========================================================================
	// Shaders
	// ========================================================================

	std::unique_ptr<IShader> GLGraphicsDevice::CreateShader(ShaderCompileRequest& req)
	{
		auto nativeShader = std::make_unique<GLShader>();

		// Construct GLSL source directory path.
		// Replace "Shaders/" with "Shaders/GLSL/" in SourceDirectory.
		auto glslSourceDir = req.SourceDirectory;
		auto pos = glslSourceDir.rfind(L"Shaders/");
		if (pos != std::wstring::npos)
			glslSourceDir.insert(pos + 8, L"GLSL/");

		auto buildDefines = [&]() -> std::string
		{
			std::string defines = "#version 450 core\n";
			for (auto& [key, value] : req.Macros)
			{
				// Skip HLSL-specific SMAA defines; GLSL SMAA implementation doesn't need them.
				if (key.find("SMAA_HLSL") != std::string::npos)
					continue;

				// Translate HLSL type names to GLSL in macro values.
				auto glslValue = value;
				auto replaceAll = [](std::string& str, const std::string& from, const std::string& to)
				{
					size_t pos = 0;
					while ((pos = str.find(from, pos)) != std::string::npos)
					{
						str.replace(pos, from.length(), to);
						pos += to.length();
					}
				};
				replaceAll(glslValue, "float4", "vec4");
				replaceAll(glslValue, "float3", "vec3");
				replaceAll(glslValue, "float2", "vec2");

				defines += "#define " + key;
				if (!glslValue.empty())
					defines += " " + glslValue;
				defines += "\n";
			}
			return defines;
		};

		auto loadAndPrepend = [&](const std::wstring& filePath, const std::string& defines) -> std::string
		{
			auto source = ReadShaderFile(filePath);

			// Resolve #include directives recursively.
			std::set<std::string> alreadyIncluded;
			source = ResolveIncludes(source, glslSourceDir, alreadyIncluded);

			// Insert defines after the #version line if present, otherwise prepend.
			auto versionPos = source.find("#version");
			if (versionPos != std::string::npos)
			{
				auto lineEnd = source.find('\n', versionPos);
				if (lineEnd != std::string::npos)
					source.insert(lineEnd + 1, defines.substr(defines.find('\n') + 1)); // Skip #version in defines.
				return source;
			}
			return defines + source;
		};

		std::string baseDefines = buildDefines();
		auto entryStr = TEN::Utils::ToString(req.EntryPoint);

		// Stage-specific defines: shader stage identification and gl_PerVertex for separable programs.
		std::string vsDefines = baseDefines + "#define VERTEX_SHADER 1\nout gl_PerVertex { vec4 gl_Position; };\n";
		std::string fsDefines = baseDefines + "#define FRAGMENT_SHADER 1\n";
		std::string gsDefines = baseDefines + "#define GEOMETRY_SHADER 1\nout gl_PerVertex { vec4 gl_Position; };\n";

		// Compile vertex shader.
		if (req.Type == ShaderType::Vertex || req.Type == ShaderType::PixelAndVertex)
		{
			// Try entry-point-specific VS first (e.g., GBuffer_Rooms.vert.glsl), then fall back to base (e.g., GBuffer.vert.glsl).
			auto vsFile = std::wstring();
			if (!req.EntryPoint.empty() && req.EntryPoint != req.FileName)
			{
				auto variantFile = glslSourceDir + req.FileName + L"_" + req.EntryPoint + L".vert.glsl";
				if (std::filesystem::exists(variantFile))
					vsFile = variantFile;
			}
			if (vsFile.empty())
				vsFile = glslSourceDir + req.FileName + L".vert.glsl";

			if (std::filesystem::exists(vsFile))
			{
				auto source = loadAndPrepend(vsFile, vsDefines);
				GLuint prog = CompileSeparableProgram(GL_VERTEX_SHADER, source,
					TEN::Utils::ToString(req.FileName) + ".vert");
				nativeShader->SetVSProgram(prog);
			}
		}

		// Compile fragment shader.
		if (req.Type == ShaderType::Pixel || req.Type == ShaderType::PixelAndVertex)
		{
			std::wstring fsFile;
			if (!req.EntryPoint.empty() && req.EntryPoint != L"" && req.EntryPoint != req.FileName)
				fsFile = glslSourceDir + req.FileName + L"_" + req.EntryPoint + L".frag.glsl";
			else
				fsFile = glslSourceDir + req.FileName + L".frag.glsl";

			if (std::filesystem::exists(fsFile))
			{
				auto source = loadAndPrepend(fsFile, fsDefines);
				GLuint prog = CompileSeparableProgram(GL_FRAGMENT_SHADER, source,
					TEN::Utils::ToString(req.FileName) + ".frag");
				nativeShader->SetFSProgram(prog);
			}
		}

		// Compile geometry shader.
		if (req.Type == ShaderType::Geometry)
		{
			auto gsFile = glslSourceDir + req.FileName + L".geom.glsl";
			if (std::filesystem::exists(gsFile))
			{
				auto source = loadAndPrepend(gsFile, gsDefines);
				GLuint prog = CompileSeparableProgram(GL_GEOMETRY_SHADER, source,
					TEN::Utils::ToString(req.FileName) + ".geom");
				nativeShader->SetGSProgram(prog);
			}
		}

		return nativeShader;
	}

	void GLGraphicsDevice::BindShader(ShaderStage shaderStage, IShader* shader, bool forceNull)
	{
		auto* glShader = static_cast<GLShader*>(shader);

		if (!glShader)
		{
			if (forceNull)
			{
				switch (shaderStage)
				{
				case ShaderStage::VertexShader:
					glUseProgramStages(_pipeline, GL_VERTEX_SHADER_BIT, 0);
					break;
				case ShaderStage::GeometryShader:
					glUseProgramStages(_pipeline, GL_GEOMETRY_SHADER_BIT, 0);
					break;
				case ShaderStage::PixelShader:
					glUseProgramStages(_pipeline, GL_FRAGMENT_SHADER_BIT, 0);
					break;
				}
			}
			return;
		}

		switch (shaderStage)
		{
		case ShaderStage::VertexShader:
			if (glShader->GetVSProgram() || forceNull)
				glUseProgramStages(_pipeline, GL_VERTEX_SHADER_BIT, glShader->GetVSProgram());
			break;
		case ShaderStage::GeometryShader:
			if (glShader->GetGSProgram() || forceNull)
				glUseProgramStages(_pipeline, GL_GEOMETRY_SHADER_BIT, glShader->GetGSProgram());
			break;
		case ShaderStage::PixelShader:
			if (glShader->GetFSProgram() || forceNull)
				glUseProgramStages(_pipeline, GL_FRAGMENT_SHADER_BIT, glShader->GetFSProgram());
			break;
		}
	}

	// ========================================================================
	// Sprite/Font/Primitive
	// ========================================================================

	std::unique_ptr<ISpriteFont> GLGraphicsDevice::InitializeSpriteFont(std::wstring fontPath)
	{
		return std::make_unique<GLSpriteFont>(fontPath);
	}

	std::unique_ptr<ISpriteBatch> GLGraphicsDevice::InitializeSpriteBatch()
	{
		auto batch = std::make_unique<GLSpriteBatch>();
		batch->SetScreenSize(_screenWidth, _screenHeight);
		return batch;
	}

	std::unique_ptr<IPrimitiveBatch> GLGraphicsDevice::InitializePrimitiveBatch()
	{
		return std::make_unique<GLPrimitiveBatch>();
	}

	// ========================================================================
	// Adapter Info
	// ========================================================================

	std::string GLGraphicsDevice::GetDefaultAdapterName()
	{
		// GL context may not exist yet (called during config init before window creation).
		if (!_glContext)
			return "OpenGL";

		const char* renderer = (const char*)glGetString(GL_RENDERER);
		return renderer ? std::string(renderer) : "Unknown";
	}

	AdapterInfo GLGraphicsDevice::GetAdapterInfo()
	{
		return GetPlatformAdapterInfo();
	}

	// ========================================================================
	// Utilities
	// ========================================================================

	Vector3 GLGraphicsDevice::Unproject(Vector3 position, Matrix projection, Matrix view, Matrix world)
	{
		// Use SimpleMath viewport unproject.
		Viewport vp(0.0f, 0.0f, (float)_screenWidth, (float)_screenHeight, 0.0f, 1.0f);
		return vp.Unproject(position, projection, view, world);
	}

	void GLGraphicsDevice::SaveScreenshot(IRenderTarget2D* renderTarget, std::wstring path)
	{
		auto* glRT = static_cast<GLRenderTarget2D*>(renderTarget);
		int w = glRT->GetWidth();
		int h = glRT->GetHeight();

		// Ensure all rendering to the texture is complete before reading back.
		glFinish();

		int bufSize = w * h * 4;
		std::vector<unsigned char> pixels(bufSize);

		// Read directly from the texture using DSA (avoids FBO binding issues).
		glGetTextureImage(glRT->GetGLTexture(), 0, GL_RGBA, GL_UNSIGNED_BYTE, bufSize, pixels.data());

		// Flip vertically (OpenGL stores textures bottom-up).
		std::vector<unsigned char> flipped(bufSize);
		for (int y = 0; y < h; y++)
			memcpy(&flipped[y * w * 4], &pixels[(h - 1 - y) * w * 4], w * 4);

		auto pathStr = TEN::Utils::ToString(path);
		stbi_write_png(pathStr.c_str(), w, h, 4, flipped.data(), w * 4);
	}

	int GLGraphicsDevice::GetRefreshRate() { return _refreshRate; }
	int GLGraphicsDevice::GetScreenWidth() { return _screenWidth; }
	int GLGraphicsDevice::GetScreenHeight() { return _screenHeight; }
}

#endif
