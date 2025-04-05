#include "framework.h"
#include "Scripting/Internal/TEN/View/DisplaySprite/ScriptDisplaySprite.h"

#include "Game/effects/DisplaySprite.h"
#include "Game/Setup.h"
#include "Objects/game_object_ids.h"
#include "Renderer/Renderer.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"

using namespace TEN::Scripting::Types;
using TEN::Renderer::g_Renderer;

/// Represents a display sprite.
//
// @tenclass View.DisplaySprite
// @pragma nostrip

namespace TEN::Scripting::DisplaySprite
{
	void ScriptDisplaySprite::Register(sol::state& state, sol::table& parent)
	{
		// NOTE: Single constructor with a sol::optional argument for the color doesn't work, hence the two constructors. -- Sezz 2023.10.19
		using ctors = sol::constructors<
			ScriptDisplaySprite(GAME_OBJECT_ID, int, const Vec2&, float, const Vec2&, const ScriptColor&),
			ScriptDisplaySprite(GAME_OBJECT_ID, int, const Vec2&, float, const Vec2&),
			ScriptDisplaySprite(const Vec2&, float, const Vec2&, const ScriptColor&),
			ScriptDisplaySprite(const Vec2&, float, const Vec2&)>;

		// Register type.
		parent.new_usertype<ScriptDisplaySprite>(
			ScriptReserved_DisplaySprite,
			ctors(),
			sol::call_constructor, ctors(),

		ScriptReserved_DisplayStringGetObjectID, &ScriptDisplaySprite::GetObjectID,
		ScriptReserved_DisplayStringGetSpriteID, &ScriptDisplaySprite::GetSpriteID,
		ScriptReserved_DisplayStringGetPosition, &ScriptDisplaySprite::GetPosition,
		ScriptReserved_DisplayStringGetRotation, &ScriptDisplaySprite::GetRotation,
		ScriptReserved_DisplayStringGetScale, &ScriptDisplaySprite::GetScale,
		ScriptReserved_DisplayStringGetColor, &ScriptDisplaySprite::GetColor,
		ScriptReserved_DisplayStringSetObjectID, &ScriptDisplaySprite::SetObjectID,
		ScriptReserved_DisplayStringSetSpriteID, &ScriptDisplaySprite::SetSpriteID,
		ScriptReserved_DisplayStringSetPosition, &ScriptDisplaySprite::SetPosition,
		ScriptReserved_DisplayStringSetRotation, &ScriptDisplaySprite::SetRotation,
		ScriptReserved_DisplayStringSetScale, &ScriptDisplaySprite::SetScale,
		ScriptReserved_DisplayStringSetColor, &ScriptDisplaySprite::SetColor,
		ScriptReserved_DisplaySpriteDraw, &ScriptDisplaySprite::Draw);
	}

	/// Create a DisplaySprite object.
	// @function DisplaySprite
	// @tparam Objects.ObjID.SpriteConstants ID of the sprite sequence object.
	// @tparam int int spriteID ID of the sprite in the sequence.
	// @tparam Vec2 pos Display position in percent.
	// @tparam float rot Rotation in degrees.
	// @tparam Vec2 scale Horizontal and vertical scale in percent. Scaling is interpreted by the DisplaySpriteEnum.ScaleMode passed to the Draw() function call.
	// @tparam[opt] Color color Color. __Default: Color(255, 255, 255, 255)__
	// @treturn DisplaySprite A new DisplaySprite object.
	ScriptDisplaySprite::ScriptDisplaySprite(GAME_OBJECT_ID objectID, int spriteID, const Vec2& pos, float rot, const Vec2& scale, const ScriptColor& color)
	{
		_objectID = objectID;
		_spriteID = std::clamp(spriteID, 0, INT_MAX);
		_position = pos;
		_rotation = rot;
		_scale = scale;
		_color = color;
	}

	ScriptDisplaySprite::ScriptDisplaySprite(GAME_OBJECT_ID objectID, int spriteID, const Vec2& pos, float rot, const Vec2& scale)
	{
		*this = ScriptDisplaySprite(objectID, spriteID, pos, rot, scale, ScriptColor(255, 255, 255, 255));
	}

	/// Create a DisplaySprite object with a video image.
	// Video should be played using @{View.PlayVideo} function in a background mode. If no video is played, sprite will not show.
	// @function DisplaySprite
	// @tparam Vec2 pos Display position in percent.
	// @tparam float rot Rotation in degrees.
	// @tparam Vec2 scale Horizontal and vertical scale in percent. Scaling is interpreted by the DisplaySpriteEnum.ScaleMode passed to the Draw() function call.
	// @tparam[opt] Color color Color. __Default: Color(255, 255, 255, 255)__
	// @treturn DisplaySprite A new DisplaySprite object with attached video image.
	ScriptDisplaySprite::ScriptDisplaySprite(const Vec2& pos, float rot, const Vec2& scale, const ScriptColor& color)
	{
		_objectID = GAME_OBJECT_ID::ID_DEFAULT_SPRITES;
		_spriteID = NO_VALUE;
		_position = pos;
		_rotation = rot;
		_scale = scale;
		_color = color;
	}

	ScriptDisplaySprite::ScriptDisplaySprite(const Vec2& pos, float rot, const Vec2& scale)
	{
		*this = ScriptDisplaySprite(pos, rot, scale, ScriptColor(255, 255, 255, 255));
	}

	/// Get the object ID of the sprite sequence object used by the display sprite.
	// @function DisplaySprite:GetObjectID()
	// @treturn Objects.ObjID.SpriteConstants Sprite sequence object ID.
	GAME_OBJECT_ID ScriptDisplaySprite::GetObjectID() const
	{
		return _objectID;
	}

	/// Get the sprite ID in the sprite sequence object used by the display sprite.
	// @function DisplaySprite:GetSpriteID()
	// @treturn int Sprite ID in the sprite sequence object. Value __-1__ means that it is a background video, played using @{View.PlayVideo}.
	int ScriptDisplaySprite::GetSpriteID() const
	{
		return _spriteID;
	}

	/// Get the display position of the display sprite in percent.
	// @function DisplaySprite:GetPosition()
	// @treturn Vec2 Display position in percent.
	Vec2 ScriptDisplaySprite::GetPosition() const
	{
		return _position;
	}

	/// Get the rotation of the display sprite in degrees.
	// @function DisplaySprite:GetRotation()
	// @treturn float Rotation in degrees.
	float ScriptDisplaySprite::GetRotation() const
	{
		return _rotation;
	}

	/// Get the horizontal and vertical scale of the display sprite in percent.
	// @function DisplaySprite:GetScale()
	// @treturn Vec2 Horizontal and vertical scale in percent.
	Vec2 ScriptDisplaySprite::GetScale() const
	{
		return _scale;
	}

	/// Get the color of the display sprite.
	// @function DisplaySprite:GetColor()
	// @treturn Color Color.
	ScriptColor ScriptDisplaySprite::GetColor() const
	{
		return _color;
	}

	/// Set the sprite sequence object ID used by the display sprite.
	// @function DisplaySprite:SetObjectID(Objects.ObjID.SpriteConstants)
	// @tparam Objects.ObjID.SpriteConstants New sprite sequence object ID.
	void ScriptDisplaySprite::SetObjectID(GAME_OBJECT_ID objectID)
	{
		_objectID = objectID;
	}

	/// Set the sprite ID in the sprite sequence object used by the display sprite.
	// @function DisplaySprite:SetSpriteID(int)
	// @tparam int New sprite ID in the sprite sequence object.
	void ScriptDisplaySprite::SetSpriteID(int spriteID)
	{
		_spriteID = spriteID;
	}

	/// Set the display position of the display sprite in percent.
	// @function DisplaySprite:SetPosition(Vec2)
	// @tparam Vec2 New display position in percent.
	void ScriptDisplaySprite::SetPosition(const Vec2& pos)
	{
		_position = pos;
	}

	/// Set the rotation of the display sprite in degrees.
	// @function DisplaySprite:SetRotation(float)
	// @tparam float New rotation in degrees.
	void ScriptDisplaySprite::SetRotation(float rot)
	{
		_rotation = rot;
	}

	/// Set the horizontal and vertical scale of the display sprite in percent.
	// @function DisplaySprite:SetScale(Vec2)
	// @tparam float New horizontal and vertical scale in percent.
	void ScriptDisplaySprite::SetScale(const Vec2& scale)
	{
		_scale = scale;
	}

	/// Set the color of the display sprite.
	// @function DisplaySprite:SetColor(Color)
	// @tparam float New color.
	void ScriptDisplaySprite::SetColor(const ScriptColor& color)
	{
		_color = color;
	}

	/// Draw the display sprite in display space for the current frame.
	// @function DisplaySprite:Draw
	// @tparam[opt] int priority Draw priority. Can be thought of as a layer, with higher values having precedence. __Default: 0__
	// @tparam[opt] View.AlignMode alignMode Align mode interpreting an offset from the sprite's position. __Default: View.AlignMode.CENTER__
	// @tparam[opt] View.ScaleMode scaleMode Scale mode interpreting the display sprite's horizontal and vertical scale. __Default: View.ScaleMode.FIT__
	// @tparam[opt] Effects.BlendID blendMode Blend mode. __Default: Effects.BlendID.ALPHABLEND__
	void ScriptDisplaySprite::Draw(sol::optional<int> priority, sol::optional<DisplaySpriteAlignMode> alignMode,
								   sol::optional<DisplaySpriteScaleMode> scaleMode, sol::optional<BlendMode> blendMode)
	{
		// NOTE: Conversion from more intuitive 100x100 screen space resolution to internal 800x600 is required.
		// In a future refactor, everything will use 100x100 natively. -- Sezz 2023.08.31
		constexpr auto POS_CONVERSION_COEFF	  = Vector2(DISPLAY_SPACE_RES.x / 100, DISPLAY_SPACE_RES.y / 100);
		constexpr auto SCALE_CONVERSION_COEFF = 0.01f;

		constexpr auto DEFAULT_PRIORITY	  = 0;
		constexpr auto DEFAULT_ALIGN_MODE = DisplaySpriteAlignMode::Center;
		constexpr auto DEFAULT_SCALE_MODE = DisplaySpriteScaleMode::Fit;
		constexpr auto DEFAULT_BLEND_MODE = BlendMode::AlphaBlend;

		// Object is not a sprite sequence; return early.
		if (_spriteID != NO_VALUE && (_objectID < GAME_OBJECT_ID::ID_HORIZON || _objectID >= GAME_OBJECT_ID::ID_NUMBER_OBJECTS))
		{
			TENLog("Attempted to draw display sprite from non-sprite sequence object " + std::to_string(_objectID), LogLevel::Warning);
			return;
		}

		// Sprite missing or sequence not found; return early.
		const auto& object = Objects[_objectID];
		if (!object.loaded || _spriteID >= abs(object.nmeshes))
		{
			TENLog(
				"Attempted to draw missing sprite " + std::to_string(_spriteID) +
				" from sprite sequence object " + std::to_string(_objectID) +
				" as display sprite.",
				LogLevel::Warning);
			return;
		}

		auto convertedPos = Vector2(_position.x, _position.y) * POS_CONVERSION_COEFF;
		short convertedRot = ANGLE(_rotation);
		auto convertedScale = Vector2(_scale.x, _scale.y) * SCALE_CONVERSION_COEFF;
		auto convertedColor = Vector4(_color.GetR(), _color.GetG(), _color.GetB(), _color.GetA()) / UCHAR_MAX;

		AddDisplaySprite(
			_objectID, _spriteID,
			convertedPos, convertedRot, convertedScale, convertedColor,
			priority.value_or(DEFAULT_PRIORITY),
			alignMode.value_or(DEFAULT_ALIGN_MODE),
			scaleMode.value_or(DEFAULT_SCALE_MODE),
			blendMode.value_or(DEFAULT_BLEND_MODE), 
			DisplaySpritePhase::Control);
	}
}
