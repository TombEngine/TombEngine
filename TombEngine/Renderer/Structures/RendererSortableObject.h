#pragma once
#include "Renderer/Graphics/Vertices/Vertex.h"
#include "Renderer/RendererEnums.h"
#include "Renderer/Structures/RendererBucket.h"
#include "Renderer/Structures/RendererEffect.h"
#include "Renderer/Structures/RendererItem.h"
#include "Renderer/Structures/RendererMesh.h"
#include "Renderer/Structures/RendererRoom.h"
#include "Renderer/Structures/RendererSpriteToDraw.h"
#include "Renderer/Structures/RendererStatic.h"
#include "Specific/Structures/fast_vector.h"

namespace TEN::Renderer::Structures
{
	// Draw state shared by every sorted polygon of one object bucket (or by a single sprite).
	// Polygons reference it through RendererSortablePolygon::ObjectIndex.
	struct RendererSortableObject
	{
		RendererObjectType ObjectType;

		Matrix World = Matrix::Identity;

		BlendMode BlendMode = BlendMode::Opaque;
		LightMode LightMode = LightMode::Dynamic;

		// Draw group of the object, assigned by SortTransparentFaces. Consecutive sorted polygons
		// sharing the group draw as a single batch.
		int GroupIndex = 0;

		// Depth range and normalized screen rectangle (min x, min y, max x, max y) of the polygon
		// centres, used to find overlapping objects whose polygons must be interleaved.
		int		MinDistance	 = INT_MAX;
		int		MaxDistance	 = 0;
		Vector4 ScreenBounds = Vector4(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);

		bool Skinned = false;

		RendererRoom*	Room   = nullptr;
		RendererBucket* Bucket = nullptr;

		union
		{
			RendererItem*		  Item;
			RendererStatic*		  Static;
			RendererEffect*		  Effect;
			RendererSpriteToDraw* Sprite;
		};
	};

	// Per-polygon entry of the transparent pass. Sprites use a single entry with no polygon.
	struct RendererSortablePolygon
	{
		RendererPolygon* Polygon	 = nullptr;
		int				 Distance	 = 0;
		int				 ObjectIndex = 0;
	};

	// Compact sort entry for the transparent pass. Key packs group rank (high 32 bits) and
	// inverted distance (low 32 bits); Index points into RenderView::TransparentPolygonsToDraw.
	struct RendererSortKey
	{
		unsigned long long Key	 = 0;
		int				   Index = 0;
	};
}
