#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/spotcam.h"

namespace TEN::Renderer
{
	void Renderer::DrawPostprocess(IRenderSurface2D* renderTarget, RenderView& view, SceneRenderMode renderMode)
	{
		_doingFullscreenPass = true;

		BindPipeline(Pipelines::FullscreenPass);
		_graphicsDevice->SetViewport(view.Viewport);
		_graphicsDevice->SetScissor(view.Viewport);

		float screenFadeFactor = renderMode == SceneRenderMode::Full ? ScreenFadeCurrent : 1.0f;
		float cinematicBarsHeight = renderMode == SceneRenderMode::Full ? CinematicBarsHeight : 0.0f;

		_stPostProcessBuffer.ScreenFadeFactor = screenFadeFactor;
		_stPostProcessBuffer.CinematicBarsHeight = cinematicBarsHeight;
		_stPostProcessBuffer.ViewportSize = Vector2i( _graphicsDevice->GetScreenWidth(),  _graphicsDevice->GetScreenHeight());
		_stPostProcessBuffer.EffectStrength = _postProcessStrength;
		_stPostProcessBuffer.Tint = _postProcessTint;
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_fullScreenVertexInputLayout.get());
		_graphicsDevice->BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		_shaders.Bind(Shader::PostProcess);

		// PostProcess shader: Camera(Slot0), Material(Slot1), PostProcess(Slot2), Blending(Slot3)
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Slot0, _cbCameraMatrices.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Slot1, _cbMaterial.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Slot2, _cbPostProcessBuffer.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Slot3, _cbBlending.get());

		// *** START OF POST-PROCESSING CHAIN ***

		// Copy render target to post process render target. --------------------------------------------------------------------
		{
			RenderPassDescriptor pass;
			pass.Name = "PostProcess: Copy";
			pass.ColorAttachments.push_back({
				_postProcessRenderTarget[0]->GetRenderTarget(), 0,
				LoadAction::Clear, StoreAction::Store, Colors::Transparent
			});
			pass.Viewport = view.Viewport;
			BeginRenderPass(pass);
		}

		BindRenderTargetAsTexture(TextureRegister::ColorMap, _renderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);

		// Ping-pong between two post-process render targets.
		int currentRenderTarget = 0;
		int destRenderTarget = 1;

		// Lens flares ----------------------------------------------------------------------------------------------------------
		_shaders.Bind(Shader::PostProcess);

		if (!view.LensFlaresToDraw.empty())
		{
			{
				RenderPassDescriptor pass;
				pass.Name = "PostProcess: Lens Flare";
				pass.ColorAttachments.push_back({
					_postProcessRenderTarget[destRenderTarget]->GetRenderTarget(), 0,
					LoadAction::Clear, StoreAction::Store, Colors::Transparent
				});
				pass.Viewport = view.Viewport;
				BeginRenderPass(pass);
			}

			_shaders.Bind(Shader::PostProcessLensFlare);

			for (int i = 0; i < view.LensFlaresToDraw.size(); i++)
			{
				_stPostProcessBuffer.LensFlares[i].Position = view.LensFlaresToDraw[i].Position;
				_stPostProcessBuffer.LensFlares[i].Color = view.LensFlaresToDraw[i].Color.ToVector3();
			}
			_stPostProcessBuffer.NumLensFlares = (int)view.LensFlaresToDraw.size();
			UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

			BindRenderTargetAsTexture(TextureRegister::ColorMap, _postProcessRenderTarget[currentRenderTarget]->GetRenderTarget(), SamplerStateRegister::PointWrap);
			DrawTriangles(3, 0);

			destRenderTarget = (destRenderTarget) == 1 ? 0 : 1;
			currentRenderTarget = (currentRenderTarget == 1) ? 0 : 1;
		}

		// Color scheme ----------------------------------------------------------------------------------------------------------
		if (_postProcessMode != PostProcessMode::None && _postProcessStrength > EPSILON)
		{
			{
				RenderPassDescriptor pass;
				pass.Name = "PostProcess: Color Scheme";
				pass.ColorAttachments.push_back({
					_postProcessRenderTarget[destRenderTarget]->GetRenderTarget(), 0,
					LoadAction::Clear, StoreAction::Store, Colors::Transparent
				});
				pass.Viewport = view.Viewport;
				BeginRenderPass(pass);
			}

			switch (_postProcessMode)
			{
			case PostProcessMode::Monochrome:
				_shaders.Bind(Shader::PostProcessMonochrome);
				break;

			case PostProcessMode::Negative:
				_shaders.Bind(Shader::PostProcessNegative);
				break;

			case PostProcessMode::Exclusion:
				_shaders.Bind(Shader::PostProcessExclusion);
				break;

			default:
				return;
			}

			BindRenderTargetAsTexture(TextureRegister::ColorMap, _postProcessRenderTarget[currentRenderTarget]->GetRenderTarget(), SamplerStateRegister::PointWrap);
			DrawTriangles(3, 0);

			destRenderTarget = (destRenderTarget == 1) ? 0 : 1;
			currentRenderTarget = (currentRenderTarget == 1) ? 0 : 1;
		}

		// Final pass ----------------------------------------------------------------------------------------------------------
		_shaders.Bind(Shader::PostProcessFinalPass);

		{
			RenderPassDescriptor pass;
			pass.Name = "PostProcess: Final";
			pass.ColorAttachments.push_back({
				renderTarget->GetRenderTarget(), 0,
				LoadAction::Clear, StoreAction::Store, Colors::Black
			});
			pass.DepthAttachment = { renderTarget->GetDepthTarget(), 0, LoadAction::Clear, StoreAction::Store, 1.0f, 0 };
			pass.Viewport = view.Viewport;
			BeginRenderPass(pass);
		}

		BindTexture(TextureRegister::ColorMap, _postProcessRenderTarget[currentRenderTarget]->GetRenderTarget(), SamplerStateRegister::PointWrap);

		DrawTriangles(3, 0);

		_doingFullscreenPass = false;
	}

	PostProcessMode Renderer::GetPostProcessMode()
	{
		return _postProcessMode;
	}

	float Renderer::GetPostProcessStrength()
	{
		return _postProcessStrength;
	}

	Vector3 Renderer::GetPostProcessTint()
	{
		return _postProcessTint;
	}

	void Renderer::SetPostProcessMode(PostProcessMode mode)
	{
		_postProcessMode = mode;
	}

	void Renderer::SetPostProcessStrength(float strength)
	{
		_postProcessStrength = strength;
	}

	void Renderer::SetPostProcessTint(Vector3 tint)
	{
		_postProcessTint = tint;
	}

	void Renderer::CopyRenderTarget(IRenderSurface2D* source, IRenderSurface2D* dest, RenderView& view)
	{
		BindPipeline(Pipelines::FullscreenPass, true);

		// Common vertex shader to all fullscreen effects
		_shaders.Bind(Shader::PostProcess);

		// We draw a fullscreen triangle
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_fullScreenVertexInputLayout.get());

		_graphicsDevice->BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		{
			RenderPassDescriptor pass;
			pass.Name = "Copy RT";
			pass.ColorAttachments.push_back({
				dest->GetRenderTarget(), 0,
				LoadAction::Clear, StoreAction::Store, Colors::Black
			});
			pass.Viewport = view.Viewport;
			BeginRenderPass(pass);
		}

		BindRenderTargetAsTexture(TextureRegister::ColorMap, source->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);
	}

	void Renderer::CopyRenderTargetAndDownscale(IRenderSurface2D* source, IRenderSurface2D* dest, float factor, RenderView& view)
	{
		RendererViewport viewport = { 0, 0, (int)( _graphicsDevice->GetScreenWidth() / factor), (int)( _graphicsDevice->GetScreenHeight() / factor), 0.0f, 1.0f };

		BindPipeline(Pipelines::FullscreenPass, true);

		// Common vertex shader to all fullscreen effects
		_shaders.Bind(Shader::PostProcess);

		// We draw a fullscreen triangle
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_fullScreenVertexInputLayout.get());
		_graphicsDevice->BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		{
			RenderPassDescriptor pass;
			pass.Name = "Copy RT Downscale";
			pass.ColorAttachments.push_back({
				dest->GetRenderTarget(), 0,
				LoadAction::Clear, StoreAction::Store, Colors::Transparent
			});
			pass.Viewport = viewport;
			BeginRenderPass(pass);
		}

		BindRenderTargetAsTexture(TextureRegister::ColorMap, source->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);

		ResetScissor();
		_graphicsDevice->SetViewport(view.Viewport);
	}

	void Renderer::ApplyGlow(IRenderSurface2D* renderTarget, RenderView& view)
	{
		BindPipeline(Pipelines::FullscreenPass, true);

		RendererViewport glowViewport = { 0, 0, (int)( _graphicsDevice->GetScreenWidth() / GLOW_DOWNSCALE_FACTOR), (int)( _graphicsDevice->GetScreenHeight() / GLOW_DOWNSCALE_FACTOR), 0.0f, 1.0f };

		_shaders.Bind(Shader::PostProcess);

		// PostProcess shader variants: Camera(Slot0), Material(Slot1), PostProcess(Slot2), Blending(Slot3)
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Slot0, _cbCameraMatrices.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Slot1, _cbMaterial.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Slot2, _cbPostProcessBuffer.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Slot3, _cbBlending.get());

		_stPostProcessBuffer.ViewportSize = Vector2i( _graphicsDevice->GetScreenWidth(),  _graphicsDevice->GetScreenHeight());

		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_fullScreenVertexInputLayout.get());
		_graphicsDevice->BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		// Downscale
		_shaders.Bind(Shader::Downscale);

		_stPostProcessBuffer.DownscaleFactor = GLOW_DOWNSCALE_FACTOR;
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		{
			RenderPassDescriptor pass;
			pass.Name = "Glow: Downscale";
			pass.ColorAttachments.push_back({
				_glowRenderTarget[0]->GetRenderTarget(), 0,
				LoadAction::Clear, StoreAction::Store, Colors::Transparent
			});
			pass.Viewport = glowViewport;
			BeginRenderPass(pass);
		}

		BindRenderTargetAsTexture(TextureRegister::ColorMap, _emissiveAndRoughnessRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);

		// Blur
		_shaders.Bind(Shader::Blur);

		_stPostProcessBuffer.TexelSize = Vector2(1.0f / ( _graphicsDevice->GetScreenWidth() / GLOW_DOWNSCALE_FACTOR), 1.0f / ( _graphicsDevice->GetScreenHeight() / GLOW_DOWNSCALE_FACTOR));
		_stPostProcessBuffer.BlurSigma = GLOW_BLUR_SIGMA;
		_stPostProcessBuffer.BlurRadius = GLOW_BLUR_RADIUS;

		// Horizontal blur
		{
			RenderPassDescriptor pass;
			pass.Name = "Glow: Blur H";
			pass.ColorAttachments.push_back({
				_glowRenderTarget[1]->GetRenderTarget(), 0,
				LoadAction::Clear, StoreAction::Store, Colors::Transparent
			});
			pass.Viewport = glowViewport;
			BeginRenderPass(pass);
		}

		_stPostProcessBuffer.BlurDirection = Vector2(1.0f, 0.0f);
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		BindRenderTargetAsTexture(TextureRegister::ColorMap, _glowRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);

		// Vertical blur
		_stPostProcessBuffer.BlurDirection = Vector2(0.0f, 1.0f);
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		{
			RenderPassDescriptor pass;
			pass.Name = "Glow: Blur V";
			pass.ColorAttachments.push_back({
				_glowRenderTarget[0]->GetRenderTarget(), 0,
				LoadAction::Clear, StoreAction::Store, Colors::Transparent
			});
			pass.Viewport = glowViewport;
			BeginRenderPass(pass);
		}

		BindRenderTargetAsTexture(TextureRegister::ColorMap, _glowRenderTarget[1]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);

		// Reset viewport
		_graphicsDevice->SetViewport(view.Viewport);
		_graphicsDevice->SetScissor(view.Viewport);

		// Copy render target to temp render target
		CopyRenderTarget(renderTarget, _postProcessRenderTarget[0].get(), view);

		// Combine glow
		_shaders.Bind(Shader::GlowCombine);

		_stPostProcessBuffer.GlowSoftAdd = 1;
		_stPostProcessBuffer.GlowIntensity = 1.0f;
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		{
			RenderPassDescriptor pass;
			pass.Name = "Glow: Combine";
			pass.ColorAttachments.push_back({
				renderTarget->GetRenderTarget(), 0,
				LoadAction::Clear, StoreAction::Store, Colors::Transparent
			});
			pass.Viewport = view.Viewport;
			BeginRenderPass(pass);
		}

		BindRenderTargetAsTexture(static_cast<TextureRegister>(0), _postProcessRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(3), _glowRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
	}
}
