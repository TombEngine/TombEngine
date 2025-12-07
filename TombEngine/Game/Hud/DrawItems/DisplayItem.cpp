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
		ItemName = itemName;
	}

	void DisplayItem::SetObjectID(GAME_OBJECT_ID objectID)
	{
		ObjectID = objectID;
	}

	void DisplayItem::SetPosition(const Vector3& newPos, bool disableInterpolation)
	{
		// Disable 2D mode when setting 3D position explicitly
		if (Use2DMode)
		{
			TENLog("DisplayItem '" + ItemName + "' switched from 2D to 3D mode via SetPosition().",
				LogLevel::Info);
			Use2DMode = false;
		}

		if (disableInterpolation)
			PrevPosition = newPos;

		Position = newPos;
	}

	void DisplayItem::SetRotation(const EulerAngles& newRot, bool disableInterpolation)
	{
		if (disableInterpolation)
			PrevOrientation = newRot;

		Orientation = newRot;
	}

	void DisplayItem::SetScale(float newScale, bool disableInterpolation)
	{
		if (disableInterpolation)
			PrevScale = newScale;

		Scale = newScale;
	}

	void DisplayItem::SetColor(Color& newColor, bool disableInterpolation)
	{
		if (disableInterpolation)
			PrevColor = newColor;

		ItemColor = newColor;
	}

	void DisplayItem::SetVisibility(bool visible)
	{
		Visible = visible;
	}

	void DisplayItem::SetMeshBits(int meshbits)
	{
		MeshBits = meshbits;
	}

	void DisplayItem::SetMeshVisibility(int meshIndex, bool isVisible)
	{
		if (!MeshExists(meshIndex))
			return;

		if (isVisible)
		{
			MeshBits.Set(meshIndex);
		}
		else
		{
			MeshBits.Clear(meshIndex);
		}
	}

	void DisplayItem::SetMeshRotation(int meshIndex, const EulerAngles& newRot, bool disableInterpolation)
	{
		if (disableInterpolation)
			PrevMeshRotations[meshIndex] = newRot;

		MeshRotations[meshIndex] = newRot;
	}

	void DisplayItem::SetAnimation(int animation)
	{
		//add checks for bounds of animation and frame
		AnimNumber = animation;
	}

	void DisplayItem::SetFrame(int frame)
	{
		//add checks for bounds of animation and frame
		FrameNumber = frame;
	}

	std::string DisplayItem::GetName() const
	{
		return ItemName;
	}

	GAME_OBJECT_ID DisplayItem::GetObjectID() const
	{
		return ObjectID;
	}

	Vector3 DisplayItem::GetPosition() const
	{
		return Position;
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
		return Orientation;
	}

	float DisplayItem::GetScale() const
	{
		return Scale;
	}

	Color DisplayItem::GetColor() const
	{
		return ItemColor;
	}

	bool DisplayItem::GetVisibility() const
	{
		return Visible;
	}

	int DisplayItem::GetMeshBits() const
	{
		return MeshBits.ToPackedBits();
	}

	bool DisplayItem::GetMeshVisibility(int meshIndex) const
	{
		return MeshBits.Test(meshIndex);
	}

	EulerAngles DisplayItem::GetMeshRotation(int meshIndex) const
	{
		auto it = MeshRotations.find(meshIndex);
		if (it != MeshRotations.end())
			return it->second;
		else
			return EulerAngles::Identity;

	}

	int DisplayItem::GetAnimation() const
	{
		return AnimNumber;
	}

	int DisplayItem::GetFrame() const
	{
		return FrameNumber;
	}

	int DisplayItem::GetPreviousFrame() const
	{
		return PrevFrameNumber;
	}

	// Interpolation Helpers
	void DisplayItem::StoreInterpolationData()
	{
		PrevPosition = Position;
		PrevOrientation = Orientation;
		PrevScale = Scale;
		PrevColor = ItemColor;
		PrevMeshRotations = MeshRotations;
		PrevFrameNumber = FrameNumber;
	}

	Vector3 DisplayItem::GetInterpolatedPosition(float t) const
	{
		return Vector3::Lerp(PrevPosition, Position, t);
	}

	EulerAngles DisplayItem::GetInterpolatedOrientation(float t) const
	{
		return EulerAngles::Lerp(PrevOrientation, Orientation, t);
	}

	float DisplayItem::GetInterpolatedScale(float t) const
	{
		return Lerp(PrevScale, Scale, t);
	}

	Color DisplayItem::GetInterpolatedColor(float t) const
	{
		return Color::Lerp(PrevColor, ItemColor, t);
	}

	EulerAngles DisplayItem::GetInterpolatedMeshRotation(int meshIndex, float t) const
	{
		auto itNow = MeshRotations.find(meshIndex);
		auto itPrev = PrevMeshRotations.find(meshIndex);

		// If only current rotation exists, or no interpolation available, return it
		if (itNow == MeshRotations.end())
			return EulerAngles::Identity;
		if (itPrev == PrevMeshRotations.end())
			return itNow->second;

		return EulerAngles::Lerp(itPrev->second, itNow->second, t);
	}

	bool DisplayItem::MeshExists(int index) const
	{
		if (index < 0 || index >= Objects[ObjectID].nmeshes)
		{
			return false;
		}

		return true;
	}

	// 2D Mode Methods

	// Set the position of the display item in screen space.
	void DisplayItem::SetScreenPosition(const Vector2& screenPos, DisplaySpriteAlignMode align)
	{
	    Use2DMode = true;
	    ScreenPosition = screenPos;
	    AlignMode = align;
	    
	    // Convert screen coordinates to 3D position
	    UpdatePositionFrom2D();
	}

	// Update the screen position of the display item in 2D mode.
	void DisplayItem::UpdatePositionFrom2D()
	{
	    if (!Use2DMode)
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
	    float ndcX = (ScreenPosition.x / 50.0f) - 1.0f;
	    float ndcY = 1.0f - (ScreenPosition.y / 50.0f);
	    
	    // Calculate aspect ratio
	    float aspectRatio = (float)g_Configuration.ScreenWidth / g_Configuration.ScreenHeight;
	    float fovTan = tan(CurrentFOV * 0.5f);
	    
	    // Calculate offset from screen center
	    float offsetX = ndcX * DepthDistance * fovTan * aspectRatio;
	    float offsetY = ndcY * DepthDistance * fovTan;
	    
	    // Calculate base 3D position
	    Vector3 basePos = camPos + (camForward * DepthDistance);
	    basePos += camRight * offsetX;
	    basePos += camUp * offsetY;
	    
	    // Apply alignment offset
	    Vector3 alignOffset = CalculateAlignmentOffset(AlignMode);
	    basePos += camRight * alignOffset.x;
	    basePos += camUp * alignOffset.y;
	    
	    // Update position
	    Position = basePos;
	}

	// Calculate the alignment offset in world space based on the current AlignMode.
	Vector3 DisplayItem::CalculateAlignmentOffset(DisplaySpriteAlignMode alignMode) const
	{
		if (!Use2DMode)
			return Vector3::Zero;

		// Temporarily set AlignMode to Center to avoid recursion
		DisplaySpriteAlignMode savedAlign = AlignMode;
		const_cast<DisplayItem*>(this)->AlignMode = DisplaySpriteAlignMode::Center;

		// Get projected object dimensions
		auto bounds = GetBounds();

		// Restore AlignMode
		const_cast<DisplayItem*>(this)->AlignMode = savedAlign;

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
		float worldOffsetX = ndcOffsetX * DepthDistance * fovTan * aspectRatio;
		float worldOffsetY = ndcOffsetY * DepthDistance * fovTan;

		return Vector3(worldOffsetX, worldOffsetY, 0.0f);
	}

	// Get the screen position of the display item in 2D mode.
	Vector2 DisplayItem::GetScreenPosition() const
	{
		if (!Use2DMode)
		{
			TENLog("GetScreenPosition() called on '" + ItemName + "' while not in 2D mode.",
				LogLevel::Warning);
			return Vector2::Zero;
		}
	    return ScreenPosition;
	}

	// Set the alignment mode for the display item in 2D mode.
	void DisplayItem::SetAlignMode(DisplaySpriteAlignMode align)
	{
		if (!Use2DMode)
		{
			TENLog("SetAlignMode() called on '" + ItemName + "' while not in 2D mode. Ignored.",
				LogLevel::Warning);
			return;
		}

		if (align == AlignMode)
			return;  // Nothing to change

		AlignMode = align;

		if (Use2DMode)
	        UpdatePositionFrom2D();
	}

	// Get the alignment mode for the display item in 2D mode.
	DisplaySpriteAlignMode DisplayItem::GetAlignMode() const
	{
		if (!Use2DMode)
		{
			TENLog("GetAlignMode() called on '" + ItemName + "' while not in 2D mode.",
				LogLevel::Warning);
			return DisplaySpriteAlignMode::Center;
		}
	    return AlignMode;
	}
}
