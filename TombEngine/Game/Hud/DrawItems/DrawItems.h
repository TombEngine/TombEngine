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

		// Fields

		std::vector<DisplayItem> _displayItems = {};
		Vector4                  _ambientLight = Vector4(1.0f, 1.0f, 0.5f, 1.0f);
		
		Vector3 _cameraPosition     = Vector3(0.0f, 0.0f, -BLOCK(1));
		Vector3 _targetPosition     = Vector3::Zero;
		Vector3 _prevCameraPosition = _cameraPosition;
		Vector3 _prevTargetPosition = _targetPosition;

	public:
		// Getters

		DisplayItem* GetItemByName(const std::string& name);
		std::vector<DisplayItem>& GetItems();
		Vector3                   GetCameraPosition() const;
		Vector3                   GetCameraTargetPosition() const;
		Vector3                   GetInterpolatedCameraPosition(float alpha) const;
		Vector3                   GetInterpolatedCameraTargetPosition(float alpha) const;
		Vector4                   GetAmbientLight() const;

		// Setters

		void SetCameraPosition(const Vector3& pos, bool disableInterpolation);
		void SetCameraTargetPosition(const Vector3& target, bool disableInterpolation);
		void SetAmbientLight(const Vector4& color);

		// Inquirers

		bool IsEmpty();
		bool TestItemExists(const std::string& name);
		bool TestObjectIDExists(GAME_OBJECT_ID objectID);

		// Utilities

		void AddItem(const std::string& name, GAME_OBJECT_ID objectID, const Vector3& origin, const EulerAngles& orient, const Vector3& scale, int meshBits);
		void RemoveItem(const std::string& name);

		void Update();
		void Draw() const;
		void Clear();

		void ResetCamera(bool disableInterpolation);
		void StoreCameraInterpolationData();
	};

	extern DrawItemsController g_DrawItems;
}
