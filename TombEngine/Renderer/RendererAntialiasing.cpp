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
		BindPipeline(Pipelines::FullscreenPass, true);

		// Common vertex shader to all fullscreen effects
		_shaders.Bind(Shader::PostProcess);

		// We draw a fullscreen triangle
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_fullScreenVertexInputLayout.get());
		_graphicsDevice->BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		// Copy render target to SMAA scene target.
		{
			RenderPassDescriptor pass;
			pass.Name = "SMAA: Copy";
			pass.ColorAttachments.push_back({
				_SMAASceneRenderTarget->GetRenderTarget(), 0,
				LoadAction::Clear, StoreAction::Store, Colors::Transparent
			});
			pass.Viewport = view.Viewport;
			BeginRenderPass(pass);
		}

		BindRenderTargetAsTexture(TextureRegister::ColorMap, renderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);

		// 1) Edge detection using color method (also depth and luma available).
		BindPipeline(Pipelines::FullscreenPass);

		{
			RenderPassDescriptor pass;
			pass.Name = "SMAA: Edge Detection";
			pass.ColorAttachments.push_back({
				_SMAAEdgesRenderTarget->GetRenderTarget(), 0,
				LoadAction::Clear, StoreAction::Store, Colors::Transparent
			});
			pass.Viewport = view.Viewport;
			BeginRenderPass(pass);
		}

		_shaders.Bind(Shader::SmaaEdgeDetection);
		_shaders.Bind(Shader::SmaaColorEdgeDetection);

		_stSMAABuffer.BlendFactor = 1.0f;
		UpdateConstantBuffer(&_stSMAABuffer, _cbSMAABuffer.get());
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Slot0, _cbSMAABuffer.get());

		BindRenderTargetAsTexture(static_cast<TextureRegister>(0), _SMAASceneRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(1), _SMAASceneSRGBRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(5), _SMAAEdgesRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(static_cast<TextureRegister>(6), _SMAABlendRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);
		BindTexture(static_cast<TextureRegister>(7), _SMAAAreaTexture.get(), SamplerStateRegister::LinearClamp);
		BindTexture(static_cast<TextureRegister>(8), _SMAASearchTexture.get(), SamplerStateRegister::LinearClamp);

		DrawTriangles(3, 0);

		// 2) Blend weights calculation.
		{
			RenderPassDescriptor pass;
			pass.Name = "SMAA: Blend Weights";
			pass.ColorAttachments.push_back({
				_SMAABlendRenderTarget->GetRenderTarget(), 0,
				LoadAction::Clear, StoreAction::Store, Colors::Transparent
			});
			pass.Viewport = view.Viewport;
			BeginRenderPass(pass);
		}

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
		{
			RenderPassDescriptor pass;
			pass.Name = "SMAA: Neighborhood";
			pass.ColorAttachments.push_back({
				renderTarget->GetRenderTarget(), 0,
				LoadAction::Load, StoreAction::Store
			});
			pass.Viewport = view.Viewport;
			BeginRenderPass(pass);
		}

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
		BindPipeline(Pipelines::FullscreenPass, true);

		// Common vertex shader to all fullscreen effects
		_shaders.Bind(Shader::PostProcess);

		// We draw a fullscreen triangle
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_fullScreenVertexInputLayout.get());
		_graphicsDevice->BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		// Copy render target to temp render target.
		{
			RenderPassDescriptor pass;
			pass.Name = "FXAA: Copy";
			pass.ColorAttachments.push_back({
				_postProcessRenderTarget[0]->GetRenderTarget(), 0,
				LoadAction::Clear, StoreAction::Store, Colors::Transparent
			});
			pass.Viewport = view.Viewport;
			BeginRenderPass(pass);
		}

		BindRenderTargetAsTexture(TextureRegister::ColorMap, renderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);

		// Apply FXAA
		{
			RenderPassDescriptor pass;
			pass.Name = "FXAA: Apply";
			pass.ColorAttachments.push_back({
				renderTarget->GetRenderTarget(), 0,
				LoadAction::Clear, StoreAction::Store, Colors::Black
			});
			pass.Viewport = view.Viewport;
			BeginRenderPass(pass);
		}

		_shaders.Bind(Shader::Fxaa);

		// FXAA shader: PostProcess(Slot0)
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::Slot0, _cbPostProcessBuffer.get());

		_stPostProcessBuffer.ViewportSize = Vector2i(_graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight());
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		BindTexture(TextureRegister::ColorMap, _postProcessRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::AnisotropicClamp);

		DrawTriangles(3, 0);
	}
}
