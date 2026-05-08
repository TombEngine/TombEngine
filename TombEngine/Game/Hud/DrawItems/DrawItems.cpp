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

	DisplayItem* DrawItemsController::GetItemByID(unsigned int id)
	{
		for (auto& item : _displayItems)
		{
			if (item.GetID() == id)
				return &item;
		}

		return nullptr;
	}

	std::vector<DisplayItem>& DrawItemsController::GetItems()
	{
		return _displayItems;
	}

	Vector3 DrawItemsController::GetCameraPosition() const
	{
		return _cameraPosition;
	}

	Vector3 DrawItemsController::GetCameraTargetPosition() const
	{
		return _targetPosition;
	}

	float DrawItemsController::GetFov() const
	{
		return _fov;
	}

	Vector3 DrawItemsController::GetInterpolatedCameraPosition(float alpha) const
	{
		if (Vector3::Distance(_prevCameraPosition, _cameraPosition) < EPSILON)
			return _cameraPosition;

		return Vector3::Lerp(_prevCameraPosition, _cameraPosition, alpha);
	}

	Vector3 DrawItemsController::GetInterpolatedCameraTargetPosition(float alpha) const
	{
		if (Vector3::Distance(_prevTargetPosition, _targetPosition) < EPSILON)
			return _targetPosition;

		return Vector3::Lerp(_prevTargetPosition, _targetPosition, alpha);
	}

	float DrawItemsController::GetInterpolatedFov(float alpha) const
	{
		if (std::abs(_prevFov - _fov) < EPSILON)
			return _fov;

		return Lerp(_prevFov, _fov, alpha);
	}

	Vector4 DrawItemsController::GetAmbientLight() const
	{
		return _ambientLight;
	}

	void DrawItemsController::SetCameraPosition(const Vector3& pos, bool disableInterpolation)
	{
		if (disableInterpolation)
			_prevCameraPosition = pos;

		_cameraPosition = pos;
	}

	void DrawItemsController::SetCameraTargetPosition(const Vector3& target, bool disableInterpolation)
	{
		if (disableInterpolation)
			_prevTargetPosition = target;

		_targetPosition = target;
	}

	void DrawItemsController::SetFOV(const float& fov, bool disableInterpolation)
	{
		if (disableInterpolation)
			_prevFov = fov;

		_fov = fov;
	}

	void DrawItemsController::SetAmbientLight(const Vector4& color)
	{
		_ambientLight = color;
	}

	bool DrawItemsController::IsEmpty()
	{
		return _displayItems.empty();
	}

	bool DrawItemsController::TestItemExists(unsigned int id)
	{
		for (auto& item : _displayItems)
		{
			if (item.GetID() == id)
			{
				return true;
			}
		}

		return false;
	}

	bool DrawItemsController::TestObjectIDExists(GAME_OBJECT_ID objectID)
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

	unsigned int DrawItemsController::AddItem(GAME_OBJECT_ID objectID, const Vector3& pos, const EulerAngles& orient, const Vector3& scale, int meshBits)
	{
		// If at capacity, don't add new item.
		if (_displayItems.size() >= DRAW_ITEM_COUNT_MAX)
			return 0;

		if (_lastID == UINT_MAX)
			_lastID = 0;

		_lastID++;

		auto newItem = DisplayItem(_lastID, objectID, pos, orient, scale);
		newItem.SetMeshBits(meshBits);
		_displayItems.push_back(newItem);

		return _lastID;
	}

	void DrawItemsController::RemoveItem(unsigned int id)
	{
		auto item = std::find_if(_displayItems.begin(), _displayItems.end(),
		[&](const DisplayItem& item)
		{
			return item.GetID() == id;
		});

		if (item != _displayItems.end())
		{
			int removedIndex = (int)(std::distance(_displayItems.begin(), item));
			_displayItems.erase(item);
		}
	}

	void DrawItemsController::Prepare()
	{
		if (_displayItems.empty())
			return;

		_displayItems.erase(std::remove_if(_displayItems.begin(), _displayItems.end(),
		[](const DisplayItem& item)
		{
			return item.GetDisposing();
		}),
			_displayItems.end());

		for (auto& item : _displayItems)
		{
			item.StoreInterpolationData();
			item.SetVisible(false);
		}

		StoreCameraInterpolationData();
	}

	void DrawItemsController::Update()
	{
		if (_displayItems.empty())
			return;

		std::sort(_displayItems.begin(), _displayItems.end(),
		[](const DisplayItem& item0, const DisplayItem& item1)
		{
			return (item0.GetPosition().z > item1.GetPosition().z);
		});
	}

	void DrawItemsController::Draw() const
	{
		if (_displayItems.empty())
			return;

		for (const auto& item : _displayItems)
			g_Renderer.DrawObjectIn3DSpace(item);
	}

	void DrawItemsController::Clear()
	{
		if (_displayItems.empty())
			return;

		_displayItems.clear();
	}

	void DrawItemsController::ResetCamera(bool disableInterpolation)
	{
		if (disableInterpolation)
		{
			_prevCameraPosition = Vector3(0.0f, 0.0f, -BLOCK(1));
			_prevTargetPosition = Vector3::Zero;
			_prevFov = DEG_TO_RAD(25.0f);
		}

		_cameraPosition = Vector3(0.0f, 0.0f, -BLOCK(1));
		_targetPosition = Vector3::Zero;
		_fov = DEG_TO_RAD(25.0f);
		_ambientLight = AMBIENT_COLOR;
	}

	void DrawItemsController::StoreCameraInterpolationData()
	{
		_prevCameraPosition = _cameraPosition;
		_prevTargetPosition = _targetPosition;
		_prevFov = _fov;
	}
}
