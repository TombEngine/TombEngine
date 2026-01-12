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

/// Represents a Display Item.
//
// @tenclass View.DisplayItem
// @pragma nostrip

namespace TEN::Scripting::DisplayItem
{
	void ScriptDisplayItem::Register(sol::state& state, sol::table& parent)
	{
		using ctors = sol::constructors<
			ScriptDisplayItem(std::string, GAME_OBJECT_ID, const Vec3&, const Rotation&, const Vec3&, int),
			ScriptDisplayItem(std::string, GAME_OBJECT_ID, const Vec3&, const Rotation&, const Vec3&),
			ScriptDisplayItem(std::string, GAME_OBJECT_ID, const Vec3&),
			ScriptDisplayItem(std::string, GAME_OBJECT_ID)>;

		// Register type.
		parent.new_usertype<ScriptDisplayItem>(
			ScriptReserved_DrawItem,
			ctors(),
			sol::call_constructor, ctors(),

			ScriptReserved_DrawItemRemove, &ScriptDisplayItem::Remove,
			ScriptReserved_DrawItemExists, &ScriptDisplayItem::Exists,
			ScriptReserved_SetObjectID, &ScriptDisplayItem::SetObjectID,
			ScriptReserved_SetPosition, &ScriptDisplayItem::SetPosition,
			ScriptReserved_SetRotation, &ScriptDisplayItem::SetRotation,
			ScriptReserved_SetScale, &ScriptDisplayItem::SetScale,
			ScriptReserved_SetColor, &ScriptDisplayItem::SetColor,

			ScriptReserved_DrawItemSetMeshBits, &ScriptDisplayItem::SetMeshBits,

			ScriptReserved_SetMeshVisible, &ScriptDisplayItem::SetMeshVisibility,
			ScriptReserved_SetJointRotation, &ScriptDisplayItem::SetMeshRotation,
			ScriptReserved_SetVisible, &ScriptDisplayItem::SetVisibility,
			ScriptReserved_SetFrameNumber, &ScriptDisplayItem::SetFrame,
			ScriptReserved_GetObjectID, & ScriptDisplayItem::GetObjectID,
			ScriptReserved_GetPosition, &ScriptDisplayItem::GetPosition,
			ScriptReserved_GetBounds, &ScriptDisplayItem::GetBounds,
			ScriptReserved_GetRotation, &ScriptDisplayItem::GetRotation,
			ScriptReserved_GetScale, &ScriptDisplayItem::GetScale,
			ScriptReserved_GetColor, &ScriptDisplayItem::GetColor,
			ScriptReserved_GetMeshVisible, &ScriptDisplayItem::GetMeshVisibility,
			ScriptReserved_GetJointRotation, &ScriptDisplayItem::GetMeshRotation,
			ScriptReserved_GetVisible, &ScriptDisplayItem::GetVisibility,
			ScriptReserved_GetFrameNumber, &ScriptDisplayItem::GetFrameNumber,
			ScriptReserved_GetEndFrame, &ScriptDisplayItem::GetEndFrame,
			ScriptReserved_GetAnimNumber, &ScriptDisplayItem::GetAnimNumber,
			ScriptReserved_DrawItemGetItem, &ScriptDisplayItem::GetItemByName,
			ScriptReserved_DrawItemRemoveItem, &ScriptDisplayItem::RemoveItem,
			ScriptReserved_DrawItemClearAll, &ScriptDisplayItem::ClearItems,
			ScriptReserved_IsNameInUse, &ScriptDisplayItem::IfItemExists,
			ScriptReserved_DrawItemIsObjectIDInUse, &ScriptDisplayItem::IfObjectIDExists,
			ScriptReserved_DrawItemSetAmbientLight, &ScriptDisplayItem::SetAmbientLight,
			ScriptReserved_DrawItemSetCamera, &ScriptDisplayItem::SetCameraPosition,
			ScriptReserved_DrawItemSetTarget, &ScriptDisplayItem::SetCameraTargetPosition,
			ScriptReserved_SetFOV, & ScriptDisplayItem::SetFOV,
			ScriptReserved_DrawItemResetCamera, &ScriptDisplayItem::ResetCamera,
			ScriptReserved_DrawItemGetAmbientLight, &ScriptDisplayItem::GetAmbientLight,
			ScriptReserved_DrawItemGetCamera, &ScriptDisplayItem::GetCameraPosition,
			ScriptReserved_DrawItemGetTarget, &ScriptDisplayItem::GetCameraTargetPosition,
			ScriptReserved_GetFOV, & ScriptDisplayItem::GetFOV);
	}

	/// Create a DisplayItem object.
	// @function DisplayItem
	// @tparam string name Lua name of the display item.
	// @tparam Objects.ObjID objectID Slot object ID.
	// @tparam[opt=Vec3(0&#44; 0&#44; 0)] Vec3 pos Position in 3D display space.
	// @tparam[opt=Rotation(0&#44; 0&#44; 0)] Rotation rot Rotation on the XYZ axes.
	// @tparam[opt=Vec3(1&#44; 1&#44; 1)] Vec3 scale Visual scale.
	// @tparam[opt] int meshBits Packed meshbits.
	// @treturn DisplayItem A new DisplayItem object.
	// @usage
	// local item = TEN.View.DisplayItem("My name", -- name
	//	TEN.Objects.ObjID.PISTOLS_ITEM, -- object ID) 

	ScriptDisplayItem::ScriptDisplayItem(const std::string& name, GAME_OBJECT_ID objectID, const Vec3& pos, const Rotation& rot, const Vec3& scale, int meshBits)
	{
		_name = name;

		auto convertedRot = rot.ToEulerAngles();
		g_DrawItems.AddItem(name, objectID, pos, convertedRot, scale, meshBits);
	}

	ScriptDisplayItem::ScriptDisplayItem(const std::string& name, GAME_OBJECT_ID objectID, const Vec3& pos, const Rotation& rot, const Vec3& scale)
	{
		_name = name;

		auto convertedRot = rot.ToEulerAngles();
		g_DrawItems.AddItem(name, objectID, pos, convertedRot, scale, ALL_JOINT_BITS);
	}

	ScriptDisplayItem::ScriptDisplayItem(const std::string& name, GAME_OBJECT_ID objectID, const Vec3& pos)
	{
		_name = name;

		auto rot = Rotation().ToEulerAngles();
		g_DrawItems.AddItem(name, objectID, pos, rot, Vector3::One, ALL_JOINT_BITS);
	}

	ScriptDisplayItem::ScriptDisplayItem(const std::string& name, GAME_OBJECT_ID objectID)
	{
		_name = name;
		g_DrawItems.AddItem(name, objectID, Vector3::Zero, EulerAngles::Identity, Vector3::One, ALL_JOINT_BITS);
	}

	ScriptDisplayItem::ScriptDisplayItem(const std::string& name)
	{
		if (!g_DrawItems.TestItemExists(name))
		{
			// Mark as invalid.
			_name.clear();
			return;
		}

		_name = name;
	}

	/// Get a DisplayItem by its name.
	// @function GetItemByName
	// @tparam string name Unique DisplayItem name.
	// @treturn DisplayItem DisplayItem reference.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	ScriptDisplayItem ScriptDisplayItem::GetItemByName(const std::string& name)
	{
		return ScriptDisplayItem(name);
	}

	/// Remove a DisplayItem by its name.
	// @function RemoveItem
	// @tparam string name Unique DisplayItem.
	// @usage
	// local item = TEN.View.DisplayItem.RemoveItem("My name")
	void ScriptDisplayItem::RemoveItem(const std::string& name)
	{
		if (!g_DrawItems.TestItemExists(name))
			return;

		g_DrawItems.RemoveItem(name);
	}

	/// Clear all DisplayItems.
	// @function ClearAllItems
	// @usage
	// TEN.View.DisplayItem.ClearAllItems()
	void ScriptDisplayItem::ClearItems()
	{
		g_DrawItems.Clear();
	}

	/// Check if a given name is used by a DisplayItem.
	// @function IsNameInUse
	// @tparam string name Name to check.
	// @treturn bool True if the name is used and a DisplayItem with a given name is present, false otherwise.
	// @usage
	// local test = TEN.View.DisplayItem.IsNameInUse("My name")
	// print(test)
	bool ScriptDisplayItem::IfItemExists(const std::string& name)
	{
		return g_DrawItems.TestItemExists(name);
	}

	/// Check if an object ID is in use by a DisplayItem. Only the first first matching DisplayItem will be checked and true will be returned immediately if found.
	// @function IsObjectIDInUse
	// @tparam Objects.ObjID Slot object ID to find.
	// @treturn bool True if ObjectID is used by a DisplayItem, false if not.
	// @usage
	// local isUsed = TEN.View.DisplayItem.IsObjectIDInUse(TEN.Objects.ObjID.PISTOLS_ITEM)
	// print(isUsed)
	bool ScriptDisplayItem::IfObjectIDExists(const GAME_OBJECT_ID objectID)
	{
		return g_DrawItems.TestObjectIDExists(objectID);
	}

	/// Set the ambient color for all DisplayItems.
	// @function SetAmbientLight
	// @tparam Color color New ambient color for all DisplayItems.
	// @usage
	// TEN.View.DisplayItem.SetAmbientLight(TEN.Color(128, 200, 255))
	void ScriptDisplayItem::SetAmbientLight(const ScriptColor& color)
	{
		g_DrawItems.SetAmbientLight(color);
	}

	/// Set the camera location. This single camera is used for all DisplayItems.
	// @function SetCameraPosition
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
	// @function SetTargetPosition
	// @tparam Vec3 pos New target camera position.
	// @bool[opt=false] disableInterpolation Disable interpolation to allow snap movements.
	// @usage
	// TEN.View.DisplayItem.SetTargetPosition(TEN.Vec3(0, 0, 1024))
	void ScriptDisplayItem::SetCameraTargetPosition(const Vec3& pos, TypeOrNil<bool> disableInterpolation)
	{
		auto convertedBool = ValueOr<bool>(disableInterpolation, false);
		g_DrawItems.SetCameraTargetPosition(pos, convertedBool);
	}

	/// Set the field of view for DisplayItems.
	// @function SetFOV
	// @tparam[opt=80] float fov Field of fiew angle in degrees (clamped to [10, 170]).
	// @bool[opt=false] disableInterpolation Disable interpolation to allow snap movements.
	// @usage
	// TEN.View.DisplayItem.SetFOV(80)
	void ScriptDisplayItem::SetFOV(TypeOrNil<float> fov, TypeOrNil<bool> disableInterpolation)
	{
		auto convertedFOV = ValueOr<float>(fov, 80.0f);
		auto convertedBool = ValueOr<bool>(disableInterpolation, false);
		auto clampedFov = ANGLE(std::clamp(abs(convertedFOV), 10.0f, 170.0f));
		g_DrawItems.SetFOV(clampedFov, convertedBool);
	}

	/// Get the ambient color of the display items.
	// @function GetAmbientLight
	// @treturn Color Ambient color.
	// @usage
	// local color = TEN.View.DisplayItem.GetAmbientLight()
	ScriptColor ScriptDisplayItem::GetAmbientLight()
	{
		return g_DrawItems.GetAmbientLight();
	}

	/// Get the camera position. This single camera is used for all DisplayItems.
	// @function GetCameraPosition
	// @treturn Vec3 Camera position for all DisplayItems.
	// @usage
	// local camPos = TEN.View.DisplayItem.GetCameraPosition()
	Vec3 ScriptDisplayItem::GetCameraPosition()
	{
		return g_DrawItems.GetCameraPosition();
	}

	/// Get the position of the camera target.
	// @function GetTargetPosition
	// @treturn Vec3 The camera target position for all of the DisplayItems..
	// @usage
	// local targetPosition = TEN.View.DisplayItem.GetTargetPosition()
	Vec3 ScriptDisplayItem::GetCameraTargetPosition()
	{
		return g_DrawItems.GetCameraTargetPosition();
	}

	/// Get field of view angle for DisplayItems.
	// @function GetFOV
	// @treturn float Current FOV angle in degrees.
	// @usage
	// local fieldOfView = TEN.View.DisplayItem.GetFOV()
	float ScriptDisplayItem::GetFOV()
	{
		return TO_DEGREES(g_DrawItems.GetFov());
	}

	/// Reset the camera position, camera target position, and field of view.
	// @function ResetCamera
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
	//
	// <h3>Quick Reference: Return Values</h3>
	// <style> table, th, td {border: 1px solid black;} .tableSP {border-collapse: collapse; width: 100%; text-align: center; } .tableSP th {background-color: #525252; color: white; padding: 12px;}</style>
	// <style> .tableSP td {padding: 6px;} .tableSP tr:nth-child(even) {background-color: #f2f2f2;} .tableSP tr:hover {background-color: #ddd;}</style>
	// <table class="tableSP">
	// <tr><th>Method</th><th>Returns on Success</th><th>Returns on Failure</th></tr>
	// <tr><td><a href="#DisplayItem:Exists">Exists</a></td><td>true/false</td><td>Never fails</td></tr>
	// <tr><td><a href="#DisplayItem:GetObjectID">GetObjectID</a></td><td>`Objects.ObjID`</td><td>nil</td></tr>
	// <tr><td><a href="#DisplayItem:GetPosition">GetPosition</a></td><td>`Vec3`</td><td>nil</td></tr>
	// <tr><td><a href="#DisplayItem:GetRotation">GetRotation</a></td><td>`Rotation`</td><td>nil</td></tr>
	// <tr><td><a href="#DisplayItem:GetScale">GetScale</a></td><td>number</td><td>nil</td></tr>
	// <tr><td><a href="#DisplayItem:GetColor">GetColor</a></td><td>`Color`</td><td>nil</td></tr>
	// <tr><td><a href="#DisplayItem:GetMeshVisible">GetMeshVisible</a></td><td>true/false</td><td>false</td></tr>
	// <tr><td><a href="#DisplayItem:GetJointRotation">GetJointRotation</a></td><td>`Rotation`</td><td>nil</td></tr>
	// <tr><td><a href="#DisplayItem:GetVisible">GetVisible</a></td><td>true/false</td><td>false</td></tr>
	// <tr><td><a href="#DisplayItem:GetAnim">GetAnim</a></td><td>number</td><td>nil</td></tr>
	// <tr><td><a href="#DisplayItem:GetFrame">GetFrame</a></td><td>number</td><td>nil</td></tr>
	// <tr><td><a href="#DisplayItem:GetEndFrame">GetEndFrame</a></td><td>number</td><td>nil</td></tr>
	// <tr><td><a href="#DisplayItem:GetBounds">GetBounds</a></td><td>{`Vec2`, `Vec2`}</td><td>nil</td></tr>
	// </table>
	//
	// <h3>Best Practices</h3>
	//
	// <b>1. Always Check Existence</b>
	// 
	// Before using a DisplayItem, always verify it exists to avoid nil errors:
	// 
	//	local item = TEN.View.DisplayItem.GetItemByName("myItem")
	//	if item:Exists() then
	//	    local pos = item:GetPosition()
	//	    if pos then  -- Double check for safety
	//	        print("Position:", pos.x, pos.y, pos.z)
	//	    end
	//	end
	//
	// <br><b>2. Handle nil Returns Gracefully</b>
	//
	// Methods that can fail return nil. Use one of the following patterns:
	// 
	//	local item = TEN.View.DisplayItem.GetItemByName("My name")
	//
	//	-- Pattern 1: if-check (recommended)
	//	local pos = item:GetPosition()
	//	if pos then
	//	    print("Position:", pos.x, pos.y, pos.z)
	//	end
	//
	//	-- Pattern 2: default value
	//	local pos = item:GetPosition()
	//	if not pos then
	//	    pos = TEN.Vec3(0, 0, 0)
	//	end
	//
	//	-- Pattern 3: early return (useful in functions)
	//	local function updateItem(name)
	//	    local item = TEN.View.DisplayItem.GetItemByName(name)
	//	    if not item:Exists() then return end
	//
	//	    local pos = item:GetPosition()
	//	    if not pos then return end
	//
	//	    -- Safe to use pos here
	//	    item:SetPosition(pos + TEN.Vec3(0, 10, 0))
	//	end
	//
	// <br><b>3. Common Mistakes</b>
	// Not checking if item exists:
	//	local pos = item:GetPosition()
	//	print(pos.x)  -- ERROR if the item doesn't exist.
	//
	// Always check first:
	//	if item:Exists() then
	//	    local pos = item:GetPosition()
	//	    if pos then print(pos.x) end
	//	end

	/// Remove the Display Item.
	// @function DisplayItem:Remove
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// item:Remove()
	void ScriptDisplayItem::Remove()
	{
		if (_name.empty())
			return;

		g_DrawItems.RemoveItem(_name);
		_name.clear();
	}

	/// Test if a Display Item exists.
	// @function DisplayItem:Exists
	// @treturn bool true if the Display Item exists, false otherwise.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("M name")
	// local test = item:Exists()
	// print(test)
	bool ScriptDisplayItem::Exists() const
	{
		return g_DrawItems.TestItemExists(_name);
	}

	/// Change the DisplayItem's object ID. 
	// @function DisplayItem:SetObjectID
	// @tparam Objects.ObjID objectID New slot object ID.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// item:SetObjectID(TEN.Objects.ObjID.BIGMEDI_ITEM)
	void ScriptDisplayItem::SetObjectID(GAME_OBJECT_ID objectID)
	{
		if (_name.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item != nullptr)
			item->SetObjectID(objectID);
	}

	/// Set the DisplayItem's position.
	// @function DisplayItem:SetPosition
	// @tparam Vec3 pos New position.
	// @bool[opt=false] disableInterpolation Disable interpolation to allow snap movements.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// item:SetPosition(TEN.Vec3(0, 200, 1024))
	void ScriptDisplayItem::SetPosition(const Vec3& pos, TypeOrNil<bool> disableInterpolation)
	{
		if (_name.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item != nullptr)
			item->SetPosition(pos, ValueOr<bool>(disableInterpolation, false));
	}

	/// Set the DisplayItem's rotation.
	// @function DisplayItem:SetRotation
	// @tparam Rotation rot New rotation.
	// @bool[opt=false] disableInterpolation Disable interpolation to allow snap movements.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// item:SetRotation(TEN.Rotation(0, 200, 1024))
	void ScriptDisplayItem::SetRotation(const Rotation& rot, TypeOrNil<bool> disableInterpolation)
	{
		if (_name.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item != nullptr)
			item->SetOrientation(rot.ToEulerAngles(), ValueOr<bool>(disableInterpolation, false));
	}

	/// Set the DisplayItem's scale.
	// @function DisplayItem:SetScale
	// @tparam float scale New scale.
	// @bool[opt=false] disableInterpolation Disable interpolation to allow snap movements.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// item:SetScale(2))
	void ScriptDisplayItem::SetScale(const Vec3& scale, TypeOrNil<bool> disableInterpolation)
	{
		if (_name.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item != nullptr)
			item->SetScale(scale, ValueOr<bool>(disableInterpolation, false));
	}

	/// Set the DisplayItem's color.
	// @function DisplayItem:SetColor
	// @tparam Color color New color.
	// @bool[opt=false] disableInterpolation Disable interpoaltion to allow snap color changes.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// item:SetColor(TEN.Color(128, 200, 255))
	void ScriptDisplayItem::SetColor(const ScriptColor& color, TypeOrNil<bool> disableInterpolation)
	{
		if (_name.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item != nullptr)
			item->SetColor(Color(color), ValueOr<bool>(disableInterpolation, false));
	}

	/// Set the packed MeshBits for the Display Item (for advanced users).
	// @function DisplayItem:SetMeshBits
	// @tparam int meshBits Packed MeshBits to be set.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// item:SetMeshBits(3)
	void ScriptDisplayItem::SetMeshBits(int meshBits)
	{
		if (_name.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item != nullptr)
			item->SetMeshBits(meshBits);
	}

	/// Make the specified mesh visible or invisible.
	// Used to show or hide a mesh in a DisplayItem.
	// @function DisplayItem:SetMeshVisible
	// @tparam int meshIndex Mesh index.
	// @tparam bool isVisible True to set visible, false to set invisible.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// item:SetMeshVisible(1, false)
	void ScriptDisplayItem::SetMeshVisibility(int meshIndex, bool isVisible)
	{
		if (_name.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item != nullptr)
			item->SetMeshVisibility(meshIndex, isVisible);
	}

	/// Set the DisplayItem's joint rotation.
	// @function DisplayItem:SetJointRotation
	// @tparam int meshIndex Joint index..
	// @tparam Rotation rot New rotation.
	// @bool[opt=false] disableInterpolation Disables interpolation to allow for snap movements.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// item:SetJointRotation(1, TEN.Rotation(0, 200, 0))
	void ScriptDisplayItem::SetMeshRotation(int meshIndex, Rotation rot, TypeOrNil<bool> disableInterpolation)
	{
		if (_name.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item != nullptr)
			item->SetMeshOrientation(meshIndex, rot.ToEulerAngles(), ValueOr<bool>(disableInterpolation, false));
	}

	/// Set the DisplayItems's visibility.
	// @bool isVisible True to set visible, false to set invisible.
	// @function DisplayItem:SetVisible
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// item:SetVisible(true)
	void ScriptDisplayItem::SetVisibility(bool isVisible)
	{
		if (_name.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item != nullptr)
			item->SetVisibility(isVisible);
	}

	/// Set the frame number from an animation.
	// This will set the specified animation to the given frame.
	// The number of frames in an animation can be seen under the heading "End frame" in
	// the WadTool animation editor.
	// @function DisplayItem:SetFrame
	// @tparam int animNumber Animation number to set.
	// @tparam int frameNumber Frame number to set.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// item:SetFrame(2, 10)
	void ScriptDisplayItem::SetFrame(int animNumber, int frameNumber)
	{
		if (_name.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item != nullptr)
		{
			item->SetAnimation(animNumber);
			item->SetFrame(frameNumber);
		}
	}

	/// Retrieve the object ID from a DisplayItem.
	// @function DisplayItem:GetObjectID
	// @treturn[1] Objects.ObjID Slot object ID.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// if item:Exists() then
	//    local objectID = item:GetObjectID()
	// end
	GAME_OBJECT_ID ScriptDisplayItem::GetObjectID() const
	{
		auto* item = g_DrawItems.GetItemByName(_name);
		if (item != nullptr)
			return item->GetObjectID();

		return ID_NO_OBJECT;
	}

	/// Get the DisplayItem's position.
	// @function DisplayItem:GetPosition
	// @treturn[1] Vec3 Position.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// if item:Exists() then
	//    local objectPosition = item:GetPosition()
	// end
	sol::optional<Vec3> ScriptDisplayItem::GetPosition() const
	{
		if (_name.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item == nullptr)
			return sol::nullopt;

		return Vec3(item->GetPosition());
	}

	/// Get the DisplayItem's rotation.
	// @function DisplayItem:GetRotation
	// @treturn[1] Rotation Rotation.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// if item:Exists() then
	//    local objectRotation = item:GetRotation()
	// end
	sol::optional<Rotation> ScriptDisplayItem::GetRotation() const
	{
		if (_name.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item == nullptr)
			return sol::nullopt;

		return Rotation(item->GetRotation());
	}

	/// Get the DisplayItem's visual scale.
	// @function DisplayItem:GetScale
	// @treturn[1] float Visual scale.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// if item:Exists() then
	//    local objectScale = item:GetScale()
	// end
	sol::optional<Vec3> ScriptDisplayItem::GetScale() const
	{
		if (_name.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item == nullptr)
			return sol::nullopt;

		return item->GetScale();
	}

	/// Get the DisplayItem's color.
	// @function DisplayItem:GetColor
	// @treturn[1] Color Color.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// local objectColor = item:GetColor()
	sol::optional<ScriptColor> ScriptDisplayItem::GetColor() const
	{
		if (_name.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item == nullptr)
			return sol::nullopt;

		return ScriptColor(item->GetColor());
	}

	/// Get the visibility state of a specified mesh in the DisplayItem.
	// @function DisplayItem:GetMeshVisible
	// @tparam int index Index of a mesh.
	// @treturn[1] bool Visibility status.
	// @treturn[2] bool False if the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// if item:Exists() then
	//    local test = item:GetMeshVisible(1)
	//    print(test)
	// end
	bool ScriptDisplayItem::GetMeshVisibility(int meshIndex) const
	{
		if (_name.empty())
			return false;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item == nullptr)
			return false;

		return item->IsMeshVisible(meshIndex);
	}

	/// Get the DisplayItem's joint rotation.
	// @function DisplayItem:GetJointRotation
	// @tparam int meshIndex Index of the joint to check.
	// @treturn[1] Rotation Joint rotation.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// if item:Exists() then
	//    local jointRotation = item:GetJointRotation(1)
	// end
	sol::optional<Rotation> ScriptDisplayItem::GetMeshRotation(int meshIndex) const
	{
		if (_name.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item == nullptr)
			return sol::nullopt;

		auto orient = item->GetMeshOrientation(meshIndex);
		return Rotation(orient);
	}

	/// Get the DisplayItem's visibility state.
	// @function DisplayItem:GetVisible
	// @treturn[1] bool Visibility state.
	// @treturn[2] bool False if the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// if item:Exists() then
	//    local test = item:GetVisible()
	//    print(test)
	// end
	bool ScriptDisplayItem::GetVisibility() const
	{
		if (_name.empty())
			return false;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item == nullptr)
			return false;

		return item->IsVisible();
	}

	/// Get the active animation number.
	// This corresponds to the number shown in the item's animation list in WadTool.
	// @function DisplayItem:GetAnim
	// @treturn[1] int Active animation number.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// if item:Exists() then
	//    local animNumber = item:GetAnim()
	// end
	sol::optional<int> ScriptDisplayItem::GetAnimNumber() const
	{
		if (_name.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item == nullptr)
			return sol::nullopt;

		return item->GetAnimNumber();
	}

	/// Get the current frame number of the active animation.
	// @function DisplayItem:GetFrame
	// @treturn[1] int Current frame number of the active animation.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// if item:Exists() then
	//    local frameNumber = item:GetFrame()
	// end
	sol::optional<int> ScriptDisplayItem::GetFrameNumber() const
	{
		if (_name.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item == nullptr)
			return sol::nullopt;

		return item->GetFrameNumber();
	}

	/// Get the end frame number of the DisplayItems's active animation.
	// This is the "End Frame" set in WADTool for the animation.
	// @function DisplayItem:GetEndFrame()
	// @treturn[1] int End frame number of the active animation.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// if item:Exists() then
	//    local endFrame = item:GetEndFrame()
	// end
	sol::optional<int> ScriptDisplayItem::GetEndFrame() const
	{
		if (_name.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item == nullptr)
			return sol::nullopt;
		
		return item->GetEndFrameNumber();
	}

	/// Get the projected 2D bounding box of the DisplayItem.
	// Projects the DisplayItem into display space and returns two Vec2 values.
	// @function DisplayItem:GetBounds
	// @treturn[1] Vec2 center Projected center position in display space percent.
	// @treturn[1] Vec2 size The projected width/height in display space percent.
	// @treturn[2] nil If the DisplayItem does not exist or has no bounds.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("My name")
	// if item:Exists() then
	//    local bounds = item:GetBounds()
	//	  if bounds then
	//        print("Center: ", bounds[1].x, bounds[1].y)
	//        print("Size: ", bounds[2].x, bounds[2].y)
	//      end
	// end
	sol::optional<std::pair<Vec2, Vec2>> ScriptDisplayItem::GetBounds() const
	{
		if (_name.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_name);
		if (item == nullptr)
			return sol::nullopt;

		auto bounds = item->GetBounds();
		if (!bounds.has_value())
			return sol::nullopt;

		float screenWidth = g_Configuration.ScreenWidth;
		float screnHeight = g_Configuration.ScreenHeight;

		const auto& center = bounds->first;
		const auto& size = bounds->second;

		// Convert to percent-based resolution.
		auto centerPercent = Vec2(center.x / screenWidth, center.y / screnHeight) * 100.0f;
		auto sizePercent = Vec2(size.x / screenWidth, size.y / screnHeight) * 100.0f;
		return std::pair<Vec2, Vec2>(centerPercent, sizePercent);
	}
}
