#pragma once

#ifdef HAS_SDLGPU

#include <vector>
#include <string>
#include <SDL3/SDL_gpu.h>
#include "Renderer/Graphics/ISpriteFont.h"

namespace TEN::Renderer::Native::SDLGPU
{
	using namespace TEN::Renderer::Graphics;

	class SDLGPUGraphicsDevice;

	class SDLGPUSpriteFont final : public ISpriteFont
	{
	private:
		SDLGPUGraphicsDevice* _graphicsDevice = nullptr;
		SDL_GPUDevice*        _device         = nullptr;
		SDL_GPUTexture*       _texture        = nullptr;
		int                   _textureWidth   = 0;
		int                   _textureHeight  = 0;
		float                 _lineSpacing    = 0.0f;
		unsigned int          _defaultChar    = ' ';

		std::vector<Glyph> _glyphs;

		int FindGlyphIndex(unsigned int character) const;

	public:
		SDLGPUSpriteFont() = default;
		~SDLGPUSpriteFont();

		SDLGPUSpriteFont(SDLGPUGraphicsDevice* graphicsDevice, SDL_GPUDevice* device, const std::string& fontPath);

		float GetLineSpacing() override;
		Vector2 MeasureString(const std::string& str) override;
		Glyph FindGlyph(char c) override;
		void DrawString(ISpriteBatch* spriteBatch, const std::string& text, Vector2 position, Vector4 color, float rotation, Vector2 origin, float scale = 1) override;

		SDL_GPUTexture* GetTexture() const { return _texture; }
		int GetTextureWidth() const { return _textureWidth; }
		int GetTextureHeight() const { return _textureHeight; }
	};
}

#endif
