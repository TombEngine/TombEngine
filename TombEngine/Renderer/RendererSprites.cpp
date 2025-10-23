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

				BindRenderTargetAsTexture(TextureRegister::GBufferDepthMap, &_depthRenderTarget, SamplerStateRegister::PointWrap);

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
					_stInstancedSpriteBuffer.Sprites[i].PerVertexColor = 0;
					_stInstancedSpriteBuffer.Sprites[i].IsSoftParticle = spriteToDraw.SoftParticle ? 1.0f : 0.0f;
					_stInstancedSpriteBuffer.Sprites[i].RenderType = (int)spriteToDraw.renderType;

					PackSpriteTextureCoordinates(i, spriteToDraw.Sprite);
				}
			};
			g_Parallel.AddTasks((int)spriteBucket.SpritesToDraw.size(), prepareSprites).wait();

			BindTexture(TextureRegister::ColorMap, spriteBucket.Sprite->Texture, SamplerStateRegister::LinearClamp);
			UpdateConstantBuffer(_stInstancedSpriteBuffer, _cbInstancedSpriteBuffer);;

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

				BindRenderTargetAsTexture(TextureRegister::GBufferDepthMap, &_depthRenderTarget, SamplerStateRegister::PointWrap);

				SetDepthState(DepthState::Read);
				SetCullMode(CullMode::None);

				_shaders.Bind(Shader::InstancedSprites);

				// Set up vertex buffer and parameters.
				unsigned int stride = sizeof(Vertex);
				unsigned int offset = 0;
				_context->IASetVertexBuffers(0, 1, _spriteVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

				wasGpuSet = true;
			}
			
			_stInstancedSpriteBuffer.Sprites[0].IsBillboard = 0;
			_stInstancedSpriteBuffer.Sprites[0].World = Matrix::Identity;
			_stInstancedSpriteBuffer.Sprites[0].IsSoftParticle = spriteBucket.IsSoftParticle ? 1.0f : 0.0f;
			_stInstancedSpriteBuffer.Sprites[0].RenderType = (int)spriteBucket.RenderType;

			_stInstancedSpriteBuffer.Sprites[0].PerVertexColor = 1;
			_stInstancedSpriteBuffer.Sprites[0].IsSoftParticle = spriteBucket.IsSoftParticle ? 1.0f : 0.0f;

			PackSpriteTextureCoordinates(0, spriteBucket.Sprite);

			UpdateConstantBuffer(_stInstancedSpriteBuffer, _cbInstancedSpriteBuffer);;

			BindTexture(TextureRegister::ColorMap, spriteBucket.Sprite->Texture, SamplerStateRegister::LinearClamp);

			int spritesToDraw = 0;

			for (auto& rDrawSprite : spriteBucket.SpritesToDraw)
			{
				auto vertex0 = Vertex{};
				vertex0.Position = rDrawSprite.vtx1;
				vertex0.UV = rDrawSprite.Sprite->UV[0];
				vertex0.Color = VectorColorToRGBA_TempToVector4(rDrawSprite.c1);
				vertex0.Effects = 0 << INDEX_IN_POLY_VERTEX_SHIFT;

				ReflectVectorOptionally(vertex0.Position);

				auto vertex1 = Vertex{};
				vertex1.Position = rDrawSprite.vtx2;
				vertex1.UV = rDrawSprite.Sprite->UV[1];
				vertex1.Color = VectorColorToRGBA_TempToVector4(rDrawSprite.c2);
				vertex1.Effects = 1 << INDEX_IN_POLY_VERTEX_SHIFT;

				ReflectVectorOptionally(vertex1.Position);

				auto vertex2 = Vertex{};
				vertex2.Position = rDrawSprite.vtx3;
				vertex2.UV = rDrawSprite.Sprite->UV[2];
				vertex2.Color = VectorColorToRGBA_TempToVector4(rDrawSprite.c3);
				vertex2.Effects = 2 << INDEX_IN_POLY_VERTEX_SHIFT;

				ReflectVectorOptionally(vertex2.Position);

				auto vertex3 = Vertex{};
				vertex3.Position = rDrawSprite.vtx4;
				vertex3.UV = rDrawSprite.Sprite->UV[3];
				vertex3.Color = VectorColorToRGBA_TempToVector4(rDrawSprite.c4);
				vertex3.Effects = 3 << INDEX_IN_POLY_VERTEX_SHIFT;

				ReflectVectorOptionally(vertex3.Position);

				_spriteVertices.push_back(vertex0);
				_spriteVertices.push_back(vertex1);
				_spriteVertices.push_back(vertex3);
				_spriteVertices.push_back(vertex2);
				_spriteVertices.push_back(vertex3);
				_spriteVertices.push_back(vertex1);

				spritesToDraw++;

				if (spritesToDraw == INSTANCED_SPRITES_BUCKET_SIZE || spritesToDraw == spriteBucket.SpritesToDraw.size())
				{
					_spriteVertexBuffer.Update(_context.Get(), _spriteVertices.data(), 0, spritesToDraw * 6);

					DrawInstancedTriangles(spritesToDraw * 6, 1, 0);

					_numInstancedSpritesDrawCalls++;

					spritesToDraw = 0;
					_spriteVertices.clear();
				}
			}
		}

		// Set up vertex parameters.
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void Renderer::DrawSingleSprite(RendererSortableObject* object, RendererObjectType lastObjectType, RenderView& view)
	{
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		BindRenderTargetAsTexture(TextureRegister::GBufferDepthMap, &_depthRenderTarget, SamplerStateRegister::LinearClamp);

		SetDepthState(DepthState::Read);
		SetCullMode(CullMode::None);
		SetBlendMode(object->Sprite->BlendMode);
		SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

		_shaders.Bind(Shader::InstancedSprites);

		_stInstancedSpriteBuffer.Sprites[0].World = object->Sprite->Type != SpriteType::ThreeD ?
			GetWorldMatrixForSprite(*object->Sprite, view) :
			Matrix::Identity;
		_stInstancedSpriteBuffer.Sprites[0].PerVertexColor = 1;
		_stInstancedSpriteBuffer.Sprites[0].IsSoftParticle = object->Sprite->SoftParticle ? 1 : 0;
		_stInstancedSpriteBuffer.Sprites[0].RenderType = (int)object->Sprite->renderType;

		PackSpriteTextureCoordinates(0, object->Sprite->Sprite);

		UpdateConstantBuffer(_stInstancedSpriteBuffer, _cbInstancedSpriteBuffer);;

		BindTexture(TextureRegister::ColorMap, object->Sprite->Sprite->Texture, SamplerStateRegister::LinearClamp);
		
		// Set up vertex buffer and parameters.
		unsigned int stride = sizeof(Vertex);
		unsigned int offset = 0;

		if (object->Sprite->Type != SpriteType::ThreeD)
		{
			_context->IASetVertexBuffers(0, 1, _quadVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		}
		else
		{
			auto vertex0 = Vertex{};
			vertex0.Position = object->Sprite->vtx1;
			vertex0.UV = object->Sprite->Sprite->UV[0];
			vertex0.Color = VectorColorToRGBA_TempToVector4(object->Sprite->c1);
			vertex0.Effects = 0 << INDEX_IN_POLY_VERTEX_SHIFT;

			auto vertex1 = Vertex{};
			vertex1.Position = object->Sprite->vtx2;
			vertex1.UV = object->Sprite->Sprite->UV[1];
			vertex1.Color = VectorColorToRGBA_TempToVector4(object->Sprite->c2);
			vertex1.Effects = 1 << INDEX_IN_POLY_VERTEX_SHIFT;

			auto vertex2 = Vertex{};
			vertex2.Position = object->Sprite->vtx3;
			vertex2.UV = object->Sprite->Sprite->UV[2];
			vertex2.Color = VectorColorToRGBA_TempToVector4(object->Sprite->c3);
			vertex2.Effects = 2 << INDEX_IN_POLY_VERTEX_SHIFT;

			auto vertex3 = Vertex{};
			vertex3.Position = object->Sprite->vtx4;
			vertex3.UV = object->Sprite->Sprite->UV[3];
			vertex3.Color = VectorColorToRGBA_TempToVector4(object->Sprite->c4);
			vertex3.Effects = 3 << INDEX_IN_POLY_VERTEX_SHIFT;

			_spriteVertices.clear();
			_spriteVertices.push_back(vertex0);
			_spriteVertices.push_back(vertex1);
			_spriteVertices.push_back(vertex3);
			_spriteVertices.push_back(vertex2);

			_spriteVertexBuffer.Update(_context.Get(), _spriteVertices.data(), 0, 4);

			_context->IASetVertexBuffers(0, 1, _spriteVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		}

		// Draw sprites with instancing.
		DrawInstancedTriangles(4, 1, 0);

		_numSortedSpritesDrawCalls++;
		_numSortedTriangles += 2;

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void Renderer::DrawSpriteSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view)
	{
		unsigned int stride = sizeof(Vertex);
		unsigned int offset = 0;

		_shaders.Bind(Shader::InstancedSprites);

		_sortedPolygonsVertexBuffer.Update(_context.Get(), _sortedPolygonsVertices.data(), 0, (int)_sortedPolygonsVertices.size());

		_context->IASetVertexBuffers(0, 1, _sortedPolygonsVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_inputLayout.Get());

		_stInstancedSpriteBuffer.Sprites[0].World = Matrix::Identity;
		_stInstancedSpriteBuffer.Sprites[0].PerVertexColor = 1;
		_stInstancedSpriteBuffer.Sprites[0].IsSoftParticle = objectInfo->Sprite->SoftParticle ? 1 : 0;
		_stInstancedSpriteBuffer.Sprites[0].RenderType = (int)objectInfo->Sprite->renderType;

		PackSpriteTextureCoordinates(0, objectInfo->Sprite->Sprite);

		UpdateConstantBuffer(_stInstancedSpriteBuffer, _cbInstancedSpriteBuffer);;

		SetDepthState(DepthState::Read);
		SetCullMode(CullMode::None);
		SetBlendMode(objectInfo->Sprite->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		BindTexture(TextureRegister::ColorMap, objectInfo->Sprite->Sprite->Texture, SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(TextureRegister::GBufferDepthMap, &_depthRenderTarget, SamplerStateRegister::PointWrap);

		DrawInstancedTriangles((int)_sortedPolygonsVertices.size(), 1, 0);

		_numSortedSpritesDrawCalls++;
		_numSortedTriangles += (int)_sortedPolygonsVertices.size() / 3;
	}

	void Renderer::PackSpriteTextureCoordinates(int instanceId, RendererSprite* sprite)
	{
		// NOTE: Strange packing due to particular HLSL 16 byte alignment requirements.
	
		_stInstancedSpriteBuffer.Sprites[instanceId].UV[0].x = sprite->UV[0].x;
		_stInstancedSpriteBuffer.Sprites[instanceId].UV[0].y = sprite->UV[1].x;
		_stInstancedSpriteBuffer.Sprites[instanceId].UV[0].z = sprite->UV[2].x;
		_stInstancedSpriteBuffer.Sprites[instanceId].UV[0].w = sprite->UV[3].x;
		_stInstancedSpriteBuffer.Sprites[instanceId].UV[1].x = sprite->UV[0].y;
		_stInstancedSpriteBuffer.Sprites[instanceId].UV[1].y = sprite->UV[1].y;
		_stInstancedSpriteBuffer.Sprites[instanceId].UV[1].z = sprite->UV[2].y;
		_stInstancedSpriteBuffer.Sprites[instanceId].UV[1].w = sprite->UV[3].y;
	}
}
