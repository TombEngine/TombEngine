#pragma once
#include "Game/GuiObjects.h"
#include "Scripting/Internal/LanguageScript.h"
#include "Math/Math.h"
#include "Specific/clock.h"
#include "Specific/configuration.h"
#include "Specific/Input/InputAction.h"

struct ItemInfo;

using namespace TEN::Input;
using namespace TEN::Math;

namespace TEN::Gui
{
	enum class InventoryMode
	{
		None,
		InGame,
		Pause,
		Statistics,
		Examine,
		Load,
		Save
	};

	enum class InventoryResult
	{
		None,
		UseItem,
		NewGame,
		HomeLevel,
		LoadGame,
		SaveGame,
		ExitGame,
		ExitToTitle,
		NewGameSelectedLevel
	};

	enum class LoadResult
	{
		None,
		Load,
		Cancel
	};

	enum class MenuType
	{
		None,
		Use,
		ChooseAmmo,
		Combine,
		Seperate,
		Equip,
		Ammo1,
		Ammo2,
		Ammo3,
		Load,
		Save,
		Examine,
		Statistics
	};

	enum class RingTypes
	{
		Inventory,
		Ammo
	};

	enum class Menu
	{
		Title,
		Pause,
		Statistics,
		SelectLevel,
		LoadGame,
		Options,
		Display,
		GeneralActions,
		VehicleActions,
		QuickActions,
		MenuActions,
		OtherSettings
	};

	struct MenuOption
	{
		MenuType	Type = MenuType::None;
		std::string Text = {};
	};

	struct ObjectList
	{
		int			InventoryItem = 0;
		EulerAngles Orientation	  = EulerAngles::Identity;
		unsigned short Bright;
	};

	struct InventoryRing
	{
		ObjectList CurrentObjectList[INVENTORY_TABLE_SIZE + 1];
		bool RingActive;
		int ObjectListMovement;
		int CurrentObjectInList;
		int NumObjectsInList;
	};

	struct SettingsData
	{
		static constexpr auto NEW_KEY_WAIT_TIMEOUT = 3.0f * FPS;

		GameConfiguration Configuration = {};

		int	 SelectedScreenResolution = 0;
		bool IgnoreInput			  = false; // Ignore input until all actions are inactive.
		int	 NewKeyWaitTimer		  = 0;
	};

	class GuiController
	{
	private:
		// Input inquirers
		bool GuiIsPulsed(ActionID actionID) const;
		bool GuiIsSelected(bool onClicked = true) const;
		bool GuiIsDeselected() const;
		bool CanSelect() const;
		bool CanDeselect() const;

		// GUI variables
		Menu MenuToDisplay = Menu::Title;
		int SelectedOption;
		int OptionCount;
		int SelectedSaveSlot;

		int TimeInMenu = NO_VALUE;
		SettingsData CurrentSettings;

		// Inventory variables
		short CombineObject1;
		short CombineObject2;
		bool ItemUsed;
		char SeperateTypeFlag;
		char CombineTypeFlag;
		InventoryRing Rings[2];
		int CurrentSelectedOption;
		bool MenuActive;
		char AmmoSelectorFlag;
		char NumAmmoSlots;
		char* CurrentAmmoType;
		AmmoList AmmoObjectList[3];
		short AmmoSelectorFadeVal;
		short AmmoSelectorFadeDir;
		short CombineRingFadeVal;
		short CombineRingFadeDir;
		short NormalRingFadeVal;
		short NormalRingFadeDir;
		unsigned char AmmoActive;
		MenuOption CurrentOptions[3];
		InventoryMode InvMode;
		int InventoryItemChosen;
		int EnterInventory;
		int LastInvItem;
		AmmoData Ammo;

	public:
		int CompassNeedleAngle;

		void Initialize();
		bool CallPause();
		bool CallInventory(ItemInfo* item, bool resetMode);
		InventoryResult TitleOptions(ItemInfo* item);
		InventoryResult DoPauseMenu(ItemInfo* item);
		void DrawInventory();
		void DrawCurrentObjectList(ItemInfo* item, RingTypes ringType);
		int IsObjectInInventory(int objectNumber);
		int ConvertObjectToInventoryItem(int objectNumber);
		int ConvertInventoryItemToObject(int objectNumber);
		void FadeAmmoSelector();
		void DrawAmmoSelector();
		bool PerformWaterskinCombine(ItemInfo* item, bool flag);
		void DrawCompass(ItemInfo* item);
		void CancelInventorySelection();
		void UseItem(ItemInfo& item, int objectNumber);

		// Getters

		const InventoryRing& GetRing(RingTypes ringType);
		int GetSelectedOption();
		Menu GetMenuToDisplay();
		InventoryMode GetInventoryMode();
		int GetInventoryItemChosen();
		int GetEnterInventory();
		int GetLastInventoryItem();
		SettingsData& GetCurrentSettings();
		int GetLoadSaveSelection();
		int GetLoopedSelectedOption(int selectedOption, int optionCount, bool canLoop);

		// Setters

		void SetSelectedOption(int menu);
		void SetMenuToDisplay(Menu menu);
		void SetInventoryMode(InventoryMode mode);
		void SetEnterInventory(int number);
		void SetInventoryItemChosen(int number);
		void SetLastInventoryItem(int itemNumber);

	private:
		void HandleDisplaySettingsInput(bool fromPauseMenu);
		void HandleControlSettingsInput(ItemInfo* item, bool fromPauseMenu);
		void HandleOtherSettingsInput(bool fromPauseMenu);
		void HandleOptionsInput();
		void BackupOptions();
		bool DoObjectsCombine(int objectNumber1, int objectNumber2);
		void InitializeInventory(ItemInfo* item);
		void FillDisplayOptions();
		bool IsItemCurrentlyCombinable(int objectNumber);
		bool IsItemInInventory(int objectNumber);
		void CombineObjects(ItemInfo* item, int objectNumber1, int objectNumber2);
		void SetupObjectListStartPosition(int newObjectNumber);
		void SetupObjectListStartPosition2(int newObjectNumber);
		void HandleObjectChangeover(int ringIndex);
		void SetupAmmoSelector();
		void ConstructObjectList(ItemInfo* item);
		void SeparateObject(ItemInfo* item, int objectNumber);
		void InsertObjectIntoList(int objectNumber);
		void InsertObjectIntoList_v2(int objectNumber);
		void SpinBack(EulerAngles& orient);
		void UpdateWeaponStatus(ItemInfo* item);
		void DoStatisticsMode();
		void DoExamineMode();
		LoadResult DoLoad();
		bool DoSave();
		void DoInventory(ItemInfo* item);
		void ConstructCombineObjectList(ItemInfo* item);
	};

	extern GuiController g_Gui;
	extern std::vector<std::string> OptionStrings;
	extern std::vector<std::string> GeneralActionStrings;
	extern std::vector<std::string> VehicleActionStrings;
	extern std::vector<std::string> QuickActionStrings;
	extern std::vector<std::string> MenuActionStrings;
}
