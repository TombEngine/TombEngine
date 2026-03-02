#pragma once

#ifdef USE_OPENGL

#include <glad/glad.h>
#include "Renderer/RendererEnums.h"
#include "Game/Debug/Debug.h"

namespace TEN::Renderer::Native::OpenGL
{
	using namespace TEN::Debug;

	struct GLFormatInfo
	{
		GLenum InternalFormat;
		GLenum Format;
		GLenum Type;
		int BytesPerPixel;
	};

	inline GLFormatInfo GetGLFormat(SurfaceFormat format)
	{
		switch (format)
		{
		case SurfaceFormat::SF_RGBA8_Unorm:
			return { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4 };
		case SurfaceFormat::SF_RGBA8_Unorm_Srgb:
			return { GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, 4 };
		case SurfaceFormat::SF_RG8_Unorm:
			return { GL_RG8, GL_RG, GL_UNSIGNED_BYTE, 2 };
		case SurfaceFormat::SF_R8_Unorm:
			return { GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1 };
		case SurfaceFormat::SF_R32_Float:
			return { GL_R32F, GL_RED, GL_FLOAT, 4 };
		case SurfaceFormat::SF_RGBA32_Float:
			return { GL_RGBA32F, GL_RGBA, GL_FLOAT, 16 };
		case SurfaceFormat::SF_BGRA8_Unorm:
			return { GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE, 4 };
		default:
			return { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4 };
		}
	}

	inline GLenum GetGLDepthFormat(DepthFormat format)
	{
		switch (format)
		{
		case DepthFormat::Depth24Stencil8:
			return GL_DEPTH24_STENCIL8;
		case DepthFormat::Depth32:
			return GL_DEPTH_COMPONENT32F;
		default:
			return GL_DEPTH_COMPONENT32F;
		}
	}

	inline GLenum GetGLDepthAttachment(DepthFormat format)
	{
		switch (format)
		{
		case DepthFormat::Depth24Stencil8:
			return GL_DEPTH_STENCIL_ATTACHMENT;
		case DepthFormat::Depth32:
			return GL_DEPTH_ATTACHMENT;
		default:
			return GL_DEPTH_ATTACHMENT;
		}
	}

	inline int GetGLBytesPerPixel(GLenum internalFormat)
	{
		switch (internalFormat)
		{
		case GL_RGBA32F:
			return 16;
		case GL_RGBA8:
		case GL_SRGB8_ALPHA8:
		case GL_R32F:
		case GL_DEPTH_COMPONENT32F:
		case GL_DEPTH24_STENCIL8:
			return 4;
		case GL_RG8:
			return 2;
		case GL_R8:
			return 1;
		default:
			return 4;
		}
	}

	inline std::string GLFormatToString(GLenum internalFormat)
	{
		switch (internalFormat)
		{
		case GL_RGBA8:            return "RGBA8";
		case GL_SRGB8_ALPHA8:     return "SRGB8_A8";
		case GL_RG8:              return "RG8";
		case GL_R8:               return "R8";
		case GL_R32F:             return "R32F";
		case GL_RGBA32F:          return "RGBA32F";
		case GL_DEPTH_COMPONENT32F: return "D32F";
		case GL_DEPTH24_STENCIL8: return "D24S8";
		default:                  return "GL_" + std::to_string((int)internalFormat);
		}
	}

	inline std::string BytesToMBString(int bytes)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%.2f", bytes / (1024.0f * 1024.0f));
		return buf;
	}

	inline int ComputeTextureSize(int width, int height, int arraySize, int mipLevels, GLenum internalFormat)
	{
		int bpp = GetGLBytesPerPixel(internalFormat);
		int totalBytes = 0;

		for (int mip = 0; mip < mipLevels; mip++)
		{
			int mipWidth = std::max(width >> mip, 1);
			int mipHeight = std::max(height >> mip, 1);
			totalBytes += mipWidth * mipHeight * bpp;
		}

		return totalBytes * arraySize;
	}

	// Make a typeless-equivalent internal format for GL texture views (SMAA use case).
	inline GLenum MakeTypelessEquivalent(GLenum format)
	{
		switch (format)
		{
		case GL_SRGB8_ALPHA8:
		case GL_RGBA8:
			return GL_RGBA8;
		default:
			return format;
		}
	}

	inline GLenum GetVertexAttribGLType(VertexInputFormat format)
	{
		switch (format)
		{
		case VertexInputFormat::VI_RGBA8_Unorm:
		case VertexInputFormat::VI_RGBA8_Uint:
		case VertexInputFormat::VI_RG8_Unorm:
			return GL_UNSIGNED_BYTE;
		case VertexInputFormat::VI_RGBA8_Snorm:
			return GL_BYTE;
		case VertexInputFormat::VI_R32_Float:
		case VertexInputFormat::VI_RG32_Float:
		case VertexInputFormat::VI_RGB32_Float:
		case VertexInputFormat::VI_RGBA32_Float:
			return GL_FLOAT;
		case VertexInputFormat::VI_R32_Uint:
			return GL_UNSIGNED_INT;
		default:
			return GL_FLOAT;
		}
	}

	inline int GetVertexAttribComponentCount(VertexInputFormat format)
	{
		switch (format)
		{
		case VertexInputFormat::VI_R32_Float:
		case VertexInputFormat::VI_R32_Uint:
			return 1;
		case VertexInputFormat::VI_RG8_Unorm:
		case VertexInputFormat::VI_RG32_Float:
			return 2;
		case VertexInputFormat::VI_RGB32_Float:
			return 3;
		case VertexInputFormat::VI_RGBA8_Unorm:
		case VertexInputFormat::VI_RGBA8_Snorm:
		case VertexInputFormat::VI_RGBA8_Uint:
		case VertexInputFormat::VI_RGBA32_Float:
			return 4;
		default:
			return 4;
		}
	}

	inline int GetVertexAttribByteSize(VertexInputFormat format)
	{
		switch (format)
		{
		case VertexInputFormat::VI_RGBA8_Unorm:
		case VertexInputFormat::VI_RGBA8_Snorm:
		case VertexInputFormat::VI_RGBA8_Uint:
			return 4;
		case VertexInputFormat::VI_RG8_Unorm:
			return 2;
		case VertexInputFormat::VI_R32_Float:
		case VertexInputFormat::VI_R32_Uint:
			return 4;
		case VertexInputFormat::VI_RG32_Float:
			return 8;
		case VertexInputFormat::VI_RGB32_Float:
			return 12;
		case VertexInputFormat::VI_RGBA32_Float:
			return 16;
		default:
			return 4;
		}
	}

	inline bool IsIntegerFormat(VertexInputFormat format)
	{
		return format == VertexInputFormat::VI_RGBA8_Uint ||
		       format == VertexInputFormat::VI_R32_Uint;
	}

	inline bool IsNormalizedFormat(VertexInputFormat format)
	{
		return format == VertexInputFormat::VI_RGBA8_Unorm ||
		       format == VertexInputFormat::VI_RGBA8_Snorm ||
		       format == VertexInputFormat::VI_RG8_Unorm;
	}
}

#endif
