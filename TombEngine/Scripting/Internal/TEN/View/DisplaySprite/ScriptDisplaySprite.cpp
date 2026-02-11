#include "framework.h"
#include "Scripting/Internal/TEN/View/DisplaySprite/ScriptDisplaySprite.h"

#include "Game/effects/DisplaySprite.h"
#include "Game/Setup.h"
#include "Objects/game_object_ids.h"
#include "Renderer/Renderer.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/View/DisplayAnchors/ScriptDisplayAnchors.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"
#include "Specific/level.h"

using namespace TEN::Scripting::Types;
using TEN::Renderer::g_Renderer;

/// Represents a display sprite.
//
// @tenclass View.DisplaySprite
// @pragma nostrip

namespace TEN::Scripting::DisplaySprite
{
	// NOTE: Conversion from more intuitive 100x100 screen space resolution to internal 800x600 is required.
	// In a future refactor, everything will use 100x100 natively. -- Sezz 2023.08.31
	constexpr auto POS_CONVERSION_COEFF = Vector2(DISPLAY_SPACE_RES.x / 100, DISPLAY_SPACE_RES.y / 100);
	constexpr auto SCALE_CONVERSION_COEFF = 0.01f;
	constexpr auto DEFAULT_PRIORITY = 0;
	constexpr auto DEFAULT_ALIGN_MODE = DisplaySpriteAlignMode::Center;
	constexpr auto DEFAULT_SCALE_MODE = DisplaySpriteScaleMode::Fit;
	constexpr auto DEFAULT_BLEND_MODE = BlendMode::AlphaBlend;
	constexpr auto DISPLAY_ASPECT = DISPLAY_SPACE_RES.x / DISPLAY_SPACE_RES.y;

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
		ScriptReserved_DisplayStringGetAnchors, &ScriptDisplaySprite::GetAnchors,
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
	// @tparam Objects.ObjID objectID ID of the sprite sequence object.
	// @tparam int index Index of the sprite in the sequence.
	// @tparam Vec2 pos Display position in percent.
	// @tparam float rot Rotation in degrees.
	// @tparam Vec2 scale Horizontal and vertical scale in percent. Scaling is interpreted by the DisplaySpriteEnum.ScaleMode passed to the Draw() function call.
	// @tparam[opt=Color(255&#44; 255&#44; 255)] Color color Color.
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
		_spriteID = VIDEO_SPRITE_ID;
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
	// @function DisplaySprite:GetObjectID
	// @treturn Objects.ObjID Sprite sequence object ID.
	GAME_OBJECT_ID ScriptDisplaySprite::GetObjectID() const
	{
		return _objectID;
	}

	/// Get the sprite ID in the sprite sequence object used by the display sprite.
	// @function DisplaySprite:GetSpriteID
	// @treturn int Sprite ID in the sprite sequence object. Value __-1__ means that it is a background video, played using @{View.PlayVideo}.
	int ScriptDisplaySprite::GetSpriteID() const
	{
		return _spriteID;
	}

	/// Get the display position of the display sprite in percent.
	// @function DisplaySprite:GetPosition
	// @treturn Vec2 Display position in percent.
	Vec2 ScriptDisplaySprite::GetPosition() const
	{
		return _position;
	}

	/// Get the rotation of the display sprite in degrees.
	// @function DisplaySprite:GetRotation
	// @treturn float Rotation in degrees.
	float ScriptDisplaySprite::GetRotation() const
	{
		return _rotation;
	}

	/// Get the horizontal and vertical scale of the display sprite in percent.
	// @function DisplaySprite:GetScale
	// @treturn Vec2 Horizontal and vertical scale in percent.
	Vec2 ScriptDisplaySprite::GetScale() const
	{
		return _scale;
	}

	/// Get the color of the display sprite.
	// @function DisplaySprite:GetColor
	// @treturn Color Color.
	ScriptColor ScriptDisplaySprite::GetColor() const
	{
		return _color;
	}

	/// Get the anchors of the display sprite.
	// Anchors are the vertices of the display sprite, which can be used to position other objects relative to it.
	// @function DisplaySprite:GetAnchors
	// @tparam[opt=DisplaySpriteAlignMode.Center] View.AlignMode alignMode Alignment mode.
	// @tparam[opt=DisplaySpriteScaleMode.Fit] View.ScaleMode scaleMode Scaling mode.
	// @treturn DisplayAnchors An object containing the anchor points of the display sprite.<br>
	// The object contains the following fields:<br>
	// - `TOP_LEFT`<br>
	// - `TOP_CENTER`<br>
	// - `TOP_RIGHT`<br>
	// - `CENTER_LEFT`<br>
	// - `CENTER`<br>
	// - `CENTER_RIGHT`<br>
	// - `BOTTOM_RIGHT`<br>
	// - `BOTTOM_CENTER`<br>
	// - `BOTTOM_LEFT`<br>
	// @usage
	// local objID = Objects.ObjID.ID_DEFAULT_SPRITES
	// local sprite = View.DisplaySprite(objID, 0, Vec2(50, 50), 0, Vec2(20, 20))
	// local anchors = sprite:GetAnchors()
	// print("Center anchor: " .. tostring(anchors.CENTER)) -- Output: Center anchor: Vec2(50.0, 50.0)
	ScriptDisplayAnchors ScriptDisplaySprite::GetAnchors(sol::optional<DisplaySpriteAlignMode> alignModeOpt, sol::optional<DisplaySpriteScaleMode> scaleModeOpt) const
	{
		ScriptDisplayAnchors anchors;

		// Object is not a sprite sequence; return default anchors.
		const auto& object = Objects[_objectID];
		if (object.nmeshes > 0)
		{
			TENLog("Attempted to retrieve anchors from non-sprite sequence object " + std::to_string(_objectID), LogLevel::Warning);
			return anchors;
		}

		// Sprite missing or sequence not found; return default anchors.
		if (!object.loaded || _spriteID >= abs(object.nmeshes))
		{
			TENLog("Attempted to retrieve the anchors of missing sprite " + std::to_string(_spriteID) + " from sprite sequence object " + std::to_string(_objectID) + " as display sprite.", LogLevel::Warning);
			return anchors;
		}

		// Get sprite data.
		int spriteIndex = object.meshIndex + _spriteID;
		if (spriteIndex < 0 || spriteIndex >= g_Level.Sprites.size())
		{
			TENLog("Invalid sprite index " + std::to_string(spriteIndex) + " for sprite sequence object " + std::to_string(_objectID), LogLevel::Warning);
			return anchors;
		}

		const auto& spriteData = g_Level.Sprites[spriteIndex];

		// Calculate sprite aspect ratio.
		float spriteWidth = abs(spriteData.x2 - spriteData.x1);
		float spriteHeight = abs(spriteData.y3 - spriteData.y1);
		float spriteAspect = (spriteHeight > 0.0f) ? (spriteWidth / spriteHeight) : 1.0f;

		// Screen data.
		auto screenRes = Vector2(g_Configuration.ScreenWidth, g_Configuration.ScreenHeight);
		float screenAspect = screenRes.x / screenRes.y;
		float aspectCorrectionBase = screenAspect / DISPLAY_ASPECT;

		// Convert scale, position and rotation.
		auto convertedScale = _scale * SCALE_CONVERSION_COEFF;
		auto convertedPos = _position * (DISPLAY_SPACE_RES / 100.0f);
		short convertedRot = ANGLE(_rotation);

		// Get modes.
		auto alignMode = alignModeOpt.value_or(DEFAULT_ALIGN_MODE);
		auto scaleMode = scaleModeOpt.value_or(DEFAULT_SCALE_MODE);

		// Calculate layout using shared helper function.
		auto layout = CalculateDisplaySpriteLayout(
			spriteAspect, convertedScale, convertedRot,
			alignMode, scaleMode, screenAspect, aspectCorrectionBase);

		// Calculate final position.
		auto position = convertedPos + layout.Offset;
		auto size = layout.HalfSize * 2.0f;

		// Build vertices centered around origin.
		std::array<Vector2, 4> vertices = {
			size / 2.0f,  // top-left
			Vector2(-size.x,  size.y) / 2.0f, // top-right
			-size / 2.0f, // bottom-right
			Vector2(size.x, -size.y) / 2.0f   // bottom-left
		};

		// Apply rotation + aspect correction + position offset.
		// NOTE: Must rotate 180 degrees to match renderer behavior.
		auto rotMatrix = Matrix::CreateRotationZ(TO_RAD(convertedRot + ANGLE(180.0f)));
		for (auto& vertex : vertices)
		{
			vertex = Vector2::Transform(vertex, rotMatrix);
			vertex *= layout.AspectCorrection;
			vertex += position;
		}

		// Scale to screen resolution.
		auto screenScale = screenRes / DISPLAY_SPACE_RES;
		for (auto& vertex : vertices)
		{
			vertex.x *= screenScale.x;
			vertex.y *= screenScale.y;
		}

		// Helper lambda for percent conversion with rounding.
		auto toPercent = [&screenRes](const Vector2& pos) -> Vec2
		{
			return Vec2(std::round((pos.x / screenRes.x) * 10000.0f) / 100.0f, std::round((pos.y / screenRes.y) * 10000.0f) / 100.0f);
		};

		// Calculate edge midpoints.
		auto center = (vertices[0] + vertices[2]) / 2.0f;
		auto centerTop = (vertices[0] + vertices[1]) / 2.0f;
		auto centerLeft = (vertices[0] + vertices[3]) / 2.0f;
		auto centerRight = (vertices[1] + vertices[2]) / 2.0f;
		auto centerBottom = (vertices[2] + vertices[3]) / 2.0f;

		// Populate anchors.
		anchors.TOP_LEFT = toPercent(vertices[0]);
		anchors.TOP_CENTER = toPercent(centerTop);
		anchors.TOP_RIGHT = toPercent(vertices[1]);
		anchors.CENTER_LEFT = toPercent(centerLeft);
		anchors.CENTER = toPercent(center);
		anchors.CENTER_RIGHT = toPercent(centerRight);
		anchors.BOTTOM_RIGHT = toPercent(vertices[2]);
		anchors.BOTTOM_CENTER = toPercent(centerBottom);
		anchors.BOTTOM_LEFT = toPercent(vertices[3]);

		return anchors;
	}

	/// Set the sprite sequence object ID used by the display sprite.
	// @function DisplaySprite:SetObjectID
	// @tparam Objects.ObjID objectID New sprite sequence object ID.
	void ScriptDisplaySprite::SetObjectID(GAME_OBJECT_ID objectID)
	{
		_objectID = objectID;
	}

	/// Set the sprite ID in the sprite sequence object used by the display sprite.
	// @function DisplaySprite:SetSpriteID
	// @tparam int spriteID New sprite ID in the sprite sequence object.
	void ScriptDisplaySprite::SetSpriteID(int spriteID)
	{
		_spriteID = spriteID;
	}

	/// Set the display position of the display sprite in percent.
	// @function DisplaySprite:SetPosition
	// @tparam Vec2 position New display position in percent.
	void ScriptDisplaySprite::SetPosition(const Vec2& pos)
	{
		_position = pos;
	}

	/// Set the rotation of the display sprite in degrees.
	// @function DisplaySprite:SetRotation
	// @tparam float rotation New rotation in degrees.
	void ScriptDisplaySprite::SetRotation(float rot)
	{
		_rotation = rot;
	}

	/// Set the horizontal and vertical scale of the display sprite in percent.
	// @function DisplaySprite:SetScale
	// @tparam Vec2 scale New horizontal and vertical scale in percent.
	void ScriptDisplaySprite::SetScale(const Vec2& scale)
	{
		_scale = scale;
	}

	/// Set the color of the display sprite.
	// @function DisplaySprite:SetColor
	// @tparam Color color New color.
	void ScriptDisplaySprite::SetColor(const ScriptColor& color)
	{
		_color = color;
	}

	/// Draw the display sprite in display space for the current frame.
	// @function DisplaySprite:Draw
	// @tparam[opt=0] int priority Draw priority. Can be thought of as a layer, with higher values having precedence.
	// Negative values will draw sprite above strings, while positive values will draw it under.
	// @tparam[opt=View.AlignMode.CENTER] View.AlignMode alignMode Align mode interpreting an offset from the sprite's position.
	// @tparam[opt=View.ScaleMode.FIT] View.ScaleMode scaleMode Scale mode interpreting the display sprite's horizontal and vertical scale.
	// @tparam[opt=Effects.BlendID.ALPHABLEND] Effects.BlendID blendMode Blend mode.
	void ScriptDisplaySprite::Draw(sol::optional<int> priority, sol::optional<DisplaySpriteAlignMode> alignMode,
								   sol::optional<DisplaySpriteScaleMode> scaleMode, sol::optional<BlendMode> blendMode)
	{
		// Object is not a sprite sequence; return early.
		const auto& object = Objects[_objectID];
		if (object.nmeshes > 0)
		{
			TENLog("Attempted to draw display sprite from non-sprite sequence object " + std::to_string(_objectID), LogLevel::Warning);
			return;
		}

		// Sprite missing or sequence not found; return early.
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
