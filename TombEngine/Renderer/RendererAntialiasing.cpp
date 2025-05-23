#include "framework.h"
#include "Renderer/Renderer.h"

using namespace TEN::Renderer::Graphics;

namespace TEN::Renderer
{
	void Renderer::ApplySMAA(RenderTarget2D* renderTarget, RenderView& view)
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

		UINT stride = sizeof(PostProcessVertex);
		UINT offset = 0;

		_context->IASetVertexBuffers(0, 1, _fullscreenTriangleVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

		// Copy render target to SMAA scene target.
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		_context->ClearRenderTargetView(_SMAASceneRenderTarget.RenderTargetView.Get(), clearColor);
		_context->OMSetRenderTargets(1, _SMAASceneRenderTarget.RenderTargetView.GetAddressOf(), nullptr);
		
		BindRenderTargetAsTexture(TextureRegister::ColorMap, renderTarget, SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);

		// 1) Edge detection using color method (also depth and luma available).
		_context->ClearRenderTargetView(_SMAAEdgesRenderTarget.RenderTargetView.Get(), clearColor);
		_context->ClearRenderTargetView(_SMAABlendRenderTarget.RenderTargetView.Get(), clearColor);

		SetCullMode(CullMode::CounterClockwise);
		_context->OMSetRenderTargets(1, _SMAAEdgesRenderTarget.RenderTargetView.GetAddressOf(), nullptr);

		_shaders.Bind(Shader::SmaaEdgeDetection);
		_shaders.Bind(Shader::SmaaColorEdgeDetection);
		 
		_stSMAABuffer.BlendFactor = 1.0f;
		_cbSMAABuffer.UpdateData(_stSMAABuffer, _context.Get());
		BindConstantBufferPS(static_cast<ConstantBufferRegister>(13), _cbSMAABuffer.get());

		BindRenderTargetAsTexture(static_cast<TextureRegister>(0), &_SMAASceneRenderTarget, SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(1), &_SMAASceneSRGBRenderTarget, SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(5), &_SMAAEdgesRenderTarget, SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(6), &_SMAABlendRenderTarget, SamplerStateRegister::LinearClamp);
		BindTexture(static_cast<TextureRegister>(7), &_SMAAAreaTexture, SamplerStateRegister::LinearClamp);
		BindTexture(static_cast<TextureRegister>(8), &_SMAASearchTexture, SamplerStateRegister::LinearClamp);

		DrawTriangles(3, 0);

		// 2) Blend weights calculation.
		_context->OMSetRenderTargets(1, _SMAABlendRenderTarget.RenderTargetView.GetAddressOf(), nullptr);

		_shaders.Bind(Shader::SmaaBlendingWeightCalculation);

		_stSMAABuffer.SubsampleIndices = Vector4::Zero;
		_cbSMAABuffer.UpdateData(_stSMAABuffer, _context.Get());

		BindRenderTargetAsTexture(static_cast<TextureRegister>(0), &_SMAASceneRenderTarget, SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(1), &_SMAASceneSRGBRenderTarget, SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(5), &_SMAAEdgesRenderTarget, SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(6), &_SMAABlendRenderTarget, SamplerStateRegister::LinearClamp);
		BindTexture(static_cast<TextureRegister>(7), &_SMAAAreaTexture, SamplerStateRegister::LinearClamp);
		BindTexture(static_cast<TextureRegister>(8), &_SMAASearchTexture, SamplerStateRegister::LinearClamp);

		DrawTriangles(3, 0);

		// 3) Neighborhood blending.
		_context->OMSetRenderTargets(1, renderTarget->RenderTargetView.GetAddressOf(), nullptr);

		_shaders.Bind(Shader::SmaaNeighborhoodBlending);

		BindRenderTargetAsTexture(static_cast<TextureRegister>(0), &_SMAASceneRenderTarget, SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(1), &_SMAASceneSRGBRenderTarget, SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(5), &_SMAAEdgesRenderTarget, SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(6), &_SMAABlendRenderTarget, SamplerStateRegister::LinearClamp);
		BindTexture(static_cast<TextureRegister>(7), &_SMAAAreaTexture, SamplerStateRegister::LinearClamp);
		BindTexture(static_cast<TextureRegister>(8), &_SMAASearchTexture, SamplerStateRegister::LinearClamp);

		DrawTriangles(3, 0);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());
	}

	void Renderer::ApplyFXAA(RenderTarget2D* renderTarget, RenderView& view)
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

		UINT stride = sizeof(PostProcessVertex);
		UINT offset = 0;

		_context->IASetVertexBuffers(0, 1, _fullscreenTriangleVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

		// Copy render target to temp render target.
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		_context->ClearRenderTargetView(_tempRenderTarget.RenderTargetView.Get(), clearColor);
		_context->OMSetRenderTargets(1, _tempRenderTarget.RenderTargetView.GetAddressOf(), nullptr);

		BindRenderTargetAsTexture(TextureRegister::ColorMap, renderTarget, SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);

		// Apply FXAA
		_context->ClearRenderTargetView(renderTarget->RenderTargetView.Get(), Colors::Black);
		_context->OMSetRenderTargets(1, renderTarget->RenderTargetView.GetAddressOf(), nullptr);

		_shaders.Bind(Shader::Fxaa);

		_stPostProcessBuffer.ViewportWidth = _screenWidth;
		_stPostProcessBuffer.ViewportHeight = _screenHeight;
		_cbPostProcessBuffer.UpdateData(_stPostProcessBuffer, _context.Get());
		
		BindTexture(TextureRegister::ColorMap, &_tempRenderTarget, SamplerStateRegister::AnisotropicClamp);

		DrawTriangles(3, 0);
	}
}