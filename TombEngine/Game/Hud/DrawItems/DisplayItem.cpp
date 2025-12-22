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
}
