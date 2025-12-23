#include "framework.h"
#include "Scripting/Internal/TEN/View/DisplayItem/ScriptDisplayItem.h"
#include "Game/Animation/animation.h"
#include "Game/Hud/DrawItems/DrawItems.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Specific/configuration.h"
#include "Game/effects/DisplaySprite.h"
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
			ScriptReserved_DrawItemResetCamera, &ScriptDisplayItem::ResetCamera,
			ScriptReserved_DrawItemGetAmbientLight, &ScriptDisplayItem::GetAmbientLight,
			ScriptReserved_DrawItemGetCamera, &ScriptDisplayItem::GetCameraPosition,
			ScriptReserved_DrawItemGetTarget, &ScriptDisplayItem::GetCameraTargetPosition
			);
	}

	/// Create a DisplayItem object.
	// @function DisplayItem
	// @tparam string itemName Lua name of the display item.
	// @tparam Objects.ObjID objectID ID of the object.
	// @tparam[opt=Vec3(0&#44; 0&#44; 0)] Vec3 position Position in 3d screen sapce.
	// @tparam[opt=Rotation(0&#44; 0&#44; 0)] Rotation rotation Rotation about x, y, and z axes.
	// @tparam[opt=Vec3(1&#44; 1&#44; 1)] Vec3 scale Set the visual scale.
	// @tparam[opt] int meshBits Packed meshbits.
	// @treturn DisplayItem A new DisplayItem object.
	// @usage
	// local item = TEN.View.DisplayItem("item1", -- name
	//	TEN.Objects.ObjID.PISTOLS_ITEM, -- object id) 

	ScriptDisplayItem::ScriptDisplayItem(const std::string& itemName, GAME_OBJECT_ID objectID, const Vec3& position, const Rotation& rotation, const Vec3& scale, int meshBits)
	{
		auto rot = rotation.ToEulerAngles();
		_itemName = itemName;
		g_DrawItems.AddItem(itemName, objectID, position, rot, scale, meshBits);
	}

	ScriptDisplayItem::ScriptDisplayItem(const std::string& itemName, GAME_OBJECT_ID objectID, const Vec3& position, const Rotation& rotation, const Vec3& scale)
	{
		auto rot = rotation.ToEulerAngles();
		_itemName = itemName;
		g_DrawItems.AddItem(itemName, objectID, position, rot, scale, ALL_JOINT_BITS);
	}

	ScriptDisplayItem::ScriptDisplayItem(const std::string& itemName, GAME_OBJECT_ID objectID, const Vec3& position)
	{
		auto rot = Rotation().ToEulerAngles();
		_itemName = itemName;
		g_DrawItems.AddItem(itemName, objectID, position, rot, Vector3::One, ALL_JOINT_BITS);
	}

	ScriptDisplayItem::ScriptDisplayItem(const std::string& itemName, GAME_OBJECT_ID objectID)
	{
		auto rot = Rotation().ToEulerAngles();
		_itemName = itemName;
		g_DrawItems.AddItem(itemName, objectID, Vector3(), rot, Vector3::One, ALL_JOINT_BITS);
	}

	ScriptDisplayItem::ScriptDisplayItem(const std::string& itemName)
	{
		if (!g_DrawItems.IfItemExists(itemName))
		{
			// Mark as invalid
			_itemName.clear();
			return;
		}

		_itemName = itemName;
	}

	/// Get a DisplayItem by its name.
	// @function GetItemByName
	// @tparam string name The unique name of the DisplayItem as set when creating it.
	// @treturn DisplayItem A DisplayItem referencing the item.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	ScriptDisplayItem ScriptDisplayItem::GetItemByName(const std::string& itemName)
	{
		return ScriptDisplayItem(itemName);
	}

	/// Removes a DisplayItem by its name.
	// @function RemoveItem
	// @tparam string name The unique name of the DisplayItem as set when creating it.
	// @usage
	// local item = TEN.View.DisplayItem.RemoveItem("item1")
	void ScriptDisplayItem::RemoveItem(const std::string& itemName)
	{
		if (!g_DrawItems.IfItemExists(itemName))
			return;

		g_DrawItems.RemoveItem(itemName);
	}

	/// Clears all DisplayItems.
	// @function ClearAllItems
	// @usage
	// TEN.View.DisplayItem.ClearAllItems()
	void ScriptDisplayItem::ClearItems()
	{
		g_DrawItems.Clear();
	}

	///Check if a given name is in use by a DisplayItem.
	// @function IsNameInUse
	// @tparam string name The name to check.
	// @treturn bool True if name is in use and a DisplayItem with a given name is present, false if not.
	// @usage
	// local test = TEN.View.DisplayItem.IsNameInUse("item1")
	// print(test)
	bool ScriptDisplayItem::IfItemExists(const std::string& itemName)
	{
		return g_DrawItems.IfItemExists(itemName);
	}

	///Check if a given ObjectID is in use by a DisplayItem. It will only check for the first matching DisplayItem and return true immediately once found.
	// @function IsObjectIDInUse
	// @tparam Objects.ObjID objectID A number representing the object ID to find.
	// @treturn bool True if ObjectID is in use by a DisplayItem, false if not.
	// @usage
	// local test = TEN.View.DisplayItem.IsObjectIDInUse(TEN.Objects.ObjID.PISTOLS_ITEM)
	// print(test)
	bool ScriptDisplayItem::IfObjectIDExists(const GAME_OBJECT_ID objectID)
	{
		return g_DrawItems.IfObjectIDExists(objectID);
	}

	//Camera functions

	/// Set the ambient color for all DisplayItems.
	// @function SetAmbientLight
	// @tparam Color color The new ambient color for all of the DisplayItem.
	// @usage
	// TEN.View.DisplayItem.SetAmbientLight(TEN.Color(128,200,255))
	void ScriptDisplayItem::SetAmbientLight(const ScriptColor& color)
	{
		g_DrawItems.SetAmbientLight(color);
	}

	/// Set the camera location. This single camera is used for all DisplayItems.
	// @function SetCameraPosition
	// @tparam Vec3 newPos The new position for the camera.
	// @bool[opt=false] disableInterpolation Disables interpolation to allow for snap movements.
	// @usage
	// TEN.View.DisplayItem.SetCameraPosition(TEN.Vec3(0,0,1024))
	void ScriptDisplayItem::SetCameraPosition(const Vec3& newPos, TypeOrNil<bool> disableInterpolation)
	{
		bool convertedBool = ValueOr<bool>(disableInterpolation, false);
		g_DrawItems.SetCameraPosition(newPos, convertedBool);
	}

	/// Set the camera target location.
	// @function SetTargetPosition
	// @tparam Vec3 newPos The new position for the camera target.
	// @bool[opt=false] disableInterpolation Disables interpolation to allow for snap movements.
	// @usage
	// TEN.View.DisplayItem.SetTargetPosition(TEN.Vec3(0,0,1024))
	void ScriptDisplayItem::SetCameraTargetPosition(const Vec3& newPos, TypeOrNil<bool> disableInterpolation)
	{
		bool convertedBool = ValueOr<bool>(disableInterpolation, false);
		g_DrawItems.SetCameraTargetPosition(newPos, convertedBool);
	}

	/// Get the DisplayItems' ambient color.
	// @function GetAmbientLight
	// @treturn Color DisplayItems' ambient color.
	// @usage
	// local color = TEN.View.DisplayItem.GetAmbientLight()
	ScriptColor ScriptDisplayItem::GetAmbientLight()
	{
		return g_DrawItems.GetAmbientLight();
	}

	///Get the position of the camera. This single camera is used for all DisplayItems.
	// @function GetCameraPosition
	// @treturn Vec3 The camera position for all of the DisplayItems.
	// @usage
	// local camPosition = TEN.View.DisplayItem.GetCameraPosition()
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

	/// Resets the position of the camera and camera target.
	// @function ResetCamera
	// @usage
	// local targetPosition = TEN.View.DisplayItem.ResetCamera()
	void ScriptDisplayItem::ResetCamera(TypeOrNil<bool> disableInterpolation)
	{
		bool convertedBool = ValueOr<bool>(disableInterpolation, false);
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
	// Methods that can fail return nil. Use one of these patterns:
	// 
	//	local item = TEN.View.DisplayItem.GetItemByName("item1")
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
	//	print(pos.x)  -- ERROR if item doesn't exist!
	//
	// Always check first:
	//	if item:Exists() then
	//	    local pos = item:GetPosition()
	//	    if pos then print(pos.x) end
	//	end

	/// Removes the Display Item.
	// @function DisplayItem:Remove
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// item:Remove()
	void ScriptDisplayItem::Remove()
	{
		if (_itemName.empty())
			return;

		g_DrawItems.RemoveItem(_itemName);
		_itemName.clear();
	}

	/// Test if the Display Item exists.
	// @function DisplayItem:Exists
	// @treturn bool true if the Display Item exists.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// local test = item:Exists()
	// print(test)
	bool ScriptDisplayItem::Exists() const
	{
		return g_DrawItems.IfItemExists(_itemName);
	}

	/// Change the DisplayItem's object ID. 
	// @function DisplayItem:SetObjectID
	// @tparam Objects.ObjID objectID The new ID.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// item:SetObjectID(TEN.Objects.ObjID.BIGMEDI_ITEM)
	void ScriptDisplayItem::SetObjectID(GAME_OBJECT_ID objectID)
	{
		if (_itemName.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_itemName);

		if (item)
			item->SetObjectID(objectID);
	}

	/// Set the DisplayItem's position.
	// @function DisplayItem:SetPosition
	// @tparam Vec3 position The new position of the Display Item.
	// @bool[opt=false] disableInterpolation Disables interpolation to allow for snap movements.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// item:SetPosition(TEN.Vec3(0,200,1024))
	void ScriptDisplayItem::SetPosition(const Vec3& newPos, TypeOrNil<bool> disableInterpolation)
	{
		if (_itemName.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_itemName);

		if (item)
			item->SetPosition(newPos, ValueOr<bool>(disableInterpolation, false));
	}

	/// Set the DisplayItem's rotation.
	// @function DisplayItem:SetRotation
	// @tparam Rotation rotation The DisplayItem's new rotation.
	// @bool[opt=false] disableInterpolation Disables interpolation to allow for snap movements.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// item:SetRotation(TEN.Rotation(0,200,1024))
	void ScriptDisplayItem::SetRotation(const Rotation& newRot, TypeOrNil<bool> disableInterpolation)
	{
		if (_itemName.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_itemName);

		if (item)
			item->SetRotation(newRot.ToEulerAngles(), ValueOr<bool>(disableInterpolation, false));
	}

	/// Set the DisplayItem's scale.
	// @function DisplayItem:SetScale
	// @tparam float scale New scale.
	// @bool[opt=false] disableInterpolation Disables interpolation to allow for snap movements.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// item:SetScale(2))
	void ScriptDisplayItem::SetScale(const Vec3& newScale, TypeOrNil<bool> disableInterpolation)
	{
		if (_itemName.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_itemName);

		if (item)
			item->SetScale(newScale, ValueOr<bool>(disableInterpolation, false));
	}

	/// Set the DisplayItem's color.
	// @function DisplayItem:SetColor
	// @tparam Color color The new color of the DisplayItem.
	// @bool[opt=false] disableInterpolation Disables interpoaltion to allow for snap color changes.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// item:SetColor(TEN.Color(128,200,255))
	void ScriptDisplayItem::SetColor(const ScriptColor& color, TypeOrNil<bool> disableInterpolation)
	{
		if (_itemName.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_itemName);

		if (item)
			item->SetColor(Color(color), ValueOr<bool>(disableInterpolation, false));
	}

	/// Set the packed MeshBits for the Display Item (for advanced users).
	// @function DisplayItem:SetMeshBits
	// @tparam int meshBits Packed MeshBits to be set.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// item:SetMeshBits(3)
	void ScriptDisplayItem::SetMeshBits(int meshBits)
	{
		if (_itemName.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_itemName);

		if (item)
			item->SetMeshBits(meshBits);
	}

	/// Makes specified mesh visible or invisible.
	// Use this to show or hide a specified mesh of a DisplayItem.
	// @function DisplayItem:SetMeshVisible
	// @tparam int meshIndex Index of a mesh.
	// @tparam bool visible true if you want the mesh to be visible, false otherwise.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// item:SetMeshVisible(1, false)
	void ScriptDisplayItem::SetMeshVisibility(int meshIndex, bool visible)
	{
		if (_itemName.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_itemName);

		if (item)
			item->SetMeshVisibility(meshIndex, visible);
	}

	/// Set the DisplayItem's joint rotation.
	// @function DisplayItem:SetJointRotation
	// @tparam int meshIndex Index of a joint to set rotation.
	// @tparam Rotation rotation The DisplayItem's new rotation.
	// @bool[opt=false] disableInterpolation Disables interpolation to allow for snap movements.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// item:SetJointRotation(1, TEN.Rotation(0,200,0))
	void ScriptDisplayItem::SetMeshRotation(int meshIndex, Rotation rotation, TypeOrNil<bool> disableInterpolation)
	{
		if (_itemName.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_itemName);

		if (item)
			item->SetMeshRotation(meshIndex, rotation.ToEulerAngles(), ValueOr<bool>(disableInterpolation, false));
	}

	/// Set the DisplayItems's visibility.
	// @bool visible true if the item should become visible, false if it should become invisible.
	// @function DisplayItem:SetVisible
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// item:SetVisible(true)
	void ScriptDisplayItem::SetVisibility(bool visible)
	{
		if (_itemName.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_itemName);

		if (item)
			item->SetVisibility(visible);
	}

	/// Set frame number from an animation.
	// This will set the specified animation to the given frame.
	// The number of frames in an animation can be seen under the heading "End frame" in
	// the WadTool animation editor.
	// @function DisplayItem:SetFrame
	// @tparam int animIndex The index of the desired animation.
	// @tparam int frame The new frame number.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// item:SetFrame(2, 10)
	void ScriptDisplayItem::SetFrame(int animIndex, int frame)
	{
		if (_itemName.empty())
			return;

		auto* item = g_DrawItems.GetItemByName(_itemName);

		if (item)
		{
			item->SetAnimation(animIndex);
			item->SetFrame(frame);
		}
	}

	/// Retrieve the object ID from a DisplayItem.
	// @function DisplayItem:GetObjectID
	// @treturn[1] Objects.ObjID A number representing the object ID of the DisplayItem.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// if item:Exists() then
	//    local objectID = item:GetObjectID()
	// end
	GAME_OBJECT_ID ScriptDisplayItem::GetObjectID() const
	{
		auto* item = g_DrawItems.GetItemByName(_itemName);

		if (item)
			return item->GetObjectID();

		return ID_NO_OBJECT;
	}
	/// Get the DisplayItem's position.
	// @function DisplayItem:GetPosition
	// @treturn[1] Vec3 DisplayItem's position.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// if item:Exists() then
	//    local objectPosition = item:GetPosition()
	// end
	sol::optional <Vec3> ScriptDisplayItem::GetPosition() const
	{
		if (_itemName.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_itemName);
		if (!item)
			return sol::nullopt;

		return Vec3(item->GetPosition());
	}

	/// Get the DisplayItem's rotation.
	// @function DisplayItem:GetRotation
	// @treturn[1] Rotation DisplayItem's rotation.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// if item:Exists() then
	//    local objectRotation = item:GetRotation()
	// end
	sol::optional <Rotation> ScriptDisplayItem::GetRotation() const
	{
		if (_itemName.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_itemName);
		if (!item)
			return sol::nullopt;

		return Rotation(item->GetRotation());
	}

	/// Get the DisplayItem's visual scale.
	// @function DisplayItem:GetScale
	// @treturn[1] float DisplayItem's visual scale.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// if item:Exists() then
	//    local objectScale = item:GetScale()
	// end
	sol::optional <Vec3> ScriptDisplayItem::GetScale() const
	{
		if (_itemName.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_itemName);
		if (!item)
			return sol::nullopt;

		return item->GetScale();
	}

	/// Get the DisplayItem's color.
	// @function DisplayItem:GetColor
	// @treturn[1] Color DisplayItem's color.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// local objectColor = item:GetColor()
	sol::optional <ScriptColor> ScriptDisplayItem::GetColor() const
	{
		if (_itemName.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_itemName);
		if (!item)
			return sol::nullopt;

		return ScriptColor(item->GetColor());
	}

	///Get visibility state of a specified mesh of a DisplayItem.
	// Returns true if specified mesh is visible on a DisplayItem, and false
	// if it is not visible.
	// @function DisplayItem:GetMeshVisible
	// @tparam int index Index of a mesh.
	// @treturn[1] bool Visibility status.
	// @treturn[2] bool False if the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// if item:Exists() then
	//    local test = item:GetMeshVisible(1)
	//    print(test)
	// end
	bool ScriptDisplayItem::GetMeshVisibility(int meshIndex) const
	{
		if (_itemName.empty())
			return false;

		auto* item = g_DrawItems.GetItemByName(_itemName);
		if (!item)
			return false;

		return item->GetMeshVisibility(meshIndex);
	}

	/// Get the DisplayItem's joint rotation.
	// @function DisplayItem:GetJointRotation
	// @tparam int meshIndex Index of a joint to get rotation.
	// @treturn[1] Rotation DisplayItem's joint rotation.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// if item:Exists() then
	//    local jointRotation = item:GetJointRotation(1)
	// end
	sol::optional <Rotation> ScriptDisplayItem::GetMeshRotation(int meshIndex) const
	{
		if (_itemName.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_itemName);
		if (!item)
			return sol::nullopt;

		auto rotation = item->GetMeshRotation(meshIndex);
		return Rotation(rotation);
	}

	/// Get the DisplayItem's visibility state.
	// @function DisplayItem:GetVisible
	// @treturn[1] bool Item's visibility state.
	// @treturn[2] bool False if the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// if item:Exists() then
	//    local test = item:GetVisible()
	//    print(test)
	// end
	bool ScriptDisplayItem::GetVisibility() const
	{
		if (_itemName.empty())
			return false;

		auto* item = g_DrawItems.GetItemByName(_itemName);
		if (!item)
			return false;

		return item->GetVisibility();
	}

	///Retrieve the index of the current animation.
	// This corresponds to the number shown in the item's animation list in WadTool.
	// @function DisplayItem:GetAnim
	// @treturn[1] int The index of the active animation.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// if item:Exists() then
	//    local animIndex = item:GetAnim()
	// end
	sol::optional <int> ScriptDisplayItem::GetAnimNumber() const
	{
		if (_itemName.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_itemName);
		if (!item)
			return sol::nullopt;

		return item->GetAnimation();
	}

	/// Retrieve frame number.
	// This is the current frame of the DisplayItems's active animation.
	// @function DisplayItem:GetFrame
	// @treturn[1] int The current frame of the active animation.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// if item:Exists() then
	//    local frameNumber = item:GetFrame()
	// end
	sol::optional <int> ScriptDisplayItem::GetFrameNumber() const
	{
		if (_itemName.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_itemName);
		if (!item)
			return sol::nullopt;

		return item->GetFrame();
	}

	///Get the end frame number of the DisplayItems's active animation.
	// This is the "End Frame" set in WADTool for the animation.
	// @function DisplayItem:GetEndFrame()
	// @treturn[1] int End frame number of the active animation.
	// @treturn[2] nil If the DisplayItem does not exist.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// if item:Exists() then
	//    local endFrame = item:GetEndFrame()
	// end
	sol::optional <int> ScriptDisplayItem::GetEndFrame() const
	{
		if (_itemName.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_itemName);
		if (!item)
			return sol::nullopt;
		
		return item->GetEndFrame();
	}

	///Get the 2D projected bounding box of this DisplayItem.
	// This function projects the DisplayItem into screen space and returns two Vec2 values:
	// @function DisplayItem:GetBounds
	// @treturn[1] Vec2 center The projected center position(percent of screen space).
	// @treturn[1] Vec2 size The projected width / height (percent of screen space).
	// @treturn[2] nil If the DisplayItem does not exist or has no bounds.
	// @usage
	// local item = TEN.View.DisplayItem.GetItemByName("item1")
	// if item:Exists() then
	//    local bounds = item:GetBounds()
	//	  if bounds then
	//        print("Center: ", bounds[1].x, bounds[1].y)
	//        print("Size: ", bounds[2].x, bounds[2].y)
	//      end
	// end
	sol::optional <std::pair<Vec2, Vec2>> ScriptDisplayItem::GetBounds() const
	{
		if (_itemName.empty())
			return sol::nullopt;

		auto* item = g_DrawItems.GetItemByName(_itemName);
		if (!item)
			return sol::nullopt;

		auto bounds = item->GetBounds();
		if (!bounds.has_value())
			return sol::nullopt;

		const float fWidth = g_Configuration.ScreenWidth;
		const float fHeight = g_Configuration.ScreenHeight;

		const Vector2& center = bounds->first;
		const Vector2& size = bounds->second;

		// Convert to percent-based resolution
		Vec2 centerPercent(center.x / fWidth * 100.0f,
			center.y / fHeight * 100.0f);

		Vec2 sizePercent(size.x / fWidth * 100.0f,
			size.y / fHeight * 100.0f);

		return std::pair<Vec2, Vec2>(centerPercent, sizePercent);
	}
}
