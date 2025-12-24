#include "framework.h"
#include "Game/Hud/DrawItems/DrawItems.h"

#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/clock.h"

using namespace TEN::Math;
using TEN::Renderer::g_Renderer;

namespace TEN::Hud
{
	DrawItemsController g_DrawItems = {};

	void DrawItemsController::AddItem(const std::string& itemName, GAME_OBJECT_ID objectID, const Vector3& position, const EulerAngles& rotation, const Vector3& scale, int meshBits)
	{
		// Check if item already exists
		for (auto& item : _displayItems)
		{
			if (item.GetName() == itemName)
			{
				// Update existing item
				item.SetObjectID(objectID);
				item.SetPosition(position, true);
				item.SetRotation(rotation, true);
				item.SetScale(scale, true);
				item.SetMeshBits(meshBits);
				return;
			}
		}

		// If at capacity, don’t add new item
		if (_displayItems.size() >= DRAW_ITEM_COUNT_MAX)
			return;

		auto newItem = DisplayItem(itemName, objectID, position, rotation, scale);
		newItem.SetMeshBits(meshBits);

		_displayItems.push_back(newItem);
	}

	void DrawItemsController::RemoveItem(const std::string& itemName)
	{
		auto item = std::find_if(_displayItems.begin(), _displayItems.end(),
			[&](const DisplayItem& item)
			{
				return item.GetName() == itemName;
			});

		if (item != _displayItems.end())
		{
			int removedIndex = static_cast<int>(std::distance(_displayItems.begin(), item));
			_displayItems.erase(item);
		}
	}

	void DrawItemsController::Update()
	{
		std::sort(_displayItems.begin(), _displayItems.end(),
			[](const DisplayItem& a, const DisplayItem& b)
			{
				return (a.GetPosition().z > b.GetPosition().z);
			});

		for (auto& item : _displayItems)
		{
			item.StoreInterpolationData();
		}

		StoreCameraInterpolationData();
	}

	void DrawItemsController::Draw() const
	{
		for (const auto& item : _displayItems)
		{
			g_Renderer.DrawObjectIn3DSpace(item);
		}
	}

	void DrawItemsController::Clear()
	{
		if (_displayItems.empty())
			return;

		_displayItems.clear();
	}

	DisplayItem* DrawItemsController::GetItemByName(const std::string& itemName)
	{
		for (auto& item : _displayItems)
		{
			if (item.GetName() == itemName)
				return &item;
		}
		return nullptr;
	}

	bool DrawItemsController::IsEmpty()
	{
		return _displayItems.empty();
	}

	bool DrawItemsController::IfItemExists(const std::string& itemName)
	{
		for (auto& item : _displayItems)
		{
			if (item.GetName() == itemName)
			{
				return true;
			}
		}
		return false;
	}

	bool DrawItemsController::IfObjectIDExists(GAME_OBJECT_ID objectID)
	{
		for (auto& item : _displayItems)
		{
			if (item.GetObjectID() == objectID)
			{
				return true;
			}
		}
		return false;
	}

	std::vector<DisplayItem>& DrawItemsController::GetItems()
	{
		return _displayItems;
	}

	void DrawItemsController::SetCameraPosition(const Vector3& pos, bool disableInterpolation)
	{
		if (disableInterpolation)
			_cameraPreviousPosition = pos;

		_cameraPosition = pos;
	}

	void DrawItemsController::SetCameraTargetPosition(const Vector3& target, bool disableInterpolation)
	{
		if (disableInterpolation)
			_targetPreviousPosition = target;

		_targetPosition = target;
	}

	void DrawItemsController::ResetCamera(bool disableInterpolation)
	{
		if (disableInterpolation)
		{
			_cameraPreviousPosition = Vector3(0.0f, 0.0f, -BLOCK(1));
			_targetPreviousPosition = Vector3::Zero;
		}
	
		_cameraPosition = Vector3(0.0f, 0.0f, -BLOCK(1));
		_targetPosition = Vector3::Zero;
	}

	void DrawItemsController::SetAmbientLight(const Vector4& lightColor)
	{
		_ambientLight = lightColor;
	}

	Vector4 DrawItemsController::GetAmbientLight() const
	{
		return _ambientLight;
	}

	Vector3 DrawItemsController::GetCameraPosition() const
	{
		return _cameraPosition;
	}

	Vector3 DrawItemsController::GetCameraTargetPosition() const
	{
		return _targetPosition;
	}

	Vector3 DrawItemsController::GetInterpolatedCameraPosition(float t) const
	{
		return Vector3::Lerp(_cameraPreviousPosition, _cameraPosition, t);
	}

	Vector3 DrawItemsController::GetInterpolatedCameraTargetPosition(float t) const
	{
		return Vector3::Lerp(_targetPreviousPosition, _targetPosition, t);
	}

	void DrawItemsController::StoreCameraInterpolationData()
	{
		_cameraPreviousPosition = _cameraPosition;
		_targetPreviousPosition = _targetPosition;
	}
}
