#pragma once

namespace TEN::Renderer::Structures
{
	struct RendererMirror
	{
		int	   RoomNumber		= 0;
		Plane  Plane			= TEN::Math::Library::Plane();
		Matrix ReflectionMatrix = Matrix::Identity;

		bool ReflectPlayer	  = false;
		bool ReflectMoveables = false;
		bool ReflectStatics	  = false;
		bool ReflectSprites	  = false;
		bool ReflectLights	  = false;
	};
}
