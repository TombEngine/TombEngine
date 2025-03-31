#include "framework.h"
#include "Renderer/Structures/RendererSprite.h"

#include "Renderer/Structures/RendererSpriteBucket.h"
#include "Renderer/Renderer.h"
#include "Specific/Parallel.h"

using namespace TEN::Renderer::Structures;

namespace TEN::Renderer
{
	void Renderer::AddSpriteBillboard(RendererSprite* sprite, const Vector3& pos, const Vector4& color, float orient2D, float scale,
									  Vector2 size, BlendMode blendMode, bool isSoftParticle, RenderView& view, SpriteRenderType renderType)
	{
		if (scale <= 0.0f)
			scale = 1.0f;

		size.x *= scale;
		size.y *= scale;

		RendererSpriteToDraw spr = {};

		spr.Type = SpriteType::Billboard;
		spr.Sprite = sprite;
		spr.pos = pos;
		spr.Rotation = orient2D;
		spr.Scale = scale;
		spr.Width = size.x;
		spr.Height = size.y;
		spr.BlendMode = blendMode;
		spr.SoftParticle = isSoftParticle;
		spr.c1 = color;
		spr.c2 = color;
		spr.c3 = color;
		spr.c4 = color;
		spr.color = color;
		spr.renderType = renderType;

		view.SpritesToDraw.push_back(spr);
	}

	void Renderer::AddSpriteBillboardConstrained(RendererSprite* sprite, const Vector3& pos, const Vector4& color, float orient2D,
												 float scale, Vector2 size, BlendMode blendMode, const Vector3& constrainAxis,
												 bool isSoftParticle, RenderView& view, SpriteRenderType renderType)
	{
		if (scale <= 0.0f)
			scale = 1.0f;

		size.x *= scale;
		size.y *= scale;

		RendererSpriteToDraw spr = {};

		spr.Type = SpriteType::CustomBillboard;
		spr.Sprite = sprite;
		spr.pos = pos;
		spr.Rotation = orient2D;
		spr.Scale = scale;
		spr.Width = size.x;
		spr.Height = size.y;
		spr.BlendMode = blendMode;
		spr.ConstrainAxis = constrainAxis;
		spr.SoftParticle = isSoftParticle;
		spr.c1 = color;
		spr.c2 = color;
		spr.c3 = color;
		spr.c4 = color;
		spr.color = color;
		spr.renderType = renderType;

		view.SpritesToDraw.push_back(spr);
	}

	void Renderer::AddSpriteBillboardConstrainedLookAt(RendererSprite* sprite, const Vector3& pos, const Vector4& color, float orient2D,
		float scale, Vector2 size, BlendMode blendMode, const Vector3& lookAtAxis,
		bool isSoftParticle, RenderView& view, SpriteRenderType renderType)
	{
		if (scale <= 0.0f)
			scale = 1.0f;

		size.x *= scale;
		size.y *= scale;

		RendererSpriteToDraw spr = {};

		spr.Type = SpriteType::LookAtBillboard;
		spr.Sprite = sprite;
		spr.pos = pos;
		spr.Rotation = orient2D;
		spr.Scale = scale;
		spr.Width = size.x;
		spr.Height = size.y;
		spr.BlendMode = blendMode;
		spr.LookAtAxis = lookAtAxis;
		spr.SoftParticle = isSoftParticle;
		spr.c1 = color;
		spr.c2 = color;
		spr.c3 = color;
		spr.c4 = color;
		spr.color = color;
		spr.renderType = renderType;

		view.SpritesToDraw.push_back(spr);
	}

	void Renderer::AddSpriteBillboardRotated(RendererSprite* sprite, const Vector3& pos, const Vector4& color, float orient2D,
		float scale, Vector2 size, BlendMode blendMode, const Vector3& rotationDir,
		bool isSoftParticle, RenderView& view, SpriteRenderType renderType)
	{
		if (scale <= 0.0f)
			scale = 1.0f;

		size.x *= scale;
		size.y *= scale;

		RendererSpriteToDraw spr = {};

		spr.Type = SpriteType::RotatedBillboard;
		spr.Sprite = sprite;
		spr.pos = pos;
		spr.Rotation = orient2D;
		spr.Scale = scale;
		spr.Width = size.x;
		spr.Height = size.y;
		spr.BlendMode = blendMode;
		spr.LookAtAxis = rotationDir;
		spr.SoftParticle = isSoftParticle;
		spr.c1 = color;
		spr.c2 = color;
		spr.c3 = color;
		spr.c4 = color;
		spr.color = color;
		spr.renderType = renderType;

		view.SpritesToDraw.push_back(spr);
	}

	void Renderer::AddQuad(RendererSprite* sprite, const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2, const Vector3& vertex3,
		const Vector4 color, float orient2D, float scale, Vector2 size, BlendMode blendMode, bool softParticles,
		RenderView& view)
	{
		AddQuad(sprite, vertex0, vertex1, vertex2, vertex3, color, color, color, color, orient2D, scale, size, blendMode, softParticles, view, SpriteRenderType::Default);
	}

	void Renderer::AddQuad(RendererSprite* sprite, const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2, const Vector3& vertex3,
		const Vector4& color0, const Vector4& color1, const Vector4& color2, const Vector4& color3, float orient2D,
		float scale, Vector2 size, BlendMode blendMode, bool isSoftParticle, RenderView& view, SpriteRenderType renderType)
	{
		if (scale <= 0.0f)
			scale = 1.0f;

		size.x *= scale;
		size.y *= scale;

		RendererSpriteToDraw spr = {};

		spr.Type = SpriteType::ThreeD;
		spr.Sprite = sprite;
		spr.vtx1 = vertex0;
		spr.vtx2 = vertex1;
		spr.vtx3 = vertex2;
		spr.vtx4 = vertex3;
		spr.c1 = color0;
		spr.c2 = color1;
		spr.c3 = color2;
		spr.c4 = color3;
		spr.Rotation = orient2D;
		spr.Scale = scale;
		spr.Width = size.x;
		spr.Height = size.y;
		spr.BlendMode = blendMode;
		spr.pos = (vertex0 + vertex1 + vertex2 + vertex3) / 4.0f;
		spr.SoftParticle = isSoftParticle;
		spr.renderType = renderType;

		view.SpritesToDraw.push_back(spr);
	}

	void Renderer::AddColoredQuad(const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2, const Vector3& vertex3,
		const Vector4& color, BlendMode blendMode, RenderView& view)
	{
		AddColoredQuad(vertex0, vertex1, vertex2, vertex3, color, color, color, color, blendMode, view, SpriteRenderType::Default);
	}

	void Renderer::AddColoredQuad(const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2, const Vector3& vertex3,
		const Vector4& color0, const Vector4& color1, const Vector4& color2, const Vector4& color3,
		BlendMode blendMode, RenderView& view, SpriteRenderType renderType)
	{
		auto sprite = RendererSpriteToDraw{};

		sprite.Type = SpriteType::ThreeD;
		sprite.Sprite = &_whiteSprite;
		sprite.vtx1 = vertex0;
		sprite.vtx2 = vertex1;
		sprite.vtx3 = vertex2;
		sprite.vtx4 = vertex3;
		sprite.c1 = color0;
		sprite.c2 = color1;
		sprite.c3 = color2;
		sprite.c4 = color3;
		sprite.BlendMode = blendMode;
		sprite.pos = (vertex0 + vertex1 + vertex2 + vertex3) / 4.0f;
		sprite.SoftParticle = false;
		sprite.renderType = renderType;

		view.SpritesToDraw.push_back(sprite);
	}

	void Renderer::SortAndPrepareSprites(RenderView& view)
	{
		if (view.SpritesToDraw.empty())
		{
			return;
		}

		_spriteBuckets.clear();

		// Sort sprites by sprite and blend mode for faster batching.
		std::sort(
			view.SpritesToDraw.begin(),
			view.SpritesToDraw.end(),
			[](RendererSpriteToDraw& rDrawSprite0, RendererSpriteToDraw& rDrawSprite1)
			{
				if (rDrawSprite0.Sprite != rDrawSprite1.Sprite)
				{
					return (rDrawSprite0.Sprite > rDrawSprite1.Sprite);
				}
				else if (rDrawSprite0.BlendMode != rDrawSprite1.BlendMode)
				{
					return (rDrawSprite0.BlendMode > rDrawSprite1.BlendMode);
				}
				else
				{
					return (rDrawSprite0.Type > rDrawSprite1.Type);
				}
			}
		);

		// Group sprites to draw in buckets for instancing (billboards only).
		RendererSpriteBucket currentSpriteBucket;

		currentSpriteBucket.Sprite = view.SpritesToDraw[0].Sprite;
		currentSpriteBucket.BlendMode = view.SpritesToDraw[0].BlendMode;
		currentSpriteBucket.IsBillboard = view.SpritesToDraw[0].Type != SpriteType::ThreeD;
		currentSpriteBucket.IsSoftParticle = view.SpritesToDraw[0].SoftParticle;
		currentSpriteBucket.RenderType = view.SpritesToDraw[0].renderType;

		for (auto& rDrawSprite : view.SpritesToDraw)
		{
			bool isBillboard = rDrawSprite.Type != SpriteType::ThreeD;

			if (rDrawSprite.Sprite != currentSpriteBucket.Sprite ||
				rDrawSprite.BlendMode != currentSpriteBucket.BlendMode ||
				rDrawSprite.SoftParticle != currentSpriteBucket.IsSoftParticle ||
				rDrawSprite.renderType != currentSpriteBucket.RenderType ||
				currentSpriteBucket.SpritesToDraw.size() == INSTANCED_SPRITES_BUCKET_SIZE ||
				isBillboard != currentSpriteBucket.IsBillboard)
			{
				_spriteBuckets.push_back(currentSpriteBucket);

				currentSpriteBucket.Sprite = rDrawSprite.Sprite;
				currentSpriteBucket.BlendMode = rDrawSprite.BlendMode;
				currentSpriteBucket.IsBillboard = isBillboard;
				currentSpriteBucket.IsSoftParticle = rDrawSprite.SoftParticle;
				currentSpriteBucket.RenderType = rDrawSprite.renderType;
				currentSpriteBucket.SpritesToDraw.clear();
			}

			if (rDrawSprite.BlendMode != BlendMode::Opaque &&
				rDrawSprite.BlendMode != BlendMode::Additive &&
				rDrawSprite.BlendMode != BlendMode::AlphaTest)
			{
				int distance = (rDrawSprite.pos - Camera.pos.ToVector3()).Length();

				RendererSortableObject object;
				object.ObjectType = RendererObjectType::Sprite;
				object.Centre = rDrawSprite.pos;
				object.Distance = distance;
				object.Sprite = &rDrawSprite;

				view.TransparentObjectsToDraw.push_back(object);
			}
			else
			{
				currentSpriteBucket.SpritesToDraw.push_back(rDrawSprite);
			}
		}

		_spriteBuckets.push_back(currentSpriteBucket);
	}

	void Renderer::DrawSprites(RenderView& view, RendererPass rendererPass)
	{
		if (view.SpritesToDraw.empty())
			return;

		// Draw instanced sprites.
		bool wasGpuSet = false;
		for (const auto& spriteBucket : _spriteBuckets)
		{
			if (spriteBucket.SpritesToDraw.empty() || !spriteBucket.IsBillboard)
				continue;

			if (!SetupBlendModeAndAlphaTest(spriteBucket.BlendMode, rendererPass, 0))
				continue;

			if (!wasGpuSet)
			{
				_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

				BindRenderTargetAsTexture(TextureRegister::DepthMap, &_depthRenderTarget, SamplerStateRegister::PointWrap);

				SetDepthState(DepthState::Read);
				SetCullMode(CullMode::None);

				_shaders.Bind(Shader::InstancedSprites);

				// Set up vertex buffer and parameters.
				unsigned int stride = sizeof(Vertex);
				unsigned int offset = 0;
				_context->IASetVertexBuffers(0, 1, _quadVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

				wasGpuSet = true;
			}

			// Define sprite preparation logic.
			auto prepareSprites = [&](int start, int end)
			{
				for (int i = start; i < end; i++)
				{
					const auto& spriteToDraw = spriteBucket.SpritesToDraw[i];

					_stInstancedSpriteBuffer.Sprites[i].World = GetWorldMatrixForSprite(spriteToDraw, view);
					_stInstancedSpriteBuffer.Sprites[i].Color = spriteToDraw.color;
					_stInstancedSpriteBuffer.Sprites[i].IsBillboard = 1.0f;
					_stInstancedSpriteBuffer.Sprites[i].IsSoftParticle = spriteToDraw.SoftParticle ? 1.0f : 0.0f;

					// NOTE: Strange packing due to particular HLSL 16 byte alignment requirements.
					_stInstancedSpriteBuffer.Sprites[i].UV[0].x = spriteToDraw.Sprite->UV[0].x;
					_stInstancedSpriteBuffer.Sprites[i].UV[0].y = spriteToDraw.Sprite->UV[1].x;
					_stInstancedSpriteBuffer.Sprites[i].UV[0].z = spriteToDraw.Sprite->UV[2].x;
					_stInstancedSpriteBuffer.Sprites[i].UV[0].w = spriteToDraw.Sprite->UV[3].x;
					_stInstancedSpriteBuffer.Sprites[i].UV[1].x = spriteToDraw.Sprite->UV[0].y;
					_stInstancedSpriteBuffer.Sprites[i].UV[1].y = spriteToDraw.Sprite->UV[1].y;
					_stInstancedSpriteBuffer.Sprites[i].UV[1].z = spriteToDraw.Sprite->UV[2].y;
					_stInstancedSpriteBuffer.Sprites[i].UV[1].w = spriteToDraw.Sprite->UV[3].y;
				}
			};
			g_Parallel.AddTasks((int)spriteBucket.SpritesToDraw.size(), prepareSprites).wait();

			BindTexture(TextureRegister::ColorMap, spriteBucket.Sprite->Texture, SamplerStateRegister::LinearClamp);
			_cbInstancedSpriteBuffer.UpdateData(_stInstancedSpriteBuffer, _context.Get());

			// Draw sprites with instancing.
			DrawInstancedTriangles(4, (int)spriteBucket.SpritesToDraw.size(), 0);

			_numInstancedSpritesDrawCalls++;
		}

		// Draw 3D non-instanced sprites.
		wasGpuSet = false;

		for (auto& spriteBucket : _spriteBuckets)
		{
			if (spriteBucket.SpritesToDraw.empty() || spriteBucket.IsBillboard)
				continue;

			if (!SetupBlendModeAndAlphaTest(spriteBucket.BlendMode, rendererPass, 0))
				continue;

			if (!wasGpuSet)
			{
				_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				BindRenderTargetAsTexture(TextureRegister::DepthMap, &_depthRenderTarget, SamplerStateRegister::PointWrap);

				SetDepthState(DepthState::Read);
				SetCullMode(CullMode::None);

				_shaders.Bind(Shader::Sprites);

				wasGpuSet = true;
			}
			
			_stSprite.IsSoftParticle = spriteBucket.IsSoftParticle ? 1.0f : 0.0f;
			_stSprite.RenderType = (int)spriteBucket.RenderType;
			_cbSprite.UpdateData(_stSprite, _context.Get());

			BindTexture(TextureRegister::ColorMap, spriteBucket.Sprite->Texture, SamplerStateRegister::LinearClamp);

			_primitiveBatch->Begin();

			for (auto& rDrawSprite : spriteBucket.SpritesToDraw)
			{
				auto vertex0 = Vertex{};
				vertex0.Position = rDrawSprite.vtx1;
				vertex0.UV = rDrawSprite.Sprite->UV[0];
				vertex0.Color = rDrawSprite.c1;

				ReflectVectorOptionally(vertex0.Position);

				auto vertex1 = Vertex{};
				vertex1.Position = rDrawSprite.vtx2;
				vertex1.UV = rDrawSprite.Sprite->UV[1];
				vertex1.Color = rDrawSprite.c2;

				ReflectVectorOptionally(vertex1.Position);

				auto vertex2 = Vertex{};
				vertex2.Position = rDrawSprite.vtx3;
				vertex2.UV = rDrawSprite.Sprite->UV[2];
				vertex2.Color = rDrawSprite.c3;

				ReflectVectorOptionally(vertex2.Position);

				auto vertex3 = Vertex{};
				vertex3.Position = rDrawSprite.vtx4;
				vertex3.UV = rDrawSprite.Sprite->UV[3];
				vertex3.Color = rDrawSprite.c4;

				ReflectVectorOptionally(vertex3.Position);

				_primitiveBatch->DrawTriangle(vertex0, vertex1, vertex3);
				_primitiveBatch->DrawTriangle(vertex1, vertex2, vertex3);

				_numTriangles += 2;
				_numSpritesDrawCalls += 2;
				_numDrawCalls += 2;
			}

			_primitiveBatch->End();
		}

		// Set up vertex parameters.
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void Renderer::DrawSingleSprite(RendererSortableObject* object, RendererObjectType lastObjectType, RenderView& view)
	{
		if (object->Sprite->Type != SpriteType::ThreeD)
		{
			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

			BindRenderTargetAsTexture(TextureRegister::DepthMap, &_depthRenderTarget, SamplerStateRegister::LinearClamp);

			SetDepthState(DepthState::Read);
			SetCullMode(CullMode::None);
			SetBlendMode(object->Sprite->BlendMode);
			SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

			_shaders.Bind(Shader::InstancedSprites);

			// Set up vertex buffer and parameters.
			unsigned int stride = sizeof(Vertex);
			unsigned int offset = 0;
			_context->IASetVertexBuffers(0, 1, _quadVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

			_stInstancedSpriteBuffer.Sprites[0].World = GetWorldMatrixForSprite(*object->Sprite, view);
			_stInstancedSpriteBuffer.Sprites[0].Color = object->Sprite->color;
			_stInstancedSpriteBuffer.Sprites[0].IsBillboard = 1;
			_stInstancedSpriteBuffer.Sprites[0].IsSoftParticle = object->Sprite->SoftParticle ? 1 : 0;

			// NOTE: Strange packing due to particular HLSL 16 byte alignment requirements.
			_stInstancedSpriteBuffer.Sprites[0].UV[0].x = object->Sprite->Sprite->UV[0].x;
			_stInstancedSpriteBuffer.Sprites[0].UV[0].y = object->Sprite->Sprite->UV[1].x;
			_stInstancedSpriteBuffer.Sprites[0].UV[0].z = object->Sprite->Sprite->UV[2].x;
			_stInstancedSpriteBuffer.Sprites[0].UV[0].w = object->Sprite->Sprite->UV[3].x;
			_stInstancedSpriteBuffer.Sprites[0].UV[1].x = object->Sprite->Sprite->UV[0].y;
			_stInstancedSpriteBuffer.Sprites[0].UV[1].y = object->Sprite->Sprite->UV[1].y;
			_stInstancedSpriteBuffer.Sprites[0].UV[1].z = object->Sprite->Sprite->UV[2].y;
			_stInstancedSpriteBuffer.Sprites[0].UV[1].w = object->Sprite->Sprite->UV[3].y;

			BindTexture(TextureRegister::ColorMap, object->Sprite->Sprite->Texture, SamplerStateRegister::LinearClamp);

			_cbInstancedSpriteBuffer.UpdateData(_stInstancedSpriteBuffer, _context.Get());
			 
			// Draw sprites with instancing.
			DrawInstancedTriangles(4, 1, 0);

			_numSortedSpritesDrawCalls++;
			_numSortedTriangles += 2;

			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}
		else
		{
			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			BindRenderTargetAsTexture(TextureRegister::DepthMap, &_depthRenderTarget, SamplerStateRegister::LinearClamp);

			SetDepthState(DepthState::Read);
			SetCullMode(CullMode::None);
			SetBlendMode(object->Sprite->BlendMode);
			SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

			_shaders.Bind(Shader::Sprites);

			_stSprite.IsSoftParticle = object->Sprite->SoftParticle ? 1 : 0;
			_stSprite.RenderType = (int)object->Sprite->renderType;
			_cbSprite.UpdateData(_stSprite, _context.Get());

			BindTexture(TextureRegister::ColorMap, object->Sprite->Sprite->Texture, SamplerStateRegister::LinearClamp);

			auto vertex0 = Vertex{};
			vertex0.Position = object->Sprite->vtx1;
			vertex0.UV = object->Sprite->Sprite->UV[0];
			vertex0.Color = object->Sprite->c1;

			auto vertex1 = Vertex{};
			vertex1.Position = object->Sprite->vtx2;
			vertex1.UV = object->Sprite->Sprite->UV[1];
			vertex1.Color = object->Sprite->c2;

			auto vertex2 = Vertex{};
			vertex2.Position = object->Sprite->vtx3;
			vertex2.UV = object->Sprite->Sprite->UV[2];
			vertex2.Color = object->Sprite->c3;

			auto vertex3 = Vertex{};
			vertex3.Position = object->Sprite->vtx4;
			vertex3.UV = object->Sprite->Sprite->UV[3];
			vertex3.Color = object->Sprite->c4;

			_primitiveBatch->Begin();
			_primitiveBatch->DrawTriangle(vertex0, vertex1, vertex3);
			_primitiveBatch->DrawTriangle(vertex1, vertex2, vertex3);
			_primitiveBatch->End();

			_numSortedSpritesDrawCalls += 2;
			_numDrawCalls += 2;
			_numTriangles += 2;
			_numSortedTriangles += 2;
		}
	}

	void Renderer::DrawSpriteSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view)
	{
		unsigned int stride = sizeof(Vertex);
		unsigned int offset = 0;

		_shaders.Bind(Shader::Sprites);

		_sortedPolygonsVertexBuffer.Update(_context.Get(), _sortedPolygonsVertices.data(), 0, (int)_sortedPolygonsVertices.size());

		_context->IASetVertexBuffers(0, 1, _sortedPolygonsVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		_stSprite.IsSoftParticle = objectInfo->Sprite->SoftParticle ? 1 : 0;
		_stSprite.RenderType = (int)objectInfo->Sprite->renderType;
		_cbSprite.UpdateData(_stSprite, _context.Get());

		SetDepthState(DepthState::Read);
		SetCullMode(CullMode::None);
		SetBlendMode(objectInfo->Sprite->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		BindTexture(TextureRegister::ColorMap, objectInfo->Sprite->Sprite->Texture, SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(TextureRegister::DepthMap, &_depthRenderTarget, SamplerStateRegister::PointWrap);

		DrawTriangles((int)_sortedPolygonsVertices.size(), 0);

		_numSortedSpritesDrawCalls++;
		_numSortedTriangles += (int)_sortedPolygonsVertices.size() / 3;
	}
}
