#pragma once

#include "Renderer/Graphics/ITexture2D.h"

namespace TEN::Renderer::Graphics
{
	// Asynchronous GPU -> CPU readback for small fixed-size textures.
	//
	// Usage:
	//   1. Create once via IGraphicsDevice::CreateGpuReadbackBuffer(width, height, format).
	//      The buffer matches that texture description; sources passed to SubmitCopy
	//      must have the same dimensions and a compatible format.
	//   2. Each frame (or every N frames), call SubmitCopy(src) to schedule a GPU copy
	//      from the source texture into the next internal slot.
	//   3. Call TryRead(out, bytes) to consume the most recently completed copy.
	//      Returns false until enough copies have been submitted to guarantee that
	//      the data is no longer in flight on the GPU. Caller should keep using
	//      its previously cached value while TryRead returns false.
	//
	// The implementation double-buffers internally so that TryRead never CPU-stalls
	// waiting on the GPU — the slot being read is always at least one SubmitCopy
	// older than the slot currently being written.
	class IGpuReadbackBuffer
	{
	public:
		virtual ~IGpuReadbackBuffer() = default;

		// Schedules a GPU copy from `src` into the next ring slot.
		// `src` must have the same width/height/format as this buffer.
		virtual void SubmitCopy(ITexture2D* src) = 0;

		// Reads the most recently completed copy into `outBytes`.
		// Returns false on cold start (fewer than 2 SubmitCopy calls so far) or
		// if the GPU hasn't finished writing yet. `byteCount` must equal the
		// size requested at creation time.
		virtual bool TryRead(void* outBytes, int byteCount) = 0;
	};
}
