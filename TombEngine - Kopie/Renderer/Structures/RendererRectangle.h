#pragma once

namespace TEN::Renderer::Structures
{
	struct RendererRectangle
	{
		int Left;
		int Top;
		int Right;
		int Bottom;

		RendererRectangle()
		{
			Left = 0;
			Top = 0;
			Right = 0;
			Bottom = 0;
		}

		RendererRectangle(int left, int top, int right, int bottom)
		{
			this->Left = left;
			this->Top = top;
			this->Right = right;
			this->Bottom = bottom;
		}
	};
}
