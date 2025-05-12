#include "framework.h"
#include "Scripting/Internal/TEN/Flow/InventoryItem/InventoryItem.h"

#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptAssert.h"

/*** Represents the properties of an object as it appears in the inventory. To be used in @{Flow.Level.objects} list.
@tenprimitive Flow.InventoryItem
@pragma nostrip
*/

/*** Create an InventoryItem.
	@function InventoryItem
	@tparam string nameKey key for the item's (localised) name. Corresponds to an entry in strings.lua.
	@tparam Objects.ObjID objectID object ID of the inventory object to change
	@tparam float yOffset y-axis offset (positive values move the item down).<br />
A value of about 100 will cause the item to display directly below its usual position.
	@tparam float scale item size (1 being standard size).<br />
A value of 0.5 will cause the item to render at half the size,
and a value of 2 will cause the item to render at twice the size.
	@tparam Rotation rot rotation around x, y, and z axes.
	@tparam RotationAxis axis Axis to rotate around when the item is observed at in the inventory.<br />
Note that this is entirely separate from the `rot` field described above.
Must one of the following:
	X
	Y
	Z
e.g. `myItem.rotAxisWhenCurrent = RotationAxis.X`
	@tparam int meshBits _Not currently implemented_ (will have no effect regardless of what you set it to).
	@tparam ItemAction action is this usable, equippable, combineable or examinable?<br/>
Must be one of:
	EQUIP
	USE
	COMBINE
	EXAMINE
e.g. `myItem.action = ItemAction.EXAMINE`
	@treturn InventoryItem an InventoryItem.
*/
InventoryItem::InventoryItem(const std::string& name, GAME_OBJECT_ID objectID, float yOffset, float scale, const Rotation& rot, RotationFlags rotFlags, int meshBits, ItemOptions action) :
	Name(name),
	YOffset(yOffset),
	Scale(scale),
	Rot(rot),
	RotFlags(rotFlags),
	MeshBits(meshBits)
{
	ObjectID = (InventoryObjectTypes)g_Gui.ConvertObjectToInventoryItem(objectID);
	SetAction(action);
}

void InventoryItem::Register(sol::table& parent)
{
	using ctors = sol::constructors<InventoryItem(const std::string&, GAME_OBJECT_ID, float, float, const Rotation&, RotationFlags, int, ItemOptions)>;

	parent.new_usertype<InventoryItem>(
		ScriptReserved_InventoryItem,
		ctors(), sol::call_constructor, ctors());
}

// TODO: Add validation so the user can't choose something unimplemented.
void InventoryItem::SetAction(ItemOptions menuAction)
{
	bool isSupported = (menuAction & ItemOptions::OPT_EQUIP) != 0 ||
					   (menuAction & ItemOptions::OPT_USE) != 0 || 
					   (menuAction & ItemOptions::OPT_EXAMINABLE) != 0 || 
					   (menuAction & ItemOptions::OPT_COMBINABLE) != 0;

	if (!ScriptAssert(isSupported, "Unsupported item menu action: " + std::to_string(menuAction)))
	{
		auto itemOption = ItemOptions::OPT_USE;
		ScriptWarn("Defaulting to " + std::to_string(itemOption));
		MenuAction = itemOption;
	}
	else
	{
		MenuAction = menuAction;
	}
}
