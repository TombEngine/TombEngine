#pragma once

#include "Game/Hud/DrawItems/DisplayItem.h"
#include "Math/Math.h"
#include "Objects/game_object_ids.h"

using namespace TEN::Math;

namespace TEN::Hud
{
	class DrawItemsController
	{
	private:
		// Constants

		static constexpr int DRAW_ITEM_COUNT_MAX = 128;
		static constexpr auto AMBIENT_COLOR = Vector4(0.5f, 0.5f, 0.25f, 1.0f);

		// Fields

		std::vector<DisplayItem> _displayItems = {};
		Vector4                  _ambientLight = AMBIENT_COLOR;
		
		float _fov = DEG_TO_RAD(25.0f);
		float _prevFov = DEG_TO_RAD(25.0f);

		Vector3 _cameraPosition     = Vector3(0.0f, 0.0f, -BLOCK(1));
		Vector3 _targetPosition     = Vector3::Zero;
		Vector3 _prevCameraPosition = _cameraPosition;
		Vector3 _prevTargetPosition = _targetPosition;

		unsigned int _lastID = 0;

	public:
		// Getters

		DisplayItem*			  GetItemByID(unsigned int id);
		std::vector<DisplayItem>& GetItems();
		Vector3					  GetCameraPosition() const;
		Vector3					  GetCameraTargetPosition() const;
		float					  GetFov() const;
		Vector3					  GetInterpolatedCameraPosition(float alpha) const;
		Vector3                   GetInterpolatedCameraTargetPosition(float alpha) const;
		float					  GetInterpolatedFov(float alpha) const;
		Vector4					  GetAmbientLight() const;

		// Setters

		void SetCameraPosition(const Vector3& pos, bool disableInterpolation);
		void SetCameraTargetPosition(const Vector3& target, bool disableInterpolation);
		void SetFOV(const float& fov, bool disableInterpolation);
		void SetAmbientLight(const Vector4& color);

		// Inquirers

		bool IsEmpty();
		bool TestItemExists(unsigned int id);
		bool TestObjectIDExists(GAME_OBJECT_ID objectID);

		// Utilities

		unsigned int AddItem(GAME_OBJECT_ID objectID, const Vector3& origin, const EulerAngles& orient, const Vector3& scale, int meshBits);
		void RemoveItem(unsigned int id);

		void Prepare();
		void Update();
		void Draw() const;
		void Clear();

		void ResetCamera(bool disableInterpolation);
		void StoreCameraInterpolationData();
	};

	extern DrawItemsController g_DrawItems;
}
