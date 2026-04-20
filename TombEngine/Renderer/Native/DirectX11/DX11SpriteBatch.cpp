#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11SpriteBatch.h"
#include "Renderer/Native/DirectX11/DX11Texture2D.h"
#include "Renderer/Native/DirectX11/DX11RenderTarget2D.h"
#include "Specific/trutils.h"

namespace TEN::Renderer::Native::DirectX11
{
	DX11SpriteBatch::DX11SpriteBatch(ID3D11Device* device, ID3D11DeviceContext* context)
	{
		_spriteBatch = std::make_unique<SpriteBatch>(context);
		_renderStates = std::make_unique<CommonStates>(device);
	}

	ID3D11ShaderResourceView* DX11SpriteBatch::GetD3D11ShaderResourceView(ITextureBase* texture)
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

	void DX11SpriteBatch::Begin(SpriteSortingMode sortingMode, BlendMode blendMode)
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

		case BlendMode::PremultipliedAlphaBlend:
			blendState = _renderStates->AlphaBlend();
			break;

		default:
			blendState = _renderStates->Opaque();
			break;
		}

		_spriteBatch->Begin(mode, blendState);
	}

	void DX11SpriteBatch::End()
	{
		_spriteBatch->End();
	}

	void DX11SpriteBatch::Draw(ITextureBase* texture, RendererRectangle area, Vector4 color)
	{
		auto dxTexture = GetD3D11ShaderResourceView(texture);

		auto rect = RECT{};
		rect.left = area.Left;
		rect.top = area.Top;
		rect.bottom = area.Bottom;
		rect.right = area.Right;

		_spriteBatch->Draw(dxTexture, rect, color);
	}
}

#endif