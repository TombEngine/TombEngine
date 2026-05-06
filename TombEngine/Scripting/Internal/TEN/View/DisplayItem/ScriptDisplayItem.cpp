#include "framework.h"
#include "Scripting/Internal/TEN/View/DisplayItem/ScriptDisplayItem.h"

#include "Game/Animation/animation.h"
#include "Game/effects/DisplaySprite.h"
#include "Game/Hud/DrawItems/DrawItems.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Specific/configuration.h"

using namespace TEN::Effects::DisplaySprite;
using namespace TEN::Animation;
using namespace TEN::Hud;
using namespace TEN::Scripting::Types;

/// Represents a display item.
// Display item is a 3D model of any object available in the level that can be drawn in 2D screen space.
//
// @tenclass View.DisplayItem
// @pragma nostrip

namespace TEN::Scripting::DisplayItem
{
	void ScriptDisplayItem::Register(sol::state& state, sol::table& parent)
	{
		using ctors = sol::constructors<
			ScriptDisplayItem(GAME_OBJECT_ID, const Vec3&, const Rotation&, const Vec3&, int),
			ScriptDisplayItem(GAME_OBJECT_ID, const Vec3&, const Rotation&, const Vec3&),
			ScriptDisplayItem(GAME_OBJECT_ID, const Vec3&, const Rotation&),
			ScriptDisplayItem(GAME_OBJECT_ID, const Vec3&),
			ScriptDisplayItem(GAME_OBJECT_ID)>;

		// Register type.
		parent.new_usertype<ScriptDisplayItem>(
			ScriptReserved_DisplayItem, ctors(),
			sol::call_constructor, ctors(),

			ScriptReserved_SetObjectID, &ScriptDisplayItem::SetObjectID,
			ScriptReserved_SetPosition, &ScriptDisplayItem::SetPosition,
			ScriptReserved_SetRotation, &ScriptDisplayItem::SetRotation,
			ScriptReserved_SetScale, &ScriptDisplayItem::SetScale,
			ScriptReserved_SetColor, &ScriptDisplayItem::SetColor,
			ScriptReserved_SetMeshVisible, &ScriptDisplayItem::SetMeshVisibility,
			ScriptReserved_SetJointRotation, &ScriptDisplayItem::SetMeshRotation,
			ScriptReserved_SetAnimNumber, &ScriptDisplayItem::SetAnimNumber,
			ScriptReserved_SetFrameNumber, &ScriptDisplayItem::SetFrameNumber,

			ScriptReserved_DisplayItemSetMeshBits, &ScriptDisplayItem::SetMeshBits,

			ScriptReserved_GetObjectID, &ScriptDisplayItem::GetObjectID,
			ScriptReserved_GetPosition, &ScriptDisplayItem::GetPosition,
			ScriptReserved_GetRotation, &ScriptDisplayItem::GetRotation,
			ScriptReserved_GetScale, &ScriptDisplayItem::GetScale,
			ScriptReserved_GetColor, &ScriptDisplayItem::GetColor,
			ScriptReserved_GetMeshVisible, &ScriptDisplayItem::GetMeshVisibility,
			ScriptReserved_GetJointRotation, &ScriptDisplayItem::GetMeshRotation,
			ScriptReserved_GetAnimNumber, &ScriptDisplayItem::GetAnimNumber,
			ScriptReserved_GetFrameNumber, &ScriptDisplayItem::GetFrameNumber,

			ScriptReserved_GetEndFrame, &ScriptDisplayItem::GetEndFrame,
			ScriptReserved_GetBounds, &ScriptDisplayItem::GetBounds,

			ScriptReserved_DisplayItemSetAmbientLight, &ScriptDisplayItem::SetAmbientLight,
			ScriptReserved_DisplayItemSetCamera, &ScriptDisplayItem::SetCameraPosition,
			ScriptReserved_DisplayItemSetTarget, &ScriptDisplayItem::SetCameraTargetPosition,
			ScriptReserved_SetFOV, &ScriptDisplayItem::SetFOV,
			ScriptReserved_DisplayItemResetCamera, &ScriptDisplayItem::ResetCamera,
			ScriptReserved_DisplayItemGetAmbientLight, &ScriptDisplayItem::GetAmbientLight,
			ScriptReserved_DisplayItemGetCamera, &ScriptDisplayItem::GetCameraPosition,
			ScriptReserved_DisplayItemGetTarget, &ScriptDisplayItem::GetCameraTargetPosition,
			ScriptReserved_GetFOV, &ScriptDisplayItem::GetFOV,
			ScriptReserved_DisplayItemDraw, &ScriptDisplayItem::Draw);
	}

	const TEN::Hud::DisplayItem* ScriptDisplayItem::TryGetItem() const
	{
		auto* item = g_DrawItems.GetItemByID(_id);
		if (item == nullptr)
		{
			TENLog(fmt::format("DisplayItem operation failed: item {} does not exist.", _id), LogLevel::Warning);
			return nullptr;
		}

		return item;
	}

	TEN::Hud::DisplayItem* ScriptDisplayItem::TryGetItem()
	{
		return const_cast<TEN::Hud::DisplayItem*>(std::as_const(*this).TryGetItem());
	}

	/// Set the ambient color for all display items.
	// @function DisplayItem.SetAmbientLight
	// @tparam Color color New ambient color for all display items.
	// @usage
	// TEN.View.DisplayItem.SetAmbientLight(TEN.Color(128, 200, 255))
	void ScriptDisplayItem::SetAmbientLight(const ScriptColor& color)
	{
		g_DrawItems.SetAmbientLight(color);
	}

	/// Set the camera location. This single camera is used for all display items.
	// @function DisplayItem.SetCameraPosition
	// @tparam Vec3 pos New camera position.
	// @bool[opt=false] disableInterpolation Disable interpolation to allow snap movements.
	// @usage
	// TEN.View.DisplayItem.SetCameraPosition(TEN.Vec3(0, 0, 1024))
	void ScriptDisplayItem::SetCameraPosition(const Vec3& pos, TypeOrNil<bool> disableInterpolation)
	{
		auto convertedBool = ValueOr<bool>(disableInterpolation, false);
		g_DrawItems.SetCameraPosition(pos, convertedBool);
	}

	/// Set the camera target location.
	// @function DisplayItem.SetTargetPosition
	// @tparam Vec3 pos New target camera position.
	// @bool[opt=false] disableInterpolation Disable interpolation to allow snap movements.
	// @usage
	// TEN.View.DisplayItem.SetTargetPosition(TEN.Vec3(0, 0, 1024))
	void ScriptDisplayItem::SetCameraTargetPosition(const Vec3& pos, TypeOrNil<bool> disableInterpolation)
	{
		auto convertedBool = ValueOr<bool>(disableInterpolation, false);
		g_DrawItems.SetCameraTargetPosition(pos, convertedBool);
	}

	/// Set the field of view for all display items.
	// @function DisplayItem.SetFOV
	// @tparam[opt=25] float fov Field of view angle in degrees (clamped to [10, 170]).
	// @bool[opt=false] disableInterpolation Disable interpolation to allow snap movements.
	// @usage
	// TEN.View.DisplayItem.SetFOV(25)
	void ScriptDisplayItem::SetFOV(TypeOrNil<float> fov, TypeOrNil<bool> disableInterpolation)
	{
		auto convertedFOV = ValueOr<float>(fov, 25.0f);
		auto convertedBool = ValueOr<bool>(disableInterpolation, false);
		auto clampedFov = DEG_TO_RAD(std::clamp(abs(convertedFOV), 10.0f, 170.0f));
		g_DrawItems.SetFOV(clampedFov, convertedBool);
	}

	/// Get the ambient color of all the display items.
	// @function DisplayItem.GetAmbientLight
	// @treturn Color Ambient color.
	// @usage
	// local color = TEN.View.DisplayItem.GetAmbientLight()
	ScriptColor ScriptDisplayItem::GetAmbientLight()
	{
		return g_DrawItems.GetAmbientLight();
	}

	/// Get the camera position. This single camera is used for all display items.
	// @function DisplayItem.GetCameraPosition
	// @treturn Vec3 Camera position for all display items.
	// @usage
	// local camPos = TEN.View.DisplayItem.GetCameraPosition()
	Vec3 ScriptDisplayItem::GetCameraPosition()
	{
		return g_DrawItems.GetCameraPosition();
	}

	/// Get the position of the camera target.
	// @function DisplayItem.GetTargetPosition
	// @treturn Vec3 The camera target position for all of the display items.
	// @usage
	// local targetPosition = TEN.View.DisplayItem.GetTargetPosition()
	Vec3 ScriptDisplayItem::GetCameraTargetPosition()
	{
		return g_DrawItems.GetCameraTargetPosition();
	}

	/// Get field of view angle for display items.
	// @function DisplayItem.GetFOV
	// @treturn float Current FOV angle in degrees.
	// @usage
	// local fieldOfView = TEN.View.DisplayItem.GetFOV()
	float ScriptDisplayItem::GetFOV()
	{
		return RAD_TO_DEG(g_DrawItems.GetFov());
	}

	/// Reset the camera position, camera target position, and field of view.
	// @function DisplayItem.ResetCamera
	// @usage
	// local targetPosition = TEN.View.DisplayItem.ResetCamera()
	void ScriptDisplayItem::ResetCamera(TypeOrNil<bool> disableInterpolation)
	{
		auto convertedBool = ValueOr<bool>(disableInterpolation, false);
		g_DrawItems.ResetCamera(convertedBool);
	}

	/// Class
	// @section Class
	// Methods for DisplayItem instances.

	/// Create a DisplayItem object.
	// @function DisplayItem
	// @tparam Objects.ObjID objectID Slot object ID.
	// @tparam[opt=Vec3(0&#44; 0&#44; 0)] Vec3 pos Position in 3D display space.
	// @tparam[opt=Rotation(0&#44; 0&#44; 0)] Rotation rot Rotation on the XYZ axes.
	// @tparam[opt=Vec3(1&#44; 1&#44; 1)] Vec3 scale Visual scale.
	// @tparam[opt] int meshBits Packed meshbits.
	// @treturn DisplayItem A new DisplayItem object.
	// @usage
	// local item = TEN.View.DisplayItem(TEN.Objects.ObjID.PISTOLS_ITEM)

	ScriptDisplayItem::ScriptDisplayItem(GAME_OBJECT_ID objectID, const Vec3& pos, const Rotation& rot, const Vec3& scale, int meshBits)
	{
		auto potentialID = g_DrawItems.AddItem(objectID, pos, rot.ToEulerAngles(), scale, meshBits);
		ScriptAssertTerminateF(potentialID, "Creation of display item failed. Possibly too many display items already active?");
		_id = potentialID;
	}

	ScriptDisplayItem::ScriptDisplayItem(GAME_OBJECT_ID objectID, const Vec3& pos, const Rotation& rot, const Vec3& scale) :
		ScriptDisplayItem(objectID, pos, rot, scale, ALL_JOINT_BITS) { }

	ScriptDisplayItem::ScriptDisplayItem(GAME_OBJECT_ID objectID, const Vec3& pos, const Rotation& rot) :
		ScriptDisplayItem(objectID, pos, rot, Vector3::One, ALL_JOINT_BITS) { }

	ScriptDisplayItem::ScriptDisplayItem(GAME_OBJECT_ID objectID, const Vec3& pos) :
		ScriptDisplayItem(objectID, pos, EulerAngles::Identity, Vector3::One, ALL_JOINT_BITS) { }

	ScriptDisplayItem::ScriptDisplayItem(GAME_OBJECT_ID objectID) :
		ScriptDisplayItem(objectID, Vector3::Zero, EulerAngles::Identity, Vector3::One, ALL_JOINT_BITS) { }

	ScriptDisplayItem::~ScriptDisplayItem()
	{
		if (!g_DrawItems.TestItemExists(_id))
		{
			TENLog(fmt::format("Attempt to remove display item with invalid ID {}.", _id), LogLevel::Warning);
			return;
		}

		g_DrawItems.RemoveItem(_id);
	}

	/// Change the display item's object ID. 
	// @function DisplayItem:SetObjectID
	// @tparam Objects.ObjID objectID New slot object ID.
	// @usage
	// item:SetObjectID(TEN.Objects.ObjID.BIGMEDI_ITEM)
	void ScriptDisplayItem::SetObjectID(GAME_OBJECT_ID objectID)
	{
		if (auto* item = TryGetItem())
			item->SetObjectID(objectID);
	}

	/// Set the display item's position.
	// @function DisplayItem:SetPosition
	// @tparam Vec3 pos New position.
	// @bool[opt=false] disableInterpolation Disable interpolation to allow snap movements.
	// @usage
	// item:SetPosition(TEN.Vec3(0, 200, 1024))
	void ScriptDisplayItem::SetPosition(const Vec3& pos, TypeOrNil<bool> disableInterpolation)
	{
		if (auto* item = TryGetItem())
			item->SetPosition(pos, ValueOr<bool>(disableInterpolation, false));
	}

	/// Set the display item's rotation.
	// @function DisplayItem:SetRotation
	// @tparam Rotation rot New rotation.
	// @bool[opt=false] disableInterpolation Disable interpolation to allow snap movements.
	// @usage
	// item:SetRotation(TEN.Rotation(0, 200, 1024))
	void ScriptDisplayItem::SetRotation(const Rotation& rot, TypeOrNil<bool> disableInterpolation)
	{
		if (auto* item = TryGetItem())
			item->SetOrientation(rot.ToEulerAngles(), ValueOr<bool>(disableInterpolation, false));
	}

	/// Set the display item's scale.
	// @function DisplayItem:SetScale
	// @tparam Vec3 scale New scale.
	// @bool[opt=false] disableInterpolation Disable interpolation to allow snap movements.
	// @usage
	// item:SetScale(Vec3(2,2,2))
	void ScriptDisplayItem::SetScale(const Vec3& scale, TypeOrNil<bool> disableInterpolation)
	{
		if (auto* item = TryGetItem())
			item->SetScale(scale, ValueOr<bool>(disableInterpolation, false));
	}

	/// Set the display item's color.
	// @function DisplayItem:SetColor
	// @tparam Color color New color.
	// @bool[opt=false] disableInterpolation Disable interpolation to allow snap color changes.
	// @usage
	// item:SetColor(TEN.Color(128, 200, 255))
	void ScriptDisplayItem::SetColor(const ScriptColor& color, TypeOrNil<bool> disableInterpolation)
	{
		if (auto* item = TryGetItem())
			item->SetColor(Color(color), ValueOr<bool>(disableInterpolation, false));
	}

	/// Set the packed mesh bits for the display item.
	// Mesh bits represent the visibility of every mesh in a given display item. Can be used in advanced workflows, such
	// as drawing a revolver with or without a lasersight.
	// @function DisplayItem:SetMeshBits
	// @tparam int meshBits Packed MeshBits to be set.
	// @usage
	// item:SetMeshBits(3)
	void ScriptDisplayItem::SetMeshBits(int meshBits)
	{
		if (auto* item = TryGetItem())
			item->SetMeshBits(meshBits);
	}

	/// Make the specified mesh of a display item visible or invisible.
	// @function DisplayItem:SetMeshVisible
	// @tparam int meshIndex Mesh index.
	// @tparam bool isVisible True to set visible, false to set invisible.
	// @usage
	// item:SetMeshVisible(1, false)
	void ScriptDisplayItem::SetMeshVisibility(int meshIndex, bool isVisible)
	{
		if (auto* item = TryGetItem())
			item->SetMeshVisible(meshIndex, isVisible);
	}

	/// Set the display item's joint rotation.
	// @function DisplayItem:SetJointRotation
	// @tparam int meshIndex Joint index..
	// @tparam Rotation rot New rotation.
	// @bool[opt=false] disableInterpolation Disables interpolation to allow for snap movements.
	// @usage
	// item:SetJointRotation(1, TEN.Rotation(0, 200, 0))
	void ScriptDisplayItem::SetMeshRotation(int meshIndex, Rotation rot, TypeOrNil<bool> disableInterpolation)
	{
		if (auto* item = TryGetItem())
			item->SetMeshOrientation(meshIndex, rot.ToEulerAngles(), ValueOr<bool>(disableInterpolation, false));
	}

	/// Set the animation number of a display item.
	// @function DisplayItem:SetAnim
	// @tparam int animNumber Animation number to set.
	// @usage
	// item:SetAnim(2)
	void ScriptDisplayItem::SetAnimNumber(int animNumber)
	{
		if (auto* item = TryGetItem())
			item->SetAnimation(animNumber);
	}

	/// Set the frame number of a display item's current animation.
	// This will set the specified animation to the given frame.
	// The number of frames in an animation can be seen under the heading "End frame" in the WadTool animation editor.
	// @function DisplayItem:SetFrame
	// @tparam int frameNumber Frame number to set.
	// @usage
	// item:SetFrame(10)
	void ScriptDisplayItem::SetFrameNumber(int frameNumber)
	{
		if (auto* item = TryGetItem())
			item->SetFrame(frameNumber);
	}

	/// Retrieve the object ID from a display item.
	// @function DisplayItem:GetObjectID
	// @treturn[1] Objects.ObjID Slot object ID.
	// @treturn[2] nil If the display item does not exist.
	// @usage
	// local objectID = item:GetObjectID()
	GAME_OBJECT_ID ScriptDisplayItem::GetObjectID() const
	{
		if (auto* item = TryGetItem())
			return item->GetObjectID();

		return ID_NO_OBJECT;
	}

	/// Get the display item's position.
	// @function DisplayItem:GetPosition
	// @treturn[1] Vec3 Position.
	// @treturn[2] nil If the display item does not exist.
	// @usage
	// local objectPosition = item:GetPosition()
	sol::optional<Vec3> ScriptDisplayItem::GetPosition() const
	{
		if (auto* item = TryGetItem())
			return Vec3(item->GetPosition());

		return sol::nullopt;
	}

	/// Get the display item's rotation.
	// @function DisplayItem:GetRotation
	// @treturn[1] Rotation Rotation.
	// @treturn[2] nil If the display item does not exist.
	// @usage
	// local objectRotation = item:GetRotation()
	sol::optional<Rotation> ScriptDisplayItem::GetRotation() const
	{
		if (auto* item = TryGetItem())
			return Rotation(item->GetRotation());

		return sol::nullopt;
	}

	/// Get the display item's scale.
	// @function DisplayItem:GetScale
	// @treturn[1] float Scale.
	// @treturn[2] nil If the display item does not exist.
	// @usage
	// local objectScale = item:GetScale()
	sol::optional<Vec3> ScriptDisplayItem::GetScale() const
	{
		if (auto* item = TryGetItem())
			return item->GetScale();

		return sol::nullopt;
	}

	/// Get the display item's color.
	// @function DisplayItem:GetColor
	// @treturn[1] Color Color.
	// @treturn[2] nil If the display item does not exist.
	// @usage
	// local objectColor = item:GetColor()
	sol::optional<ScriptColor> ScriptDisplayItem::GetColor() const
	{
		if (auto* item = TryGetItem())
			return ScriptColor(item->GetColor());

		return sol::nullopt;
	}

	/// Get the visibility state of a specified mesh in the display item.
	// @function DisplayItem:GetMeshVisible
	// @tparam int index Index of a mesh.
	// @treturn[1] bool Visibility status.
	// @treturn[2] bool False if the display item does not exist.
	// @usage
	// local test = item:GetMeshVisible(1)
	// print(test)
	bool ScriptDisplayItem::GetMeshVisibility(int meshIndex) const
	{
		if (auto* item = TryGetItem())
			return item->GetMeshVisible(meshIndex);

		return false;
	}

	/// Get the display item's joint rotation.
	// @function DisplayItem:GetJointRotation
	// @tparam int meshIndex Index of the joint to check.
	// @treturn[1] Rotation Joint rotation.
	// @treturn[2] nil If the display item does not exist.
	// @usage
	// local jointRotation = item:GetJointRotation(1)
	sol::optional<Rotation> ScriptDisplayItem::GetMeshRotation(int meshIndex) const
	{
		if (auto* item = TryGetItem())
			return Rotation(item->GetMeshOrientation(meshIndex));

		return sol::nullopt;
	}

	/// Get the current animation number of a display item.
	// This corresponds to the number shown in the item's animation list in WadTool.
	// @function DisplayItem:GetAnim
	// @treturn[1] int Active animation number.
	// @treturn[2] nil If the display item does not exist.
	// @usage
	// local animNumber = item:GetAnim()
	sol::optional<int> ScriptDisplayItem::GetAnimNumber() const
	{
		if (auto* item = TryGetItem())
			return item->GetAnimNumber();

		return sol::nullopt;
	}

	/// Get the current frame number of the active animation of a display item.
	// @function DisplayItem:GetFrame
	// @treturn[1] int Current frame number of the active animation.
	// @treturn[2] nil If the display item does not exist.
	// @usage
	// local frameNumber = item:GetFrame()
	sol::optional<int> ScriptDisplayItem::GetFrameNumber() const
	{
		if (auto* item = TryGetItem())
			return item->GetFrameNumber();

		return sol::nullopt;
	}

	/// Get the end frame number of the display item's active animation.
	// This is the "End Frame" set in WadTool for the animation.
	// @function DisplayItem:GetEndFrame()
	// @treturn[1] int End frame number of the active animation.
	// @treturn[2] nil If the display item does not exist.
	// @usage
	// local endFrame = item:GetEndFrame()

	sol::optional<int> ScriptDisplayItem::GetEndFrame() const
	{
		if (auto* item = TryGetItem())
			return item->GetEndFrameNumber();

		return sol::nullopt;
	}

	/// Get the projected 2D bounding box of the display item.
	// Projects the display item into display space and returns two Vec2 values.
	// @function DisplayItem:GetBounds
	// @treturn[1] Vec2 center Projected center position in display space in percent.
	// @treturn[1] Vec2 size The projected width/height in display space in percent.
	// @treturn[2] nil If the display item does not exist or has no bounds.
	// @usage
	// local bounds = item:GetBounds()
	// if bounds then
	//	 print("Center: ", bounds[1].x, bounds[1].y)
	//	 print("Size: ", bounds[2].x, bounds[2].y)
	// end
	// 
	sol::optional<std::pair<Vec2, Vec2>> ScriptDisplayItem::GetBounds() const
	{
		if (auto* item = TryGetItem())
		{
			auto bounds = item->GetBounds();
			if (!bounds.has_value())
				return sol::nullopt;

			float screenWidth = g_Configuration.ScreenWidth;
			float screenHeight = g_Configuration.ScreenHeight;

			const auto& center = bounds->first;
			const auto& size = bounds->second;

			// Convert to percent-based resolution.
			auto centerPercent = Vec2(center.x / screenWidth, center.y / screenHeight) * 100.0f;
			auto sizePercent = Vec2(size.x / screenWidth, size.y / screenHeight) * 100.0f;
			return std::pair<Vec2, Vec2>(centerPercent, sizePercent);
		}

		return sol::nullopt;
	}

	/// Draw the display item in display space for the current frame.
	// @function DisplayItem:Draw
	// @usage
	// item:Draw()
	void ScriptDisplayItem::Draw()
	{
		if (auto* item = TryGetItem())
			item->SetVisible(true);
	}
}
