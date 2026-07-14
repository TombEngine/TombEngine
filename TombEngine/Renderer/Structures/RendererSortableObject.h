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
	struct RendererSortableObject
	{
		RendererObjectType ObjectType;

		Matrix	World	 = Matrix::Identity;
		Vector3 Centre	 = Vector3::Zero; // TODO: Rename to Center.
		int		Distance = 0;

		BlendMode BlendMode = BlendMode::Opaque;
		LightMode LightMode = LightMode::Dynamic;

		// Position of the object's draw group in the sorted pass order, assigned by
		// SortTransparentFaces. Polygons sharing the rank draw as a single batch.
		int GroupRank = 0;

		bool Skinned = false;

		RendererRoom*	 Room	 = nullptr;
		RendererBucket*	 Bucket	 = nullptr;
		RendererPolygon* Polygon = nullptr;

		union
		{
			RendererItem*		  Item;
			RendererStatic*		  Static;
			RendererEffect*		  Effect;
			RendererSpriteToDraw* Sprite;
		};
	};
}
