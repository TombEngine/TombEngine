#pragma once

namespace TEN::Renderer::Structures
{
	// Image file of the 2D layer: the next renderer loads it from its path. Width and height are 0 when the file is missing.
	struct RendererImage
	{
		std::string Path   = {};
		int         Width  = 0;
		int         Height = 0;

		bool IsValid() const { return (Width > 0 && Height > 0); }
	};
}
