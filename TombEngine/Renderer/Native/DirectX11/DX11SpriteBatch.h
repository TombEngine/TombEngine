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

		DX11SpriteBatch(ID3D11Device* device, ID3D11DeviceContext* context)
		{
			_spriteBatch = std::make_unique<SpriteBatch>(context);
			_renderStates = std::make_unique<CommonStates>(device);
		}

		SpriteBatch* GetNativeSpriteBatch() { return _spriteBatch.get(); }

		ID3D11ShaderResourceView* GetD3D11ShaderResourceView(ITextureBase* texture)
		{
			ID3D11ShaderResourceView* srv = nullptr;

			if (auto tex2D = dynamic_cast<DX11Texture2D*>(texture))
			{
				srv = tex2D->GetD3D11ShaderResourceView();
			}
			else if (auto rt2D = dynamic_cast<DX11RenderTarget2D*>(texture))
			{
				srv = rt2D->GetD3D11ShaderResourceView();
			}

			return srv;
		}

		void Begin(SpriteSortingMode sortingMode, BlendMode blendMode) override
		{
			SpriteSortMode mode;
			switch (sortingMode)
			{
			case SpriteSortingMode::BackToFront:
				mode = SpriteSortMode_BackToFront;
				break;

			case SpriteSortingMode::FrontToBack:
				mode = SpriteSortMode_FrontToBack;
				break;

			case SpriteSortingMode::Deferred:
				mode = SpriteSortMode_Deferred;
				break;

			case SpriteSortingMode::Texture:
				mode = SpriteSortMode_Texture;
				break;

			case SpriteSortingMode::Immediate:
				mode = SpriteSortMode_Immediate;
				break;

			default:
				mode = SpriteSortMode_BackToFront;
				break;
			}

			ID3D11BlendState* blendState;
			switch (blendMode)
			{
			case BlendMode::Opaque:
				blendState = _renderStates->Opaque();
				break;

			case BlendMode::Additive:
				blendState = _renderStates->Additive();
				break;

			case BlendMode::AlphaBlend:
				blendState = _renderStates->NonPremultiplied();
				break;

			default:
				blendState = _renderStates->Opaque();
				break;
			}

			_spriteBatch->Begin(mode, blendState);
		}

		void End() override
		{
			_spriteBatch->End();
		}

		void Draw(ITextureBase* texture, RendererRectangle area, Vector4 color) override
		{
			auto dxTexture = GetD3D11ShaderResourceView(texture);

			auto rect = RECT{};
			rect.left = area.Left;
			rect.top = area.Top;
			rect.bottom = area.Bottom;
			rect.right = area.Right;

			_spriteBatch->Draw(dxTexture, rect, color);
		}
	};
}

#endif
