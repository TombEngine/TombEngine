#include "framework.h"

#include <SimpleMath.h>

#include "Game/camera.h"
#include "Game/control/control.h"
#include "Game/spotcam.h"
#include "Game/effects/DisplaySprite.h"
#include "Game/effects/weather.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/game_object_ids.h"
#include "Objects/Utils/object_helper.h"
#include "Renderer/Renderer.h"
#include "Renderer/Structures/RendererHudBar.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/trutils.h"
#include "Specific/winmain.h"

TEN::Renderer::RendererHudBar* g_AirBar;
TEN::Renderer::RendererHudBar* g_ExposureBar;
TEN::Renderer::RendererHudBar* g_HealthBar;
TEN::Renderer::RendererHudBar* g_StaminaBar;
TEN::Renderer::RendererHudBar* g_LoadingBar;

namespace TEN::Renderer
{
	using namespace DirectX::SimpleMath;
	using namespace TEN::Renderer::Structures;
	using namespace TEN::Effects::DisplaySprite;
	using namespace TEN::Effects::Environment;
	using namespace TEN::Math;

	void Renderer::InitializeGameBars()
	{
		constexpr auto AIR_BAR_POS		= Vector2(630.0f, 30.0f);
		constexpr auto EXPOSURE_BAR_POS = Vector2(630.0f, 70.0f);
		constexpr auto HEALTH_BAR_POS	= Vector2(20.0f, 30.0f);
		constexpr auto STAMINA_BAR_POS	= Vector2(630.0f, 50.0f);
		constexpr auto LOADING_BAR_POS	= Vector2(325.0f, 550.0f);

		static const auto AIR_BAR_COLORS = std::array<Vector4, RendererHudBar::COLOR_COUNT>
		{
			// Top
			Vector4(0.0f, 0.0f, 0.35f, 1.0f),
			Vector4(0.0f, 0.18f, 0.38f, 1.0f),

			// Center
			Vector4(0.0f, 0.15f, 0.6f, 1.0f),

			// Bottom
			Vector4(0.0f, 0.0f, 0.35f, 1.0f),
			Vector4(0.0f, 0.18f, 0.38f, 1.0f)
		};
		
		static const auto EXPOSURE_BAR_COLORS = std::array<Vector4, RendererHudBar::COLOR_COUNT>
		{
			// Top
			Vector4(0.18f, 0.3f, 0.72f, 1.0f),
			Vector4(0.18f, 0.3f, 0.72f, 1.0f),

			// Center
			Vector4(0.18f, 0.3f, 0.72f, 1.0f),

			// Bottom
			Vector4(0.18f, 0.3f, 0.72f, 1.0f),
			Vector4(0.18f, 0.3f, 0.72f, 1.0f)
		};

		static const auto HEALTH_BAR_COLORS = std::array<Vector4, RendererHudBar::COLOR_COUNT>
		{
			// Top
			Vector4(0.32f, 0.0f, 0.0f, 1.0f),
			Vector4(0.0f, 0.32f, 0.0f, 1.0f),

			// Center
			Vector4(0.3f, 0.32f, 0.0f, 1.0f),

			// Bottom
			Vector4(0.32f, 0.0f, 0.0f, 1.0f),
			Vector4(0.0f, 0.32f, 0.0f, 1.0f)
		};
		
		static const auto STAMINA_BAR_COLORS = std::array<Vector4, RendererHudBar::COLOR_COUNT>
		{
			// Top
			Vector4(0.3f, 0.02f, 0.0f, 1.0f),
			Vector4(0.55f, 0.45f, 0.02f, 1.0f),

			// Center
			Vector4(0.95f, 0.45f, 0.09f, 1.0f),

			// Bottom
			Vector4(0.3f, 0.02f, 0.0f, 1.0f),
			Vector4(0.55f, 0.45f, 0.02f, 1.0f)
		};

		static const auto LOADING_BAR_COLORS = std::array<Vector4, RendererHudBar::COLOR_COUNT>
		{
			// Top
			Vector4(0.0f, 0.0f, 0.35f, 1.0f),
			Vector4(0.0f, 0.18f, 0.38f, 1.0f),

			// Center
			Vector4(0.0f, 0.15f, 0.6f, 1.0f),

			// Bottom
			Vector4(0.0f, 0.0f, 0.35f, 1.0f),
			Vector4(0.0f, 0.18f, 0.38f, 1.0f)
		};

		g_AirBar = new RendererHudBar(_device.Get(), AIR_BAR_POS, RendererHudBar::SIZE_DEFAULT, 1, AIR_BAR_COLORS);
		g_ExposureBar = new RendererHudBar(_device.Get(), EXPOSURE_BAR_POS, RendererHudBar::SIZE_DEFAULT, 1, EXPOSURE_BAR_COLORS);
		g_HealthBar = new RendererHudBar(_device.Get(), HEALTH_BAR_POS, RendererHudBar::SIZE_DEFAULT, 1, HEALTH_BAR_COLORS);
		g_StaminaBar = new RendererHudBar(_device.Get(), STAMINA_BAR_POS, RendererHudBar::SIZE_DEFAULT, 1, STAMINA_BAR_COLORS);
		g_LoadingBar = new RendererHudBar(_device.Get(), LOADING_BAR_POS, RendererHudBar::SIZE_DEFAULT, 1, LOADING_BAR_COLORS);
	}

	void Renderer::DrawBar(float percent, const RendererHudBar& bar, GAME_OBJECT_ID textureSlot, int frame, bool isPoisoned)
	{
		if (!CheckIfSlotExists(ID_BAR_BORDER_GRAPHICS, "Bar rendering"))
			return;

		unsigned int strides = sizeof(Vertex);
		unsigned int offset = 0;
	
		_context->ClearDepthStencilView(_backBuffer.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0xFF);
		
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetVertexBuffers(0, 1, bar.VertexBufferBorder.Buffer.GetAddressOf(), &strides, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetIndexBuffer(bar.IndexBufferBorder.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		
		_shaders.Bind(Shader::Hud);
		_shaders.Bind(Shader::HudDTexture);

		SetBlendMode(BlendMode::Opaque);
		SetDepthState(DepthState::None);
		SetCullMode(CullMode::None);

		BindConstantBufferVS(ConstantBufferRegister::Hud, _cbHUD.get());

		RendererSprite* borderSprite = &_sprites[Objects[ID_BAR_BORDER_GRAPHICS].meshIndex];
		_stHUDBar.BarStartUV = borderSprite->UV[0];
		_stHUDBar.BarScale = Vector2(borderSprite->Width / (float)borderSprite->Texture->Width, borderSprite->Height / (float)borderSprite->Texture->Height);
		_cbHUDBar.UpdateData(_stHUDBar, _context.Get());
		BindConstantBufferVS(ConstantBufferRegister::HudBar, _cbHUDBar.get());
		BindConstantBufferPS(ConstantBufferRegister::HudBar, _cbHUDBar.get());
		 
		BindTexture(TextureRegister::Hud, borderSprite->Texture, SamplerStateRegister::LinearClamp);

		DrawIndexedTriangles(56, 0, 0);

		_context->PSSetShaderResources(0, 1, _sprites[Objects[textureSlot].meshIndex].Texture->ShaderResourceView.GetAddressOf());

		_context->ClearDepthStencilView(_backBuffer.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0xFF);
		
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetVertexBuffers(0, 1, bar.InnerVertexBuffer.Buffer.GetAddressOf(), &strides, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetIndexBuffer(bar.InnerIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		
		_shaders.Bind(Shader::Hud);
		_shaders.Bind(Shader::HudBarColor);

		_stHUDBar.Percent = percent;
		_stHUDBar.Poisoned = isPoisoned;
		_stHUDBar.Frame = frame;	
		RendererSprite* innerSprite = &_sprites[Objects[textureSlot].meshIndex];
		_stHUDBar.BarStartUV = innerSprite->UV[0];
		_stHUDBar.BarScale = Vector2(innerSprite->Width / (float)innerSprite->Texture->Width, innerSprite->Height / (float)innerSprite->Texture->Height);
		_cbHUDBar.UpdateData(_stHUDBar, _context.Get());

		BindConstantBufferVS(ConstantBufferRegister::HudBar, _cbHUDBar.get());
		BindConstantBufferPS(ConstantBufferRegister::HudBar, _cbHUDBar.get());
		 
		BindTexture(TextureRegister::Hud, innerSprite->Texture, SamplerStateRegister::LinearClamp);

		DrawIndexedTriangles(12, 0, 0);
	}

	void Renderer::DrawLoadingBar(float percentage)
	{
		if (!g_GameFlow->GetSettings()->Hud.LoadingBar)
			return;

		unsigned int strides = sizeof(Vertex);
		unsigned int offset = 0;
		
		_context->ClearDepthStencilView(_backBuffer.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0xFF);
	
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetVertexBuffers(0, 1, g_LoadingBar->VertexBufferBorder.Buffer.GetAddressOf(), &strides, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetIndexBuffer(g_LoadingBar->IndexBufferBorder.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	
		_shaders.Bind(Shader::Hud);
		_shaders.Bind(Shader::HudDTexture);

		SetBlendMode(BlendMode::Opaque);
		SetDepthState(DepthState::None);
		SetCullMode(CullMode::None);

		BindConstantBufferVS(ConstantBufferRegister::Hud, _cbHUD.get());
		BindTexture(TextureRegister::Hud, &_loadingBarBorder, SamplerStateRegister::LinearClamp);

		_stHUDBar.BarStartUV = Vector2::Zero;
		_stHUDBar.BarScale = Vector2::One;
		_cbHUDBar.UpdateData(_stHUDBar, _context.Get());
		BindConstantBufferVS(ConstantBufferRegister::HudBar, _cbHUDBar.get());
		BindConstantBufferPS(ConstantBufferRegister::HudBar, _cbHUDBar.get());

		DrawIndexedTriangles(56, 0, 0);

		_context->ClearDepthStencilView(_backBuffer.DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0xFF);
		
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetVertexBuffers(0, 1, g_LoadingBar->InnerVertexBuffer.Buffer.GetAddressOf(), &strides, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetIndexBuffer(g_LoadingBar->InnerIndexBuffer.Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	
		_shaders.Bind(Shader::Hud);
		_shaders.Bind(Shader::HudBarColor);
		
		_stHUDBar.Percent = percentage / 100.0f;
		_stHUDBar.Poisoned = false;
		_stHUDBar.Frame = 0; 
		_cbHUDBar.UpdateData(_stHUDBar, _context.Get());
		BindConstantBufferVS(ConstantBufferRegister::HudBar, _cbHUDBar.get());
		BindConstantBufferPS(ConstantBufferRegister::HudBar, _cbHUDBar.get());

		BindTexture(TextureRegister::Hud, &_loadingBarInner, SamplerStateRegister::LinearClamp);

		DrawIndexedTriangles(12, 0, 0);
	}

	void Renderer::AddLine2D(const Vector2& origin, const Vector2& target, const Color& color, RendererDebugPage page)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto line = RendererLine2D{ origin, target, color };
		_lines2DToDraw.push_back(line);
	}

	void Renderer::DrawOverlays(RenderView& view)
	{
		auto flashColor = Weather.FlashColor();
		if (flashColor != Vector3::Zero)
		{
			SetBlendMode(BlendMode::Additive);
			DrawFullScreenQuad(_whiteTexture.ShaderResourceView.Get(), flashColor);
		}

		if (CurrentLevel == 0)
			return;

		if (!Lara.Control.Look.OpticRange && !SpotcamOverlay)
			return;

		SetBlendMode(BlendMode::AlphaBlend);

		if (Lara.Control.Look.OpticRange != 0 && !Lara.Control.Look.IsUsingLasersight)
		{
			DrawFullScreenSprite(&_sprites[Objects[ID_BINOCULAR_GRAPHICS].meshIndex], Vector3::One, false);
		}
		else if (Lara.Control.Look.OpticRange != 0 && Lara.Control.Look.IsUsingLasersight)
		{
			DrawFullScreenSprite(&_sprites[Objects[ID_LASERSIGHT_GRAPHICS].meshIndex], Vector3::One);

			SetBlendMode(BlendMode::Opaque);

			// Draw the aiming point
			Vertex vertices[4];

			vertices[0].Position.x = -4.0f / _screenWidth;
			vertices[0].Position.y = 4.0f / _screenHeight;
			vertices[0].Position.z = 0.0f;
			vertices[0].UV.x = 0.0f;
			vertices[0].UV.y = 0.0f;
			vertices[0].Color = Vector4(1.0f, 0.0f, 0.0f, 1.0f);

			vertices[1].Position.x = 4.0f / _screenWidth;
			vertices[1].Position.y = 4.0f / _screenHeight;
			vertices[1].Position.z = 0.0f;
			vertices[1].UV.x = 1.0f;
			vertices[1].UV.y = 0.0f;
			vertices[1].Color = Vector4(1.0f, 0.0f, 0.0f, 1.0f);

			vertices[2].Position.x = 4.0f / _screenWidth;
			vertices[2].Position.y = -4.0f / _screenHeight;
			vertices[2].Position.z = 0.0f;
			vertices[2].UV.x = 1.0f;
			vertices[2].UV.y = 1.0f;
			vertices[2].Color = Vector4(1.0f, 0.0f, 0.0f, 1.0f);

			vertices[3].Position.x = -4.0f / _screenWidth;
			vertices[3].Position.y = -4.0f / _screenHeight;
			vertices[3].Position.z = 0.0f;
			vertices[3].UV.x = 0.0f;
			vertices[3].UV.y = 1.0f;
			vertices[3].Color = Vector4(1.0f, 0.0f, 0.0f, 1.0f);

			_shaders.Bind(Shader::FullScreenQuad);

			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_context->IASetInputLayout(_inputLayout.Get());

			_primitiveBatch->Begin();
			_primitiveBatch->DrawQuad(vertices[0], vertices[1], vertices[2], vertices[3]);
			_primitiveBatch->End();
		}
		else
		{
			// TODO: Vignette goes here! -- Lwmte, 21.08.21
		}
	}

	void Renderer::DrawFullScreenImage(ID3D11ShaderResourceView* texture, float fade, ID3D11RenderTargetView* target,
		ID3D11DepthStencilView* depthTarget)
	{
		// Reset GPU state
		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::None);

		_context->OMSetRenderTargets(1, &target, depthTarget);
		_context->RSSetViewports(1, &_viewport);
		ResetScissor();

		DrawFullScreenQuad(texture, Vector3(fade), true);
	}

	void Renderer::DrawDisplaySprites(RenderView& renderView)
	{
		constexpr auto VERTEX_COUNT = 4;

		if (renderView.DisplaySpritesToDraw.empty())
			return;

		_shaders.Bind(Shader::FullScreenQuad);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		Texture2D* texture2DPtr = nullptr;
		for (const auto& spriteToDraw : renderView.DisplaySpritesToDraw)
		{
			if (texture2DPtr == nullptr)
			{
				_primitiveBatch->Begin();

				BindTexture(TextureRegister::ColorMap, spriteToDraw.SpritePtr->Texture, SamplerStateRegister::AnisotropicClamp);
				SetBlendMode(spriteToDraw.BlendMode);
			}
			else if (texture2DPtr != spriteToDraw.SpritePtr->Texture || _lastBlendMode != spriteToDraw.BlendMode)
			{
				_primitiveBatch->End();
				_primitiveBatch->Begin();

				BindTexture(TextureRegister::ColorMap, spriteToDraw.SpritePtr->Texture, SamplerStateRegister::AnisotropicClamp);
				SetBlendMode(spriteToDraw.BlendMode);
			}

			// Calculate vertex base.
			auto vertices = std::array<Vector2, VERTEX_COUNT>
			{
				spriteToDraw.Size / 2,
				Vector2(-spriteToDraw.Size.x, spriteToDraw.Size.y) / 2,
				-spriteToDraw.Size / 2,
				Vector2(spriteToDraw.Size.x, -spriteToDraw.Size.y) / 2
			};

			// Transform vertices.
			// NOTE: Must rotate 180 degrees to account for +Y being down.
			auto rotMatrix = Matrix::CreateRotationZ(TO_RAD(spriteToDraw.Orientation + ANGLE(180.0f)));
			for (auto& vertex : vertices)
			{
				// Rotate.
				vertex = Vector2::Transform(vertex, rotMatrix);

				// Apply aspect correction.
				vertex *= spriteToDraw.AspectCorrection;

				// Offset to position and convert to NDC.
				vertex += spriteToDraw.Position;
				vertex = TEN::Utils::Convert2DPositionToNDC(vertex);
			}

			// Define renderer vertices.
			auto rVertices = std::array<Vertex, VERTEX_COUNT>{};
			for (int i = 0; i < rVertices.size(); i++)
			{
				rVertices[i].Position = Vector3(vertices[i]);
				rVertices[i].UV = spriteToDraw.SpritePtr->UV[i];
				rVertices[i].Color = Vector4(spriteToDraw.Color.x, spriteToDraw.Color.y, spriteToDraw.Color.z, spriteToDraw.Color.w);
			}
			
			_primitiveBatch->DrawQuad(rVertices[0], rVertices[1], rVertices[2], rVertices[3]);

			texture2DPtr = spriteToDraw.SpritePtr->Texture;
		}
		
		_primitiveBatch->End();
	}

	void Renderer::DrawFullScreenQuad(ID3D11ShaderResourceView* texture, Vector3 color, bool fit, float customAspect)
	{
		constexpr auto VERTEX_COUNT = 4;
		constexpr auto UV_RANGE		= std::pair<Vector2, Vector2>(Vector2(0.0f), Vector2(1.0f));

		auto uvStart = Vector2::Zero;
		auto uvEnd	 = Vector2::One;

		if (fit)
		{
			ID3D11Texture2D* texture2DPtr;
			texture->GetResource(reinterpret_cast<ID3D11Resource**>(&texture2DPtr));

			auto desc = D3D11_TEXTURE2D_DESC();
			texture2DPtr->GetDesc(&desc);

			float screenAspect = float(_screenWidth) / float(_screenHeight);
			float imageAspect  = customAspect == 0.0f ? float(desc.Width) / float(desc.Height) : customAspect;

			if (screenAspect > imageAspect)
			{
				float diff = ((screenAspect - imageAspect) / screenAspect) / 2;
				uvStart.y += diff;
				uvEnd.y -= diff;
			}
			else
			{
				float diff = ((imageAspect - screenAspect) / imageAspect) / 2;
				uvStart.x += diff;
				uvEnd.x -= diff;
			}
		}

		auto vertices = std::array<Vertex, VERTEX_COUNT>{};
		auto colorVec4 = Vector4(color.x, color.y, color.z, 1.0f);

		vertices[0].Position = Vector3(-1.0f, 1.0f, 0.0f);
		vertices[0].UV.x = uvStart.x;
		vertices[0].UV.y = uvStart.y;
		vertices[0].Color = colorVec4;

		vertices[1].Position = Vector3(1.0f, 1.0f, 0.0f);
		vertices[1].UV.x = uvEnd.x;
		vertices[1].UV.y = uvStart.y;
		vertices[1].Color = colorVec4;

		vertices[2].Position = Vector3(1.0f, -1.0f, 0.0f);
		vertices[2].UV.x = uvEnd.x;
		vertices[2].UV.y = uvEnd.y;
		vertices[2].Color = colorVec4;

		vertices[3].Position = Vector3(-1.0f, -1.0f, 0.0f);
		vertices[3].UV.x = uvStart.x;
		vertices[3].UV.y = uvEnd.y;
		vertices[3].Color = colorVec4;

		_shaders.Bind(Shader::FullScreenQuad);

		_context->PSSetShaderResources(0, 1, &texture);

		auto* sampler = _renderStates->AnisotropicClamp();
		_context->PSSetSamplers(0, 1, &sampler);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		_primitiveBatch->Begin();
		_primitiveBatch->DrawQuad(vertices[0], vertices[1], vertices[2], vertices[3]);
		_primitiveBatch->End();
	}

	void Renderer::DrawFullScreenSprite(RendererSprite* sprite, DirectX::SimpleMath::Vector3 color, bool fit)
	{
		Vector2 uvStart = { 0.0f, 0.0f };
		Vector2 uvEnd = { 1.0f, 1.0f };

		ID3D11ShaderResourceView* texture = sprite->Texture->ShaderResourceView.Get();

		if (fit)
		{
			float screenAspect = float(_screenWidth) / float(_screenHeight);
			float imageAspect = float(sprite->Width) / float(sprite->Height);

			if (screenAspect > imageAspect)
			{
				float diff = (screenAspect - imageAspect) / screenAspect / 2;
				uvStart.y += diff;
				uvEnd.y -= diff;
			}
			else
			{
				float diff = (imageAspect - screenAspect) / imageAspect / 2;
				uvStart.x += diff;
				uvEnd.x -= diff;
			}
		}

		auto scale = Vector2(sprite->Width / (float)sprite->Texture->Width, sprite->Height / (float)sprite->Texture->Height);
		uvStart.x = uvStart.x * scale.x + sprite->UV[0].x;
		uvStart.y = uvStart.y * scale.y + sprite->UV[0].y;
		uvEnd.x = uvEnd.x * scale.x + sprite->UV[0].x;
		uvEnd.y = uvEnd.y * scale.y + sprite->UV[0].y;

		Vertex vertices[4];

		vertices[0].Position.x = -1.0f;
		vertices[0].Position.y = 1.0f;
		vertices[0].Position.z = 0.0f;
		vertices[0].UV.x = uvStart.x;
		vertices[0].UV.y = uvStart.y;
		vertices[0].Color = Vector4(color.x, color.y, color.z, 1.0f);

		vertices[1].Position.x = 1.0f;
		vertices[1].Position.y = 1.0f;
		vertices[1].Position.z = 0.0f;
		vertices[1].UV.x = uvEnd.x;
		vertices[1].UV.y = uvStart.y;
		vertices[1].Color = Vector4(color.x, color.y, color.z, 1.0f);

		vertices[2].Position.x = 1.0f;
		vertices[2].Position.y = -1.0f;
		vertices[2].Position.z = 0.0f;
		vertices[2].UV.x = uvEnd.x;
		vertices[2].UV.y = uvEnd.y;
		vertices[2].Color = Vector4(color.x, color.y, color.z, 1.0f);

		vertices[3].Position.x = -1.0f;
		vertices[3].Position.y = -1.0f;
		vertices[3].Position.z = 0.0f;
		vertices[3].UV.x = uvStart.x;
		vertices[3].UV.y = uvEnd.y;
		vertices[3].Color = Vector4(color.x, color.y, color.z, 1.0f);

		_shaders.Bind(Shader::FullScreenQuad);

		_context->PSSetShaderResources(0, 1, &texture);
		auto* sampler = _renderStates->AnisotropicClamp();
		_context->PSSetSamplers(0, 1, &sampler);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		_primitiveBatch->Begin();
		_primitiveBatch->DrawQuad(vertices[0], vertices[1], vertices[2], vertices[3]);
		_primitiveBatch->End();
	}

	void Renderer::AddDisplaySprite(const RendererSprite& sprite, const Vector2& pos2D, short orient, const Vector2& size, const Vector4& color,
									  int priority, BlendMode blendMode, const Vector2& aspectCorrection, RenderView& renderView)
	{
		auto spriteToDraw = RendererDisplaySpriteToDraw{};

		spriteToDraw.SpritePtr = &sprite;
		spriteToDraw.Position = pos2D;
		spriteToDraw.Orientation = orient;
		spriteToDraw.Size = size;
		spriteToDraw.Color = color;
		spriteToDraw.Priority = priority;
		spriteToDraw.BlendMode = blendMode;
		spriteToDraw.AspectCorrection = aspectCorrection;

		renderView.DisplaySpritesToDraw.push_back(spriteToDraw);
	}

	void Renderer::CollectDisplaySprites(RenderView& renderView)
	{
		constexpr auto DISPLAY_SPACE_ASPECT = DISPLAY_SPACE_RES.x / DISPLAY_SPACE_RES.y;

		// Calculate screen aspect ratio.
		auto screenRes = GetScreenResolution().ToVector2();
		float screenResAspect = screenRes.x / screenRes.y;

		// Calculate aspect ratio correction base.
		auto aspectCorrectionBase = screenResAspect / DISPLAY_SPACE_ASPECT;

		for (const auto& displaySprite : DisplaySprites)
		{
			// If sprite is a video texture, bypass it if texture is inactive.
			if (displaySprite.SpriteID == NO_VALUE && _videoSprite.Texture == nullptr)
			{
				TENLog("Failed to stream video texture to a sprite. Video is not playing.", LogLevel::Warning);
				continue;
			}

			const auto& sprite = displaySprite.SpriteID == NO_VALUE ? _videoSprite : _sprites[Objects[displaySprite.ObjectID].meshIndex + displaySprite.SpriteID];

			// Calculate sprite aspect ratio.
			float spriteAspect = (float)sprite.Width / (float)sprite.Height;

			auto halfSize = Vector2::Zero;
			auto aspectCorrection = Vector2::One;

			// Calculate size and aspect correction.
			switch (displaySprite.ScaleMode)
			{
			default:
			case DisplaySpriteScaleMode::Fit:
				if (screenResAspect >= spriteAspect)
				{
					halfSize = (Vector2(DISPLAY_SPACE_RES.y) * displaySprite.Scale) / 2;
					halfSize.x *= (spriteAspect >= 1.0f) ? spriteAspect : (1.0f / spriteAspect);
					
					aspectCorrection.x = 1.0f / aspectCorrectionBase;
				}
				else
				{
					halfSize = (Vector2(DISPLAY_SPACE_RES.x) * displaySprite.Scale) / 2;
					halfSize.y *= (spriteAspect >= 1.0f) ? (1.0f / spriteAspect) : spriteAspect;

					aspectCorrection.y = aspectCorrectionBase;
				}
				break;

			case DisplaySpriteScaleMode::Fill:
				if (screenResAspect >= spriteAspect)
				{
					halfSize = (Vector2(DISPLAY_SPACE_RES.x) * displaySprite.Scale) / 2;
					halfSize.y *= (spriteAspect >= 1.0f) ? (1.0f / spriteAspect) : spriteAspect;

					aspectCorrection.y = aspectCorrectionBase;
				}
				else
				{
					halfSize = (Vector2(DISPLAY_SPACE_RES.y) * displaySprite.Scale) / 2;
					halfSize.x *= (spriteAspect >= 1.0f) ? spriteAspect : (1.0f / spriteAspect);

					aspectCorrection.x = 1.0f / aspectCorrectionBase;
				}
				break;

			case DisplaySpriteScaleMode::Stretch:
				if (screenResAspect >= 1.0f)
				{
					halfSize = (DISPLAY_SPACE_RES.x * displaySprite.Scale) / 2;
					halfSize.y *= (screenResAspect >= 1.0f) ? (1.0f / screenResAspect) : screenResAspect;

					aspectCorrection.y = aspectCorrectionBase;
				}
				else
				{
					halfSize = (Vector2(DISPLAY_SPACE_RES.y) * displaySprite.Scale) / 2;
					halfSize.x *= (screenResAspect >= 1.0f) ? (1.0f / screenResAspect) : screenResAspect;

					aspectCorrection.x = 1.0f / aspectCorrectionBase;
				}
				break;
			}

			// Calculate position offset.
			auto offset = Vector2::Zero;
			switch (displaySprite.AlignMode)
			{
			default:
			case DisplaySpriteAlignMode::Center:
				break;

			case DisplaySpriteAlignMode::CenterTop:
				offset = Vector2(0.0f, halfSize.y);
				break;

			case DisplaySpriteAlignMode::CenterBottom:
				offset = Vector2(0.0f, -halfSize.y);
				break;

			case DisplaySpriteAlignMode::CenterLeft:
				offset = Vector2(halfSize.x, 0.0f);
				break;

			case DisplaySpriteAlignMode::CenterRight:
				offset = Vector2(-halfSize.x, 0.0f);
				break;

			case DisplaySpriteAlignMode::TopLeft:
				offset = Vector2(halfSize.x, halfSize.y);
				break;

			case DisplaySpriteAlignMode::TopRight:
				offset = Vector2(-halfSize.x, halfSize.y);
				break;

			case DisplaySpriteAlignMode::BottomLeft:
				offset = Vector2(halfSize.x, -halfSize.y);
				break;

			case DisplaySpriteAlignMode::BottomRight:
				offset = Vector2(-halfSize.x, -halfSize.y);
				break;
			}

			// Rotate position offset according to orientation.
			auto rotMatrix = Matrix::CreateRotationZ(TO_RAD(displaySprite.Orientation));
			offset = Vector2::Transform(offset, rotMatrix) * aspectCorrection;

			AddDisplaySprite(
				sprite,
				displaySprite.Position + offset,
				displaySprite.Orientation,
				halfSize * 2,
				displaySprite.Color,
				displaySprite.Priority,
				displaySprite.BlendMode,
				aspectCorrection,
				renderView);
		}

		std::sort(
			renderView.DisplaySpritesToDraw.begin(), renderView.DisplaySpritesToDraw.end(),
			[](const RendererDisplaySpriteToDraw& spriteToDraw0, const RendererDisplaySpriteToDraw& spriteToDraw1)
			{
				// Same priority; sort by blend mode.
				if (spriteToDraw0.Priority == spriteToDraw1.Priority)
					return (spriteToDraw0.BlendMode < spriteToDraw1.BlendMode);

				// Sort by priority.
				return (spriteToDraw0.Priority < spriteToDraw1.Priority);
			});
	}
}
