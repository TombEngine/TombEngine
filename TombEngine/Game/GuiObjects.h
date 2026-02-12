#pragma once
#include "Math/Math.h"
#include "Scripting/Internal/LanguageScript.h"

using namespace TEN::Math;

struct ItemInfo;

namespace TEN::Gui
{
	constexpr auto MAX_COMBINES = 60;

	enum InventoryObjectTypes
	{
		// Weapons and ammo types
		INV_OBJECT_PISTOLS,
		INV_OBJECT_PISTOLS_AMMO,
		INV_OBJECT_UZIS,
		INV_OBJECT_UZI_AMMO,
		INV_OBJECT_SHOTGUN,
		INV_OBJECT_SHOTGUN_AMMO_1,
		INV_OBJECT_SHOTGUN_AMMO_2,
		INV_OBJECT_REVOLVER,
		INV_OBJECT_REVOLVER_AMMO,
		INV_OBJECT_REVOLVER_LASER,
		INV_OBJECT_CROSSBOW,
		INV_OBJECT_CROSSBOW_LASER,
		INV_OBJECT_CROSSBOW_AMMO_1,
		INV_OBJECT_CROSSBOW_AMMO_2,
		INV_OBJECT_CROSSBOW_AMMO_3,
		INV_OBJECT_HK,
		INV_OBJECT_HK_LASERSIGHT,
		INV_OBJECT_HK_AMMO,
		INV_HK_MODE1,
		INV_HK_MODE2,
		INV_HK_MODE3,
		INV_OBJECT_GRENADE_LAUNCHER,
		INV_OBJECT_GRENADE_AMMO_1,
		INV_OBJECT_GRENADE_AMMO_2,
		INV_OBJECT_GRENADE_AMMO_3,
		INV_OBJECT_HARPOON_GUN,
		INV_OBJECT_HARPOON_AMMO,
		INV_OBJECT_ROCKET_LAUNCHER,
		INV_OBJECT_ROCKET_AMMO,

		// Misc. objects
		INV_OBJECT_LASERSIGHT,
		INV_OBJECT_SILENCER,
		INV_OBJECT_LARGE_MEDIPACK,
		INV_OBJECT_SMALL_MEDIPACK,
		INV_OBJECT_BINOCULARS,
		INV_OBJECT_FLARES,
		INV_OBJECT_STOPWATCH,
		INV_OBJECT_LOAD_FLOPPY,
		INV_OBJECT_SAVE_FLOPPY,
		INV_OBJECT_BURNING_TORCH,
		INV_OBJECT_CROWBAR,
		INV_OBJECT_DIARY,
		INV_OBJECT_COMPASS,
		INV_OBJECT_BEETLE,
		INV_OBJECT_BEETLE_PART1,
		INV_OBJECT_BEETLE_PART2,
		INV_OBJECT_SMALL_WATERSKIN_EMPTY,
		INV_OBJECT_SMALL_WATERSKIN_1L,
		INV_OBJECT_SMALL_WATERSKIN_2L,
		INV_OBJECT_SMALL_WATERSKIN_3L,
		INV_OBJECT_BIG_WATERSKIN_EMPTY,
		INV_OBJECT_BIG_WATERSKIN_1L,
		INV_OBJECT_BIG_WATERSKIN_2L,
		INV_OBJECT_BIG_WATERSKIN_3L,
		INV_OBJECT_BIG_WATERSKIN_4L,
		INV_OBJECT_BIG_WATERSKIN_5L,
		INV_OBJECT_OPEN_DIARY,

		// Puzzle, keys, pickups, examines
		INV_OBJECT_PUZZLE1,
		INV_OBJECT_PUZZLE2,
		INV_OBJECT_PUZZLE3,
		INV_OBJECT_PUZZLE4,
		INV_OBJECT_PUZZLE5,
		INV_OBJECT_PUZZLE6,
		INV_OBJECT_PUZZLE7,
		INV_OBJECT_PUZZLE8,
		INV_OBJECT_PUZZLE9,
		INV_OBJECT_PUZZLE10,
		INV_OBJECT_PUZZLE11,
		INV_OBJECT_PUZZLE12,
		INV_OBJECT_PUZZLE13,
		INV_OBJECT_PUZZLE14,
		INV_OBJECT_PUZZLE15,
		INV_OBJECT_PUZZLE16,

		INV_OBJECT_PUZZLE1_COMBO1,
		INV_OBJECT_PUZZLE1_COMBO2,
		INV_OBJECT_PUZZLE2_COMBO1,
		INV_OBJECT_PUZZLE2_COMBO2,
		INV_OBJECT_PUZZLE3_COMBO1,
		INV_OBJECT_PUZZLE3_COMBO2,
		INV_OBJECT_PUZZLE4_COMBO1,
		INV_OBJECT_PUZZLE4_COMBO2,
		INV_OBJECT_PUZZLE5_COMBO1,
		INV_OBJECT_PUZZLE5_COMBO2,
		INV_OBJECT_PUZZLE6_COMBO1,
		INV_OBJECT_PUZZLE6_COMBO2,
		INV_OBJECT_PUZZLE7_COMBO1,
		INV_OBJECT_PUZZLE7_COMBO2,
		INV_OBJECT_PUZZLE8_COMBO1,
		INV_OBJECT_PUZZLE8_COMBO2,
		INV_OBJECT_PUZZLE9_COMBO1,
		INV_OBJECT_PUZZLE9_COMBO2,
		INV_OBJECT_PUZZLE10_COMBO1,
		INV_OBJECT_PUZZLE10_COMBO2,
		INV_OBJECT_PUZZLE11_COMBO1,
		INV_OBJECT_PUZZLE11_COMBO2,
		INV_OBJECT_PUZZLE12_COMBO1,
		INV_OBJECT_PUZZLE12_COMBO2,
		INV_OBJECT_PUZZLE13_COMBO1,
		INV_OBJECT_PUZZLE13_COMBO2,
		INV_OBJECT_PUZZLE14_COMBO1,
		INV_OBJECT_PUZZLE14_COMBO2,
		INV_OBJECT_PUZZLE15_COMBO1,
		INV_OBJECT_PUZZLE15_COMBO2,
		INV_OBJECT_PUZZLE16_COMBO1,
		INV_OBJECT_PUZZLE16_COMBO2,

		INV_OBJECT_KEY1,
		INV_OBJECT_KEY2,
		INV_OBJECT_KEY3,
		INV_OBJECT_KEY4,
		INV_OBJECT_KEY5,
		INV_OBJECT_KEY6,
		INV_OBJECT_KEY7,
		INV_OBJECT_KEY8,
		INV_OBJECT_KEY9,
		INV_OBJECT_KEY10,
		INV_OBJECT_KEY11,
		INV_OBJECT_KEY12,
		INV_OBJECT_KEY13,
		INV_OBJECT_KEY14,
		INV_OBJECT_KEY15,
		INV_OBJECT_KEY16,

		INV_OBJECT_KEY1_COMBO1,
		INV_OBJECT_KEY1_COMBO2,
		INV_OBJECT_KEY2_COMBO1,
		INV_OBJECT_KEY2_COMBO2,
		INV_OBJECT_KEY3_COMBO1,
		INV_OBJECT_KEY3_COMBO2,
		INV_OBJECT_KEY4_COMBO1,
		INV_OBJECT_KEY4_COMBO2,
		INV_OBJECT_KEY5_COMBO1,
		INV_OBJECT_KEY5_COMBO2,
		INV_OBJECT_KEY6_COMBO1,
		INV_OBJECT_KEY6_COMBO2,
		INV_OBJECT_KEY7_COMBO1,
		INV_OBJECT_KEY7_COMBO2,
		INV_OBJECT_KEY8_COMBO1,
		INV_OBJECT_KEY8_COMBO2,
		INV_OBJECT_KEY9_COMBO1,
		INV_OBJECT_KEY9_COMBO2,
		INV_OBJECT_KEY10_COMBO1,
		INV_OBJECT_KEY10_COMBO2,
		INV_OBJECT_KEY11_COMBO1,
		INV_OBJECT_KEY11_COMBO2,
		INV_OBJECT_KEY12_COMBO1,
		INV_OBJECT_KEY12_COMBO2,
		INV_OBJECT_KEY13_COMBO1,
		INV_OBJECT_KEY13_COMBO2,
		INV_OBJECT_KEY14_COMBO1,
		INV_OBJECT_KEY14_COMBO2,
		INV_OBJECT_KEY15_COMBO1,
		INV_OBJECT_KEY15_COMBO2,
		INV_OBJECT_KEY16_COMBO1,
		INV_OBJECT_KEY16_COMBO2,

		INV_OBJECT_PICKUP1,
		INV_OBJECT_PICKUP2,
		INV_OBJECT_PICKUP3,
		INV_OBJECT_PICKUP4,
		INV_OBJECT_PICKUP5,
		INV_OBJECT_PICKUP6,
		INV_OBJECT_PICKUP7,
		INV_OBJECT_PICKUP8,
		INV_OBJECT_PICKUP9,
		INV_OBJECT_PICKUP10,
		INV_OBJECT_PICKUP11,
		INV_OBJECT_PICKUP12,
		INV_OBJECT_PICKUP13,
		INV_OBJECT_PICKUP14,
		INV_OBJECT_PICKUP15,
		INV_OBJECT_PICKUP16,

		INV_OBJECT_PICKUP1_COMBO1,
		INV_OBJECT_PICKUP1_COMBO2,
		INV_OBJECT_PICKUP2_COMBO1,
		INV_OBJECT_PICKUP2_COMBO2,
		INV_OBJECT_PICKUP3_COMBO1,
		INV_OBJECT_PICKUP3_COMBO2,
		INV_OBJECT_PICKUP4_COMBO1,
		INV_OBJECT_PICKUP4_COMBO2,
		INV_OBJECT_PICKUP5_COMBO1,
		INV_OBJECT_PICKUP5_COMBO2,
		INV_OBJECT_PICKUP6_COMBO1,
		INV_OBJECT_PICKUP6_COMBO2,
		INV_OBJECT_PICKUP7_COMBO1,
		INV_OBJECT_PICKUP7_COMBO2,
		INV_OBJECT_PICKUP8_COMBO1,
		INV_OBJECT_PICKUP8_COMBO2,
		INV_OBJECT_PICKUP9_COMBO1,
		INV_OBJECT_PICKUP9_COMBO2,
		INV_OBJECT_PICKUP10_COMBO1,
		INV_OBJECT_PICKUP10_COMBO2,
		INV_OBJECT_PICKUP11_COMBO1,
		INV_OBJECT_PICKUP11_COMBO2,
		INV_OBJECT_PICKUP12_COMBO1,
		INV_OBJECT_PICKUP12_COMBO2,
		INV_OBJECT_PICKUP13_COMBO1,
		INV_OBJECT_PICKUP13_COMBO2,
		INV_OBJECT_PICKUP14_COMBO1,
		INV_OBJECT_PICKUP14_COMBO2,
		INV_OBJECT_PICKUP15_COMBO1,
		INV_OBJECT_PICKUP15_COMBO2,
		INV_OBJECT_PICKUP16_COMBO1,
		INV_OBJECT_PICKUP16_COMBO2,

		INV_OBJECT_EXAMINE1,
		INV_OBJECT_EXAMINE2,
		INV_OBJECT_EXAMINE3,
		INV_OBJECT_EXAMINE4,
		INV_OBJECT_EXAMINE5,
		INV_OBJECT_EXAMINE6,
		INV_OBJECT_EXAMINE7,
		INV_OBJECT_EXAMINE8,

		INV_OBJECT_EXAMINE1_COMBO1,
		INV_OBJECT_EXAMINE1_COMBO2,
		INV_OBJECT_EXAMINE2_COMBO1,
		INV_OBJECT_EXAMINE2_COMBO2,
		INV_OBJECT_EXAMINE3_COMBO1,
		INV_OBJECT_EXAMINE3_COMBO2,
		INV_OBJECT_EXAMINE4_COMBO1,
		INV_OBJECT_EXAMINE4_COMBO2,
		INV_OBJECT_EXAMINE5_COMBO1,
		INV_OBJECT_EXAMINE5_COMBO2,
		INV_OBJECT_EXAMINE6_COMBO1,
		INV_OBJECT_EXAMINE6_COMBO2,
		INV_OBJECT_EXAMINE7_COMBO1,
		INV_OBJECT_EXAMINE7_COMBO2,
		INV_OBJECT_EXAMINE8_COMBO1,
		INV_OBJECT_EXAMINE8_COMBO2,

		INVENTORY_TABLE_SIZE
	};
	
	enum ItemOptions : uint64_t
	{
		OPT_ALWAYS_COMBINE		   = (1 << 0),
		OPT_EQUIP				   = (1 << 1),
		OPT_USE					   = (1 << 2),
		OPT_COMBINABLE			   = (1 << 3),
		OPT_SEPARABLE			   = (1 << 4),
		OPT_EXAMINABLE			   = (1 << 5),
		OPT_CHOOSE_AMMO_SHOTGUN	   = (1 << 6),
		OPT_CHOOSE_AMMO_CROSSBOW   = (1 << 7),
		OPT_CHOOSE_AMMO_GRENADEGUN = (1 << 8),
		OPT_CHOOSE_AMMO_UZI		   = (1 << 9),
		OPT_CHOOSE_AMMO_PISTOLS	   = (1 << 10),
		OPT_CHOOSE_AMMO_REVOLVER   = (1 << 11),
		OPT_LOAD				   = (1 << 12),
		OPT_SAVE				   = (1 << 13),
		OPT_CHOOSE_AMMO_HK		   = (1 << 14),
		OPT_STATS				   = (1 << 15),
		OPT_CHOOSE_AMMO_HARPOON	   = (1 << 16),
		OPT_CHOOSE_AMMO_ROCKET	   = (1 << 17)
	};

	enum RotationFlags
	{
		INV_ROT_X = (1 << 0),
		INV_ROT_Y = (1 << 1),
		INV_ROT_Z = (1 << 2)
	};

	enum BeetleComponentFlags
	{
		BEETLECOMP_FLAG_BEETLE	= (1 << 0),
		BEETLECOMP_FLAG_COMBO_1 = (1 << 1),
		BEETLECOMP_FLAG_COMBO_2 = (1 << 2)
	};

	struct InventoryObject
	{
		short ObjectNumber;
		short YOffset;
		float Scale1;
		EulerAngles Orientation;
		unsigned __int64 Options;
		const char* ObjectName;
		unsigned int MeshBits;
		short RotFlags;
	};

	struct CombineList
	{
		void  (*CombineRoutine)(ItemInfo* item, bool flag);
		short Item1;
		short Item2;
		short CombinedItem;
	};

	struct AmmoList
	{
		short		InventoryItem = 0;
		short		Amount		  = 0;
		EulerAngles Orientation	  = EulerAngles::Identity;
	};

	struct AmmoData
	{
		unsigned short AmountShotGunAmmo1;
		unsigned short AmountShotGunAmmo2;
		unsigned short AmountHKAmmo1;
		unsigned short AmountCrossBowAmmo1;
		unsigned short AmountCrossBowAmmo2;
		unsigned short AmountCrossBowAmmo3;
		unsigned short AmountGrenadeAmmo1;
		unsigned short AmountGrenadeAmmo2;
		unsigned short AmountGrenadeAmmo3;
		unsigned short AmountRocketsAmmo;
		unsigned short AmountHarpoonAmmo;
		unsigned short AmountUziAmmo;
		unsigned short AmountRevolverAmmo;
		unsigned short AmountPistolsAmmo;
		char CurrentPistolsAmmoType;
		char CurrentUziAmmoType;
		char CurrentRevolverAmmoType;
		char CurrentShotGunAmmoType;
		char CurrentHKAmmoType;
		char CurrentGrenadeGunAmmoType;
		char CurrentCrossBowAmmoType;
		char CurrentHarpoonAmmoType;
		char CurrentRocketAmmoType;
		char StashedCurrentPistolsAmmoType;
		char StashedCurrentUziAmmoType;
		char StashedCurrentRevolverAmmoType;
		char StashedCurrentShotGunAmmoType;
		char StashedCurrentGrenadeGunAmmoType;
		char StashedCurrentCrossBowAmmoType;
		char StashedCurrentSelectedOption;
		char StashedCurrentHKAmmoType;
		char StashedCurrentHarpoonAmmoType;
		char StashedCurrentRocketAmmoType;
	};

	extern InventoryObject InventoryObjectTable[];
	extern CombineList CombineTable[];

	void CombineRevolverLasersight(ItemInfo* item, bool flag);
	void CombineCrossbowLasersight(ItemInfo* item, bool flag);
	void CombineHKLasersight(ItemInfo* item, bool flag);
	void CombinePuzzleItem1(ItemInfo* item, bool flag);
	void CombinePuzzleItem2(ItemInfo* item, bool flag);
	void CombinePuzzleItem3(ItemInfo* item, bool flag);
	void CombinePuzzleItem4(ItemInfo* item, bool flag);
	void CombinePuzzleItem5(ItemInfo* item, bool flag);
	void CombinePuzzleItem6(ItemInfo* item, bool flag);
	void CombinePuzzleItem7(ItemInfo* item, bool flag);
	void CombinePuzzleItem8(ItemInfo* item, bool flag);
	void CombinePuzzleItem9(ItemInfo* item, bool flag);
	void CombinePuzzleItem10(ItemInfo* item, bool flag);
	void CombinePuzzleItem11(ItemInfo* item, bool flag);
	void CombinePuzzleItem12(ItemInfo* item, bool flag);
	void CombinePuzzleItem13(ItemInfo* item, bool flag);
	void CombinePuzzleItem14(ItemInfo* item, bool flag);
	void CombinePuzzleItem15(ItemInfo* item, bool flag);
	void CombinePuzzleItem16(ItemInfo* item, bool flag);
	void CombineKeyItem1(ItemInfo* item, bool flag);
	void CombineKeyItem2(ItemInfo* item, bool flag);
	void CombineKeyItem3(ItemInfo* item, bool flag);
	void CombineKeyItem4(ItemInfo* item, bool flag);
	void CombineKeyItem5(ItemInfo* item, bool flag);
	void CombineKeyItem6(ItemInfo* item, bool flag);
	void CombineKeyItem7(ItemInfo* item, bool flag);
	void CombineKeyItem8(ItemInfo* item, bool flag);
	void CombineKeyItem9(ItemInfo* item, bool flag);
	void CombineKeyItem10(ItemInfo* item, bool flag);
	void CombineKeyItem11(ItemInfo* item, bool flag);
	void CombineKeyItem12(ItemInfo* item, bool flag);
	void CombineKeyItem13(ItemInfo* item, bool flag);
	void CombineKeyItem14(ItemInfo* item, bool flag);
	void CombineKeyItem15(ItemInfo* item, bool flag);
	void CombineKeyItem16(ItemInfo* item, bool flag);
	void CombinePickupItem1(ItemInfo* item, bool flag);
	void CombinePickupItem2(ItemInfo* item, bool flag);
	void CombinePickupItem3(ItemInfo* item, bool flag);
	void CombinePickupItem4(ItemInfo* item, bool flag);
	void CombinePickupItem5(ItemInfo* item, bool flag);
	void CombinePickupItem6(ItemInfo* item, bool flag);
	void CombinePickupItem7(ItemInfo* item, bool flag);
	void CombinePickupItem8(ItemInfo* item, bool flag);
	void CombinePickupItem9(ItemInfo* item, bool flag);
	void CombinePickupItem10(ItemInfo* item, bool flag);
	void CombinePickupItem11(ItemInfo* item, bool flag);
	void CombinePickupItem12(ItemInfo* item, bool flag);
	void CombinePickupItem13(ItemInfo* item, bool flag);
	void CombinePickupItem14(ItemInfo* item, bool flag);
	void CombinePickupItem15(ItemInfo* item, bool flag);
	void CombinePickupItem16(ItemInfo* item, bool flag);
	void CombineExamine1(ItemInfo* item, bool flag);
	void CombineExamine2(ItemInfo* item, bool flag);
	void CombineExamine3(ItemInfo* item, bool flag);
	void CombineExamine4(ItemInfo* item, bool flag);
	void CombineExamine5(ItemInfo* item, bool flag);
	void CombineExamine6(ItemInfo* item, bool flag);
	void CombineExamine7(ItemInfo* item, bool flag);
	void CombineExamine8(ItemInfo* item, bool flag);
	void CombineClockWorkBeetle(ItemInfo* item, bool flag);
}
