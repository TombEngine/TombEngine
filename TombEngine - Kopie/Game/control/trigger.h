#pragma once

struct ItemInfo;
class FloorInfo;

#define TRIG_BITS(T) ((T & 0x3FFF) >> 10)

constexpr auto ONESHOT		 = 0x0100;
constexpr auto SWONESHOT	 = 0x0040;
constexpr auto ATONESHOT	 = 0x0080;
constexpr auto VALUE_BITS	 = 0x03FF;
constexpr auto TIMER_BITS	 = 0x00FF;
constexpr auto TRIGGER_BITS  = 0x003F;
constexpr auto FUNCTION_BITS = 0x001F;
constexpr auto CODE_BITS	 = 0x3E00;
constexpr auto END_BIT		 = 0x8000;
constexpr auto TRIGGERED	 = 0x0020;

enum TRIGGER_TYPES
{
	TRIGGER,
	PAD,
	SWITCH,
	KEY,
	PICKUP,
	HEAVY,
	ANTIPAD,
	COMBAT,
	DUMMY,
	ANTITRIGGER,
	HEAVYSWITCH,
	HEAVYANTITRIGGER,
	MONKEY,
	TIGHTROPE_T,
	CRAWLDUCK_T,
	CLIMB_T
};

enum TRIGOBJECTS_TYPES
{
	TO_OBJECT,
	TO_CAMERA,
	TO_SINK,
	TO_FLIPMAP,
	TO_FLIPON,
	TO_FLIPOFF,
	TO_TARGET,
	TO_FINISH,
	TO_CD,
	TO_FLIPEFFECT,
	TO_SECRET,
	TO_BODYBAG,
	TO_FLYBY,
	TO_CUTSCENE,
	TO_UNUSED1, // TRNG FmvNg
	TO_UNUSED2, // TRNG TimerfieldNg
	TO_VOLUMEEVENT,
	TO_GLOBALEVENT
};

extern int TriggerTimer;
extern int KeyTriggerActive;

bool GetKeyTrigger(ItemInfo* item);
int GetSwitchTrigger(ItemInfo* item, short* itemNumbersPtr, int attatchedToSwitch);
bool SwitchTrigger(short itemNumber, short timer);
int KeyTrigger(short itemNumber);
bool PickupTrigger(short itemNumber);
void RefreshCamera(short type, short* data);
int TriggerActive(ItemInfo* item);
short* GetTriggerIndex(FloorInfo* floor, int x, int y, int z);
short* GetTriggerIndex(ItemInfo* item);
void TestTriggers(int x, int y, int z, short roomNumber, bool heavy, int heavyFlags = 0);
void TestTriggers(ItemInfo* item, bool isHeavy, int heavyFlags = 0);
void ProcessSectorFlags(ItemInfo* item);

void Antitrigger(short const value, short const flags = 0);
void Trigger(short const value, short const flags = 0);
