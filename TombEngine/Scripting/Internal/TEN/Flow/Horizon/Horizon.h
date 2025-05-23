#pragma once

#include "Objects/game_object_ids.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"

namespace TEN::Scripting
{
	class Horizon
	{
		public:
			static void Register(sol::table& parent);

		private:
			// Fields

			bool			_enabled	  = false;
			GAME_OBJECT_ID	_objectID	  = GAME_OBJECT_ID::ID_HORIZON;
			Vec3			_position	  = {};
			Rotation		_rotation	  = {};
			float			_scale		  = 1.0f;
			float			_transparency = 1.0f;

			Vec3	 _prevPosition = {};
			Rotation _prevRotation = {};
			float	 _prevScale	   = 1.0f;

		public:
			// Constructors

			Horizon() = default;
			Horizon(bool enabled);
			Horizon(GAME_OBJECT_ID objectID);

			// Getters

			bool			GetEnabled() const;
			GAME_OBJECT_ID	GetObjectID() const;
			const Vec3		GetPosition() const;
			const Rotation	GetRotation() const;
			const float 	GetTransparency() const;

			const Vec3		GetPrevPosition() const;
			const Rotation	GetPrevRotation() const;

			// Setters
			
			void SetEnabled(bool value);
			void SetObjectID(GAME_OBJECT_ID objectID);
			void SetPosition(const Vec3& pos, TypeOrNil<bool> disableInterpolation);
			void SetRotation(const Rotation& orient, TypeOrNil<bool> disableInterpolation);
			void SetTransparency(float value);
	};
}