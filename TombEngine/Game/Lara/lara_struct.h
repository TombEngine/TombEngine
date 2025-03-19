#pragma once
#include "Game/Lara/PlayerContext.h"
#include "Math/Math.h"
#include "Objects/game_object_ids.h"
#include "Objects/objectslist.h"

using namespace TEN::Math;

struct CreatureInfo;
struct FX_INFO;
struct ItemInfo;

using namespace TEN::Entities::Player;

// Inventory object constants
constexpr int NUM_PUZZLES		  = ID_PUZZLE_ITEM16 - ID_PUZZLE_ITEM1 + 1;
constexpr int NUM_PUZZLE_PIECES   = ID_PUZZLE_ITEM16_COMBO2 - ID_PUZZLE_ITEM1_COMBO1 + 1;
constexpr int NUM_KEYS			  = ID_KEY_ITEM16 - ID_KEY_ITEM1 + 1;
constexpr int NUM_KEY_PIECES	  = ID_KEY_ITEM16_COMBO2 - ID_KEY_ITEM1_COMBO1 + 1;
constexpr int NUM_PICKUPS		  = ID_PICKUP_ITEM16 - ID_PICKUP_ITEM1 + 1;
constexpr int NUM_PICKUPS_PIECES  = ID_PICKUP_ITEM16_COMBO2 - ID_PICKUP_ITEM1_COMBO1 + 1;
constexpr int NUM_EXAMINES		  = ID_EXAMINE8 - ID_EXAMINE1 + 1;
constexpr int NUM_EXAMINES_PIECES = ID_EXAMINE8_COMBO2 - ID_EXAMINE1_COMBO1 + 1;

#pragma region state_and_animation
enum LaraState
{
	// TR1
	LS_WALK_FORWARD = 0,
	LS_RUN_FORWARD = 1,
	LS_IDLE = 2,
	LS_JUMP_FORWARD = 3,
	LS_POSE = 4,
	LS_RUN_BACK = 5,
	LS_TURN_RIGHT_SLOW = 6,
	LS_TURN_LEFT_SLOW = 7,
	LS_DEATH = 8,
	LS_FREEFALL = 9,
	LS_HANG = 10,
	LS_REACH = 11,
	LS_SPLAT = 12,
	LS_UNDERWATER_IDLE = 13,
	LS_GRAB_TO_FALL = 14,
	LS_JUMP_PREPARE = 15,
	LS_WALK_BACK = 16,
	LS_UNDERWATER_SWIM_FORWARD = 17,
	LS_UNDERWATER_INERTIA = 18,
	LS_GRABBING = 19,
	LS_TURN_RIGHT_FAST = 20,
	LS_STEP_RIGHT = 21,
	LS_STEP_LEFT = 22,
	LS_ROLL_180_BACKWARD = 23,
	LS_SLIDE_FORWARD = 24,
	LS_JUMP_BACK = 25,
	LS_JUMP_RIGHT = 26,
	LS_JUMP_LEFT = 27,
	LS_JUMP_UP = 28,
	LS_FALL_BACK = 29,
	LS_SHIMMY_LEFT = 30,
	LS_SHIMMY_RIGHT = 31,
	LS_SLIDE_BACK = 32,
	LS_ONWATER_IDLE = 33,
	LS_ONWATER_FORWARD = 34,
	LS_ONWATER_DIVE = 35,
	LS_PUSHABLE_PUSH = 36,
	LS_PUSHABLE_PULL = 37,
	LS_PUSHABLE_GRAB = 38,
	LS_PICKUP = 39,
	LS_SWITCH_DOWN = 40,
	LS_SWITCH_UP = 41,
	LS_INSERT_KEY = 42,
	LS_INSERT_PUZZLE = 43,
	LS_WATER_DEATH = 44,
	LS_ROLL_180_FORWARD = 45,
	LS_BOULDER_DEATH = 46,
	LS_ONWATER_BACK = 47,
	LS_ONWATER_LEFT = 48,
	LS_ONWATER_RIGHT = 49,
	LS_USE_MIDAS = 50,
	LS_MIDAS_DEATH = 51,
	LS_SWAN_DIVE = 52,
	LS_FREEFALL_DIVE = 53,
	LS_HANDSTAND = 54,
	LS_ONWATER_EXIT = 55,

	// TR2
	LS_LADDER_IDLE = 56,
	LS_LADDER_UP = 57,
	LS_LADDER_LEFT = 58,
	LS_LADDER_STOP = 59,
	LS_LADDER_RIGHT = 60,
	LS_LADDER_DOWN = 61,
	LS_AUTO_JUMP = 62,
	LS_TEST_2 = 63,
	LS_TEST_3 = 64,
	LS_WADE_FORWARD = 65,
	LS_UNDERWATER_ROLL = 66,
	LS_PICKUP_FLARE = 67,
	LS_JUMP_ROLL_180 = 68,
	LS_KICK = 69,
	LS_ZIP_LINE = 70,

	// TR3
	LS_CROUCH_IDLE = 71,
	LS_CROUCH_ROLL = 72,
	LS_SPRINT = 73,
	LS_SPRINT_DIVE = 74,
	LS_MONKEY_IDLE = 75,
	LS_MONKEY_FORWARD = 76,
	LS_MONKEY_SHIMMY_LEFT = 77,
	LS_MONKEY_SHIMMY_RIGHT = 78,
	LS_MONKEY_TURN_180 = 79,
	LS_CRAWL_IDLE = 80,
	LS_CRAWL_FORWARD = 81,
	LS_MONKEY_TURN_LEFT = 82,
	LS_MONKEY_TURN_RIGHT = 83,
	LS_CRAWL_TURN_LEFT = 84,
	LS_CRAWL_TURN_RIGHT = 85,
	LS_CRAWL_BACK = 86,
	LS_HANG_TO_CRAWL = 87,
	LS_CRAWL_TO_HANG = 88,
	LS_MISC_CONTROL = 89,

	// TR4
	LS_ROPE_TURN_CLOCKWISE = 90,
	LS_ROPE_TURN_COUNTER_CLOCKWISE = 91,
	LS_GIANT_BUTTON_PUSH = 92,
	LS_TRAPDOOR_FLOOR_OPEN = 93,
	LS_FREEFALL_BIS = 94,
	LS_ROUND_HANDLE = 95,
	LS_COGWHEEL = 96,
	LS_LEVERSWITCH_PUSH = 97,
	LS_HOLE = 98,
	LS_POLE_IDLE = 99,
	LS_POLE_UP = 100,
	LS_POLE_DOWN = 101,
	LS_POLE_TURN_CLOCKWISE = 102,
	LS_POLE_TURN_COUNTER_CLOCKWISE = 103,
	LS_PULLEY = 104,
	LS_CROUCH_TURN_LEFT = 105,
	LS_CROUCH_TURN_RIGHT = 106,
	LS_SHIMMY_OUTER_LEFT = 107,
	LS_SHIMMY_OUTER_RIGHT = 108,
	LS_SHIMMY_INNER_LEFT = 109,
	LS_SHIMMY_INNER_RIGHT = 110,
	LS_ROPE_IDLE = 111,
	LS_ROPE_UP = 112,
	LS_ROPE_DOWN = 113,
	LS_ROPE_SWING = 114,
	LS_ROPE_UNKNOWN = 115,
	LS_CORRECT_POSITION = 116,
	LS_DOUBLEDOOR_PUSH = 117,
	LS_FLY_CHEAT = 118,

	// TR5
	LS_TIGHTROPE_IDLE = 119,
	LS_TIGHTROPE_TURN_180 = 120,
	LS_TIGHTROPE_WALK = 121,
	LS_TIGHTROPE_UNBALANCE_LEFT = 122,
	LS_TIGHTROPE_UNBALANCE_RIGHT = 123,
	LS_TIGHTROPE_ENTER = 124,
	LS_TIGHTROPE_DISMOUNT = 125,
	LS_DOVE_SWITCH = 126,
	LS_TIGHTROPE_RECOVER_BALANCE = 127,
	LS_HORIZONTAL_BAR_SWING = 128,
	LS_HORIZONTAL_BAR_LEAP = 129,
	LS_UNKNOWN_1 = 130,
	LS_RADIO_LISTENING = 131,
	LS_RADIO_OFF = 132,
	LS_UNKNOWN_2 = 133,
	LS_UNKNOWN_3 = 134,
	LS_UNKNOWN_4 = 135,
	LS_UNKNOWN_5 = 136,
	LS_PICKUP_FROM_CHEST = 137,
	LS_LADDER_TO_CROUCH = 138,

	// TombEngine
	LS_SHIMMY_45_OUTER_LEFT = 139,
	LS_SHIMMY_45_OUTER_RIGHT = 140,
	LS_SHIMMY_45_INNER_LEFT = 141,
	LS_SHIMMY_45_INNER_RIGHT = 142,
	LS_SLOPE_CLIMB_IDLE = 143,
	LS_SLOPE_CLIMB_UP = 144,
	LS_SLOPE_CLIMB_DOWN = 145,
	LS_COGWHEEL_UNGRAB = 146,
	LS_STEP_UP = 147,
	LS_STEP_DOWN = 148,
	LS_SLOPE_CLIMB_FALL = 149,
	LS_LADDER_DISMOUNT_LEFT = 150,
	LS_LADDER_DISMOUNT_RIGHT = 151,
	LS_TURN_LEFT_FAST = 152,
	LS_CRAWL_EXIT_STEP_DOWN = 153,
	LS_CRAWL_EXIT_JUMP = 154,
	LS_CRAWL_EXIT_FLIP = 155,
	LS_SLOPE_CLIMB_HANG = 156,
	LS_SLOPE_CLIMB_SHIMMY = 157,
	LS_SLOPE_CLIMB_START = 158,
	LS_SLOPE_CLIMB_STOP = 159,
	LS_SLOPE_CLIMB_END = 160,
	LS_CRAWL_STEP_UP = 161,
	LS_CRAWL_STEP_DOWN = 162,
	LS_MONKEY_BACK = 163,
	LS_VAULT = 164,
	LS_VAULT_2_STEPS = 165,
	LS_VAULT_3_STEPS = 166,
	LS_VAULT_1_STEP_CROUCH = 167,
	LS_VAULT_2_STEPS_CROUCH = 168,
	LS_VAULT_3_STEPS_CROUCH = 169,
	LS_SOFT_SPLAT = 170,
	LS_CROUCH_TURN_180 = 171,
	LS_CRAWL_TURN_180 = 172,
	LS_TURN_180 = 173,

	// 174-188 reserved for ladder object. -- Sezz 2023.04.16

	LS_REMOVE_PUZZLE = 189,
	LS_PUSHABLE_EDGE_SLIP = 190,
	LS_SPRINT_SLIDE = 191,
	LS_TREAD_WATER_VAULT_1_STEP_DOWN_TO_STAND = 192,
	LS_TREAD_WATER_VAULT_0_STEPS_TO_STAND = 193,
	LS_TREAD_WATER_VAULT_1_STEP_UP_TO_STAND = 194,
	LS_TREAD_WATER_VAULT_1_STEP_DOWN_TO_CROUCH = 195,
	LS_TREAD_WATER_VAULT_0_STEPS_TO_CROUCH = 196,
	LS_TREAD_WATER_VAULT_1_STEP_UP_TO_CROUCH = 197,

	NUM_LARA_STATES
};

enum LaraAnim
{
	// TR1
	LA_RUN = 0,												// Run forward (looped)
	LA_WALK = 1,											// Walk forward (looped)
	LA_WALK_TO_STAND_RIGHT = 2,								// Walk > stand, right foot first
	LA_WALK_TO_STAND_LEFT = 3,								// Walk > stand, left foot first
	LA_WALK_TO_RUN_RIGHT = 4,								// Walk > run, right foot first
	LA_WALK_TO_RUN_LEFT = 5,								// Walk > run, left foot first
	LA_STAND_TO_RUN = 6,									// Stand > run
	LA_RUN_TO_WALK_RIGHT = 7,								// Run > walk, right foot first
	LA_RUN_TO_STAND_LEFT = 8,								// Run > stand, left foot first
	LA_RUN_TO_WALK_LEFT = 9,								// Run > walk, right foot first
	LA_RUN_TO_STAND_RIGHT = 10,								// Run > stand, right foot first
	LA_STAND_SOLID = 11,									// Stand solid
																// TODO: gradually reduce reliance on this anim for erroneous collisions.
	LA_TURN_RIGHT_SLOW = 12,								// Rotate right slowly
	LA_TURN_LEFT_SLOW = 13,									// Rotate left slowly
	LA_JUMP_FORWARD_LAND_START_UNUSED = 14,					// Forward jump > land (1/2)
	LA_JUMP_FORWARD_LAND_END_UNUSED = 15,					// Forward jump > land (2/2)
	LA_RUN_TO_JUMP_FORWARD_RIGHT_START = 16,				// Run > take off for forward jump, right foot first (1/2)
	LA_RUN_TO_JUMP_FORWARD_RIGHT_END = 17,					// Run > take off for forward jump, right foot first (2/2)
	LA_RUN_TO_JUMP_FORWARD_LEFT_START = 18,					// Run > take off for forward jump, left foot first (1/2)
	LA_RUN_TO_JUMP_FORWARD_LEFT_END = 19,					// Run > take off for forward jump, left foot first (2/2)
	LA_STAND_TO_WALK_START = 20,							// Stand > walk forward (1/2)
	LA_STAND_TO_WALK_END = 21,								// Stand > walk forward (1/2)
	LA_JUMP_FORWARD_TO_FREEFALL_UNUSED = 22,				// Jump > fall (possibly unused?)
																// TODO: confirm lack of dispatch for this anim.
	LA_FREEFALL = 23,										// Freefall, after falling more than 7 steps (looped)
	LA_FREEFALL_LAND = 24,									// Freefall > hard landing
	LA_FREEFALL_DEATH = 25,									// Freefall death
	LA_STAND_TO_JUMP_UP_START = 26,							// Stand > jump up (1/2)
	LA_STAND_TO_JUMP_UP_END = 27,							// Stand > jump up (2/2)
	LA_JUMP_UP = 28,										// Jump up, ready to grab ledge (looped)
	LA_JUMP_UP_TO_HANG = 29,								// Grab ledge > hang
	LA_JUMP_UP_TO_FREEFALL = 30,							// Jump up > fall
	LA_JUMP_UP_LAND = 31,									// Jump up > land
	LA_JUMP_WALL_SMASH_START = 32,							// Directional jump smash > fall (1/2)
	LA_JUMP_WALL_SMASH_END = 33,							// Directional jump smash > fall (2/2)
																// TODO: create matching anims for all directional wall smashes.
	LA_FALL_START = 34,										// Start falling
	LA_FALL = 35,											// Light fall (looped)
	LA_FALL_TO_FREEFALL = 36,								// Light fall > freefall
	LA_HANG_TO_FREEFALL_UNUSED = 37,						// Hang > freefall
	LA_WALK_BACK_TO_STAND_RIGHT = 38,						// Walk backwards > stand, right foot first
	LA_WALK_BACK_TO_STAND_LEFT = 39,						// Walk back > stand, left foot first
	LA_WALK_BACK = 40,										// Walk back (looped)
	LA_STAND_TO_WALK_BACK = 41,								// Stand > walk back
	LA_VAULT_TO_STAND_3_STEPS = 42,							// Standing 3-step vault > stand
	LA_VAULT_TO_STAND_3_STEPS_TO_RUN = 43,					// Standing 3-step vault > run
	LA_TURN_RIGHT_FAST = 44,								// Rotate right quickly
	LA_HANG_IDLE = 45,										// Hang from ledge (looped)
	LA_UNDERWATER_USE_PUZZLE = 46,							// Insert puzzle underwater
	LA_ROLL_180_START_ALTERNATE_UNUSED = 47,				// Standing roll 180 (1/2)
	LA_ROLL_180_END_ALTERNATE_UNUSED = 48,					// Standing roll 180 (2/2)
	LA_JUMP_FORWARD_TO_FREEFALL = 49,						// Jump forward > freefall
	LA_VAULT_TO_STAND_2_STEPS_START = 50,					// Standing 2-step vault > stand (1/2) 
	LA_VAULT_TO_STAND_2_STEPS_END = 51,						// Standing 2-step vault > stand (2/2) 
	LA_VAULT_TO_STAND_2_STEPS_END_TO_RUN = 52,				// Standing 2-step vault > stand (2/2) > run
	LA_RUN_WALL_SMASH_LEFT = 53,							// Running smash > stand, left foot first
	LA_RUN_WALL_SMASH_RIGHT = 54,							// Running smash > stand, right foot first
	LA_RUN_UP_STEP_LEFT = 55,								// Run up a step, left foot first
	LA_RUN_UP_STEP_RIGHT = 56,								// Run up a step, right foot first
	LA_WALK_UP_STEP_LEFT = 57,								// Walk up a step, left foot first
	LA_WALK_UP_STEP_RIGHT = 58,								// Walk up a step, right foot first
	LA_WALK_DOWN_STEP_RIGHT = 59,							// Walk down a step, right foot first
	LA_WALK_DOWN_STEP_LEFT = 60,							// Walk down a step, left foot first
	LA_WALK_BACK_DOWN_LEFT = 61,							// Walk back down a step, left foot first
	LA_WALK_BACK_DOWN_RIGHT = 62,							// Walk back down a step, right foot first
	LA_WALLSWITCH_DOWN = 63,								// Activate horizontal wall switch
	LA_WALLSWITCH_UP = 64,									// Deactivate horizontal wall switch
	LA_SIDESTEP_LEFT = 65,									// Sidestep left (looped)
	LA_SIDESTEP_LEFT_END = 66,								// Sidestep left > stand
	LA_SIDESTEP_RIGHT = 67,									// Sidestep right (looped)
	LA_SIDESTEP_RIGHT_END = 68,								// Sidestep right > stand
	LA_TURN_LEFT_FAST = 69,									// Rotate left quickly
	LA_SLIDE_FORWARD = 70,									// Slide forward (looped)
	LA_SLIDE_FORWARD_TO_STAND_START = 71,					// Slide forward > stand (1/2)
	LA_SLIDE_FORWARD_TO_STAND_END = 72,						// Slide forward > stand (2/2)
	LA_STAND_TO_JUMP_PREPARE = 73,							// Prepare standing jump
	LA_JUMP_BACK_START = 74,								// Prepare standing jump > jump back
	LA_JUMP_BACK = 75,										// Jump back (looped)
	LA_JUMP_FORWARD_START = 76,								// Prepare standing jump > jump forward
	LA_JUMP_FORWARD = 77,									// Jump forward (looped)
	LA_JUMP_LEFT_START = 78,								// Prepare standing jump > jump left
	LA_JUMP_LEFT = 79,										// Jump left (looped)
	LA_JUMP_RIGHT_START = 80,								// Prepare standing jump > jump right
	LA_JUMP_RIGHT = 81,										// Jump right (looped)
	LA_LAND = 82,											// Light fall > land
	LA_JUMP_BACK_TO_FREEFALL = 83,							// Jump back > freefall
	LA_JUMP_LEFT_TO_FREEFALL = 84,							// Jump left > freefall
	LA_JUMP_RIGHT_TO_FREEFALL = 85,							// Jump right > freefall
	LA_UNDERWATER_SWIM = 86,								// Swim forward (looped)
	LA_UNDERWATER_SWIM_DRIFT = 87,							// Swim forward drift
	LA_HOP_BACK_START = 88,									// Hop back (1/3)
	LA_HOP_BACK_CONTINUE = 89,								// Hop back (2/3)
	LA_HOP_BACK_END = 90,									// Hop back (3/3)
	LA_JUMP_UP_START = 91,									// Prepare standing jump > jump up
	LA_LAND_TO_RUN = 92,									// Jump forward > land running
	LA_FALL_BACK = 93,										// Light fall back
	LA_JUMP_FORWARD_TO_REACH_3 = 94,						// Jump forward > reach, 3rd opportunity
	LA_REACH = 95,											// Reach (looped)
	LA_REACH_TO_HANG = 96,									// Reach > hang
	LA_HANG_TO_STAND = 97,									// Pull up from hang > stand (1/2)
	LA_REACH_TO_FREEFALL = 98,								// Reach > freefall
	LA_UNDERWATER_USE_KEY = 99,								// Use key underwater
	LA_JUMP_FORWARD_TO_REACH_4 = 100,						// Jump forward > reach, 4th opportunity
	LA_JUMP_FORWARD_TO_REACH_ALTERNATE_UNUSED = 101,		// Jump forward (from jump forward anim) > reach
	LA_HANG_TO_STAND_END = 102,								// Pull up from hang > stand (2/2) TODO: remove, not used anymore
	LA_STAND_IDLE = 103,									// Stand idle (looped)
	LA_SLIDE_BACK_START = 104,								// Land on slope > slide back
	LA_SLIDE_BACK = 105,									// Slide back (looped) 
	LA_SLIDE_BACK_END = 106,								// Slide back > stand
	LA_UNDERWATER_SWIM_TO_IDLE = 107,						// Swim forward > underwater idle
	LA_UNDERWATER_IDLE = 108,								// Underwater idle (looped)
	LA_UNDERWARER_IDLE_TO_SWIM = 109,						// Underwater idle > swim forward underwater
	LA_ONWATER_IDLE = 110,									// Tread water idle (looped)
	LA_ONWATER_TO_STAND_1_STEP = 111,						// Pull up 1 step from tread > stand
	LA_FREEFALL_DIVE = 112,									// Freefall > underwater
	LA_ONWATER_DIVE_ALTERNATE_1_UNUSED = 113,				// Tread water > underwater
	LA_UNDERWATER_RESURFACE = 114,							// Underwater > tread water
	LA_ONWATER_DIVE_ALTERNATE_2_UNUSED = 115,				// Tread water > underwater
																// TODO: could reuse as link for an on-water "sprint".
	LA_ONWATER_SWIM = 116,									// Swim treading (looped)
	LA_ONWATER_SWIM_TO_IDLE = 117,							// Swim treading > tread water idle
	LA_ONWATER_IDLE_TO_SWIM = 118,							// Tread water > swim treading
	LA_ONWATER_DIVE = 119,									// Tread water > underwater
	LA_PUSHABLE_GRAB = 120,									// Grab pushable object (looped)
	LA_PUSHABLE_RELEASE = 121,								// Release pushable object
	LA_PUSHABLE_OBJECT_PULL = 122,							// Pull pushable object (looped)
	LA_PUSHABLE_OBJECT_PUSH = 123,							// Push pushable object (looped)
	LA_UNDERWATER_DEATH = 124,								// Drowning death
	LA_STAND_HIT_FRONT = 125,								// Jerk back standing from damage
	LA_STAND_HIT_BACK = 126,								// Jerk forward standing from damage
	LA_STAND_HIT_LEFT = 127,								// Jerk right standing from damage
	LA_STAND_HIT_RIGHT = 128,								// Jerk left standing from damage
	LA_WATERLEVER_PULL = 129,								// Pull underwater lever
	LA_UNDERWATER_PICKUP = 130,								// Pickup underwater
	LA_USE_KEY = 131,										// Use key
	LA_ONWATER_DEATH = 132,									// Treading death
	LA_RUN_DEATH = 133,										// Running death
	LA_USE_PUZZLE = 134,									// Insert receptacle item
	LA_PICKUP = 135,										// Standing pickup
	LA_SHIMMY_LEFT = 136,									// Shimmy left
	LA_SHIMMY_RIGHT = 137,									// Shimmy right
	LA_STAND_DEATH = 138,									// Standing death
	LA_BOULDER_DEATH = 139,									// Boulder death
	LA_ONWATER_IDLE_TO_SWIM_BACK = 140,						// Tread water > swim back treading
	LA_ONWATER_SWIM_BACK = 141,								// Swim back treading (looped)
	LA_ONWATER_SWIM_BACK_TO_IDLE = 142,						// Swim back treading > tread water idle
	LA_ONWATER_SWIM_LEFT = 143,								// Swim left treading (looped)
	LA_ONWATER_SWIM_RIGHT = 144,							// Swim right treading (looped)
	LA_LAND_DEATH = 145,									// Landing death
	LA_ROLL_180_START = 146,								// Standing roll 180 (1/3)
	LA_ROLL_180_CONTINUE = 147,								// Standing roll 180 (2/3)
	LA_ROLL_180_END = 148,									// Standing roll 180 (3/3)
	LA_SPIKE_DEATH = 149,									// Spike death
	LA_REACH_TO_MONKEY = 150,								// Reach > monkey swing
	LA_SWANDIVE_ROLL = 151,									// Swan dive > roll landing
	LA_SWANDIVE_DIVE = 152,									// Swan dive > underwater
	LA_SWANDIVE_FREEFALL = 153,								// Swan dive freefall
	LA_SWANDIVE_FREEFALL_DIVE = 154,						// Swan dive freefall > underwater
	LA_SWANDIVE_FREEFALL_DEATH = 155,						// Swan dive freefall death
	LA_SWANDIVE_LEFT_START = 156,							// Run > swan dive, left foot first
	LA_SWANDIVE_RIGHT_START = 157,							// Run > swan dive, right foot first
	LA_SWANDIVE = 158,										// Swan dive
	LA_HANG_HANDSTAND = 159,								// Hang > stand via handstand
	
	// TR2
	LA_STAND_TO_LADDER = 160,								// Stand > ladder idle
	LA_LADDER_UP = 161,										// Ascend ladder (looped)
	LA_LADDER_UP_LEFT_END = 162,							// Ascend ladder > ladder idle, left foot first
	LA_LADDER_UP_RIGHT_END = 163,							// Ascend ladder > ladder idle, right foot first
	LA_LADDER_IDLE = 164,									// Ladder idle (looped)
	LA_LADDER_UP_START = 165,								// Ladder idle > ascend ladder
	LA_LADDER_DOWN_LEFT_END = 166,							// Descend ladder > ladder idle, left foot first
	LA_LADDER_DOWN_RIGHT_END = 167,							// Descend ladder > ladder idle, right foot first
	LA_LADDER_DOWN = 168,									// Descend ladder (looped)
	LA_LADDER_DOWN_START = 169,								// Ladder idle > descend ladder
	LA_LADDER_RIGHT = 170,									// Climb ladder right (looped)
	LA_LADDER_LEFT = 171,									// Climb ladder left (looped)
	LA_LADDER_HANG = 172,									// Ladder hang (looped)
																// TODO: possible to make this a generic hang animation for ledges and ladders, thereby splitting 96?
	LA_LADDER_HANG_TO_IDLE = 173,							// Ladder hang > ladder idle
	LA_LADDER_TO_STAND = 174,								// Ladder idle > stand, pulling up
	LA_UNKNOWN = 175,										// Pushed back?
	LA_ONWATER_TO_WADE_0_STEP = 176,						// Tread water > wade
																// TODO: implement this properly?
	LA_WADE = 177,											// Wade (looped)
	LA_RUN_TO_WADE_RIGHT = 178,								// Run > wade, right foot first
	LA_RUN_TO_WADE_LEFT = 179,								// Run > wade, left foot first
	LA_WADE_TO_RUN_RIGHT = 180,								// Wade > run, right foot first
	LA_WADE_TO_RUN_LEFT = 181,								// Wade > run, left foot first
	LA_LADDER_TO_JUMP_BACK_START = 182,						// Ladder idle > jump back (1/2)
	LA_LADDER_TO_JUMP_BACK_END = 183,						// Ladder idle > jump back (2/2)
																// TODO: combine 182 and 183?
	LA_WADE_TO_STAND_RIGHT = 184,							// Wade > stand, right foot first
	LA_WADE_TO_STAND_LEFT = 185,							// Wade > stand, left foot first
	LA_STAND_TO_WADE = 186,									// Stand > wade
	LA_LADDER_SHIMMY_UP = 187,								// Ascend ladder hanging
	LA_LADDER_SHIMMY_DOWN = 188,							// Descend ladder hanging
	LA_DISCARD_FLARE = 189,									// Throw flare standing
	LA_ONWATER_TO_WADE_1_STEP = 190,						// Tread water up a step > wade
	LA_ONWATER_TO_STAND_0_STEP = 191,						// Pull up from tread > stand
	LA_UNDERWATER_TO_STAND = 192,							// Underwater > stand
	LA_ONWATER_TO_STAND_M1_STEP = 193,						// Pull up on lower step from tread > stand
	LA_LADDER_TO_HANG_DOWN = 194,							// Descend ladder > hang
																// TODO: this links to regular hang at 96. Address this?
	LA_SWITCH_SMALL_DOWN = 195,								// Activate small switch
	LA_SWITCH_SMALL_UP = 196,								// Deactivate small switch
	LA_BUTTON_SMALL_PUSH = 197,								// Push small button
	LA_UNDERWATER_SWIM_TO_IDLE_2 = 198,						// Underwater swim > underwater idle, 2nd opportunity
	LA_UNDERWATER_SWIM_TO_IDLE_3 = 199,						// Underwater swim > underwater idle, 3rd opportunity
	LA_UNDERWATER_SWIM_TO_IDLE_1 = 200,						// Underwater swim > underwater idle, 1st opportunity
	LA_LADDER_TO_HANG_RIGHT = 201,							// Climb ladder right > hang
	LA_LADDER_TO_HANG_LEFT = 202,							// Climb ladder left > hang
	LA_UNDERWATER_ROLL_180_START = 203,						// Underwater roll 180 (1/2)
	LA_PICKUP_FLARE = 204,									// Pickup flare standing
	LA_UNDERWATER_ROLL_180_END = 205,						// Underwater roll 180 (2/2)
	LA_UNDERWATER_PICKUP_FLARE = 206,						// Pickup flare underwater
	LA_RUN_JUMP_RIGHT_ROLL_180_START = 207,					// Run jump, right foot first, roll 180 (1/2)
	LA_SWANDIVE_SOMERSAULT = 208,							// Swandive somersault
	LA_RUN_JUMP_RIGHT_ROLL_180_END = 209,					// Run jump, right foot first, roll 180 (2/2)
	LA_JUMP_FORWARD_ROLL_180_START = 210,					// Jump forward roll 180 (1/2)
	LA_JUMP_FORWARD_ROLL_180_END = 211,						// Jump forward roll 180 (2/2)
	LA_JUMP_BACK_ROLL_180_START = 212,						// Jump back roll 180 (1/2)
	LA_JUMP_BACK_ROLL_180_END = 213,						// Jump back roll 180 (2/2)
	LA_ZIPLINE_MOUNT = 214,									// Stand > ride sipline
	LA_ZIPLINE_RIDE = 215,									// Ride zipline (looped)
	LA_ZIPLINE_DISMOUNT = 216,								// Ride zipline > jump forward
	
	// TR3
	LA_STAND_TO_CROUCH_START = 217,									// Stand > crouch (1/2)
	LA_CROUCH_ROLL_FORWARD_START_ALTERNATE = 218,					// Crouch roll forward (1/3)
	LA_CROUCH_ROLL_FORWARD_CONTINUE = 219,							// Crouch roll forward (2/3)
	LA_CROUCH_ROLL_FORWARD_END = 220,								// Crouch roll forward (3/3)
	LA_CROUCH_TO_STAND = 221,										// Crouch >  stand
	LA_CROUCH_IDLE = 222,											// Crouch (looped)
	LA_SPRINT = 223,												// Sprint (looped)
	LA_RUN_TO_SPRINT_LEFT = 224,									// Run > sprint, left foot first
	LA_RUN_TO_SPRINT_RIGHT = 225,									// Run > sprint, right foot first
	LA_SPRINT_TO_STAND_RIGHT = 226,									// Sprint > stand, right foot first
	LA_SPRINT_TO_STAND_RIGHT_END_ALTERNATE_UNUSED = 227,			// Sprint > stand, right foot first end
	LA_SPRINT_TO_STAND_LEFT = 228,									// Sprint > stand, left foot first
	LA_SPRINT_TO_STAND_LEFT_END_ALTERNATE_UNUSED = 229,				// Sprint > stand, left foot first end
	LA_SPRINT_ROLL_TO_RUN_LEFT_START = 230,							// Sprint roll, left foot first > run (1/2)
	LA_SPRINT_ROLL_TO_RUN_RIGHT_CONTINUE_ALTERNATE_UNUSED = 231,	// Sprint roll, left foot first > run (2/3)
	LA_SPRINT_ROLL_TO_RUN_LEFT_END = 232,							// Sprint roll, left foot first > run (2/2)
	LA_JUMP_UP_TO_MONKEY = 233,										// Jump up > monkey swing
	LA_MONKEY_IDLE = 234,											// Monkey swing idle (looped)
	LA_MONKEY_TO_FREEFALL = 235,									// Monkey swing > freefall
	LA_MONKEY_FORWARD = 236,										// Monkey swing forward (looped)
	LA_MONKEY_FORWARD_TO_IDLE_RIGHT = 237,							// Monkey-swing forward > monkey swing idle, right hand first
	LA_MONKEY_FORWARD_TO_IDLE_LEFT = 238,							// Monkey-swing forward > monkey swing idle, left hand first
	LA_MONKEY_IDLE_TO_FORWARD_LEFT = 239,							// Monkey idle > monkey forward, left hand first
	LA_SPRINT_ROLL_TO_RUN_LEFT_START_ALTERNATE_UNUSED = 240,		// Sprint roll, left foot first > run (1/3)
	LA_SPRINT_ROLL_TO_RUN_LEFT_CONTINUE_ALTERNATE_UNUSED = 241,		// Sprint roll, left foot first > run (2/3)
	LA_SPRINT_ROLL_TO_RUN_LEFT_END_ALTERNATE_UNUSED = 242,			// Sprint roll, left foot first > run (3/3)
	LA_SPRINT_TO_RUN_LEFT = 243,									// Sprint > run, left foot first
	LA_SPRINT_TO_RUN_RIGHT = 244,									// Sprint > run, right foot first
	LA_KNEE_DEATH = 245,											// TR1 knee death
	LA_SLIDE_TO_RUN = 246,											// Slide forward > run
	LA_CROUCH_ROLL_FORWARD_START = 247,								// Crouch roll forward (1/3)
	LA_JUMP_FORWARD_TO_REACH_1 = 248,								// Jump forward > reach, 1st opportunity
	LA_JUMP_FORWARD_TO_REACH_2 = 249,								// Jump forward > reach, 2nd opportunity
	LA_RUN_JUMP_LEFT_TO_REACH = 251,								// Run jump, left foot first > reach
	LA_MONKEY_IDLE_TO_FORWARD_RIGHT = 252,							// Monkey swing idle > monkey swing forward, right hand first
	LA_MONKEY_SHIMMY_LEFT = 253,									// Monkey swing shimmy left (looped)
	LA_MONKEY_SHIMMY_LEFT_END = 254,								// Monkey swing shimmy left > monkey swing idle
	LA_MONKEY_SHIMMY_RIGHT = 255,									// Monkey swing shimmy right (looped)
	LA_MONKEY_SHIMMY_RIGHT_END = 256,								// Monkey swing shimmy right > monkey swing idle
																		// TODO: generic shimmy anims between ledges and ladders?
	LA_MONKEY_TURN_180_START = 257,									// Monkey swing turn 180 (1/2)
	LA_CROUCH_TO_CRAWL_START = 258,									// Crouch > crawl (1/3)
	LA_CRAWL_TO_CROUCH_START = 259,									// Crawl > crouch (1/3)
	LA_CRAWL = 260,													// Crawl forward (looped)
	LA_CRAWL_IDLE_TO_FORWARD = 261,									// Crawl idle > crawl forward
	LA_CRAWL_TO_IDLE_LEFT = 262,									// Crawl forward > crawl idle, left leg first
	LA_CRAWL_IDLE = 263,											// Crwal idle
	LA_CROUCH_TO_CRAWL_END = 264,									// Crawl > crouch (2/2)
	LA_TURN_180_START = 265,										// Standing turn 180 (1/2)
	LA_TURN_180_END = 266,											// Standing turn 180 (2/2)
	LA_CRAWL_TO_IDLE_RIGHT = 267,									// Crawl forward > crawl idle, right leg first
	LA_MONKEY_TURN_180_END = 268,									// Monkey swing turn 180 (2/2)
	LA_CRAWL_TURN_LEFT = 269,										// Crawl rotate left (looped)
	LA_CRAWL_TURN_RIGHT = 270,										// Crawl rotate right (looped)
	LA_MONKEY_TURN_LEFT = 271,										// Monkey swing rotate left
	LA_MONKEY_TURN_RIGHT = 272,										// Monkey swing rotate right
	LA_CROUCH_TO_CRAWL_CONTINUE = 273,								// Crouch > crawl (2/3)
	LA_CRAWL_TO_CROUCH_CONTINUE = 274,								// Crawl > crouch (2/3)
	LA_CRAWL_IDLE_TO_CRAWL_BACK = 275,								// Crawl > crawl back
	LA_CRAWL_BACK = 276,											// Crawl back (looped)
	LA_CRAWL_BACK_TO_IDLE_RIGHT = 277,								// Crawl back > crawl idle, right foot first
	LA_REMOVE_PUZZLE = 278,											// Remove puzzle item > idle
	LA_CRAWL_BACK_TO_IDLE_LEFT = 279,								// Crawl back > crawl idle, left foot first
	LA_REMOVE_PUZZLE_UNDERWATER = 280,								// Remove puzzle item > underwater idle
	LA_CRAWL_TURN_LEFT_TO_IDLE_EARLY = 281,							// Crawl rotate left > crawl idle, early opportunity
	LA_CRAWL_TURN_RIGHT_TO_IDLE_EARLY = 282,						// Crawl rotate right > crawl idle, early opportunity
	LA_MONKEY_TURN_LEFT_TO_IDLE_EARLY = 283,						// Turn left on monkey swing > monkey swing idle, 1st opportunity
	LA_MONKEY_TURN_LEFT_TO_IDLE_LATE = 284,							// Turn left on monkey swing > monkey swing idle, 2nd opportunity
	LA_MONKEY_TURN_RIGHT_TO_IDLE_EARLY = 285,						// Turn right on monkey swing > monkey swing idle, 1st opportunity
	LA_MONKEY_TURN_RIGHT_TO_IDLE_LATE = 286,						// Turn right on monkey swing > monkey swing idle, 2nd opportunity
	LA_HANG_TO_CROUCH_START = 287,									// Pull up from hang > crouch (1/2)
	LA_HANG_TO_CROUCH_END = 288,									// Pull up from hang > crouch (2/2)
	LA_CRAWL_TO_HANG_START = 289,									// Crawl > hang (1/3)
	LA_CRAWL_TO_HANG_CONTINUE = 290,								// Crawl > hang (2/3)
																		// TODO: position commands in 302 may be stacked in 290, so can remove 302?
	LA_CROUCH_PICKUP = 291,											// Crouching pickup
	LA_CRAWL_PICKUP = 292,											// Crawling pickup
	LA_CROUCH_HIT_BACK = 293,										// Jerk back crouching from damage
	LA_CROUCH_HIT_FRONT = 294,										// Jerk forward crouching from damage
	LA_CROUCH_HIT_LEFT = 295,										// Jerk right crouching from damage
	LA_CROUCH_HIT_RIGHT = 296,										// Jerk left crouching from damage
	LA_CRAWL_HIT_BACK = 297,										// Jerk forward crawling from damage
	LA_CRAWL_HIT_FRONT = 298,										// Jerk back crawling from damage
	LA_CRAWL_HIT_LEFT = 299,										// Jerk left crawling from damage
	LA_CRAWL_HIT_RIGHT = 300,										// Jerk right crawling from damage
	LA_CRAWL_DEATH = 301,											// Crawl death
	LA_CRAWL_TO_HANG_END = 302,										// Crawl > hang (3/3)
	LA_STAND_TO_CROUCH_ABORT = 303,									// Stand > crouch abort
	LA_RUN_TO_CROUCH_LEFT_START = 304,								// Run > crouch, left foot first (1/2)
	LA_RUN_TO_CROUCH_RIGHT_START = 305,								// Run > crouch, right foot first (1/2)
	LA_RUN_TO_CROUCH_LEFT_END = 306,								// Run > crouch, left foot first (2/2)
	LA_RUN_TO_CROUCH_RIGHT_END = 307,								// Run > crouch, right foot first (2/2)
	LA_SPRINT_ROLL_TO_RUN_RIGHT_START = 308,						// Sprint roll, right foot first > run (2/2)
	LA_SPRINT_ROLL_TO_RUN_RIGHT_END = 309,							// Sprint roll, right foot first > run (2/2)
	LA_SPRINT_TO_CROUCH_LEFT = 310,									// Sprint roll, left foot first > crouch
	LA_SPRINT_TO_CROUCH_RIGHT = 311,								// Sprint roll, right foot first > crouch
	LA_CROUCH_PICKUP_FLARE = 312,									// Pickup flare crouching

	// TR4
	LA_DOOR_OPEN_PUSH = 313,								// Push door open using doorknob
	LA_DOOR_OPEN_PULL = 314,								// Pull door open using doorknob
	LA_DOOR_OPEN_KICK = 315,								// Kick door open
	LA_BUTTON_GIANT_PUSH = 316,								// Push giant button 
	LA_TRAPDOOR_FLOOR_OPEN = 317,							// Open trapdoor below
	LA_TRAPDOOR_CEILING_OPEN = 318,							// Jump up to open trapdoor above
	LA_TURNSWITCH_GRAB_CLOCKWISE = 319,						// Grab turnswitch to push clockwise	
	LA_TURNSWITCH_GRAB_COUNTER_CLOCKWISE = 320,				// Grab turnswitch to push counter-clockwise
	LA_COGWHEEL_PULL = 321,									// Pull cog wheel
	LA_COGWHEEL_GRAB = 322,									// Stand > grab cog wheel
	LA_COGWHEEL_RELEASE = 323,								// Release cogwheel > stand
	LA_LEVER_PUSH = 324,									// Push floor lever
	LA_HOLESWITCH_ACTIVATE = 325,							// Reach inside hole to activate/pickup
	LA_STAND_TO_POLE = 326,									// Stand > pole idle
	LA_POLE_JUMP_BACK = 327,								// Pole idle > jump back
	LA_POLE_IDLE = 328,										// Pole idle (looped)
	LA_POLE_UP_START = 329,									// Ascend pole (1/2)
	LA_POLE_TO_FREEFALL = 330,								// Pole idle > freefall
	LA_REACH_TO_POLE = 331,									// Reach > pole idle
	LA_POLE_TURN_CLOCKWISE_START = 332,						// Rotate clockwise on pole (1/2)
	LA_POLE_TURN_COUNTER_CLOCKWISE_START = 333,				// Rotate counter-clockwise on pole (1/2)
	LA_POLE_DOWN_START = 334,								// Pole idle > descend pole
	LA_POLE_DOWN = 335,										// Descend pole (looped)
	LA_POLE_DOWN_END = 336,									// Descend pole > pole idle
	LA_JUMP_UP_TO_POLE = 337,								// Jump up > pole
	LA_POLE_UP_END = 338,									// Ascend pole (2/2)
	LA_PULLEY_GRAB = 339,									// Stand > pull pulley
	LA_PULLEY_PULL = 340,									// Pull pulley
	LA_PULLEY_RELEASE = 341,								// Pull pulley > stand
	LA_POLE_TO_STAND = 342,									// Pole > stand
	LA_PUSHABLE_BLOCK_PULL = 343,							// Pull pushable block
	LA_POLE_TURN_CLOCKWISE_END = 344,						// Rotate clockwise on pole (2/2)
	LA_PUSHABLE_BLOCK_PUSH = 345,							// Push pushable block
	LA_POLE_TURN_COUNTER_CLOCKWISE_END = 346,				// Rotate counter-clockwise on pole (2/2)
	LA_TURNSWITCH_PUSH_CLOCKWISE_START = 347,				// Push turnswitch clockwise (1/3)
	LA_TURNSWITCH_PUSH_CLOCKWISE_CONTINUE = 348,			// Push turnswitch clockwise (2/3) 
	LA_TURNSWITCH_PUSH_CLOCKWISE_END = 349,					// Push turnswitch clockwise (3/3)
	LA_TURNSWITCH_PUSH_COUNTER_CLOCKWISE_START = 350,		// Push turnswitch counter-clockwise (1/3)
	LA_TURNSWITCH_PUSH_COUNTER_CLOCKWISE_CONTINUE = 351,	// Push turnswitch counter-clockwise (2/3)
	LA_TURNSWITCH_PUSH_COUNTER_CLOCKWISE_END = 352,			// Push turnswitch counter-clockwise (3/3)
	LA_CROUCH_TURN_LEFT = 353,								// Rotate left crouching (looped)
	LA_CROUCH_TURN_RIGHT = 354,								// Rotate right crouching (looped)
	LA_SHIMMY_LEFT_CORNER_OUTER_90 = 355,				    // Shimmy around outer left corner (90)
	LA_SHIMMY_LEFT_CORNER_OUTER_45 = 356,					// Shimmy around outer left corner (45)
	LA_SHIMMY_RIGHT_CORNER_OUTER_90 = 357,				    // Shimmy around outer right corner (90)
	LA_SHIMMY_RIGHT_CORNER_OUTER_45 = 358,					// Shimmy around outer right corner (45)
	LA_SHIMMY_LEFT_CORNER_INNER_90 = 359,				    // Shimmy around inner left corner (90)
	LA_SHIMMY_LEFT_CORNER_INNER_45 = 360,					// Shimmy around inner left corner (45)
	LA_SHIMMY_RIGHT_CORNER_INNER_90 = 361,				    // Shimmy around inner right corner (90)
	LA_SHIMMY_RIGHT_CORNER_INNER_45 = 362,					// Shimmy around inner right corner (45)
	LA_LADDER_LEFT_CORNER_OUTER_START = 363,				// Ladder around outer left corner
	LA_PUSHABLE_OBJECT_PUSH_EDGE_SLIP = 364,
	LA_LADDER_RIGHT_CORNER_OUTER_START = 365,				// Ladder around outer right corner
	LA_PUSHABLE_BLOCK_PUSH_EDGE_SLIP = 366,
	LA_LADDER_LEFT_CORNER_INNER_START = 367,				// Ladder around inner left corner
	LA_LADDER_LEFT_CORNER_INNER_END = 368,
	LA_LADDER_RIGHT_CORNER_INNER_START = 369,				// Ladder around inner right corner
	LA_LADDER_RIGHT_CORNER_INNER_END = 370,					// TODO: Remove.
	LA_JUMP_UP_TO_ROPE_START = 371,							// Jump up > rope idle (1/2)
	LA_TRAIN_OVERBOARD_DEATH = 372,							// Train overboard death
	LA_JUMP_UP_TO_ROPE_END = 373,							// Jump up > rope idle (2/2)
	LA_ROPE_IDLE = 374,										// Rope idle (looped)
	LA_ROPE_DOWN_START = 375,								// Rope idle > descend rope
	LA_ROPE_UP = 376,										// Ascend rope (looped)
	LA_ROPE_UP_TO_HANG_IDLE_UNUSED = 377,					// Ascend rope > rope hang
	LA_ROPE_HANG_TO_FREEFALL_UNUSED = 378,					// Rope hang > freefall
																// NOTE: 391 is a duplicate with a different state.
	LA_REACH_TO_ROPE_SWING = 379,							// Reach > rope swing
	LA_ROPE_SWING_JUMP_FLIP_TO_REACH_UNUSED = 380,			// Rope swing backflip > reach
	LA_ROPE_SWING_TO_FREEFALL_UNUSED_1 = 381,				// Rope swing > freefall, 1st opportunity
	LA_ROPE_SWING_TO_FREEFALL_UNUSED_3 = 382,				// Rope swing > freefall, 3rd opportunity
	LA_ROPE_SWING_TO_FREEFALL_UNUSED_6 = 383,				// Rope swing > freefall, 6th opportunity
	LA_ROPE_DOWN = 384,										// Descend rope (looped)
	LA_ROPE_DOWN_END = 385,									// Descend rope > rope idle
	LA_ROPE_SWING_TO_REACH_3 = 386,							// Rope swing > reach, 3rd opportunity
	LA_ROPE_IDLE_TO_SWING = 387,							// Rope idle > swing
	LA_ROPE_SWING_TO_FREEFALL_UNUSED_5 = 388,				// Rope swing > freefall, 5th opportunity
	LA_ROPE_SWING_TO_FREEFALL_UNUSED_4 = 389,				// Rope swing > freefall, 4th opportunity
	LA_ROPE_SWING_TO_FREEFALL_UNUSED_2 = 390,				// Rope swing > freefall, 2nd opportunity
	LA_ROPE_HANG_TO_FREEFALL_DUPLICATE_UNUSED = 391,		// Rope hang > freefall
	LA_ROPE_TURN_CLOCKWISE = 392,							// Turn clockwise on rope
	LA_ROPE_TURN_COUNTER_CLOCKWISE = 393,					// Turn counter-clockwise on rope
	LA_ROPE_SWING = 394,									// Swing on rope
	LA_LADDER_TO_HANG_UNUSED = 395,							// Ladder idle > hang
	LA_ROPE_HANG_TO_SWING_BACK_CONTINUE_UNUSED = 396,		// Rope hang > swing back (2/3)
	LA_ROPE_HANG_TO_SWING_BACK_END_UNUSED = 397,			// Rope hang > swing back (3/3)
	LA_ROPE_HANG_TO_SWING_BACK_START_UNUSED = 398,			// Rope hang > swing back (1/3)
	LA_ROPE_HANG_TO_SWING_FORWARD_SOFT_UNUSED = 399,		// Rope hang > swing forward, soft
																// NOTE: same as first 16 frames of 408.
	LA_WATERSKIN_POUR_LOW = 400,							// Pour waterskin low
	LA_WATERSKIN_FILL = 401,								// Fill waterskin
	LA_WATERSKIN_POUR_HIGH = 402,							// Pour waterskin on scale
	LA_DOOR_OPEN_CROWBAR = 403,								// Open door with crowbar
	LA_ROPE_HANG_TO_SWING_FORWARD_HARD_UNUSED = 404,		// Rope hang > swing forward, hard
	LA_ROPE_SWING_HANDOVER_UNUSED = 405,					// Rope swing > hand over to next rope
	LA_ROPE_SWING_TO_REACH_1 = 406,							// Rope swing > reach, 1st opportunity
	LA_ROPE_SWING_TO_REACH_2 = 407,							// Rope swing > reach, 2nd opportunity
	LA_ROPE_HANG_IDLE_UNUSED = 408,							// Rope hang (looped)
	LA_ROPE_SWING_TO_REACH_UNUSED_3 = 409,					// Rope swing > reach, 3rd unused opportunity
	LA_ROPE_SWING_TO_REACH_UNUSED_2 = 410,					// Rope swing > reach, 2nd unused opportunity
	LA_ROPE_SWING_TO_REACH_UNUSED_1 = 411,					// Rope swing > reach, 1st unused opportunity
	LA_DOUBLEDOOR_OPEN_PUSH = 412,							// Push double doors 
	LA_BUTTON_LARGE_PUSH = 413,								// Push big button 
	LA_JUMPSWITCH_PULL = 414,								// Pull jumpswitch
	LA_UNDERWATER_CEILING_SWITCH_PULL = 415,				// Pull underwater ceiling switch
	LA_UNDERWATER_DOOR_OPEN = 416,							// Open underwater_door
	LA_PUSHABLE_OBJECT_PUSH_TO_STAND = 417,						// Push pushable object (not looped) > stand
	LA_PUSHABLE_OBJECT_PULL_TO_STAND = 418,						// Pull pushable object (not looped) > stand
	LA_CROWBAR_PRY_WALL_FAST = 419,							// Pry item off wall quickly
	LA_CROWBAR_USE_ON_FLOOR = 420,							// Use crowbar to activate floor lever
	LA_CRAWL_JUMP_FLIP_DOWN = 421,							// Roll jump down from crawl
	LA_HARP_PLAY = 422,										// Play harp
	LA_TRIDENT_SET = 423,									// Place trident on Poseidon statue
	LA_PICKUP_PEDESTAL_HIGH = 424,							// Standing pickup from high pedestal
	LA_PICKUP_PEDESTAL_LOW = 425,							// Standing pickup from low pedestal
	LA_SENET_ROLL = 426,									// Roll Senet sticks
	LA_TORCH_LIGHT_1 = 427,									// Light torch with flame 0-1 steps high
	LA_TORCH_LIGHT_2 = 428,									// Light torch with flame 2-3 steps high
	LA_TORCH_LIGHT_3 = 429,									// Light torch with flame 4-5 steps high
	LA_TORCH_LIGHT_4 = 430,									// Light torch with flame 6-7 steps high
	LA_TORCH_LIGHT_5 = 431,									// Light torch with flame higher than 7 steps
	LA_DETONATOR_USE = 432,									// Use mine detonator
	LA_CORRECT_POSITION_FRONT = 433,						// Adjust position forward
	LA_CORRECT_POSITION_LEFT = 434,							// Adjust position left
	LA_CORRECT_POSITION_RIGHT = 435,						// Adjust position right
	LA_CROWBAR_USE_ON_FLOOR_FAIL = 436,						// Use crowbar on floor fail
	LA_KEYCARD_USE = 437,									// Use swipe card
																// TODO: 437 is also taken by MAGIC_DEATH, currently absent from default WAD.
	LA_MINE_DEATH = 438,									// Mine explosion death
	LA_PICKUP_SARCOPHAGUS = 439,							// Pickup from sarcophagus
	LA_DRAG_BODY = 440,										// Drag dead body
	LA_BINOCULARS_IDLE = 441,								// Stand, looking through binoculars
	LA_UNDERWATER_FLOOR_TRAPDOOR = 442,						// Underwater floor trapdoor
	LA_ELEVATOR_RECOVER = 443,								// Recover from elevator crash
	LA_MECHANICAL_BEETLE_USE = 444,							// Wind mechanical beetle, place on floor
	LA_FLY_CHEAT = 445,										// Fly cheat

	// TR5
	LA_TIGHTROPE_WALK = 446,								// Tightrope walk (looped)
	LA_TIGHTROPE_WALK_TO_STAND_LEFT = 447,					// Tightrope walk > tightrope stand, left foot first
	LA_TIGHTROPE_IDLE = 448,								// Stand idle on tightrope (looped)
	LA_TIGHTROPE_WALK_TO_STAND_RIGHT = 449,					// Tightrope walk > tightrope stand, right foot first
	LA_TIGHTROPE_STAND_TO_WALK = 450,						// Tightrope stand > tightrope walk 
	LA_TIGHTROPE_TURN_180 = 451,							// Turn 180 on rightrope
	LA_TIGHTROPE_UNBALANCE_LEFT = 452,						// Lean left on tightrope
	LA_TIGHTROPE_RECOVER_LEFT = 453,						// Lean left on tightrope > tightrope stand
	LA_TIGHTROPE_FALL_LEFT = 454,							// Fall left from tightrope > freefall
																// TODO: investigate why this doesn't link directly to freefall as other anims do.
	LA_TIGHTROPE_UNBALANCE_RIGHT = 455,						// Lean right on tightrope
	LA_TIGHTROPE_RECOVER_RIGHT = 456,						// Lean right on tightrope > tightrope stand
	LA_TIGHTROPE_FALL_RIGHT = 457,							// Fall right from tightrope > freefall
	LA_TIGHTROPE_START = 458,								// Stand > tightrope walk
	LA_TIGHTROPE_END = 459,									// Tightrope walk > stand
	LA_DOVESWITCH_TURN = 460,								// Examine and turn doveswitch
	LA_SWINGBAR_GRAB = 461,									// Reach > swingbar swing
	LA_SWINGBAR_SWING = 462,								// Swing around swingbar (looped)
	LA_SWINGBAR_JUMP = 463,									// Jump off swingbar > reach
	LA_LOOT_CABINET = 464,									// Search cabinet
	LA_LOOT_DRAWER = 465,									// Search drawer
	LA_LOOT_SHELF = 466,									// Search shelves
	LA_RADIO_START = 467,									// Stand > listen to headgear
	LA_RADIO_IDLE = 468,									// Listen to headgear (looped)
	LA_RADIO_END = 469,										// Listen to headgear > stand
	LA_VALVE_TURN = 470,									// Turn valve wheel
	LA_CROWBAR_PRY_WALL_SLOW = 471,							// Pry item off wall slowly
	LA_LOOT_CHEST = 472,									// Search chest on ground
	LA_LADDER_TO_CROUCH = 473,								// Pull up from ladder > crouch

	// TombEngine
	LA_VAULT_TO_CROUCH_1_STEP = 474, 						// Vault standing up 1 step > crouch
	LA_VAULT_TO_CROUCH_2_STEP = 475,						// Vault standing up 2 steps > crouch
	LA_VAULT_TO_CROUCH_3_STEP = 476,						// Vault standing up 2 steps > crouch
	LA_CRAWL_JUMP_DOWN_1_STEP = 477,						// Jump down 1 step from crawl > stand
	LA_CRAWL_JUMP_DOWN = 478,								// Jump down 2 steps and beyond from crawl > fall
	LA_CRAWL_STEP_UP = 479,									// Crawl up step > crawl idle
	LA_CRAWL_STEP_DOWN = 480,								// Crawl down step > crawl idle
	LA_ONWATER_TO_CROUCH_1_STEP = 481,						// Pull up 1 step from tread > stand
	LA_ONWATER_TO_CROUCH_0_STEP = 482,						// Pull up flat step from tread > stand
	LA_ONWATER_TO_CROUCH_M1_STEP = 483,						// Pull up lower step from tread > stand
	LA_LADDER_TO_MONKEY = 484,								// Ladder idle > monkey swing idle
	LA_ONWATER_TURN_180_START = 485,						// Tread water 180 turn (1/2)
	LA_ONWATER_TURN_180_END = 486,							// Tread water 180 turn (2/2)
	LA_MONKEY_TO_LADDER_OVERHEAD_START = 487,				// Monkey idle > ladder idle, overhead (1/2)
	LA_MONKEY_TO_LADDER_OVERHEAD_END = 488,					// Monkey idle > ladder idle, overhead (2/2)
	LA_JUMP_PREPARE_TO_STAND = 489,							// Jump prepare > stand
	LA_MONKEY_IDLE_TO_BACK_LEFT = 490,						// Monkey swing idle > monkey swing back, left hand leading
	LA_MONKEY_IDLE_TO_BACK_RIGHT = 491,						// Monkey swing idle > monkey swing back, right hand leading
	LA_MONKEY_BACK = 492,									// Monkey swing back
	LA_MONKEY_BACK_TO_IDLE_LEFT = 493,						// Monkey swing back > monkey swing idle, left hand leading
	LA_MONKEY_BACK_TO_IDLE_RIGHT = 494,						// Monkey swing back > monkey swing idle, right hand leading
	LA_REACH_TO_HANG_OSCILLATE = 495,						// Reach > hang, thin ledge
	LA_SWANDIVE_ROLL_TO_RUN = 496,							// Swandive roll > run
	LA_LADDER_DISMOUNT_LEFT_START = 497,					// Ladder dismount left (1/2)
	LA_LADDER_DISMOUNT_LEFT_END = 498,						// Ladder dismount left (2/2)
	LA_LADDER_DISMOUNT_RIGHT_START = 499,					// Ladder dismount right (1/2)
	LA_LADDER_DISMOUNT_RIGHT_END = 500,						// Ladder dismount right (2/2)
	LA_ONWATER_TO_LADDER = 501,								// Tread water > ladder idle
	LA_POSE_START = 502,									// Stand > AFK pose
	LA_POSE_CONTINUE = 503,									// AFK pose (looped)
	LA_POSE_END = 504,										// AFK pose > stand

	LA_OVERHANG_IDLE_LEFT = 505,
	LA_OVERHANG_IDLE_RIGHT = 506,
	LA_OVERHANG_CLIMB_UP_LEFT = 507,
	LA_OVERHANG_CLIMB_UP_RIGHT = 508,
	LA_OVERHANG_CLIMB_DOWN_LEFT = 509,
	LA_OVERHANG_CLIMB_DOWN_RIGHT = 510,
	LA_OVERHANG_DROP_LEFT = 511,
	LA_OVERHANG_DROP_RIGHT = 512,
	LA_OVERHANG_IDLE_2_HANG_LEFT = 513,
	LA_OVERHANG_IDLE_2_HANG_RIGHT = 514,
	LA_OVERHANG_HANG_2_IDLE_LEFT = 515,
	LA_OVERHANG_HANG_2_IDLE_RIGHT = 516,
	LA_OVERHANG_HANG_SWING = 517,
	LA_OVERHANG_HANG_LOOP = 518,
	LA_OVERHANG_HANG_DROP = 519,
	LA_OVERHANG_SHIMMY_LEFT = 520,
	LA_OVERHANG_SHIMMY_LEFT_STOP = 521,
	LA_OVERHANG_SHIMMY_RIGHT = 522,
	LA_OVERHANG_SHIMMY_RIGHT_STOP = 523,
	LA_OVERHANG_LEDGE_VAULT_START = 524,
	LA_OVERHANG_LEDGE_VAULT = 525,
	LA_OVERHANG_LADDER_SLOPE_CONCAVE = 526,
	LA_OVERHANG_SLOPE_LADDER_CONCAVE = 527,
	LA_OVERHANG_LADDER_SLOPE_CONVEX = 528,
	LA_OVERHANG_SLOPE_LADDER_CONVEX_START = 529,
	LA_OVERHANG_SLOPE_LADDER_CONVEX = 530,
	LA_OVERHANG_MONKEY_SLOPE_CONCAVE = 531,
	LA_OVERHANG_SLOPE_MONKEY_CONCAVE = 532,
	LA_OVERHANG_SLOPE_MONKEY_CONCAVE_END = 533,
	LA_OVERHANG_MONKEY_SLOPE_CONVEX = 534,
	LA_OVERHANG_MONKEY_SLOPE_CONVEX_END = 535,
	LA_OVERHANG_SLOPE_MONKEY_CONVEX = 536,
	LA_OVERHANG_EXIT_MONKEY_FORWARD = 537,
	LA_OVERHANG_EXIT_MONKEY_IDLE = 538,
	LA_OVERHANG_EXIT_LADDER = 539,
	LA_OVERHANG_EXIT_VAULT = 540,
	LA_OVERHANG_EXIT_DROP = 541,
	LA_OVERHANG_EXIT_HANG = 542,

	LA_VAULT_TO_STAND_3_STEPS_END = 543,
	LA_RUN_START_LEFT = 544,
	LA_RUN_TO_STAND_LEFT_SOFT = 545,
	LA_RUN_TO_STAND_RIGHT_SOFT = 546,
	LA_MONKEY_JUMP_START = 547,
	LA_MONKEY_JUMP_CONTINUE = 548,
	LA_SPRINT_JUMP_LEFT_START = 549,
	LA_SPRINT_JUMP_LEFT_CONTINUE = 550,
	LA_SPRINT_JUMP_LEFT_TO_REACH_1 = 551,
	LA_SPRINT_JUMP_LEFT_TO_REACH_2 = 552,
	LA_SPRINT_JUMP_LEFT_TO_REACH_3 = 553,
	LA_SPRINT_JUMP_LEFT_TO_REACH_4 = 554,
	LA_SPRINT_JUMP_RIGHT_START = 555,
	LA_SPRINT_JUMP_RIGHT_CONTINUE = 556,
	LA_SPRINT_JUMP_RIGHT_TO_REACH_1 = 557,
	LA_SPRINT_JUMP_RIGHT_TO_REACH_2 = 558,
	LA_SPRINT_JUMP_RIGHT_TO_REACH_3 = 559,
	LA_SPRINT_JUMP_RIGHT_TO_REACH_4 = 560,
	LA_CROUCH_TURN_180_START = 561,
	LA_CROUCH_TURN_180_END = 562,
	LA_CRAWL_TURN_180_START = 653,
	LA_CRAWL_TURN_180_END = 654,
	LA_LEDGE_JUMP_UP_START = 565,
	LA_LEDGE_JUMP_UP_END = 566,
	LA_LEDGE_JUMP_BACK_START = 567,
	LA_LEDGE_JUMP_BACK_END = 568,

	// 569-598 reserved for ladder object. -- Sezz 2023.04.16

	NUM_LARA_ANIMS

	// TRASHED ANIMS (reuse slots before going any higher and remove entries from this list when you do):
	// 370,
	// 442
};

enum LaraExtraAnim
{
	LEA_PULL_DAGGER_FROM_DRAGON = 0,
	LEA_WAKE_UP = 1,
	LEA_SPEAR_GUARDIAN_DEATH = 2,
	LEA_TRAIN_DEATH_START = 3,
	LEA_TREX_DEATH = 4,
	LEA_SHARK_DEATH = 5,
	LEA_GIANT_MUTANT_DEATH = 6,
	LEA_SHIVA_DEATH = 7,
	LEA_ACTIVATE_TNT_SWITCH = 8,
	LEA_MIDAS_GOLD_SWITCH = 9,
	LEA_MIDAS_DEATH = 10,
	LEA_STRIKE_GONG = 11,
	LEA_WILLARD_DEATH = 12,
	LEA_TRAIN_DEATH_END = 13,
	LEA_SETH_DEATH = 14,
	LEA_YETI_DEATH = 15,
	LEA_BIG_SCORPION_DEATH = 16
};
#pragma endregion

enum LARA_MESHES
{
	LM_HIPS,
	LM_LTHIGH,
	LM_LSHIN,
	LM_LFOOT,
	LM_RTHIGH,
	LM_RSHIN,
	LM_RFOOT,
	LM_TORSO,
	LM_RINARM,
	LM_ROUTARM,
	LM_RHAND,
	LM_LINARM,
	LM_LOUTARM,
	LM_LHAND,
	LM_HEAD,

	NUM_LARA_MESHES
};

enum LaraWeaponTypeCarried
{
	WTYPE_MISSING	 = 0,
	WTYPE_PRESENT	 = (1 << 0),
	WTYPE_SILENCER	 = (1 << 1),
	WTYPE_LASERSIGHT = (1 << 2),
	WTYPE_AMMO_1	 = (1 << 3),
	WTYPE_AMMO_2	 = (1 << 4),
	WTYPE_AMMO_3	 = (1 << 5),
	WTYPE_MASK_AMMO	 = WTYPE_AMMO_1 | WTYPE_AMMO_2 | WTYPE_AMMO_3,
};

enum class WeaponAmmoType
{
	Ammo1,
	Ammo2,
	Ammo3,

	Count
};

enum class PlayerAmmoType
{
	None,
	Pistol,
	Revolver,
	Uzi,
	ShotgunNormal,
	ShotgunWide,
	HK,
	CrossbowBoltNormal,
	CrossbowBoltPoison,
	CrossbowBoltExplosive,
	GrenadeNormal,
	GrenadeFrag,
	GrenadeFlash,
	Harpoon,
	Rocket,

	Count
};

enum class LaraWeaponType
{
	None,
	Pistol,
	Revolver,
	Uzi,
	Shotgun,
	HK,
	Crossbow,
	Flare,
	Torch,
	GrenadeLauncher,
	HarpoonGun,
	RocketLauncher,
	Snowmobile,

	NumWeapons
};

enum class HolsterSlot
{
	Empty			= ID_LARA_HOLSTERS,
	Pistols			= ID_LARA_HOLSTERS_PISTOLS,
	Uzis			= ID_LARA_HOLSTERS_UZIS,
	Revolver		= ID_LARA_HOLSTERS_REVOLVER,
	Shotgun			= ID_SHOTGUN_ANIM,
	HK				= ID_HK_ANIM,
	Harpoon			= ID_HARPOON_ANIM,
	Crossbow		= ID_CROSSBOW_ANIM,
	GrenadeLauncher = ID_GRENADE_ANIM,
	RocketLauncher	= ID_ROCKET_ANIM
};

enum class HandStatus
{
	Free,
	Busy,
	WeaponDraw,
	WeaponUndraw,
	WeaponReady,
	Special
};

enum class WaterStatus
{
	Dry,
	Wade,
	TreadWater,
	Underwater,
	FlyCheat
};

enum class JumpDirection
{
	None,
	Up,
	Forward,
	Back,
	Left,
	Right
};

enum class LookMode
{
	None,
	Vertical,
	Horizontal,
	Free
};

enum class TorchState
{
	Holding,
	Throwing,
	Dropping,
	JustLit
};

struct Ammo
{
private:
	unsigned int Count		= 0;
	bool		 IsInfinite = false;

public:
	static unsigned int Clamp(long value)
	{
		return std::clamp<unsigned int>(value, 0, UINT_MAX);
	}

	bool HasInfinite() const
	{
		return IsInfinite;
	}

	unsigned int GetCount() const
	{
		return Count;
	}

	void SetInfinite(bool isInfinite)
	{
		IsInfinite = isInfinite;
	}

	Ammo& operator --()
	{
		TENAssert(Count > 0, "Ammo count is already 0.");
		--Count;
		return *this;
	}

	Ammo operator --(int)
	{
		auto temp = *this;
		--*this;
		return temp;
	}

	Ammo& operator ++()
	{
		++Count;
		return *this;
	}

	Ammo operator ++(int)
	{
		auto temp = *this;
		++*this;
		return temp;
	}

	Ammo& operator =(unsigned int value)
	{
		Count = value;
		return *this;
	}

	bool operator ==(unsigned int value)
	{
		return (Count == value);
	}

	Ammo& operator =(Ammo& ammo)
	{
		Count = ammo.Count;
		IsInfinite = ammo.Count;
		return *this;
	}

	Ammo operator +(unsigned int value)
	{
		auto temp = *this;
		temp += value;
		return temp;
	}

	Ammo operator -(unsigned int value)
	{
		auto temp = *this;
		temp -= value;
		return temp;
	}

	Ammo& operator +=(unsigned int value)
	{
		long temp = Count + value;
		Count = Clamp(temp);
		return *this;
	}

	Ammo& operator -=(unsigned int value)
	{
		long temp = Count - value;
		Count = Clamp(temp);
		return *this;
	}

	operator bool()
	{
		return (IsInfinite || (Count > 0));
	}
};

struct HolsterInfo
{
	HolsterSlot LeftHolster  = HolsterSlot::Empty;
	HolsterSlot RightHolster = HolsterSlot::Empty;
	HolsterSlot BackHolster  = HolsterSlot::Empty;
};

struct CarriedWeaponInfo
{
	bool Present	   = false;
	bool HasLasersight = false; // TODO: Duplicated in LaraInventoryData.
	bool HasSilencer   = false; // TODO: Unused and duplicated in LaraInventoryData.

	Ammo				  Ammo[(int)WeaponAmmoType::Count] = {};
	WeaponAmmoType		  SelectedAmmo					   = WeaponAmmoType::Ammo1; // WeaponAmmoType_enum
	LaraWeaponTypeCarried WeaponMode					   = LaraWeaponTypeCarried::WTYPE_MISSING;
};

struct ArmInfo
{
	GAME_OBJECT_ID AnimObjectID = GAME_OBJECT_ID::ID_NO_OBJECT;
	int			   AnimNumber	= 0;
	int			   FrameNumber	= 0;

	EulerAngles Orientation = EulerAngles::Identity;
	bool		Locked		= false;

	int GunFlash = 0;
	int GunSmoke = 0;
};

struct FlareData
{
	int			 Frame		 = 0;
	unsigned int Life		 = 0;
	bool		 ControlLeft = false;
};

struct TorchData
{
	bool	   IsLit = false;
	TorchState State = TorchState::Holding;
};

struct LaraCountData
{
	unsigned int Pose			= 0;
	unsigned int PositionAdjust = 0;
	unsigned int Run			= 0;
	unsigned int Death			= 0;
};

struct LookControlData
{
	LookMode	Mode		= LookMode::None;
	EulerAngles Orientation = EulerAngles::Identity;
	EulerAngles	TurnRate	= EulerAngles::Identity;

	short OpticRange		= 0;
	bool  IsUsingBinoculars = false;
	bool  IsUsingLasersight = false;
};

struct RopeControlData
{
	byte Segment = 0;
	byte Direction = 0;

	short ArcFront = 0;
	short ArcBack = 0;

	short LastX = 0;
	short MaxXForward = 0;
	short MaxXBackward = 0;

	int DFrame = 0;
	int Frame = 0;
	unsigned short FrameRate = 0;

	unsigned short Y = 0;
	int Ptr = 0;
	int Offset = 0;
	int DownVel = 0;
	byte Flag = 0;
	int Count = 0;
};

struct SubsuitControlData
{
	short XRot = 0;
	short DXRot = 0;
	int Velocity[2] = {};
	int VerticalVelocity = 0;

	// TODO: These appear to be unused.
	short XRotVel = 0;
	unsigned short HitCount = 0;
};

// TODO: Give tightrope a property for difficulty?
struct TightropeControlData
{
	short		 TightropeItem	 = 0;
	bool		 CanDismount	 = false;
	float		 Balance		 = 0.0f;
	unsigned int TimeOnTightrope = 0;
};

struct WeaponControlData
{
	LaraWeaponType GunType		  = LaraWeaponType::None;
	LaraWeaponType RequestGunType = LaraWeaponType::None;
	LaraWeaponType LastGunType	  = LaraWeaponType::None;
	HolsterInfo	   HolsterInfo	  = {};

	short WeaponItem = -1;
	bool  HasFired	 = false;
	bool  Fired		 = false;

	bool UziLeft  = false;
	bool UziRight = false;

	// TODO: Interval and Timer count frame time for now, but should count delta time in the future. -- Sezz 2022.11.14
	unsigned int NumShotsFired = 0;
	float		 Interval	   = 0.0f;
	float		 Timer		   = 0.0f;
};

struct PlayerControlData
{
	short MoveAngle = 0;
	short TurnRate	= 0;

	HandStatus	  HandStatus	= {};
	WaterStatus	  WaterStatus	= {};
	JumpDirection JumpDirection = {};
	LaraCountData Count			= {};

	LookControlData		 Look	   = {};
	RopeControlData		 Rope	   = {};
	SubsuitControlData	 Subsuit   = {};
	TightropeControlData Tightrope = {};
	WeaponControlData	 Weapon	   = {};

	bool IsClimbingLadder = false;
	bool IsLocked		  = false;
	bool IsLow			  = false;
	bool IsMonkeySwinging = false;
	bool IsMoving		  = false;
	bool IsRunJumpQueued  = false;
	bool KeepLow		  = false;

	bool CanClimbLadder = false;
	bool CanLook		= false;
	bool CanMonkeySwing = false;
};

// TODO: Refactor status handling to use floats.
struct PlayerStatusData
{
	int Air		 = 0;
	int Exposure = 0;
	int Poison	 = 0;
	int Stamina	 = 0;
};

struct PlayerEffectData
{
	std::array<float, NUM_LARA_MESHES> DripNodes   = {};
	std::array<float, NUM_LARA_MESHES> BubbleNodes = {};
};

struct PlayerInventoryData
{
	bool IsBusy	 = false;
	bool OldBusy = false;

	byte BeetleLife;
	int BeetleComponents; // BeetleComponentFlags enum
	byte SmallWaterskin;  // 1 = has waterskin, 2 = has waterskin with 1 liter, etc. max value is 4 (has skin + 3 = 4)
	byte BigWaterskin;	  // 1 = has waterskin, 2 = has waterskin with 1 liter, etc. max value is 6 (has skin + 5 liters = 6)

	// TODO: Rename prefixes back to "Num".
	int TotalSmallMedipacks;
	int TotalLargeMedipacks;
	int TotalFlares;
	unsigned int TotalSecrets;

	bool HasLoad	   = false;
	bool HasSave	   = false;
	bool HasStopwatch  = false;
	bool HasCompass	   = false;
	bool HasDiary	   = false;
	bool HasBinoculars = false;
	bool HasCrowbar	   = false;
	bool HasTorch	   = false;
	bool HasLasersight = false;
	bool HasSilencer   = false; // TODO: Unused.

	int Puzzles[NUM_PUZZLES]			= {};
	int Keys[NUM_KEYS]					= {};
	int Pickups[NUM_PICKUPS]			= {};
	int Examines[NUM_EXAMINES]			= {};
	int PuzzlesCombo[NUM_PUZZLES * 2]	= {};
	int KeysCombo[NUM_KEYS * 2]			= {};
	int PickupsCombo[NUM_PICKUPS * 2]	= {};
	int ExaminesCombo[NUM_EXAMINES * 2] = {};
};

struct LaraInfo
{
	static constexpr auto TARGET_COUNT_MAX = 16;

	PlayerContext		Context	  = PlayerContext();
	PlayerControlData	Control	  = {};
	PlayerStatusData	Status	  = {};
	PlayerEffectData	Effect	  = {};
	PlayerInventoryData Inventory = {};

	// TODO: Move to PlayerControlData.
	FlareData		  Flare = {};
	TorchData		  Torch = {};
	CarriedWeaponInfo Weapons[(int)LaraWeaponType::NumWeapons] = {}; // TODO: Move to WeaponControlData.

	EulerAngles ExtraHeadRot	= EulerAngles::Identity;
	EulerAngles ExtraTorsoRot	= EulerAngles::Identity;
	EulerAngles TargetArmOrient = EulerAngles::Identity;
	ArmInfo		LeftArm			= {};
	ArmInfo		RightArm		= {};

	ItemInfo* TargetEntity = nullptr; // TargetEntityPtr. Should use item number instead?
	std::array<ItemInfo*, TARGET_COUNT_MAX> TargetList	= {};
	std::array<ItemInfo*, TARGET_COUNT_MAX> LastTargets = {};

	// TODO: Rewrite and restore spasm effect. Also move to PlayerEffectData?
	int HitFrame	 = 0; // Frame index.
	int HitDirection = 0; // Cardinal direction.

	// Item number? Only ever set to NO_VALUE or 1. Probably anim object ID. Might not be needed since AnimObjectID is kept in item.Animation.
	int ExtraAnim = 0;

	signed char Location		= 0;
	signed char HighestLocation = 0;
	signed char LocationPad		= 0;
};
