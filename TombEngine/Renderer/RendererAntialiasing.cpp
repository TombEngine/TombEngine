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

		BindFullscreenQuadState();

		auto fullscreenPass = [&](IRenderTarget2D* target, const XMVECTORF32& clearColor, const char* label) {
			RenderPassDescriptor pass;
			pass.ColorAttachments = { ColorAttachmentDescriptor::Clear(target, clearColor) };
			pass.HasViewport      = true;
			pass.Viewport         = view.Viewport;
			pass.DebugLabel       = label;
			BeginRenderPass(pass);
		};

		// Copy scene to SMAA scene target.
		fullscreenPass(_SMAASceneRenderTarget->GetRenderTarget(), Colors::Transparent, "SMAA Scene Copy");
		BindRenderTargetAsTexture(TextureRegister::ColorMap, renderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);
		EndRenderPass();

		// Pre-clear blend RT (the edge-detection subpass needs a clean blend target).
		fullscreenPass(_SMAABlendRenderTarget->GetRenderTarget(), Colors::Transparent, "SMAA Blend Pre-clear");
		EndRenderPass();

		// 1) Edge detection (color method).
		fullscreenPass(_SMAAEdgesRenderTarget->GetRenderTarget(), Colors::Transparent, "SMAA Edge Detection");
		SetCullMode(CullMode::CounterClockwise);

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
		EndRenderPass();

		// 2) Blend weight calculation.
		fullscreenPass(_SMAABlendRenderTarget->GetRenderTarget(), Colors::Transparent, "SMAA Blend Weights");

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
		EndRenderPass();

		// 3) Neighborhood blending (writes back into the source target).
		{
			RenderPassDescriptor pass;
			pass.ColorAttachments = { ColorAttachmentDescriptor::Keep(renderTarget->GetRenderTarget()) };
			pass.HasViewport      = true;
			pass.Viewport         = view.Viewport;
			pass.DebugLabel       = "SMAA Neighborhood Blend";
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
		EndRenderPass();

		SetPrimitiveType(PrimitiveType::TriangleList);
		SetInputLayout(_vertexInputLayout.get());
	}

	void Renderer::ApplyFXAA(IRenderSurface2D* renderTarget, RenderView& view)
	{
		SetBlendMode(BlendMode::Opaque, true);
		SetCullMode(CullMode::CounterClockwise, true);
		SetDepthState(DepthState::Write, true);

		BindFullscreenQuadState();

		auto fullscreenPass = [&](IRenderTarget2D* target, const XMVECTORF32& clearColor, const char* label) {
			RenderPassDescriptor pass;
			pass.ColorAttachments = { ColorAttachmentDescriptor::Clear(target, clearColor) };
			pass.HasViewport      = true;
			pass.Viewport         = view.Viewport;
			pass.DebugLabel       = label;
			BeginRenderPass(pass);
		};

		// Copy scene to temp render target.
		fullscreenPass(_postProcessRenderTarget[0]->GetRenderTarget(), Colors::Transparent, "FXAA Scene Copy");
		BindRenderTargetAsTexture(TextureRegister::ColorMap, renderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);
		DrawTriangles(3, 0);
		EndRenderPass();

		// Apply FXAA back into the source target.
		fullscreenPass(renderTarget->GetRenderTarget(), Colors::Black, "FXAA");

		_shaders.Bind(Shader::Fxaa);

		_stPostProcessBuffer.ViewportSize = Vector2i(_graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight());
		UpdateConstantBuffer(&_stPostProcessBuffer, _cbPostProcessBuffer.get());

		BindTexture(TextureRegister::ColorMap, _postProcessRenderTarget[0]->GetRenderTarget(), SamplerStateRegister::AnisotropicClamp);

		DrawTriangles(3, 0);
		EndRenderPass();
	}
}
