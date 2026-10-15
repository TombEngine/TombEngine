#pragma once

#include "Game/effects/DisplaySprite.h"
#include "Objects/game_object_ids.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/View/DisplayAnchors/ScriptDisplayAnchors.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"

using namespace TEN::Scripting::Types;

enum BLEND_MODE;

namespace TEN::Scripting::DisplaySprite
{
	class ScriptDisplaySprite
	{
	public:
		static void Register(sol::state& state, sol::table& parent);

	private:
		// Members
		GAME_OBJECT_ID _objectID = GAME_OBJECT_ID::ID_DEFAULT_SPRITES;
		int			   _spriteID = 0;

		Vec2		_position = Vec2(0.0f, 0.0f);
		float		_rotation = 0.0f;
		Vec2		_scale	  = Vec2(0.0f, 0.0f);
		ScriptColor _color	  = ScriptColor(255, 255, 255, 255);

		bool _hasScissor       = false;
		Vec2 _scissorPos       = Vec2(0.0f, 0.0f);
		Vec2 _scissorSize      = Vec2(100.0f, 100.0f);
		TEN::Effects::DisplaySprite::DisplaySpriteAlignMode _scissorAlignMode = TEN::Effects::DisplaySprite::DisplaySpriteAlignMode::TopLeft;

	public:
		// Constructors
		ScriptDisplaySprite(GAME_OBJECT_ID objectID, int spriteID, const Vec2& pos, float rot, const Vec2& scale, const ScriptColor& color);
		ScriptDisplaySprite(GAME_OBJECT_ID objectID, int spriteID, const Vec2& pos, float rot, const Vec2& scale);
		ScriptDisplaySprite(const Vec2& pos, float rot, const Vec2& scale, const ScriptColor& color);
		ScriptDisplaySprite(const Vec2& pos, float rot, const Vec2& scale);

		// Getters
		GAME_OBJECT_ID		 GetObjectID() const;
		int					 GetSpriteID() const;
		Vec2				 GetPosition() const;
		float				 GetRotation() const;
		Vec2				 GetScale() const;
		ScriptColor			 GetColor() const;
		ScriptDisplayAnchors GetAnchors(sol::optional<TEN::Effects::DisplaySprite::DisplaySpriteAlignMode> alignModeOpt,
			sol::optional<TEN::Effects::DisplaySprite::DisplaySpriteScaleMode> scaleModeOpt) const;

		// Setters
		void SetObjectID(GAME_OBJECT_ID objectID);
		void SetSpriteID(int spriteID);
		void SetPosition(const Vec2& pos);
		void SetRotation(float rot);
		void SetScale(const Vec2& scale);
		void SetColor(const ScriptColor& color);
		void SetScissor(const Vec2& pos, const Vec2& size, sol::optional<TEN::Effects::DisplaySprite::DisplaySpriteAlignMode> alignMode);
		void ClearScissor();

		// Utilities
		void Draw(sol::optional<int> priority, sol::optional<TEN::Effects::DisplaySprite::DisplaySpriteAlignMode> alignMode,
				  sol::optional<TEN::Effects::DisplaySprite::DisplaySpriteScaleMode> scaleMode, sol::optional<BlendMode> blendMode);
	};
}
