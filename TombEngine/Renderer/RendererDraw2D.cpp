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
	using namespace TEN::SpotCam;

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

		g_AirBar = new RendererHudBar(_graphicsDevice.get(), AIR_BAR_POS, RendererHudBar::SIZE_DEFAULT, 1, AIR_BAR_COLORS);
		g_ExposureBar = new RendererHudBar(_graphicsDevice.get(), EXPOSURE_BAR_POS, RendererHudBar::SIZE_DEFAULT, 1, EXPOSURE_BAR_COLORS);
		g_HealthBar = new RendererHudBar(_graphicsDevice.get(), HEALTH_BAR_POS, RendererHudBar::SIZE_DEFAULT, 1, HEALTH_BAR_COLORS);
		g_StaminaBar = new RendererHudBar(_graphicsDevice.get(), STAMINA_BAR_POS, RendererHudBar::SIZE_DEFAULT, 1, STAMINA_BAR_COLORS);
		g_LoadingBar = new RendererHudBar(_graphicsDevice.get(), LOADING_BAR_POS, RendererHudBar::SIZE_DEFAULT, 1, LOADING_BAR_COLORS);
	}

	void Renderer::DrawBar(float percent, const RendererHudBar& bar, GAME_OBJECT_ID textureSlot, int frame, bool isPoisoned)
	{
		if (!CheckIfSlotExists(ID_BAR_BORDER_GRAPHICS, "Bar rendering"))
			return;

		_graphicsDevice->ClearDepthStencil(_backBuffer->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 0.0f, 0xFF);
		
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());
		_graphicsDevice->BindVertexBuffer(bar.VertexBufferBorder.get());
		_graphicsDevice->BindIndexBuffer(bar.IndexBufferBorder.get());
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		
		_shaders.Bind(Shader::Hud);
		_shaders.Bind(Shader::HudDTexture);

		SetBlendMode(BlendMode::Opaque);
		SetDepthState(DepthState::None);
		SetCullMode(CullMode::None);

		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Hud, _cbHUD.get());

		RendererSprite* borderSprite = &_sprites[Objects[ID_BAR_BORDER_GRAPHICS].meshIndex];
		_stHUDBar.BarStartUV = borderSprite->UV[0];
		_stHUDBar.BarScale = Vector2(borderSprite->Width / (float)borderSprite->Texture->GetWidth(), borderSprite->Height / (float)borderSprite->Texture->GetHeight());
		UpdateConstantBuffer(&_stHUDBar, _cbHUDBar.get());
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::HudBar, _cbHUDBar.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::HudBar, _cbHUDBar.get());
		 
		BindTexture(TextureRegister::Hud, borderSprite->Texture, SamplerStateRegister::LinearClamp);

		DrawIndexedTriangles(56, 0, 0);

		BindTexture(static_cast<TextureRegister>(0), _sprites[Objects[textureSlot].meshIndex].Texture, SamplerStateRegister::AnisotropicClamp);

		_graphicsDevice->ClearDepthStencil(_backBuffer->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 0.0f, 0xFF);

		
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());
		_graphicsDevice->BindVertexBuffer(bar.InnerVertexBuffer.get());
		_graphicsDevice->BindIndexBuffer(bar.InnerIndexBuffer.get());
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		
		_shaders.Bind(Shader::Hud);
		_shaders.Bind(Shader::HudBarColor);

		_stHUDBar.Percent = percent;
		_stHUDBar.Poisoned = isPoisoned;
		_stHUDBar.Frame = frame;	
		RendererSprite* innerSprite = &_sprites[Objects[textureSlot].meshIndex];
		_stHUDBar.BarStartUV = innerSprite->UV[0];
		_stHUDBar.BarScale = Vector2(innerSprite->Width / (float)innerSprite->Texture->GetWidth(), innerSprite->Height / (float)innerSprite->Texture->GetHeight());
		UpdateConstantBuffer(&_stHUDBar, _cbHUDBar.get());

		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::HudBar, _cbHUDBar.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::HudBar, _cbHUDBar.get());
		 
		BindTexture(TextureRegister::Hud, innerSprite->Texture, SamplerStateRegister::LinearClamp);

		DrawIndexedTriangles(12, 0, 0);
	}

	void Renderer::DrawLoadingBar(float percentage)
	{
		if (!g_GameFlow->GetSettings()->Hud.LoadingBar)
			return;

		_graphicsDevice->ClearDepthStencil(_backBuffer->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 0.0f, 0xFF);
	
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);	
		_graphicsDevice->BindVertexBuffer(g_LoadingBar->VertexBufferBorder.get());
		_graphicsDevice->BindIndexBuffer(g_LoadingBar->IndexBufferBorder.get());

		_shaders.Bind(Shader::Hud);
		_shaders.Bind(Shader::HudDTexture);

		SetBlendMode(BlendMode::Opaque);
		SetDepthState(DepthState::None);
		SetCullMode(CullMode::None);

		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Hud, _cbHUD.get());
		BindTexture(TextureRegister::Hud, _loadingBarBorder.get(), SamplerStateRegister::LinearClamp);

		_stHUDBar.BarStartUV = Vector2::Zero;
		_stHUDBar.BarScale = Vector2::One;
		UpdateConstantBuffer(&_stHUDBar, _cbHUDBar.get());
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::HudBar, _cbHUDBar.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::HudBar, _cbHUDBar.get());

		DrawIndexedTriangles(56, 0, 0);

		_graphicsDevice->ClearDepthStencil(_backBuffer->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 0.0f, 0xFF);

		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->BindVertexBuffer(g_LoadingBar->InnerVertexBuffer.get());
		_graphicsDevice->BindIndexBuffer(g_LoadingBar->InnerIndexBuffer.get());

		_shaders.Bind(Shader::Hud);
		_shaders.Bind(Shader::HudBarColor);
		
		_stHUDBar.Percent = percentage / 100.0f;
		_stHUDBar.Poisoned = false;
		_stHUDBar.Frame = 0; 
		UpdateConstantBuffer(&_stHUDBar, _cbHUDBar.get());
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::HudBar, _cbHUDBar.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::HudBar, _cbHUDBar.get());

		BindTexture(TextureRegister::Hud, _loadingBarInner.get(), SamplerStateRegister::LinearClamp);

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
			DrawFullScreenQuad(_whiteTexture.get(), flashColor);
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

			vertices[0].Position.x = -4.0f / _graphicsDevice->GetScreenWidth();
			vertices[0].Position.y = 4.0f / _graphicsDevice->GetScreenHeight();
			vertices[0].Position.z = 0.0f;
			vertices[0].UV.x = 0.0f;
			vertices[0].UV.y = 0.0f;
			vertices[0].Color = VectorColorToRGBA(Vector4(1.0f, 0.0f, 0.0f, 1.0f));

			vertices[1].Position.x = 4.0f / _graphicsDevice->GetScreenWidth();
			vertices[1].Position.y = 4.0f / _graphicsDevice->GetScreenHeight();
			vertices[1].Position.z = 0.0f;
			vertices[1].UV.x = 1.0f;
			vertices[1].UV.y = 0.0f;
			vertices[1].Color = VectorColorToRGBA(Vector4(1.0f, 0.0f, 0.0f, 1.0f));

			vertices[2].Position.x = 4.0f / _graphicsDevice->GetScreenWidth();
			vertices[2].Position.y = -4.0f / _graphicsDevice->GetScreenHeight();
			vertices[2].Position.z = 0.0f;
			vertices[2].UV.x = 1.0f;
			vertices[2].UV.y = 1.0f;
			vertices[2].Color = VectorColorToRGBA(Vector4(1.0f, 0.0f, 0.0f, 1.0f));

			vertices[3].Position.x = -4.0f / _graphicsDevice->GetScreenWidth();
			vertices[3].Position.y = -4.0f / _graphicsDevice->GetScreenHeight();
			vertices[3].Position.z = 0.0f;
			vertices[3].UV.x = 0.0f;
			vertices[3].UV.y = 1.0f;
			vertices[3].Color = VectorColorToRGBA(Vector4(1.0f, 0.0f, 0.0f, 1.0f));

			_shaders.Bind(Shader::FullScreenQuad);

			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

			_primitiveBatch->Begin();
			_primitiveBatch->DrawQuad(vertices[0], vertices[1], vertices[2], vertices[3]);
			_primitiveBatch->End();
		}
		else
		{
			// TODO: Vignette goes here! -- Lwmte, 21.08.21
		}
	}

	void Renderer::DrawFullScreenImage(ITextureBase* texture, float fade, IRenderTarget2D* target, IDepthTarget* depthTarget)
	{
		// Reset GPU state
		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::None);

		_graphicsDevice->BindRenderTarget(target, depthTarget);
		_graphicsDevice->SetViewport(_viewport);
		_graphicsDevice->SetScissor(_viewport);

		DrawFullScreenQuad(texture, Vector3(fade), true);
	}

	void Renderer::DrawDisplaySprites(RenderView& renderView, bool negativePriority)
	{
		constexpr auto VERTEX_COUNT = 4;

		if (renderView.DisplaySpritesToDraw.empty())
			return;

		ITexture2D* texture2DPtr = nullptr;
		for (const auto& spriteToDraw : renderView.DisplaySpritesToDraw)
		{
			if ((spriteToDraw.Priority >= 0) == negativePriority)
				continue;

			if (texture2DPtr == nullptr)
			{
				_shaders.Bind(Shader::FullScreenQuad);

				_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
				_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

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
				rVertices[i].Color = VectorColorToRGBA(Vector4(spriteToDraw.Color.x, spriteToDraw.Color.y, spriteToDraw.Color.z, spriteToDraw.Color.w));
			}
			
			_primitiveBatch->DrawQuad(rVertices[0], rVertices[1], rVertices[2], rVertices[3]);

			texture2DPtr = spriteToDraw.SpritePtr->Texture;
		}
		
		if (texture2DPtr != nullptr)
			_primitiveBatch->End();
	}

	void Renderer::DrawFullScreenQuad(ITextureBase* texture, Vector3 color, bool fit, float customAspect)
	{
		constexpr auto VERTEX_COUNT = 4;
		constexpr auto UV_RANGE		= std::pair<Vector2, Vector2>(Vector2(0.0f), Vector2(1.0f));

		auto uvStart = Vector2::Zero;
		auto uvEnd	 = Vector2::One;

		if (fit)
		{
			float screenAspect = float(_graphicsDevice->GetScreenWidth()) / float(_graphicsDevice->GetScreenHeight());
			float imageAspect  = customAspect == 0.0f ? float(texture->GetWidth()) / float(texture->GetHeight()) : customAspect;

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
		vertices[0].Color = VectorColorToRGBA(colorVec4);

		vertices[1].Position = Vector3(1.0f, 1.0f, 0.0f);
		vertices[1].UV.x = uvEnd.x;
		vertices[1].UV.y = uvStart.y;
		vertices[1].Color = VectorColorToRGBA(colorVec4);

		vertices[2].Position = Vector3(1.0f, -1.0f, 0.0f);
		vertices[2].UV.x = uvEnd.x;
		vertices[2].UV.y = uvEnd.y;
		vertices[2].Color = VectorColorToRGBA(colorVec4);

		vertices[3].Position = Vector3(-1.0f, -1.0f, 0.0f);
		vertices[3].UV.x = uvStart.x;
		vertices[3].UV.y = uvEnd.y;
		vertices[3].Color = VectorColorToRGBA(colorVec4);

		_shaders.Bind(Shader::FullScreenQuad);

		BindTexture(TextureRegister::ColorMap, texture, SamplerStateRegister::AnisotropicClamp);

		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

		_primitiveBatch->Begin();
		_primitiveBatch->DrawQuad(vertices[0], vertices[1], vertices[2], vertices[3]);
		_primitiveBatch->End();
	}

	void Renderer::DrawFullScreenSprite(RendererSprite* sprite, DirectX::SimpleMath::Vector3 color, bool fit)
	{
		Vector2 uvStart = { 0.0f, 0.0f };
		Vector2 uvEnd = { 1.0f, 1.0f };

		ITexture2D* texture = sprite->Texture;

		if (fit)
		{
			float screenAspect = float(_graphicsDevice->GetScreenWidth()) / float(_graphicsDevice->GetScreenHeight());
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

		auto scale = Vector2(sprite->Width / (float)sprite->Texture->GetWidth(), sprite->Height / (float)sprite->Texture->GetHeight());
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
		vertices[0].Color = VectorColorToRGBA(Vector4(color.x, color.y, color.z, 1.0f));

		vertices[1].Position.x = 1.0f;
		vertices[1].Position.y = 1.0f;
		vertices[1].Position.z = 0.0f;
		vertices[1].UV.x = uvEnd.x;
		vertices[1].UV.y = uvStart.y;
		vertices[1].Color = VectorColorToRGBA(Vector4(color.x, color.y, color.z, 1.0f));

		vertices[2].Position.x = 1.0f;
		vertices[2].Position.y = -1.0f;
		vertices[2].Position.z = 0.0f;
		vertices[2].UV.x = uvEnd.x;
		vertices[2].UV.y = uvEnd.y;
		vertices[2].Color = VectorColorToRGBA(Vector4(color.x, color.y, color.z, 1.0f));

		vertices[3].Position.x = -1.0f;
		vertices[3].Position.y = -1.0f;
		vertices[3].Position.z = 0.0f;
		vertices[3].UV.x = uvStart.x;
		vertices[3].UV.y = uvEnd.y;
		vertices[3].Color = VectorColorToRGBA(Vector4(color.x, color.y, color.z, 1.0f));

		_shaders.Bind(Shader::FullScreenQuad);

		BindTexture(TextureRegister::ColorMap, texture, SamplerStateRegister::AnisotropicClamp);

		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

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
		float screenAspect = screenRes.x / screenRes.y;

		// Calculate aspect ratio correction base.
		float aspectCorrectionBase = screenAspect / DISPLAY_SPACE_ASPECT;

		for (const auto& displaySprite : DisplaySprites)
		{
			// If sprite is a video texture, bypass it if texture is inactive.
			if (displaySprite.SpriteID == VIDEO_SPRITE_ID && (_videoSprite.Texture == nullptr || !_videoSprite.Texture->IsValid()))
				continue;

			const auto& sprite = displaySprite.SpriteID == VIDEO_SPRITE_ID ? _videoSprite : _sprites[Objects[displaySprite.ObjectID].meshIndex + displaySprite.SpriteID];

			// Calculate sprite aspect ratio.
			float spriteAspect = (float)sprite.Width / (float)sprite.Height;

			// Calculate layout using helper function.
			auto layout = CalculateDisplaySpriteLayout(
				spriteAspect, displaySprite.Scale, displaySprite.Orientation,
				displaySprite.AlignMode, displaySprite.ScaleMode,
				screenAspect, aspectCorrectionBase);

			AddDisplaySprite(
				sprite,
				displaySprite.Position + layout.Offset,
				displaySprite.Orientation,
				layout.HalfSize * 2,
				displaySprite.Color,
				displaySprite.Priority,
				displaySprite.BlendMode,
				layout.AspectCorrection,
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
