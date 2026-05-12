#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/spotcam.h"

namespace TEN::Renderer
{
	// Helper: open a fullscreen color pass with LoadAction::Clear.
	static void BuildFullscreenColorPass(RenderPassDescriptor& pass, IRenderTarget2D* target,
		const XMVECTORF32& clearColor, const RendererViewport& viewport, const char* label)
	{
		pass.ColorAttachments = { ColorAttachmentDescriptor::Clear(target, clearColor) };
		pass.HasViewport      = true;
		pass.Viewport         = viewport;
		pass.DebugLabel       = label;
	}

	void Renderer::ApplyDOF(IRenderSurface2D* renderTarget, RenderView& view)
	{
		if (_currentDOF.Strength <= EPSILON || _currentDOF.Mode == DOFMode::None)
			return;

		SetBlendMode(BlendMode::Opaque, true);
		SetCullMode(CullMode::CounterClockwise, true);
		SetDepthState(DepthState::Write, true);

		// Common VS for all fullscreen passes — the DOF pixel shaders are PS-only.
		_shaders.Bind(Shader::PostProcess);

		SetPrimitiveType(PrimitiveType::TriangleList);
		SetInputLayout(_fullScreenVertexInputLayout.get());
		BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		auto halfWidth  = std::max(1, (int)_dofViewport.Width);
		auto halfHeight = std::max(1, (int)_dofViewport.Height);

		_stPostProcessBuffer.ViewportSize = Vector2i(_graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight());
		_stPostProcessBuffer.TexelSize    = Vector2(1.0f / halfWidth, 1.0f / halfHeight);
		_stPostProcessBuffer.DofParams    = Vector4(_currentDOF.Distance, _currentDOF.Range, _currentDOF.Strength, (float)_currentDOF.Mode);
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		// Copy full-resolution scene to PPRT[0] for the final composite.
		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, _postProcessRenderTarget[0]->GetRenderTarget(),
				Colors::Transparent, view.Viewport, "DOF Scene Copy");
			BeginRenderPass(pass);
		}
		BindRenderTargetAsTexture(TextureRegister::ColorMap, renderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
		EndRenderPass();

		// Half-resolution downsample + packed signed CoC in alpha → DofRT[0].
		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, _dofRenderTarget[0]->GetRenderTarget(),
				Colors::Transparent, _dofViewport, "DOF Downsample");
			BeginRenderPass(pass);
		}
		_shaders.Bind(Shader::PostProcessDofDownsample);
		BindRenderTargetAsTexture(TextureRegister::ColorMap,        _postProcessRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(TextureRegister::GBufferDepthMap, _depthRenderTarget->GetRenderTarget(),          SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);
		EndRenderPass();

		// Far (background) blur — reads undilated DofRT[0], writes DofRT[1].
		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, _dofRenderTarget[1]->GetRenderTarget(),
				Colors::Transparent, _dofViewport, "DOF Far Blur");
			BeginRenderPass(pass);
		}
		_shaders.Bind(Shader::PostProcessDofFarBlur);
		BindRenderTargetAsTexture(TextureRegister::ColorMap, _dofRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
		EndRenderPass();

		// Near CoC dilation — 3x3 min-filter expands foreground CoC outward → DofRT[2].
		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, _dofRenderTarget[2]->GetRenderTarget(),
				Colors::Transparent, _dofViewport, "DOF Near Dilate");
			BeginRenderPass(pass);
		}
		_shaders.Bind(Shader::PostProcessDofNearDilate);
		BindRenderTargetAsTexture(TextureRegister::ColorMap, _dofRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
		EndRenderPass();

		// Near (foreground) blur — reads dilated DofRT[2], writes DofRT[0].
		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, _dofRenderTarget[0]->GetRenderTarget(),
				Colors::Transparent, _dofViewport, "DOF Near Blur");
			BeginRenderPass(pass);
		}
		_shaders.Bind(Shader::PostProcessDofNearBlur);
		BindRenderTargetAsTexture(TextureRegister::ColorMap, _dofRenderTarget[2]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
		EndRenderPass();

		// Full-resolution composite: sharp + far blur (DofRT[1]) + near blur (DofRT[0]).
		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, renderTarget->GetRenderTarget(),
				Colors::Transparent, view.Viewport, "DOF Composite");
			BeginRenderPass(pass);
		}
		_shaders.Bind(Shader::PostProcessDofComposite);
		BindRenderTargetAsTexture(TextureRegister::ColorMap,        _postProcessRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(TextureRegister::GBufferDepthMap, _depthRenderTarget->GetRenderTarget(),          SamplerStateRegister::PointWrap);
		BindRenderTargetAsTexture(TextureRegister::NearBlurMap,     _dofRenderTarget[0]->GetRenderTarget(),         SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(TextureRegister::FarBlurMap,      _dofRenderTarget[1]->GetRenderTarget(),         SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
		EndRenderPass();
	}

	void Renderer::ApplyDistortion(IRenderSurface2D* renderTarget, RenderView& view)
	{
		if (!_hasDistortionMask)
			return;

		SetBlendMode(BlendMode::Opaque, true);
		SetCullMode(CullMode::CounterClockwise, true);
		SetDepthState(DepthState::Write, true);

		// Common VS for all fullscreen passes (PostProcessDistortion is PS-only).
		_shaders.Bind(Shader::PostProcess);

		SetPrimitiveType(PrimitiveType::TriangleList);
		SetInputLayout(_fullScreenVertexInputLayout.get());
		BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		_stPostProcessBuffer.ViewportSize = Vector2i(_graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight());
		_stPostProcessBuffer.TexelSize    = Vector2(1.0f / _graphicsDevice->GetScreenWidth(), 1.0f / _graphicsDevice->GetScreenHeight());
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		// Copy scene to PPRT[0].
		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, _postProcessRenderTarget[0]->GetRenderTarget(),
				Colors::Transparent, view.Viewport, "Distortion Scene Copy");
			BeginRenderPass(pass);
		}
		BindRenderTargetAsTexture(TextureRegister::ColorMap, renderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
		EndRenderPass();

		// Apply distortion: PPRT[0] + depth + mask → PPRT[1].
		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, _postProcessRenderTarget[1]->GetRenderTarget(),
				Colors::Transparent, view.Viewport, "Distortion");
			BeginRenderPass(pass);
		}
		_shaders.Bind(Shader::PostProcessDistortion);
		BindRenderTargetAsTexture(TextureRegister::ColorMap,        _postProcessRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(TextureRegister::GBufferDepthMap, _depthRenderTarget->GetRenderTarget(),          SamplerStateRegister::PointWrap);
		BindRenderTargetAsTexture(TextureRegister::DistortionMap,   _distortionRenderTarget->GetRenderTarget(),     SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
		EndRenderPass();

		// Copy PPRT[1] back to renderTarget.
		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, renderTarget->GetRenderTarget(),
				Colors::Transparent, view.Viewport, "Distortion Copy Back");
			BeginRenderPass(pass);
		}
		_shaders.Bind(Shader::PostProcess);
		BindRenderTargetAsTexture(TextureRegister::ColorMap, _postProcessRenderTarget[1]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
		EndRenderPass();
	}

	void Renderer::DrawPostprocess(IRenderSurface2D* renderTarget, RenderView& view, SceneRenderMode renderMode)
	{
		_doingFullscreenPass = true;

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::Write);

		float screenFadeFactor = renderMode == SceneRenderMode::Full ? ScreenFadeCurrent : 1.0f;
		float cinematicBarsHeight = renderMode == SceneRenderMode::Full ? CinematicBarsHeight : 0.0f;

		_stPostProcessBuffer.ScreenFadeFactor    = screenFadeFactor;
		_stPostProcessBuffer.CinematicBarsHeight = cinematicBarsHeight;
		_stPostProcessBuffer.ViewportSize        = Vector2i(_graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight());
		_stPostProcessBuffer.EffectStrength      = _postProcessStrength;
		_stPostProcessBuffer.Tint                = _postProcessTint;
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		_shaders.Bind(Shader::PostProcess);

		SetPrimitiveType(PrimitiveType::TriangleList);
		SetInputLayout(_fullScreenVertexInputLayout.get());
		BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		// Copy scene to post-process ping-pong target 0.
		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, _postProcessRenderTarget[0]->GetRenderTarget(),
				Colors::Transparent, view.Viewport, "Postprocess Scene Copy");
			BeginRenderPass(pass);
		}
		BindRenderTargetAsTexture(TextureRegister::ColorMap, _renderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);
		EndRenderPass();

		int currentRenderTarget = 0;
		int destRenderTarget = 1;

		// Lens flares.
		if (!view.LensFlaresToDraw.empty())
		{
			{
				RenderPassDescriptor pass;
				BuildFullscreenColorPass(pass, _postProcessRenderTarget[destRenderTarget]->GetRenderTarget(),
					Colors::Transparent, view.Viewport, "Postprocess Lens Flares");
				BeginRenderPass(pass);
			}

			_shaders.Bind(Shader::PostProcessLensFlare);

			for (int i = 0; i < view.LensFlaresToDraw.size(); i++)
			{
				_stPostProcessBuffer.LensFlares[i].Position = view.LensFlaresToDraw[i].Position;
				_stPostProcessBuffer.LensFlares[i].Color    = view.LensFlaresToDraw[i].Color.ToVector3();
			}
			_stPostProcessBuffer.NumLensFlares = (int)view.LensFlaresToDraw.size();
			UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

			BindRenderTargetAsTexture(TextureRegister::ColorMap, _postProcessRenderTarget[currentRenderTarget]->GetRenderTarget(), SamplerStateRegister::PointWrap);
			DrawTriangles(3, 0);
			EndRenderPass();

			destRenderTarget    = (destRenderTarget    == 1) ? 0 : 1;
			currentRenderTarget = (currentRenderTarget == 1) ? 0 : 1;
		}

		// Color scheme.
		if (_postProcessMode != PostProcessMode::None && _postProcessStrength > EPSILON)
		{
			{
				RenderPassDescriptor pass;
				BuildFullscreenColorPass(pass, _postProcessRenderTarget[destRenderTarget]->GetRenderTarget(),
					Colors::Transparent, view.Viewport, "Postprocess Color Scheme");
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
				EndRenderPass();
				return;
			}

			BindRenderTargetAsTexture(TextureRegister::ColorMap, _postProcessRenderTarget[currentRenderTarget]->GetRenderTarget(), SamplerStateRegister::PointWrap);
			DrawTriangles(3, 0);
			EndRenderPass();

			destRenderTarget    = (destRenderTarget    == 1) ? 0 : 1;
			currentRenderTarget = (currentRenderTarget == 1) ? 0 : 1;
		}

		// Final pass: composited image to renderTarget (clears color + depth).
		_shaders.Bind(Shader::PostProcessFinalPass);

		{
			RenderPassDescriptor pass;
			pass.ColorAttachments = { ColorAttachmentDescriptor::Clear(renderTarget->GetRenderTarget(), Colors::Black) };
			pass.DepthAttachment  = DepthAttachmentDescriptor::Clear(renderTarget->GetDepthTarget());
			pass.HasViewport      = true;
			pass.Viewport         = view.Viewport;
			pass.DebugLabel       = "Postprocess Final";
			BeginRenderPass(pass);
		}

		BindTexture(TextureRegister::ColorMap, _postProcessRenderTarget[currentRenderTarget]->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);
		EndRenderPass();

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

		_shaders.Bind(Shader::PostProcess);

		SetPrimitiveType(PrimitiveType::TriangleList);
		SetInputLayout(_fullScreenVertexInputLayout.get());
		BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, dest->GetRenderTarget(), Colors::Black, view.Viewport, "CopyRenderTarget");
			BeginRenderPass(pass);
		}

		BindRenderTargetAsTexture(TextureRegister::ColorMap, source->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);
		EndRenderPass();
	}

	void Renderer::CopyRenderTargetAndDownscale(IRenderSurface2D* source, IRenderSurface2D* dest, float factor, RenderView& view)
	{
		int w = (_graphicsDevice->GetScreenWidth()  + (int)factor - 1) / (int)factor;
		int h = (_graphicsDevice->GetScreenHeight() + (int)factor - 1) / (int)factor;
		RendererViewport viewport = { 0, 0, w, h, 0.0f, 1.0f };

		SetBlendMode(BlendMode::Opaque, true);
		SetCullMode(CullMode::CounterClockwise, true);
		SetDepthState(DepthState::Write, true);

		_shaders.Bind(Shader::PostProcess);

		SetPrimitiveType(PrimitiveType::TriangleList);
		SetInputLayout(_fullScreenVertexInputLayout.get());
		BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, dest->GetRenderTarget(), Colors::Transparent, viewport, "CopyRenderTargetAndDownscale");
			BeginRenderPass(pass);
		}

		BindRenderTargetAsTexture(TextureRegister::ColorMap, source->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);
		EndRenderPass();
	}

	void Renderer::ApplyGlow(IRenderSurface2D* renderTarget, RenderView& view)
	{
		SetBlendMode(BlendMode::Opaque, true);
		SetCullMode(CullMode::CounterClockwise, true);
		SetDepthState(DepthState::Write, true);

		RendererViewport glowViewport = { 0, 0, (int)(_graphicsDevice->GetScreenWidth() / GLOW_DOWNSCALE_FACTOR), (int)(_graphicsDevice->GetScreenHeight() / GLOW_DOWNSCALE_FACTOR), 0.0f, 1.0f };

		_shaders.Bind(Shader::PostProcess);

		_stPostProcessBuffer.ViewportSize = Vector2i(_graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight());

		SetPrimitiveType(PrimitiveType::TriangleList);
		SetInputLayout(_fullScreenVertexInputLayout.get());
		BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		// Downscale emissive into glow[0].
		_shaders.Bind(Shader::Downscale);

		_stPostProcessBuffer.DownscaleFactor = GLOW_DOWNSCALE_FACTOR;
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, _glowRenderTarget[0]->GetRenderTarget(),
				Colors::Transparent, glowViewport, "Glow Downscale");
			BeginRenderPass(pass);
		}
		BindRenderTargetAsTexture(TextureRegister::ColorMap, _emissiveAndRoughnessRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
		EndRenderPass();

		// Blur.
		_shaders.Bind(Shader::Blur);

		_stPostProcessBuffer.TexelSize  = Vector2(1.0f / (_graphicsDevice->GetScreenWidth() / GLOW_DOWNSCALE_FACTOR), 1.0f / (_graphicsDevice->GetScreenHeight() / GLOW_DOWNSCALE_FACTOR));
		_stPostProcessBuffer.BlurSigma  = GLOW_BLUR_SIGMA;
		_stPostProcessBuffer.BlurRadius = GLOW_BLUR_RADIUS;

		// Horizontal blur: glow[0] -> glow[1].
		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, _glowRenderTarget[1]->GetRenderTarget(),
				Colors::Transparent, glowViewport, "Glow Blur Horizontal");
			BeginRenderPass(pass);
		}

		_stPostProcessBuffer.BlurDirection = Vector2(1.0f, 0.0f);
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		BindRenderTargetAsTexture(TextureRegister::ColorMap, _glowRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
		EndRenderPass();

		// Vertical blur: glow[1] -> glow[0].
		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, _glowRenderTarget[0]->GetRenderTarget(),
				Colors::Transparent, glowViewport, "Glow Blur Vertical");
			BeginRenderPass(pass);
		}

		_stPostProcessBuffer.BlurDirection = Vector2(0.0f, 1.0f);
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		BindRenderTargetAsTexture(TextureRegister::ColorMap, _glowRenderTarget[1]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
		EndRenderPass();

		// Copy scene to temp render target (helper opens its own pass).
		CopyRenderTarget(renderTarget, _postProcessRenderTarget[0].get(), view);

		// Combine glow back into renderTarget.
		_shaders.Bind(Shader::GlowCombine);

		_stPostProcessBuffer.GlowSoftAdd   = 1;
		_stPostProcessBuffer.GlowIntensity = 1.0f;
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		{
			RenderPassDescriptor pass;
			BuildFullscreenColorPass(pass, renderTarget->GetRenderTarget(),
				Colors::Transparent, view.Viewport, "Glow Combine");
			BeginRenderPass(pass);
		}

		BindRenderTargetAsTexture(TextureRegister::ColorMap,    _postProcessRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(TextureRegister::EmissiveMap, _glowRenderTarget[0]->GetRenderTarget(),        SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
		EndRenderPass();
	}
}
