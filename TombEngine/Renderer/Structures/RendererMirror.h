#pragma once

namespace TEN::Renderer::Structures
{
	// Mirror of the view, copied from the level mirror; the Reflect flags select which kinds of objects are reflected.
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
