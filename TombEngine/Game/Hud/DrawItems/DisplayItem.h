#pragma once

#include "Math/Constants.h"
#include "Objects/game_object_ids.h"
#include "Specific/Structures/BitField.h"

using namespace TEN::Math;
using namespace TEN::Utils;

namespace TEN::Hud
{
	constexpr float DEPTH_DISTANCE_2D = 3800.0f; // Fixed distance from camera in 2D mode.

	struct DisplayItem
	{
	private:
		// Fields

		unsigned int _id = 0;
		GAME_OBJECT_ID _objectID = GAME_OBJECT_ID::ID_NO_OBJECT;

		bool _visible = false;
		bool _disposing = false;
		bool _wasInterpolated = false;

		Vector3                              _position         = Vector3::Zero;
		EulerAngles                          _orientation      = EulerAngles::Identity;
		Vector3                              _scale            = Vector3::Zero;
		Color                                _color            = NEUTRAL_COLOR;
		BitField                             _meshBits         = BitField::Default;
		std::unordered_map<int, EulerAngles> _meshOrientations = {};

		int _animNumber      = 0;
		int _frameNumber     = 0;
		int _prevFrameNumber = 0;

		Vector3                              _prevPosition         = Vector3::Zero;
		EulerAngles                          _prevOrientation      = EulerAngles::Identity;
		Vector3                              _prevScale            = Vector3::Zero;
		Color                                _prevColor            = NEUTRAL_COLOR;
		std::unordered_map<int, EulerAngles> _prevMeshOrientations = {};

	public:
		// Constructors

		DisplayItem() = default;
		DisplayItem(unsigned int id, GAME_OBJECT_ID objectID, const Vector3& pos, const EulerAngles& orient, const Vector3& scale);

		// Getters

		unsigned int                               GetID() const;
		GAME_OBJECT_ID                             GetObjectID() const;
		const Vector3&                             GetPosition() const;
		std::optional<std::pair<Vector2, Vector2>> GetBounds() const;
		const EulerAngles&                         GetRotation() const;
		const Vector3&                             GetScale() const;
		const Color&                               GetColor() const;
		const EulerAngles&                         GetMeshOrientation(int meshIndex) const;
		int                                        GetMeshBits() const;

		// Getters

		bool GetVisible() const;
		bool GetDisposing() const;
		bool GetMeshVisible(int meshIndex) const;
		int  GetAnimNumber() const;
		int  GetFrameNumber() const;
		int  GetEndFrameNumber() const;
		int  GetPrevFrameNumber() const;

		Vector3     GetInterpolatedPosition(float alpha) const;
		EulerAngles GetInterpolatedOrientation(float alpha) const;
		Vector3     GetInterpolatedScale(float alpha) const;
		Color       GetInterpolatedColor(float alpha) const;
		EulerAngles GetInterpolatedMeshRotation(int meshIndex, float alpha) const;

		// Setters

		void SetObjectID(GAME_OBJECT_ID objectID);
		void SetPosition(const Vector3& pos, bool disableInterpolation);
		void SetOrientation(const EulerAngles& orient, bool disableInterpolation);
		void SetScale(const Vector3& scale, bool disableInterpolation);
		void SetColor(Color& color, bool disableInterpolation);
		void SetVisible(bool visible);
		void SetDisposing(bool disposing);
		void SetMeshBits(int meshbits);
		void SetMeshVisible(int meshIndex, bool visible);
		void SetMeshOrientation(int meshIndex, const EulerAngles& orient, bool disableInterpolation);
		void SetAnimation(int animNumber);
		void SetFrame(int frameNumber);

		// Inquirers

		bool MeshExists(int meshIndex) const;

		// Utilities

		void StoreInterpolationData();
	};
}
