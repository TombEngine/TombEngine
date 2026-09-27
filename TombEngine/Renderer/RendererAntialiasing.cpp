#include "framework.h"
#include "Renderer/Renderer.h"

using namespace TEN::Renderer::Graphics;

namespace TEN::Renderer
{
	void Renderer::ApplyAntialiasing(IRenderSurface2D* renderTarget, RenderView& view)
	{
		switch (g_Configuration.AntialiasingMode)
		{
		case AntialiasingMode::None:
			break;

		case AntialiasingMode::Low:
			ApplyFXAA(_renderTarget.get(), _gameCamera);
			break;

		case AntialiasingMode::Medium:
		case AntialiasingMode::High:
			ApplySMAA(_renderTarget.get(), _gameCamera);
			break;
		}
	}

	void Renderer::ApplySMAA(IRenderSurface2D* renderTarget, RenderView& view)
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

		// Copy render target to SMAA scene target.
		_graphicsDevice->ClearRenderTarget2D(_SMAASceneRenderTarget->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(_SMAASceneRenderTarget->GetRenderTarget(), nullptr);
		
		BindRenderTargetAsTexture(TextureRegister::ColorMap, renderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);

		// 1) Edge detection using color method (also depth and luma available).
		_graphicsDevice->ClearRenderTarget2D(_SMAAEdgesRenderTarget->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->ClearRenderTarget2D(_SMAABlendRenderTarget->GetRenderTarget(), Colors::Transparent);

		SetCullMode(CullMode::CounterClockwise);
		_graphicsDevice->BindRenderTarget(_SMAAEdgesRenderTarget->GetRenderTarget(), nullptr);

		_shaders.Bind(Shader::SmaaEdgeDetection);
		_shaders.Bind(Shader::SmaaColorEdgeDetection);
		 
		_stSMAABuffer.BlendFactor = 1.0f;
		UpdateConstantBuffer(&_stSMAABuffer, _cbSMAABuffer.get());
		BindConstantBuffer(ShaderStage::PixelShader, static_cast<ConstantBufferRegister>(13), _cbSMAABuffer.get());

		BindRenderTargetAsTexture(static_cast<TextureRegister>(0), _SMAASceneRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(1), _SMAASceneSRGBRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(5), _SMAAEdgesRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(6), _SMAABlendRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindTexture(static_cast<TextureRegister>(7), _SMAAAreaTexture.get(), SamplerStateRegister::LinearClamp);
		BindTexture(static_cast<TextureRegister>(8), _SMAASearchTexture.get(), SamplerStateRegister::LinearClamp);

		DrawTriangles(3, 0);

		// 2) Blend weights calculation.
		_graphicsDevice->BindRenderTarget(_SMAABlendRenderTarget->GetRenderTarget(), nullptr);
		
		_shaders.Bind(Shader::SmaaBlendingWeightCalculation);

		_stSMAABuffer.SubsampleIndices = Vector4::Zero;
		UpdateConstantBuffer(&_stSMAABuffer, _cbSMAABuffer.get());

		BindRenderTargetAsTexture(static_cast<TextureRegister>(0), _SMAASceneRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(1), _SMAASceneSRGBRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(5), _SMAAEdgesRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(6), _SMAABlendRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindTexture(static_cast<TextureRegister>(7), _SMAAAreaTexture.get(), SamplerStateRegister::LinearClamp);
		BindTexture(static_cast<TextureRegister>(8), _SMAASearchTexture.get(), SamplerStateRegister::LinearClamp);

		DrawTriangles(3, 0);

		// 3) Neighborhood blending.
		_graphicsDevice->BindRenderTarget(renderTarget->GetRenderTarget(), nullptr);

		_shaders.Bind(Shader::SmaaNeighborhoodBlending);

		BindRenderTargetAsTexture(static_cast<TextureRegister>(0), _SMAASceneRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(1), _SMAASceneSRGBRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(5), _SMAAEdgesRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(6), _SMAABlendRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindTexture(static_cast<TextureRegister>(7), _SMAAAreaTexture.get(), SamplerStateRegister::LinearClamp);
		BindTexture(static_cast<TextureRegister>(8), _SMAASearchTexture.get(), SamplerStateRegister::LinearClamp);

		DrawTriangles(3, 0);

		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());
	}

	void Renderer::ApplyFXAA(IRenderSurface2D* renderTarget, RenderView& view)
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

		// Copy render target to temp render target.
		_graphicsDevice->ClearRenderTarget2D(_postProcessRenderTarget[0]->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(_postProcessRenderTarget[0]->GetRenderTarget(), nullptr);

		BindRenderTargetAsTexture(TextureRegister::ColorMap, renderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);

		// Apply FXAA
		_graphicsDevice->ClearRenderTarget2D(renderTarget->GetRenderTarget(), Colors::Black);
		_graphicsDevice->BindRenderTarget(renderTarget->GetRenderTarget(), nullptr);

		_shaders.Bind(Shader::Fxaa);

		_stPostProcessBuffer.ViewportSize = Vector2i(_graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight());
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());
		
		BindTexture(TextureRegister::ColorMap, _postProcessRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::AnisotropicClamp);

		DrawTriangles(3, 0);
	}
}