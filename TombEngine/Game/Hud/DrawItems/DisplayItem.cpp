#include "framework.h"
#include "Game/Hud/DrawItems/DisplayItem.h"

#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Specific/clock.h"
#include "Game/effects/DisplaySprite.h"
using namespace TEN::Effects::DisplaySprite;

using namespace TEN::Math;

namespace TEN::Hud
{
	void DisplayItem::SetName(std::string itemName)
	{
		_itemName = itemName;
	}

	void DisplayItem::SetObjectID(GAME_OBJECT_ID objectID)
	{
		_objectID = objectID;
	}

	void DisplayItem::SetPosition(const Vector3& newPos, bool disableInterpolation)
	{
		// Disable 2D mode when setting 3D position explicitly
		if (_use2DMode)
		{
			TENLog("DisplayItem '" + _itemName + "' switched from 2D to 3D mode via SetPosition().",
				LogLevel::Info);
			_use2DMode = false;
		}

		if (disableInterpolation)
			_prevPose.Position = newPos;

		_pose.Position = newPos;
	}

	void DisplayItem::SetRotation(const EulerAngles& newRot, bool disableInterpolation)
	{
		if (disableInterpolation)
			_prevPose.Orientation = newRot;

		_pose.Orientation = newRot;
	}

	void DisplayItem::SetScale(const Vector3& newScale, bool disableInterpolation)
	{
		if (disableInterpolation)
			_prevPose.Scale = newScale;

		_pose.Scale = newScale;
	}

	void DisplayItem::SetColor(Color& newColor, bool disableInterpolation)
	{
		if (disableInterpolation)
			_prevColor = newColor;

		_color = newColor;
	}

	void DisplayItem::SetVisibility(bool visible)
	{
		_visible = visible;
	}

	void DisplayItem::SetMeshBits(int meshbits)
	{
		_meshBits = meshbits;
	}

	void DisplayItem::SetMeshVisibility(int meshIndex, bool isVisible)
	{
		if (!MeshExists(meshIndex))
			return;

		if (isVisible)
		{
			_meshBits.Set(meshIndex);
		}
		else
		{
			_meshBits.Clear(meshIndex);
		}
	}

	void DisplayItem::SetMeshRotation(int meshIndex, const EulerAngles& newRot, bool disableInterpolation)
	{
		if (disableInterpolation)
			_prevMeshRotations[meshIndex] = newRot;

		_meshRotations[meshIndex] = newRot;
	}

	void DisplayItem::SetAnimation(int animation)
	{
		//add checks for bounds of animation and frame
		_animNumber = animation;
	}

	void DisplayItem::SetFrame(int frame)
	{
		//add checks for bounds of animation and frame
		_frameNumber = frame;
	}

	std::string DisplayItem::GetName() const
	{
		return _itemName;
	}

	GAME_OBJECT_ID DisplayItem::GetObjectID() const
	{
		return _objectID;
	}

	Vector3 DisplayItem::GetPosition() const
	{
		return _pose.Position.ToVector3();
	}

	std::optional<std::pair<Vector2, Vector2>> DisplayItem::GetBounds() const
	{
		auto bounds = g_Renderer.GetDisplayItemBounds(*this);
		if (!bounds.has_value())
			return std::nullopt;

		// bounds->first  = center  (Vector2)
		// bounds->second = size    (Vector2)
		return bounds;
	}

	EulerAngles DisplayItem::GetRotation() const
	{
		return _pose.Orientation;
	}

	Vector3 DisplayItem::GetScale() const
	{
		return _pose.Scale;
	}

	Color DisplayItem::GetColor() const
	{
		return _color;
	}

	bool DisplayItem::GetVisibility() const
	{
		return _visible;
	}

	int DisplayItem::GetMeshBits() const
	{
		return _meshBits.ToPackedBits();
	}

	bool DisplayItem::GetMeshVisibility(int meshIndex) const
	{
		return _meshBits.Test(meshIndex);
	}

	EulerAngles DisplayItem::GetMeshRotation(int meshIndex) const
	{
		auto it = _meshRotations.find(meshIndex);
		if (it != _meshRotations.end())
			return it->second;
		else
			return EulerAngles::Identity;

	}

	int DisplayItem::GetAnimation() const
	{
		return _animNumber;
	}

	int DisplayItem::GetFrame() const
	{
		return _frameNumber;
	}

	int DisplayItem::GetPreviousFrame() const
	{
		return _prevFrameNumber;
	}

	// Interpolation Helpers
	void DisplayItem::StoreInterpolationData()
	{
		_prevPose = _pose;
		_prevColor = _color;
		_prevMeshRotations = _meshRotations;
		_prevFrameNumber = _frameNumber;
	}

	Vector3 DisplayItem::GetInterpolatedPosition(float t) const
	{
		return Vector3::Lerp(_prevPose.Position.ToVector3(), _pose.Position.ToVector3(), t);
	}

	EulerAngles DisplayItem::GetInterpolatedOrientation(float t) const
	{
		return EulerAngles::Lerp(_prevPose.Orientation, _pose.Orientation, t);
	}

	Vector3 DisplayItem::GetInterpolatedScale(float t) const
	{
		return Vector3::Lerp(_prevPose.Scale, _pose.Scale, t);
	}

	Color DisplayItem::GetInterpolatedColor(float t) const
	{
		return Color::Lerp(_prevColor, _color, t);
	}

	EulerAngles DisplayItem::GetInterpolatedMeshRotation(int meshIndex, float t) const
	{
		auto itNow = _meshRotations.find(meshIndex);
		auto itPrev = _prevMeshRotations.find(meshIndex);

		// If only current rotation exists, or no interpolation available, return it
		if (itNow == _meshRotations.end())
			return EulerAngles::Identity;
		if (itPrev == _prevMeshRotations.end())
			return itNow->second;

		return EulerAngles::Lerp(itPrev->second, itNow->second, t);
	}

	bool DisplayItem::MeshExists(int index) const
	{
		if (index < 0 || index >= Objects[_objectID].nmeshes)
		{
			return false;
		}

		return true;
	}

	// 2D Mode Methods

	// Set the position of the display item in screen space.
	void DisplayItem::SetScreenPosition(const Vector2& screenPos, DisplaySpriteAlignMode align)
	{
	    _use2DMode = true;
	    _screenPosition = screenPos;
	    _alignMode = align;
	    
	    // Convert screen coordinates to 3D position
	    UpdatePositionFrom2D();
	}

	// Update the screen position of the display item in 2D mode.
	void DisplayItem::UpdatePositionFrom2D()
	{
	    if (!_use2DMode)
	        return;

	    float t = g_Renderer.GetInterpolationFactor();
	    
	    // Get camera info
	    Vector3 camPos = g_DrawItems.GetInterpolatedCameraPosition(t);
	    Vector3 camTarget = g_DrawItems.GetInterpolatedCameraTargetPosition(t);
	    
	    // Calculate camera direction
	    Vector3 camForward = (camTarget - camPos);
	    camForward.Normalize();
	    
	    Vector3 worldUp = Vector3::Up;
	    Vector3 camRight = camForward.Cross(worldUp);
	    camRight.Normalize();
	    Vector3 camUp = camRight.Cross(camForward);
	    camUp.Normalize();
	    
	    // Convert screen percentage to normalized coordinates [-1, 1]
	    float ndcX = (_screenPosition.x / 50.0f) - 1.0f;
	    float ndcY = 1.0f - (_screenPosition.y / 50.0f);
	    
	    // Calculate aspect ratio
	    float aspectRatio = (float)g_Configuration.ScreenWidth / g_Configuration.ScreenHeight;
	    float fovTan = tan(CurrentFOV * 0.5f);
	    
	    // Calculate offset from screen center
	    float offsetX = ndcX * _depthDistance * fovTan * aspectRatio;
	    float offsetY = ndcY * _depthDistance * fovTan;
	    
	    // Calculate base 3D position
	    Vector3 basePos = camPos + (camForward * _depthDistance);
	    basePos += camRight * offsetX;
	    basePos += camUp * offsetY;
	    
	    // Apply alignment offset
	    Vector3 alignOffset = CalculateAlignmentOffset(_alignMode);
	    basePos += camRight * alignOffset.x;
	    basePos += camUp * alignOffset.y;
	    
	    // Update position
		_pose.Position = basePos;
	}

	// Calculate the alignment offset in world space based on the current AlignMode.
	Vector3 DisplayItem::CalculateAlignmentOffset(DisplaySpriteAlignMode alignMode) const
	{
		if (!_use2DMode)
			return Vector3::Zero;

		// Temporarily set AlignMode to Center to avoid recursion
		DisplaySpriteAlignMode savedAlign = _alignMode;
		const_cast<DisplayItem*>(this)->_alignMode = DisplaySpriteAlignMode::Center;

		// Get projected object dimensions
		auto bounds = GetBounds();

		// Restore AlignMode
		const_cast<DisplayItem*>(this)->_alignMode = savedAlign;

		if (!bounds.has_value())
			return Vector3::Zero;

		// bounds->second is in PIXEL (width, height)
		const Vector2& sizePixels = bounds->second;

		// Convert from pixels to screen percentage
		float screenWidth = (float)g_Configuration.ScreenWidth;
		float screenHeight = (float)g_Configuration.ScreenHeight;

		float sizePercentX = (sizePixels.x / screenWidth) * 100.0f;
		float sizePercentY = (sizePixels.y / screenHeight) * 100.0f;

		// Calculate offset in screen percentage (half size)
		Vector2 offsetPercent = Vector2::Zero;

		switch (alignMode)
		{
		case DisplaySpriteAlignMode::Center:
			break;

		case DisplaySpriteAlignMode::TopLeft:
			offsetPercent.x = sizePercentX * 0.5f;
			offsetPercent.y = -sizePercentY * 0.5f;
			break;

		case DisplaySpriteAlignMode::CenterTop:
			offsetPercent.y = -sizePercentY * 0.5f;
			break;

		case DisplaySpriteAlignMode::TopRight:
			offsetPercent.x = -sizePercentX * 0.5f;
			offsetPercent.y = -sizePercentY * 0.5f;
			break;

		case DisplaySpriteAlignMode::CenterLeft:
			offsetPercent.x = sizePercentX * 0.5f;
			break;

		case DisplaySpriteAlignMode::CenterRight:
			offsetPercent.x = -sizePercentX * 0.5f;
			break;

		case DisplaySpriteAlignMode::BottomLeft:
			offsetPercent.x = sizePercentX * 0.5f;
			offsetPercent.y = sizePercentY * 0.5f;
			break;

		case DisplaySpriteAlignMode::CenterBottom:
			offsetPercent.y = sizePercentY * 0.5f;
			break;

		case DisplaySpriteAlignMode::BottomRight:
			offsetPercent.x = -sizePercentX * 0.5f;
			offsetPercent.y = sizePercentY * 0.5f;
			break;
		}

		// Convert screen percentage offset to 3D world offset
		// Use the same formula as UpdatePositionFrom2D
		float t = g_Renderer.GetInterpolationFactor();
		float fovTan = tan(CurrentFOV * 0.5f);
		float aspectRatio = screenWidth / screenHeight;

		// Convert screen percentage to NDC
		float ndcOffsetX = (offsetPercent.x / 50.0f);  // 50% = 1.0 NDC
		float ndcOffsetY = (offsetPercent.y / 50.0f);

		// Convert NDC to world units
		float worldOffsetX = ndcOffsetX * _depthDistance * fovTan * aspectRatio;
		float worldOffsetY = ndcOffsetY * _depthDistance * fovTan;

		return Vector3(worldOffsetX, worldOffsetY, 0.0f);
	}

	// Get the screen position of the display item in 2D mode.
	Vector2 DisplayItem::GetScreenPosition() const
	{
		if (!_use2DMode)
		{
			TENLog("GetScreenPosition() called on '" + _itemName + "' while not in 2D mode.",
				LogLevel::Warning);
			return Vector2::Zero;
		}
	    return _screenPosition;
	}

	// Set the alignment mode for the display item in 2D mode.
	void DisplayItem::SetAlignMode(DisplaySpriteAlignMode align)
	{
		if (!_use2DMode)
		{
			TENLog("SetAlignMode() called on '" + _itemName + "' while not in 2D mode. Ignored.",
				LogLevel::Warning);
			return;
		}

		if (align == _alignMode)
			return;  // Nothing to change

		_alignMode = align;

		if (_use2DMode)
	        UpdatePositionFrom2D();
	}

	// Get the alignment mode for the display item in 2D mode.
	DisplaySpriteAlignMode DisplayItem::GetAlignMode() const
	{
		if (!_use2DMode)
		{
			TENLog("GetAlignMode() called on '" + _itemName + "' while not in 2D mode.",
				LogLevel::Warning);
			return DisplaySpriteAlignMode::Center;
		}
	    return _alignMode;
	}
}
