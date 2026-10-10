#include "framework.h"
#include "Renderer/Renderer.h"

#include "Renderer/Structures/RendererSprite.h"
#include "Renderer/Structures/RendererSpriteBucket.h"
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
		spr.c1 = spr.c2 = spr.c3 = spr.c4 = spr.color = color;
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
		spr.c1 = spr.c2 = spr.c3 = spr.c4 = spr.color = color;
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
		spr.c1 = spr.c2 = spr.c3 = spr.c4 = spr.color = color;
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

	void Renderer::AddQuad(RendererSprite* sprite, const Vector3& pos, const Vector4& color, float orient2D, float scale, float size, BlendMode blendMode, const Vector3& constraintAxis, bool isSoftParticle, RenderView& view, SpriteRenderType renderType)
	{
		auto rot = Matrix::CreateFromYawPitchRoll(constraintAxis.y, constraintAxis.x, constraintAxis.z);
		auto half = size * (float)scale * (float)scale * 0.5f;
		auto right = Vector3::Transform(Vector3(half, 0.0f, 0.0f), rot);
		auto up = Vector3::Transform(Vector3(0.0f, half, 0.0f), rot);

		AddQuad(sprite, pos - right + up, pos + right + up,	pos + right - up, pos - right - up, color, orient2D, scale,	Vector2(size, size), blendMode, true, view);
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
				rDrawSprite.BlendMode != BlendMode::Distortion &&
				rDrawSprite.BlendMode != BlendMode::AlphaTest)
			{
				int distance = (rDrawSprite.pos - view.Camera.WorldPosition).Length();

				auto object = RendererSortableObject{};
				object.ObjectType = RendererObjectType::Sprite;
				object.Sprite = &rDrawSprite;

				CollectSortedSprite(view, object, distance);
			}
			else
			{
				currentSpriteBucket.SpritesToDraw.push_back(rDrawSprite);
			}
		}

		_spriteBuckets.push_back(currentSpriteBucket);
	}

}
