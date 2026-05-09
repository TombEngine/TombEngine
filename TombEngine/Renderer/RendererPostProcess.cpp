#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/spotcam.h"

namespace TEN::Renderer
{
	void Renderer::ApplyDOF(IRenderSurface2D* renderTarget, RenderView& view)
	{
		if (_currentDOF.Strength <= EPSILON || _currentDOF.Mode == DOFMode::None)
			return;

		SetBlendMode(BlendMode::Opaque, true);
		SetCullMode(CullMode::CounterClockwise, true);
		SetDepthState(DepthState::Write, true);

		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_fullScreenVertexInputLayout.get());
		_graphicsDevice->BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		auto halfWidth = std::max(1, (int)_dofViewport.Width);
		auto halfHeight = std::max(1, (int)_dofViewport.Height);

		_stPostProcessBuffer.ViewportSize = Vector2i(_graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight());
		_stPostProcessBuffer.TexelSize = Vector2(1.0f / halfWidth, 1.0f / halfHeight);
		_stPostProcessBuffer.DofParams = Vector4(_currentDOF.Distance, _currentDOF.Range, _currentDOF.Strength, (float)_currentDOF.Mode);
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		// Copy full-resolution scene for the final composite.
		_graphicsDevice->SetViewport(view.Viewport);
		_graphicsDevice->SetScissor(view.Viewport);
		_shaders.Bind(Shader::PostProcess);
		_graphicsDevice->ClearRenderTarget2D(_postProcessRenderTarget[0]->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(_postProcessRenderTarget[0]->GetRenderTarget(), nullptr);
		BindRenderTargetAsTexture(TextureRegister::ColorMap, renderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);

		// Half-resolution downsample + packed signed CoC in alpha → RT[0].
		_graphicsDevice->SetViewport(_dofViewport);
		_graphicsDevice->SetScissor(_dofViewport);
		_shaders.Bind(Shader::PostProcessDofDownsample);
		_graphicsDevice->ClearRenderTarget2D(_dofRenderTarget[0]->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(_dofRenderTarget[0]->GetRenderTarget(), nullptr);
		BindRenderTargetAsTexture(TextureRegister::ColorMap, _postProcessRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(TextureRegister::GBufferDepthMap, _depthRenderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);

		// Far (background) blur — reads undilated RT[0], writes RT[1].
		_shaders.Bind(Shader::PostProcessDofFarBlur);
		_graphicsDevice->ClearRenderTarget2D(_dofRenderTarget[1]->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(_dofRenderTarget[1]->GetRenderTarget(), nullptr);
		BindRenderTargetAsTexture(TextureRegister::ColorMap, _dofRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);

		// Near CoC dilation — 3x3 min-filter expands foreground CoC outward → RT[2].
		_shaders.Bind(Shader::PostProcessDofNearDilate);
		_graphicsDevice->ClearRenderTarget2D(_dofRenderTarget[2]->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(_dofRenderTarget[2]->GetRenderTarget(), nullptr);
		BindRenderTargetAsTexture(TextureRegister::ColorMap, _dofRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);

		// Near (foreground) blur — reads dilated RT[2], writes RT[0] (downsample no longer needed).
		_shaders.Bind(Shader::PostProcessDofNearBlur);
		_graphicsDevice->ClearRenderTarget2D(_dofRenderTarget[0]->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(_dofRenderTarget[0]->GetRenderTarget(), nullptr);
		BindRenderTargetAsTexture(TextureRegister::ColorMap, _dofRenderTarget[2]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);

		// Full-resolution composite: sharp + far blur (RT[1]) + near blur (RT[0]).
		_graphicsDevice->SetViewport(view.Viewport);
		_graphicsDevice->SetScissor(view.Viewport);
		_shaders.Bind(Shader::PostProcessDofComposite);
		_graphicsDevice->ClearRenderTarget2D(renderTarget->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(renderTarget->GetRenderTarget(), nullptr);
		BindRenderTargetAsTexture(TextureRegister::ColorMap, _postProcessRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(TextureRegister::GBufferDepthMap, _depthRenderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
		BindRenderTargetAsTexture(TextureRegister::DistortionMap, _dofRenderTarget[1]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(TextureRegister::CausticsMap, _dofRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
	}

	void Renderer::ApplyDistortion(IRenderSurface2D* renderTarget, RenderView& view)
	{
		if (!_hasDistortionMask)
			return;

		SetBlendMode(BlendMode::Opaque, true);
		SetCullMode(CullMode::CounterClockwise, true);
		SetDepthState(DepthState::Write, true);
		_graphicsDevice->SetViewport(view.Viewport);
		_graphicsDevice->SetScissor(view.Viewport);

		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_fullScreenVertexInputLayout.get());
		_graphicsDevice->BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		_stPostProcessBuffer.ViewportSize = Vector2i(_graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight());
		_stPostProcessBuffer.TexelSize = Vector2(1.0f / _graphicsDevice->GetScreenWidth(), 1.0f / _graphicsDevice->GetScreenHeight());
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		_shaders.Bind(Shader::PostProcess);

		_graphicsDevice->ClearRenderTarget2D(_postProcessRenderTarget[0]->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(_postProcessRenderTarget[0]->GetRenderTarget(), nullptr);
		BindRenderTargetAsTexture(TextureRegister::ColorMap, renderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);

		_graphicsDevice->ClearRenderTarget2D(_postProcessRenderTarget[1]->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(_postProcessRenderTarget[1]->GetRenderTarget(), nullptr);
		_shaders.Bind(Shader::PostProcessDistortion);
		BindRenderTargetAsTexture(TextureRegister::ColorMap, _postProcessRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(TextureRegister::GBufferDepthMap, _depthRenderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
		BindRenderTargetAsTexture(TextureRegister::DistortionMap, _distortionRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);

		_shaders.Bind(Shader::PostProcess);
		_graphicsDevice->ClearRenderTarget2D(renderTarget->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(renderTarget->GetRenderTarget(), nullptr);
		BindRenderTargetAsTexture(TextureRegister::ColorMap, _postProcessRenderTarget[1]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
	}

	void Renderer::DrawPostprocess(IRenderSurface2D* renderTarget, RenderView& view, SceneRenderMode renderMode)
	{
		_doingFullscreenPass = true;

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::Write);
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

		// *** START OF POST-PROCESSING CHAIN ***
		
		// Copy render target to post process render target. --------------------------------------------------------------------
		_graphicsDevice->ClearRenderTarget2D(_postProcessRenderTarget[0]->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(_postProcessRenderTarget[0]->GetRenderTarget(), nullptr);

		BindRenderTargetAsTexture(TextureRegister::ColorMap, _renderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);

		// Ping-pong between two post-process render targets.
		int currentRenderTarget = 0;
		int destRenderTarget = 1;

		// Lens flares ----------------------------------------------------------------------------------------------------------
		_shaders.Bind(Shader::PostProcess);
		
		if (!view.LensFlaresToDraw.empty())
		{
			_graphicsDevice->ClearRenderTarget2D(_postProcessRenderTarget[destRenderTarget]->GetRenderTarget(), Colors::Transparent);
			_graphicsDevice->BindRenderTarget(_postProcessRenderTarget[destRenderTarget]->GetRenderTarget(), nullptr);

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
			_graphicsDevice->ClearRenderTarget2D(_postProcessRenderTarget[destRenderTarget]->GetRenderTarget(), Colors::Transparent);
			_graphicsDevice->BindRenderTarget(_postProcessRenderTarget[destRenderTarget]->GetRenderTarget(), nullptr);

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

		_graphicsDevice->ClearRenderTarget2D(renderTarget->GetRenderTarget(), Colors::Black);
		_graphicsDevice->ClearDepthStencil(renderTarget->GetDepthTarget(), DepthStencilClearFlags::DepthAndStencil, 1.0f, 0);
		_graphicsDevice->BindRenderTarget(renderTarget->GetRenderTarget(), renderTarget->GetDepthTarget());

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

	DOFState Renderer::GetDOF() const 
	{
		return _lastDOF;
	}

	void Renderer::SetDOF(const DOFState& state, bool save)
	{
		_currentDOF.Mode     = state.Mode;
		_currentDOF.Distance = state.Distance;
		_currentDOF.Range    = std::max(0.0f, state.Range);
		_currentDOF.Strength = std::clamp(state.Strength, 0.0f, 1.0f);

		if (save)
			_lastDOF = _currentDOF;
	}

	void Renderer::RestoreDOF()
	{
		_currentDOF = _lastDOF;
	}

	void Renderer::CopyRenderTarget(IRenderSurface2D* source, IRenderSurface2D* dest, RenderView& view)
	{
		SetBlendMode(BlendMode::Opaque, true);
		SetCullMode(CullMode::CounterClockwise, true);
		SetDepthState(DepthState::Write, true);
		_graphicsDevice->SetViewport(view.Viewport);
		_graphicsDevice->SetScissor(view.Viewport);

		// Common vertex shader to all fullscreen effects
		_shaders.Bind(Shader::PostProcess);

		// We draw a fullscreen triangle
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_fullScreenVertexInputLayout.get());

		_graphicsDevice->BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		_graphicsDevice->ClearRenderTarget2D(dest->GetRenderTarget(), Colors::Black);
		_graphicsDevice->BindRenderTarget(dest->GetRenderTarget(), nullptr);

		BindRenderTargetAsTexture(TextureRegister::ColorMap, source->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);
	}

	void Renderer::CopyRenderTargetAndDownscale(IRenderSurface2D* source, IRenderSurface2D* dest, float factor, RenderView& view)
	{
		int w = (_graphicsDevice->GetScreenWidth() + factor - 1) / factor;
		int h = (_graphicsDevice->GetScreenHeight() + factor - 1) / factor;

		RendererViewport viewport = { 0, 0, w, h, 0.0f, 1.0f };
		_graphicsDevice->SetViewport(viewport);
		_graphicsDevice->SetScissor(viewport);

		SetBlendMode(BlendMode::Opaque, true);
		SetCullMode(CullMode::CounterClockwise, true);
		SetDepthState(DepthState::Write, true);

		// Common vertex shader to all fullscreen effects
		_shaders.Bind(Shader::PostProcess);

		// We draw a fullscreen triangle
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_fullScreenVertexInputLayout.get());
		_graphicsDevice->BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		_graphicsDevice->ClearRenderTarget2D(dest->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(dest->GetRenderTarget(), nullptr);

		BindRenderTargetAsTexture(TextureRegister::ColorMap, source->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);

		ResetScissor();
		_graphicsDevice->SetViewport(view.Viewport);
	}

	void Renderer::ApplyGlow(IRenderSurface2D* renderTarget, RenderView& view)
	{
		SetBlendMode(BlendMode::Opaque, true);
		SetCullMode(CullMode::CounterClockwise, true);
		SetDepthState(DepthState::Write, true);

		RendererViewport viewport = { 0, 0, (int)( _graphicsDevice->GetScreenWidth() / GLOW_DOWNSCALE_FACTOR), (int)( _graphicsDevice->GetScreenHeight() / GLOW_DOWNSCALE_FACTOR), 0.0f, 1.0f };
		_graphicsDevice->SetViewport(viewport);
		_graphicsDevice->SetScissor(viewport);

		_shaders.Bind(Shader::PostProcess);

		_stPostProcessBuffer.ViewportSize = Vector2i( _graphicsDevice->GetScreenWidth(),  _graphicsDevice->GetScreenHeight());

		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_fullScreenVertexInputLayout.get());
		_graphicsDevice->BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		// Downscale 
		_shaders.Bind(Shader::Downscale);

		_stPostProcessBuffer.DownscaleFactor = GLOW_DOWNSCALE_FACTOR;
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		_graphicsDevice->ClearRenderTarget2D(_glowRenderTarget[0]->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(_glowRenderTarget[0]->GetRenderTarget(), nullptr);

		BindRenderTargetAsTexture(TextureRegister::ColorMap, _emissiveAndRoughnessRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);

		// Blur
		_shaders.Bind(Shader::Blur);

		_stPostProcessBuffer.TexelSize = Vector2(1.0f / ( _graphicsDevice->GetScreenWidth() / GLOW_DOWNSCALE_FACTOR), 1.0f / ( _graphicsDevice->GetScreenHeight() / GLOW_DOWNSCALE_FACTOR));
		_stPostProcessBuffer.BlurSigma = GLOW_BLUR_SIGMA;
		_stPostProcessBuffer.BlurRadius = GLOW_BLUR_RADIUS;

		// Horizontal blur
		_graphicsDevice->ClearRenderTarget2D(_glowRenderTarget[1]->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(_glowRenderTarget[1]->GetRenderTarget(), nullptr);

		_stPostProcessBuffer.BlurDirection = Vector2(1.0f, 0.0f);
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		BindRenderTargetAsTexture(TextureRegister::ColorMap, _glowRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);

		// Vertical blur
		_stPostProcessBuffer.BlurDirection = Vector2(0.0f, 1.0f);
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		_graphicsDevice->ClearRenderTarget2D(_glowRenderTarget[0]->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(_glowRenderTarget[0]->GetRenderTarget(), nullptr);

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

		_graphicsDevice->ClearRenderTarget2D(renderTarget->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(renderTarget->GetRenderTarget(), nullptr);

		BindRenderTargetAsTexture(static_cast<TextureRegister>(0), _postProcessRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(3), _glowRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
	}
}