#pragma once
#include "Renderer/Structures/RendererSortableObject.h"

namespace TEN::Renderer::Structures
{
	// Batch of consecutive sortable objects sharing GPU state, recorded by DrawSortedFaces
	// while it accumulates geometry, and drawn after a single buffer upload per flush.
	struct RendererSortedBatch
	{
		RendererSortableObject* Object; // First object of the batch, provides the shared state.
		int Base;                       // Base index (base vertex for sprites) into the sorted buffers.
		int Count;                      // Index count (vertex count for sprites).
	};
}
