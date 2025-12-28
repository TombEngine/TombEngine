#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include "Renderer/RendererUtils.h"
#include "Renderer/Graphics/IGraphicsDevice.h"
#include "Renderer/Native/DirectX11/DX11TextureBase.h"
#include <wrl/client.h>
#include <SpriteFont.h>
#include <PrimitiveBatch.h>
#include <CommonStates.h>
#include <memory>

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Utils;
	using namespace TEN::Renderer::Graphics;
	using namespace DirectX;

	using Microsoft::WRL::ComPtr;

	// TODO: Rewrite it in the future after migrating to the new font system using native DX11 classes.
	class DX11SpriteBatch final : public ISpriteBatch
	{

	private:
		std::unique_ptr<SpriteBatch>  _spriteBatch  = nullptr;
		std::unique_ptr<CommonStates> _renderStates = nullptr;

	public:
		~DX11SpriteBatch() = default;

		DX11SpriteBatch(ID3D11Device* device, ID3D11DeviceContext* context);

		SpriteBatch* GetNativeSpriteBatch() { return _spriteBatch.get(); }

		ID3D11ShaderResourceView* GetD3D11ShaderResourceView(ITextureBase* texture);

		void Begin(SpriteSortingMode sortingMode, BlendMode blendMode) override;
		void End() override;
		void Draw(ITextureBase* texture, RendererRectangle area, Vector4 color) override;
	};
}

#endif
