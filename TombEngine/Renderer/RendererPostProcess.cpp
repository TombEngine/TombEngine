#include "framework.h"
#include "Renderer/Renderer.h"
#include "Game/spotcam.h"

namespace TEN::Renderer
{
	void Renderer::DrawPostprocess(RenderTarget2D* renderTarget, RenderView& view, SceneRenderMode renderMode)
	{
		_doingFullscreenPass = true;

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::Write);
		_context->RSSetViewports(1, &view.Viewport);
		ResetScissor();

		float screenFadeFactor = renderMode == SceneRenderMode::Full ? ScreenFadeCurrent : 1.0f;
		float cinematicBarsHeight = renderMode == SceneRenderMode::Full ? Smoothstep(CinematicBarsHeight) * SPOTCAM_CINEMATIC_BARS_HEIGHT : 0.0f;

		_stPostProcessBuffer.ScreenFadeFactor = screenFadeFactor;
		_stPostProcessBuffer.CinematicBarsHeight = cinematicBarsHeight;
		_stPostProcessBuffer.ViewportSize = Vector2i(_screenWidth, _screenHeight);
		_stPostProcessBuffer.EffectStrength = _postProcessStrength;
		_stPostProcessBuffer.Tint = _postProcessTint;
		UpdateConstantBuffer(_stPostProcessBuffer, _cbPostProcessBuffer);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_fullscreenTriangleInputLayout.Get());

		unsigned int stride = sizeof(PostProcessVertex);
		unsigned int offset = 0;

		_context->IASetVertexBuffers(0, 1, _fullscreenTriangleVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

		_shaders.Bind(Shader::PostProcess);

		// *** START OF POST-PROCESSING CHAIN ***
		
		// Copy render target to post process render target. --------------------------------------------------------------------
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		_context->ClearRenderTargetView(_postProcessRenderTarget[0].RenderTargetView.Get(), clearColor);
		_context->OMSetRenderTargets(1, _postProcessRenderTarget[0].RenderTargetView.GetAddressOf(), nullptr);

		BindRenderTargetAsTexture(TextureRegister::ColorMap, &_renderTarget, SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);

		// Ping-pong between two post-process render targets.
		int currentRenderTarget = 0;
		int destRenderTarget = 1;

		// Lens flares ----------------------------------------------------------------------------------------------------------
		_shaders.Bind(Shader::PostProcess);
		
		if (!view.LensFlaresToDraw.empty())
		{
			_context->ClearRenderTargetView(_postProcessRenderTarget[destRenderTarget].RenderTargetView.Get(), clearColor);
			_context->OMSetRenderTargets(1, _postProcessRenderTarget[destRenderTarget].RenderTargetView.GetAddressOf(), nullptr);

			_shaders.Bind(Shader::PostProcessLensFlare);

			for (int i = 0; i < view.LensFlaresToDraw.size(); i++)
			{
				_stPostProcessBuffer.LensFlares[i].Position = view.LensFlaresToDraw[i].Position;
				_stPostProcessBuffer.LensFlares[i].Color = view.LensFlaresToDraw[i].Color.ToVector3();
			}
			_stPostProcessBuffer.NumLensFlares = (int)view.LensFlaresToDraw.size();
			UpdateConstantBuffer(_stPostProcessBuffer, _cbPostProcessBuffer);

			BindRenderTargetAsTexture(TextureRegister::ColorMap, &_postProcessRenderTarget[currentRenderTarget], SamplerStateRegister::PointWrap);
			DrawTriangles(3, 0);

			destRenderTarget = (destRenderTarget) == 1 ? 0 : 1;
			currentRenderTarget = (currentRenderTarget == 1) ? 0 : 1;
		}

		// Color scheme ----------------------------------------------------------------------------------------------------------
		if (_postProcessMode != PostProcessMode::None && _postProcessStrength > EPSILON)
		{
			_context->ClearRenderTargetView(_postProcessRenderTarget[destRenderTarget].RenderTargetView.Get(), clearColor);
			_context->OMSetRenderTargets(1, _postProcessRenderTarget[destRenderTarget].RenderTargetView.GetAddressOf(), nullptr);

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

			BindRenderTargetAsTexture(TextureRegister::ColorMap, &_postProcessRenderTarget[currentRenderTarget], SamplerStateRegister::PointWrap);
			DrawTriangles(3, 0);

			destRenderTarget = (destRenderTarget == 1) ? 0 : 1;
			currentRenderTarget = (currentRenderTarget == 1) ? 0 : 1;
		}

		// Final pass ----------------------------------------------------------------------------------------------------------
		_shaders.Bind(Shader::PostProcessFinalPass);

		_context->ClearRenderTargetView(renderTarget->RenderTargetView.Get(), Colors::Black);
		_context->OMSetRenderTargets(1, renderTarget->RenderTargetView.GetAddressOf(), nullptr);

		BindTexture(TextureRegister::ColorMap, &_postProcessRenderTarget[currentRenderTarget], SamplerStateRegister::PointWrap);

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

	void Renderer::CopyRenderTarget(RenderTarget2D* source, RenderTarget2D* dest, RenderView& view)
	{
		SetBlendMode(BlendMode::Opaque, true);
		SetCullMode(CullMode::CounterClockwise, true);
		SetDepthState(DepthState::Write, true);
		_context->RSSetViewports(1, &view.Viewport);
		ResetScissor();

		// Common vertex shader to all fullscreen effects
		_shaders.Bind(Shader::PostProcess);

		// We draw a fullscreen triangle
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_fullscreenTriangleInputLayout.Get());

		unsigned int stride = sizeof(PostProcessVertex);
		unsigned int offset = 0;

		_context->IASetVertexBuffers(0, 1, _fullscreenTriangleVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		_context->ClearRenderTargetView(dest->RenderTargetView.Get(), clearColor);
		_context->OMSetRenderTargets(1, dest->RenderTargetView.GetAddressOf(), nullptr);

		BindRenderTargetAsTexture(TextureRegister::ColorMap, source, SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);
	}

	void Renderer::CopyRenderTargetAndDownscale(RenderTarget2D* source, RenderTarget2D* dest, float factor, RenderView& view)
	{
		D3D11_VIEWPORT viewport;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = _screenWidth / factor;
		viewport.Height = _screenHeight / factor;
		viewport.MinDepth = 0;
		viewport.MaxDepth = 1;

		_context->RSSetViewports(1, &viewport);

		D3D11_RECT rects[1];
		rects[0].left = 0;
		rects[0].right = viewport.Width;
		rects[0].top = 0;
		rects[0].bottom = viewport.Height;

		_context->RSSetScissorRects(1, rects);

		SetBlendMode(BlendMode::Opaque, true);
		SetCullMode(CullMode::CounterClockwise, true);
		SetDepthState(DepthState::Write, true);

		// Common vertex shader to all fullscreen effects
		_shaders.Bind(Shader::PostProcess);

		// We draw a fullscreen triangle
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_fullscreenTriangleInputLayout.Get());

		unsigned int stride = sizeof(PostProcessVertex);
		unsigned int offset = 0;

		_context->IASetVertexBuffers(0, 1, _fullscreenTriangleVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		_context->ClearRenderTargetView(dest->RenderTargetView.Get(), clearColor);
		_context->OMSetRenderTargets(1, dest->RenderTargetView.GetAddressOf(), nullptr);

		BindRenderTargetAsTexture(TextureRegister::ColorMap, source, SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);

		ResetScissor();
		_context->RSSetViewports(1, &view.Viewport);
	}

	void Renderer::ApplyGlow(RenderTarget2D* renderTarget, RenderView& view)
	{
		SetBlendMode(BlendMode::Opaque, true);
		SetCullMode(CullMode::CounterClockwise, true);
		SetDepthState(DepthState::Write, true);

		D3D11_VIEWPORT viewport;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = _screenWidth / GLOW_DOWNSCALE_FACTOR;
		viewport.Height = _screenHeight / GLOW_DOWNSCALE_FACTOR;
		viewport.MinDepth = 0;
		viewport.MaxDepth = 1;

		_context->RSSetViewports(1, &viewport);

		D3D11_RECT rects[1];
		rects[0].left = 0;
		rects[0].right = _screenWidth / GLOW_DOWNSCALE_FACTOR;
		rects[0].top = 0;
		rects[0].bottom = _screenHeight / GLOW_DOWNSCALE_FACTOR;

		_context->RSSetScissorRects(1, rects);

		_shaders.Bind(Shader::PostProcess);

		_stPostProcessBuffer.ViewportSize = Vector2i(_screenWidth, _screenHeight);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_fullscreenTriangleInputLayout.Get());

		unsigned int stride = sizeof(PostProcessVertex);
		unsigned int offset = 0;

		_context->IASetVertexBuffers(0, 1, _fullscreenTriangleVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

		// Downscale 
		_shaders.Bind(Shader::Downscale);

		_stPostProcessBuffer.DownscaleFactor = GLOW_DOWNSCALE_FACTOR;
		UpdateConstantBuffer(_stPostProcessBuffer, _cbPostProcessBuffer);

		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		_context->ClearRenderTargetView(_glowRenderTarget[0].RenderTargetView.Get(), clearColor);
		_context->OMSetRenderTargets(1, _glowRenderTarget[0].RenderTargetView.GetAddressOf(), nullptr);

		BindRenderTargetAsTexture(TextureRegister::ColorMap, &_emissiveAndRoughnessRenderTarget, SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);

		// Blur
		_shaders.Bind(Shader::Blur);

		_stPostProcessBuffer.TexelSize = Vector2(1.0f / (_screenWidth / GLOW_DOWNSCALE_FACTOR), 1.0f / (_screenHeight / GLOW_DOWNSCALE_FACTOR));
		_stPostProcessBuffer.BlurSigma = GLOW_BLUR_SIGMA;
		_stPostProcessBuffer.BlurRadius = GLOW_BLUR_RADIUS;

		// Horizontal blur
		_context->ClearRenderTargetView(_glowRenderTarget[1].RenderTargetView.Get(), clearColor);
		_context->OMSetRenderTargets(1, _glowRenderTarget[1].RenderTargetView.GetAddressOf(), nullptr);

		_stPostProcessBuffer.BlurDirection = Vector2(1.0f, 0.0f);
		UpdateConstantBuffer(_stPostProcessBuffer, _cbPostProcessBuffer);

		BindRenderTargetAsTexture(TextureRegister::ColorMap, &_glowRenderTarget[0], SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);

		// Vertical blur
		_stPostProcessBuffer.BlurDirection = Vector2(0.0f, 1.0f);
		UpdateConstantBuffer(_stPostProcessBuffer, _cbPostProcessBuffer);

		_context->ClearRenderTargetView(_glowRenderTarget[0].RenderTargetView.Get(), clearColor);
		_context->OMSetRenderTargets(1, _glowRenderTarget[0].RenderTargetView.GetAddressOf(), nullptr);

		BindRenderTargetAsTexture(TextureRegister::ColorMap, &_glowRenderTarget[1], SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);

		// Reset viewport
		_context->RSSetViewports(1, &view.Viewport);
		ResetScissor();

		// Copy render target to temp render target
		CopyRenderTarget(renderTarget, &_postProcessRenderTarget[0], view);

		// Combine glow
		_shaders.Bind(Shader::GlowCombine);

		_stPostProcessBuffer.GlowSoftAdd = 1;
		_stPostProcessBuffer.GlowIntensity = 1.0f;
		UpdateConstantBuffer(_stPostProcessBuffer, _cbPostProcessBuffer);

		_context->ClearRenderTargetView(renderTarget->RenderTargetView.Get(), clearColor);
		_context->OMSetRenderTargets(1, renderTarget->RenderTargetView.GetAddressOf(), nullptr);

		BindRenderTargetAsTexture(static_cast<TextureRegister>(0), &_postProcessRenderTarget[0], SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(3), &_glowRenderTarget[0], SamplerStateRegister::LinearClamp);
		DrawTriangles(3, 0);
	}
}