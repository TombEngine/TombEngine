#pragma once
#include "framework.h"
#include "Math/Math.h"
#include "Objects/game_object_ids.h"
#include "Specific/Structures/BitField.h"
#include "Game/effects/DisplaySprite.h"

using namespace TEN::Effects::DisplaySprite;
using namespace TEN::Math;
using namespace TEN::Utils;

namespace TEN::Hud
{
	constexpr float DEPTH_DISTANCE_2D = 3800.0f; // Fixed distance from the camera in 2D mode

	struct DisplayItem
	{
	private:
		std::string _itemName;
		GAME_OBJECT_ID _objectID = GAME_OBJECT_ID::ID_NO_OBJECT;

		Vector3		_position = Vector3::Zero;
		EulerAngles _orientation = EulerAngles::Identity;
		Vector3		_scale = Vector3::Zero;

		Color _color = Vector4::One;

		BitField _meshBits = BitField::Default;

		Vector3		_prevPosition = Vector3::Zero;
		EulerAngles _prevOrientation = EulerAngles::Identity;
		Vector3		_prevScale = Vector3::Zero;
		Color		_prevColor = Vector4::One;

		bool _visible = true;

		std::unordered_map<int, EulerAngles> _meshRotations;
		std::unordered_map<int, EulerAngles> _prevMeshRotations;

		int _animNumber = 0;
		int _frameNumber = 0;
		int _prevFrameNumber = 0;

	public:
		DisplayItem() = default;
		DisplayItem(const std::string& name, GAME_OBJECT_ID objectID, const Vector3& pos, const EulerAngles& rot, const Vector3& scale);

		void SetName(std::string itemName);
		void SetObjectID(GAME_OBJECT_ID objectID);
		void SetPosition(const Vector3& newPos, bool disableInterpolation);
		void SetRotation(const EulerAngles& newRot, bool disableInterpolation);
		void SetScale(const Vector3& newScale, bool disableInterpolation);
		void SetColor(Color& newColor, bool disableInterpolation);
		void SetVisibility(bool visible);
		void SetMeshBits(int meshbits);
		void SetMeshVisibility(int meshIndex, bool visible);
		void SetMeshRotation(int meshIndex, const EulerAngles& rot, bool disableInterpolation);
		
		void SetAnimation(int animation);
		void SetFrame(int frame);

		std::string GetName() const;
		GAME_OBJECT_ID GetObjectID() const;
		Vector3 GetPosition() const;
		std::optional<std::pair<Vector2, Vector2>> GetBounds() const;
		EulerAngles GetRotation() const;
		Vector3 GetScale() const;
		Color GetColor() const;
		bool GetVisibility() const;
		int GetMeshBits() const;
		bool GetMeshVisibility(int meshIndex) const;
		EulerAngles GetMeshRotation(int meshIndex) const;

		int GetAnimation() const;
		int GetFrame() const;
		int GetEndFrame() const;
		int GetPreviousFrame() const;

		// Interpolation Helpers
		void StoreInterpolationData();
		Vector3 GetInterpolatedPosition(float t) const;
		EulerAngles GetInterpolatedOrientation(float t) const;
		Vector3 GetInterpolatedScale(float t) const;
		Color GetInterpolatedColor(float t) const;
		EulerAngles GetInterpolatedMeshRotation(int meshIndex, float t) const;

		// Utilities
		bool MeshExists(int index) const;
	};

}
