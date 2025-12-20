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

	struct DisplayItem
	{
	private:
		std::string _itemName;
		GAME_OBJECT_ID _objectID = GAME_OBJECT_ID::ID_NO_OBJECT;

		Vector3		_position = Vector3::Zero;
		EulerAngles _orientation = EulerAngles::Identity;

		float _scale = 0.0f;

		Color _itemColor = Vector4::One;

		BitField _meshBits = BitField::Default;

		Vector3		_prevPosition = Vector3::Zero;
		EulerAngles _prevOrientation = EulerAngles::Identity;
		float		_prevScale = 0.0f;
		Color		_prevColor = Vector4::One;

		bool _visible = true;

		std::unordered_map<int, EulerAngles> _meshRotations;
		std::unordered_map<int, EulerAngles> _prevMeshRotations;

		int _animNumber = 0;
		int _frameNumber = 0;
		int _prevFrameNumber = 0;

		// 2D Mode Properties
		bool _use2DMode = false;
		Vector2 _screenPosition = Vector2::Zero;
		DisplaySpriteAlignMode _alignMode = DisplaySpriteAlignMode::Center;
		float _depthDistance = 3800.0f; // Fixed distance from the camera in 2D mode

	public:
		void SetName(std::string itemName);
		void SetObjectID(GAME_OBJECT_ID objectID);
		void SetPosition(const Vector3& newPos, bool disableInterpolation);
		void SetRotation(const EulerAngles& newRot, bool disableInterpolation);
		void SetScale(float newScale, bool disableInterpolation);
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
		float GetScale() const;
		Color GetColor() const;
		bool GetVisibility() const;
		int GetMeshBits() const;
		bool GetMeshVisibility(int meshIndex) const;
		EulerAngles GetMeshRotation(int meshIndex) const;

		int GetAnimation() const;
		int GetFrame() const;
		int GetPreviousFrame() const;

		// Interpolation Helpers
		void StoreInterpolationData();
		Vector3 GetInterpolatedPosition(float t) const;
		EulerAngles GetInterpolatedOrientation(float t) const;
		float GetInterpolatedScale(float t) const;
		Color GetInterpolatedColor(float t) const;
		EulerAngles GetInterpolatedMeshRotation(int meshIndex, float t) const;

		// Utilities
		bool MeshExists(int index) const;

		// 2D Mode Methods
		void SetScreenPosition(const Vector2& screenPos, DisplaySpriteAlignMode align = DisplaySpriteAlignMode::Center);
		Vector2 GetScreenPosition() const;
		void SetAlignMode(DisplaySpriteAlignMode align);
		DisplaySpriteAlignMode GetAlignMode() const;

	private:
		// 2D Mode Helpers
		Vector3 CalculateAlignmentOffset(DisplaySpriteAlignMode alignMode) const;
		void UpdatePositionFrom2D();
	};

}
