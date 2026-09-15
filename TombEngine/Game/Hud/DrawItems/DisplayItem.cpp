#include "framework.h"
#include "Game/Hud/DrawItems/DisplayItem.h"

#include "Game/Animation/Animation.h"
#include "Math/Math.h"
#include "Objects/game_object_ids.h"
#include "Renderer/Renderer.h"
#include "Specific/clock.h"
#include "Specific/Structures/BitField.h"

using namespace TEN::Math;

namespace TEN::Hud
{
	DisplayItem::DisplayItem(unsigned int id, GAME_OBJECT_ID objectID, const Vector3& pos, const EulerAngles& orient, const Vector3& scale)
	{
		_id = id;
		_objectID = objectID;
		_position = pos;
		_orientation = orient;
		_scale = scale;
		_prevPosition = pos;
		_prevOrientation = orient;
		_prevScale = scale;

		// Initialize animation state.
		RecomputeFrameData();
		_prevFrameData = _frameData;
	}

	unsigned int DisplayItem::GetID() const
	{
		return _id;
	}

	GAME_OBJECT_ID DisplayItem::GetObjectID() const
	{
		return _objectID;
	}

	const Vector3& DisplayItem::GetPosition() const
	{
		return _position;
	}

	std::optional<std::pair<Vector2, Vector2>> DisplayItem::GetBounds() const
	{
		auto bounds = g_Renderer.GetDisplayItemBounds(*this);
		if (!bounds.has_value())
			return std::nullopt;

		return bounds;
	}

	const EulerAngles& DisplayItem::GetRotation() const
	{
		return _orientation;
	}

	const Vector3& DisplayItem::GetScale() const
	{
		return _scale;
	}

	const Color& DisplayItem::GetColor() const
	{
		return _color;
	}

	const EulerAngles& DisplayItem::GetMeshOrientation(int meshIndex) const
	{
		auto it = _meshOrientations.find(meshIndex);
		if (it != _meshOrientations.end())
		{
			return it->second;
		}
		else
		{
			return EulerAngles::Identity;
		}
	}

	int DisplayItem::GetMeshBits() const
	{
		return _meshBits.ToPackedBits();
	}

	int DisplayItem::GetAnimNumber() const
	{
		return _animNumber;
	}

	int DisplayItem::GetFrameNumber() const
	{
		return _frameNumber;
	}

	int DisplayItem::GetEndFrameNumber() const
	{
		const auto& anim = GetAnimData(_objectID, _animNumber);
		return (anim.EndFrameNumber);
	}

	int DisplayItem::GetPrevFrameNumber() const
	{
		return _prevFrameNumber;
	}

	Vector3 DisplayItem::GetInterpolatedPosition(float alpha) const
	{
		return Vector3::Lerp(_prevPosition, _position, alpha);
	}

	EulerAngles DisplayItem::GetInterpolatedOrientation(float alpha) const
	{
		return EulerAngles::Lerp(_prevOrientation, _orientation, alpha);
	}

	Vector3 DisplayItem::GetInterpolatedScale(float alpha) const
	{
		return Vector3::Lerp(_prevScale, _scale, alpha);
	}

	Color DisplayItem::GetInterpolatedColor(float alpha) const
	{
		return Color::Lerp(_prevColor, _color, alpha);
	}

	EulerAngles DisplayItem::GetInterpolatedMeshRotation(int meshIndex, float alpha) const
	{
		auto it = _meshOrientations.find(meshIndex);
		auto prevIt = _prevMeshOrientations.find(meshIndex);

		// If only current orientation exists or no interpolation available, return it.
		if (it == _meshOrientations.end())
			return EulerAngles::Identity;
		if (prevIt == _prevMeshOrientations.end())
			return it->second;

		return EulerAngles::Lerp(prevIt->second, it->second, alpha);
	}

	FrameData DisplayItem::GetInterpolatedFrame(float alpha) const
	{
		return LerpFrameData(_prevFrameData, _frameData, alpha);
	}

	void DisplayItem::SetObjectID(GAME_OBJECT_ID objectID)
	{
		_objectID = objectID;

		// Reset animation state for the new object.
		_animNumber = 0;
		_frameNumber = 0;
		RecomputeFrameData();
		_prevFrameData = _frameData;
	}

	void DisplayItem::SetPosition(const Vector3& pos, bool disableInterpolation)
	{
		constexpr auto DELTA_TOLERANCE = 1000.0f;

		if (!_wasInterpolated || disableInterpolation || Vector3::Distance(pos, _position) > DELTA_TOLERANCE)
			_prevPosition = pos;

		_position = pos;
	}

	void DisplayItem::SetOrientation(const EulerAngles& orient, bool disableInterpolation)
	{
		constexpr auto DELTA_TOLERANCE = ANGLE(90);

		if (!_wasInterpolated || disableInterpolation || !EulerAngles::Compare(orient, _orientation, DELTA_TOLERANCE))
			_prevOrientation = orient;

		_orientation = orient;
	}

	void DisplayItem::SetScale(const Vector3& scale, bool disableInterpolation)
	{
		if (!_wasInterpolated || disableInterpolation)
			_prevScale = scale;

		_scale = scale;
	}

	void DisplayItem::SetColor(Color& color, bool disableInterpolation)
	{
		if (!_wasInterpolated || disableInterpolation)
			_prevColor = color;

		_color = color;
	}

	void DisplayItem::SetVisible(bool visible)
	{
		_visible = visible;
	}

	void DisplayItem::SetDisposing(bool disposing)
	{
		_disposing = disposing;
	}

	void DisplayItem::SetMeshBits(int meshBits)
	{
		_meshBits = meshBits;
	}

	void DisplayItem::SetMeshVisible(int meshIndex, bool isVisible)
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

	void DisplayItem::SetMeshOrientation(int meshIndex, const EulerAngles& orient, bool disableInterpolation)
	{
		if (disableInterpolation)
			_prevMeshOrientations[meshIndex] = orient;

		_meshOrientations[meshIndex] = orient;
	}

	void DisplayItem::SetAnimation(int animNumber)
	{
		const auto& object = Objects[_objectID];
		if (animNumber >= 0 && animNumber < object.Animations.size())
			_animNumber = animNumber;
		else
			_animNumber = 0;

		// Start playback from the beginning and snap interpolation.
		_frameNumber = 0;
		RecomputeFrameData();
		_prevFrameData = _frameData;
	}

	void DisplayItem::SetFrame(int frameNumber)
	{
		const auto& anim = GetAnimData(_objectID, _animNumber);
		_frameNumber = std::clamp(frameNumber, 0, anim.EndFrameNumber);

		// Snap interpolation so the frame takes effect immediately.
		RecomputeFrameData();
		_prevFrameData = _frameData;
	}

	bool DisplayItem::GetVisible() const
	{
		return _visible;
	}

	bool DisplayItem::GetDisposing() const
	{
		return _disposing;
	}

	bool DisplayItem::GetMeshVisible(int meshIndex) const
	{
		return _meshBits.Test(meshIndex);
	}

	bool DisplayItem::MeshExists(int meshIndex) const
	{
		if (meshIndex < 0 || meshIndex >= Objects[_objectID].nmeshes)
		{
			return false;
		}

		return true;
	}

	void DisplayItem::StoreInterpolationData()
	{
		_prevPosition = _position;
		_prevOrientation = _orientation;
		_prevScale = _scale;
		_prevColor = _color;
		_prevMeshOrientations = _meshOrientations;
		_prevFrameNumber = _frameNumber;
		_prevFrameData = _frameData;
		_wasInterpolated = true;
	}

	void DisplayItem::Animate()
	{
		// Objects without animations have nothing to advance.
		if (Objects[_objectID].Animations.empty())
			return;

		int prevAnimNumber = _animNumber;

		// Advance frame number.
		_frameNumber++;

		// Handle end frame link transition.
		const auto& anim = GetAnimData(_objectID, _animNumber);
		if (_frameNumber > anim.EndFrameNumber)
		{
			_animNumber = anim.NextAnimNumber;
			_frameNumber = anim.NextFrameNumber;
		}

		// Recompute effective frame data.
		RecomputeFrameData();

		// Snap interpolation across animation transitions to avoid cross-animation artifacts.
		if (_animNumber != prevAnimNumber)
			_prevFrameData = _frameData;
	}

	void DisplayItem::RecomputeFrameData()
	{
		const auto& anim = GetAnimData(_objectID, _animNumber);
		int frameNumber = std::clamp(_frameNumber, 0, (int)std::max((int)anim.Frames.size() - 1, 0));
		_frameData = anim.Frames[frameNumber];
	}

	FrameData DisplayItem::LerpFrameData(const FrameData& from, const FrameData& to, float alpha)
	{
		auto result = FrameData{};
		result.RootPosition = Vector3::Lerp(from.RootPosition, to.RootPosition, alpha);

		int count = (int)std::min(from.BoneOrientations.size(), to.BoneOrientations.size());
		result.BoneOrientations.resize(count);
		for (int i = 0; i < count; i++)
			result.BoneOrientations[i] = Quaternion::Slerp(from.BoneOrientations[i], to.BoneOrientations[i], alpha);

		return result;
	}
}
