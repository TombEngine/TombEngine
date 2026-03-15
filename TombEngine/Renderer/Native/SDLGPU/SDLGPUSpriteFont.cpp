#include "framework.h"

#ifdef HAS_SDLGPU

#include "Renderer/Native/SDLGPU/SDLGPUSpriteFont.h"
#include "Renderer/Native/SDLGPU/SDLGPUGraphicsDevice.h"
#include "Renderer/Native/SDLGPU/SDLGPUSpriteBatch.h"

#include <fstream>
#include <filesystem>
#include <cstring>
#include <algorithm>

namespace TEN::Renderer::Native::SDLGPU
{
	// DirectXTK .spritefont binary format parser.
	SDLGPUSpriteFont::SDLGPUSpriteFont(SDLGPUGraphicsDevice* graphicsDevice, SDL_GPUDevice* device,
	                                     const std::string& fontPath)
		: _graphicsDevice(graphicsDevice), _device(device)
	{
		std::ifstream file(std::filesystem::path(fontPath), std::ios::binary);
		if (!file)
		{
			TENLog("Failed to open spritefont file: " + fontPath, LogLevel::Error);
			return;
		}

		// Read header: "DXTKfont" magic.
		char magic[8];
		file.read(magic, 8);
		if (std::string(magic, 8) != "DXTKfont")
		{
			TENLog("Invalid spritefont file format", LogLevel::Error);
			return;
		}

		// Read glyph count.
		unsigned int glyphCount;
		file.read(reinterpret_cast<char*>(&glyphCount), 4);

		// Read glyph table.
		struct RawGlyph
		{
			unsigned int Character;
			int32_t TexLeft, TexTop, TexRight, TexBottom;
			float XOffset, YOffset, XAdvance;
		};

		std::vector<RawGlyph> rawGlyphs(glyphCount);
		file.read(reinterpret_cast<char*>(rawGlyphs.data()), glyphCount * sizeof(RawGlyph));

		// Read line spacing.
		file.read(reinterpret_cast<char*>(&_lineSpacing), 4);

		// Read default character.
		unsigned int defaultChar;
		file.read(reinterpret_cast<char*>(&defaultChar), 4);
		_defaultChar = defaultChar;

		// Read texture data.
		unsigned int texWidth, texHeight;
		unsigned int texFormat; // DXGI format.
		unsigned int texStride;
		unsigned int texRows;

		file.read(reinterpret_cast<char*>(&texWidth), 4);
		file.read(reinterpret_cast<char*>(&texHeight), 4);
		file.read(reinterpret_cast<char*>(&texFormat), 4);
		file.read(reinterpret_cast<char*>(&texStride), 4);
		file.read(reinterpret_cast<char*>(&texRows), 4);

		_textureWidth = texWidth;
		_textureHeight = texHeight;

		unsigned int texDataSize = texStride * texRows;
		std::vector<unsigned char> texData(texDataSize);
		file.read(reinterpret_cast<char*>(texData.data()), texDataSize);

		// Create SDL_GPU texture from atlas data.
		SDL_GPUTextureFormat gpuFormat;

		if (texFormat == 74) // DXGI_FORMAT_BC2_UNORM
		{
			gpuFormat = SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM;
		}
		else if (texFormat == 28) // DXGI_FORMAT_R8G8B8A8_UNORM
		{
			gpuFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		}
		else if (texFormat == 87) // DXGI_FORMAT_B8G8R8A8_UNORM
		{
			gpuFormat = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
		}
		else
		{
			TENLog("Unsupported spritefont texture format: " + std::to_string(texFormat), LogLevel::Error);
			return;
		}

		SDL_GPUTextureCreateInfo tci = {};
		tci.type = SDL_GPU_TEXTURETYPE_2D;
		tci.format = gpuFormat;
		tci.width = texWidth;
		tci.height = texHeight;
		tci.layer_count_or_depth = 1;
		tci.num_levels = 1;
		tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

		_texture = SDL_CreateGPUTexture(device, &tci);
		if (!_texture)
		{
			TENLog("Failed to create spritefont GPU texture", LogLevel::Error);
			return;
		}

		// Upload texture data via transfer buffer.
		SDL_GPUTransferBufferCreateInfo tbci = {};
		tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		tbci.size = texDataSize;
		auto* transferBuffer = SDL_CreateGPUTransferBuffer(device, &tbci);

		void* mapped = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
		std::memcpy(mapped, texData.data(), texDataSize);
		SDL_UnmapGPUTransferBuffer(device, transferBuffer);

		auto* cmdBuf = SDL_AcquireGPUCommandBuffer(device);
		auto* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

		SDL_GPUTextureTransferInfo src = {};
		src.transfer_buffer = transferBuffer;
		src.offset = 0;

		SDL_GPUTextureRegion dst = {};
		dst.texture = _texture;
		dst.w = texWidth;
		dst.h = texHeight;
		dst.d = 1;

		SDL_UploadToGPUTexture(copyPass, &src, &dst, false);
		SDL_EndGPUCopyPass(copyPass);
		SDL_SubmitGPUCommandBuffer(cmdBuf);

		SDL_ReleaseGPUTransferBuffer(device, transferBuffer);

		// Store glyphs.
		_glyphs.reserve(glyphCount);
		for (const auto& rg : rawGlyphs)
		{
			Glyph g;
			g.Character = rg.Character;
			g.Subrect = RendererRectangle(rg.TexLeft, rg.TexTop, rg.TexRight, rg.TexBottom);
			g.XOffset = rg.XOffset;
			g.YOffset = rg.YOffset;
			g.XAdvance = rg.XAdvance;
			_glyphs.push_back(g);
		}
	}

	SDLGPUSpriteFont::~SDLGPUSpriteFont()
	{
		if (_texture && _device)
			SDL_ReleaseGPUTexture(_device, _texture);
	}

	int SDLGPUSpriteFont::FindGlyphIndex(unsigned int character) const
	{
		for (int i = 0; i < (int)_glyphs.size(); ++i)
		{
			if (_glyphs[i].Character == character)
				return i;
		}
		return -1;
	}

	float SDLGPUSpriteFont::GetLineSpacing()
	{
		return _lineSpacing;
	}

	Vector2 SDLGPUSpriteFont::MeasureString(const std::string& str)
	{
		float x = 0, maxX = 0, y = _lineSpacing;

		for (char c : str)
		{
			if (c == '\n')
			{
				maxX = std::max(maxX, x);
				x = 0;
				y += _lineSpacing;
				continue;
			}

			int idx = FindGlyphIndex((unsigned int)(unsigned char)c);
			if (idx < 0)
				idx = FindGlyphIndex(_defaultChar);
			if (idx < 0)
				continue;

			auto& g = _glyphs[idx];

			x += g.XOffset;
			if (x < 0)
				x = 0;

			float w = (float)(g.Subrect.Right - g.Subrect.Left);
			maxX = std::max(maxX, x + w);
			x += w + g.XAdvance;
		}

		return Vector2(std::max(maxX, x), y);
	}

	Glyph SDLGPUSpriteFont::FindGlyph(char c)
	{
		int idx = FindGlyphIndex((unsigned int)(unsigned char)c);
		if (idx < 0)
			idx = FindGlyphIndex(_defaultChar);
		if (idx >= 0)
			return _glyphs[idx];

		return Glyph{};
	}

	void SDLGPUSpriteFont::DrawString(ISpriteBatch* spriteBatch, const std::string& text,
	                                    Vector2 position, Vector4 color, float rotation,
	                                    Vector2 origin, float scale)
	{
		if (!_texture || _textureWidth == 0 || _textureHeight == 0)
			return;

		auto* batch = static_cast<SDLGPUSpriteBatch*>(spriteBatch);
		float invW = 1.0f / (float)_textureWidth;
		float invH = 1.0f / (float)_textureHeight;

		float x = position.x - origin.x * scale;
		float y = position.y - origin.y * scale;

		for (size_t i = 0; i < text.size(); ++i)
		{
			char c = text[i];

			if (c == '\n')
			{
				x = position.x - origin.x * scale;
				y += _lineSpacing * scale;
				continue;
			}

			int idx = FindGlyphIndex((unsigned int)(unsigned char)c);
			if (idx < 0)
				idx = FindGlyphIndex(_defaultChar);
			if (idx < 0)
				continue;

			auto& g = _glyphs[idx];

			x += g.XOffset * scale;

			float gy = y + g.YOffset * scale;
			float gw = (float)(g.Subrect.Right - g.Subrect.Left) * scale;
			float gh = (float)(g.Subrect.Bottom - g.Subrect.Top) * scale;

			RendererRectangle area((int)x, (int)gy, (int)(x + gw), (int)(gy + gh));

			float u0 = g.Subrect.Left * invW;
			float v0 = g.Subrect.Top * invH;
			float u1 = g.Subrect.Right * invW;
			float v1 = g.Subrect.Bottom * invH;

			batch->DrawWithUV(_texture, area, u0, v0, u1, v1, color);

			x += gw + g.XAdvance * scale;
		}
	}
}

#endif
