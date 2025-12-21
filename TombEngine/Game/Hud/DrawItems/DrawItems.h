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
		static constexpr auto DRAW_ITEM_COUNT_MAX	= 128;

		// Fields
		std::vector<DisplayItem> _displayItems = {};
		
		Vector3 _cameraPosition = Vector3(0.0f, 0.0f, -BLOCK(1));
		Vector3 _targetPosition = Vector3::Zero;

		Vector3 _cameraPreviousPosition = _cameraPosition;
		Vector3 _targetPreviousPosition = _targetPosition;

		Vector4 _ambientLight = Vector4(1.0f, 1.0f, 0.5f, 1.0f);

	public:

		void AddItem(const std::string& itemName, GAME_OBJECT_ID objectID, const Vector3& origin, const EulerAngles& newRot, const Vector3& scale, int meshBits);
		void RemoveItem(const std::string& itemName);

		void Update();
		void Draw() const;
		void Clear();

		DisplayItem* GetItemByName(const std::string& itemName);

		bool IsEmpty();
		bool IfItemExists(const std::string& itemName);
		bool IfObjectIDExists(GAME_OBJECT_ID objectID);

		std::vector<DisplayItem>& GetItems();

		// Camera settings.
		void SetCameraPosition(const Vector3& pos, bool disableInterpolation);
		void SetCameraTargetPosition(const Vector3& target, bool disableInterpolation);
		void ResetCamera(bool disableInterpolation);

		void SetAmbientLight(const Vector4& lightColor);
		Vector4 GetAmbientLight() const;

		Vector3 GetCameraPosition() const;
		Vector3 GetCameraTargetPosition() const;

		Vector3 GetInterpolatedCameraPosition(float t) const;
		Vector3 GetInterpolatedCameraTargetPosition(float t) const;

		void StoreCameraInterpolationData();

	};

	extern DrawItemsController g_DrawItems;
}
