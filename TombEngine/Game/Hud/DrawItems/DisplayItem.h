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
		std::string ItemName;
		GAME_OBJECT_ID ObjectID = GAME_OBJECT_ID::ID_NO_OBJECT;

		Vector3		Position = Vector3::Zero;
		EulerAngles Orientation = EulerAngles::Identity;

		float Scale = 0.0f;

		Color ItemColor = Vector4::One;

		BitField MeshBits = BitField::Default;

		Vector3		PrevPosition = Vector3::Zero;
		EulerAngles PrevOrientation = EulerAngles::Identity;
		float		PrevScale = 0.0f;
		Color		PrevColor = Vector4::One;

		bool Visible = true;

		std::unordered_map<int, EulerAngles> MeshRotations;
		std::unordered_map<int, EulerAngles> PrevMeshRotations;

		int AnimNumber = 0;
		int FrameNumber = 0;
		int PrevFrameNumber = 0;

		// 2D Mode Properties
		bool Use2DMode = false;
		Vector2 ScreenPosition = Vector2::Zero;
		DisplaySpriteAlignMode AlignMode = DisplaySpriteAlignMode::Center;
		float DepthDistance = 3800.0f; // Fixed distance from the camera in 2D mode

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

		//Utilities
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
