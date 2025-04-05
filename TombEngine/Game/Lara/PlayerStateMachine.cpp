#include "framework.h"
#include "Game/Lara/PlayerStateMachine.h"

#include "Game/collision/collide_room.h"
#include "Game/items.h"
#include "Game/Lara/lara_basic.h"
#include "Game/Lara/lara_cheat.h"
#include "Game/Lara/lara_climb.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_crawl.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_hang.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_initialise.h"
#include "Game/Lara/lara_jump.h"
#include "Game/Lara/lara_monkey.h"
#include "Game/Lara/lara_objects.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/Lara/lara_overhang.h"
#include "Game/Lara/lara_slide.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara_surface.h"
#include "Game/Lara/lara_swim.h"
#include "Game/Lara/lara_tests.h"

namespace TEN::Entities::Player
{
	using PlayerBehaviorStateRoutine	 = std::function<void(ItemInfo* item, CollisionInfo* coll)>;
	using PlayerBehaviorStateRoutinePair = std::pair<PlayerBehaviorStateRoutine, PlayerBehaviorStateRoutine>;

	auto PlayerBehaviorStateRoutines = std::array<PlayerBehaviorStateRoutinePair, NUM_LARA_STATES>{};

	void InitializePlayerStateMachine()
	{
		PlayerBehaviorStateRoutines[LS_WALK_FORWARD] = std::pair(lara_as_walk_forward, lara_col_walk_forward);
		PlayerBehaviorStateRoutines[LS_RUN_FORWARD] = std::pair(lara_as_run_forward, lara_col_run_forward);
		PlayerBehaviorStateRoutines[LS_IDLE] = std::pair(lara_as_idle, lara_col_idle);
		PlayerBehaviorStateRoutines[LS_JUMP_FORWARD] = std::pair(lara_as_jump_forward, lara_col_jump_forward);
		PlayerBehaviorStateRoutines[LS_POSE] = std::pair(lara_as_pose, lara_col_idle);
		PlayerBehaviorStateRoutines[LS_RUN_BACK] = std::pair(lara_as_run_back, lara_col_run_back);
		PlayerBehaviorStateRoutines[LS_TURN_RIGHT_SLOW] = std::pair(lara_as_turn_slow, lara_col_turn_slow);
		PlayerBehaviorStateRoutines[LS_TURN_LEFT_SLOW] = std::pair(lara_as_turn_slow, lara_col_turn_slow);
		PlayerBehaviorStateRoutines[LS_DEATH] = std::pair(lara_as_death, lara_col_death);
		PlayerBehaviorStateRoutines[LS_FREEFALL] = std::pair(lara_as_freefall, lara_col_freefall);
		PlayerBehaviorStateRoutines[LS_HANG] = std::pair(lara_as_hang, lara_col_hang);
		PlayerBehaviorStateRoutines[LS_REACH] = std::pair(lara_as_reach, lara_col_reach);
		PlayerBehaviorStateRoutines[LS_SPLAT] = std::pair(lara_as_splat, lara_col_splat);
		PlayerBehaviorStateRoutines[LS_UNDERWATER_IDLE] = std::pair(lara_as_underwater_idle, lara_col_underwater_idle);
		PlayerBehaviorStateRoutines[LS_GRAB_TO_FALL] = std::pair(lara_void_func, lara_col_land);
		PlayerBehaviorStateRoutines[LS_JUMP_PREPARE] = std::pair(lara_as_jump_prepare, lara_col_jump_prepare);
		PlayerBehaviorStateRoutines[LS_WALK_BACK] = std::pair(lara_as_walk_back, lara_col_walk_back);
		PlayerBehaviorStateRoutines[LS_UNDERWATER_SWIM_FORWARD] = std::pair(lara_as_underwater_swim_forward, lara_col_underwater_swim_forward);
		PlayerBehaviorStateRoutines[LS_UNDERWATER_INERTIA] = std::pair(lara_as_underwater_inertia, lara_col_underwater_inertia);
		PlayerBehaviorStateRoutines[LS_GRABBING] = std::pair(lara_as_controlled_no_look, lara_default_col);
		PlayerBehaviorStateRoutines[LS_TURN_RIGHT_FAST] = std::pair(lara_as_turn_fast, lara_col_turn_fast);
		PlayerBehaviorStateRoutines[LS_STEP_RIGHT] = std::pair(lara_as_step_right, lara_col_step_right);
		PlayerBehaviorStateRoutines[LS_STEP_LEFT] = std::pair(lara_as_step_left, lara_col_step_left);
		PlayerBehaviorStateRoutines[LS_ROLL_180_BACKWARD] = std::pair(lara_as_roll_180_back, lara_col_roll_180_back);
		PlayerBehaviorStateRoutines[LS_SLIDE_FORWARD] = std::pair(lara_as_slide_forward, lara_col_slide_forward);
		PlayerBehaviorStateRoutines[LS_JUMP_BACK] = std::pair(lara_as_jump_back, lara_col_jump_back);
		PlayerBehaviorStateRoutines[LS_JUMP_RIGHT] = std::pair(lara_as_jump_right, lara_col_jump_right);
		PlayerBehaviorStateRoutines[LS_JUMP_LEFT] = std::pair(lara_as_jump_left, lara_col_jump_left);
		PlayerBehaviorStateRoutines[LS_JUMP_UP] = std::pair(lara_as_jump_up, lara_col_jump_up);
		PlayerBehaviorStateRoutines[LS_FALL_BACK] = std::pair(lara_as_fall_back, lara_col_fall_back);
		PlayerBehaviorStateRoutines[LS_SHIMMY_LEFT] = std::pair(lara_as_shimmy_left, lara_col_shimmy_left);
		PlayerBehaviorStateRoutines[LS_SHIMMY_RIGHT] = std::pair(lara_as_shimmy_right, lara_col_shimmy_right);
		PlayerBehaviorStateRoutines[LS_SLIDE_BACK] = std::pair(lara_as_slide_back, lara_col_slide_back);
		PlayerBehaviorStateRoutines[LS_ONWATER_IDLE] = std::pair(lara_as_surface_idle, lara_col_surface_idle);
		PlayerBehaviorStateRoutines[LS_ONWATER_FORWARD] = std::pair(lara_as_surface_swim_forward, lara_col_surface_swim_forward);
		PlayerBehaviorStateRoutines[LS_ONWATER_DIVE] = std::pair(lara_as_surface_dive, lara_col_surface_dive);
		PlayerBehaviorStateRoutines[LS_PUSHABLE_PUSH] = std::pair(lara_as_pushable_push, lara_void_func);
		PlayerBehaviorStateRoutines[LS_PUSHABLE_PULL] = std::pair(lara_as_pushable_pull, lara_void_func);
		PlayerBehaviorStateRoutines[LS_PUSHABLE_GRAB] = std::pair(lara_as_pushable_grab, lara_default_col);
		PlayerBehaviorStateRoutines[LS_PICKUP] = std::pair(lara_as_pickup, lara_col_pickup);
		PlayerBehaviorStateRoutines[LS_SWITCH_DOWN] = std::pair(lara_as_switch_on, lara_default_col);
		PlayerBehaviorStateRoutines[LS_SWITCH_UP] = std::pair(lara_as_switch_off, lara_default_col);
		PlayerBehaviorStateRoutines[LS_INSERT_KEY] = std::pair(lara_as_use_key, lara_default_col);
		PlayerBehaviorStateRoutines[LS_INSERT_PUZZLE] = std::pair(lara_as_use_puzzle, lara_default_col);
		PlayerBehaviorStateRoutines[LS_WATER_DEATH] = std::pair(lara_as_underwater_death, lara_col_underwater_death);
		PlayerBehaviorStateRoutines[LS_ROLL_180_FORWARD] = std::pair(lara_as_roll_180_forward, lara_col_roll_180_forward);
		PlayerBehaviorStateRoutines[LS_BOULDER_DEATH] = std::pair(lara_as_special, lara_void_func);
		PlayerBehaviorStateRoutines[LS_ONWATER_BACK] = std::pair(lara_as_surface_swim_back, lara_col_surface_swim_back);
		PlayerBehaviorStateRoutines[LS_ONWATER_LEFT] = std::pair(lara_as_surface_swim_left, lara_col_surface_swim_left);
		PlayerBehaviorStateRoutines[LS_ONWATER_RIGHT] = std::pair(lara_as_surface_swim_right, lara_col_surface_swim_right);
		PlayerBehaviorStateRoutines[LS_USE_MIDAS] = std::pair(lara_void_func, lara_void_func);
		PlayerBehaviorStateRoutines[LS_MIDAS_DEATH] = std::pair(lara_void_func, lara_void_func);
		PlayerBehaviorStateRoutines[LS_SWAN_DIVE] = std::pair(lara_as_swan_dive, lara_col_swan_dive);
		PlayerBehaviorStateRoutines[LS_FREEFALL_DIVE] = std::pair(lara_as_freefall_dive, lara_col_freefall_dive);
		PlayerBehaviorStateRoutines[LS_HANDSTAND] = std::pair(lara_as_handstand, lara_default_col);
		PlayerBehaviorStateRoutines[LS_ONWATER_EXIT] = std::pair(lara_as_surface_climb_out, lara_default_col);
		PlayerBehaviorStateRoutines[LS_LADDER_IDLE] = std::pair(lara_as_climb_idle, lara_col_climb_idle);
		PlayerBehaviorStateRoutines[LS_LADDER_UP] = std::pair(lara_as_climb_up, lara_col_climb_up);
		PlayerBehaviorStateRoutines[LS_LADDER_LEFT] = std::pair(lara_as_climb_left, lara_col_climb_left);
		PlayerBehaviorStateRoutines[LS_LADDER_STOP] = std::pair(lara_as_climb_end, lara_col_climb_end);
		PlayerBehaviorStateRoutines[LS_LADDER_RIGHT] = std::pair(lara_as_climb_right, lara_col_climb_right);
		PlayerBehaviorStateRoutines[LS_LADDER_DOWN] = std::pair(lara_as_climb_down, lara_col_climb_down);
		PlayerBehaviorStateRoutines[LS_AUTO_JUMP] = std::pair(lara_as_auto_jump, lara_col_jump_prepare);
		PlayerBehaviorStateRoutines[LS_TEST_2] = std::pair(lara_void_func, lara_void_func);
		PlayerBehaviorStateRoutines[LS_TEST_3] = std::pair(lara_void_func, lara_void_func);
		PlayerBehaviorStateRoutines[LS_WADE_FORWARD] = std::pair(lara_as_wade_forward, lara_col_wade_forward);
		PlayerBehaviorStateRoutines[LS_UNDERWATER_ROLL] = std::pair(lara_as_underwater_roll_180, lara_col_underwater_roll_180);
		PlayerBehaviorStateRoutines[LS_PICKUP_FLARE] = std::pair(lara_as_pickup_flare, lara_col_pickup);
		PlayerBehaviorStateRoutines[LS_JUMP_ROLL_180] = std::pair(lara_void_func, lara_void_func);
		PlayerBehaviorStateRoutines[LS_KICK] = std::pair(lara_void_func, lara_void_func);
		PlayerBehaviorStateRoutines[LS_ZIP_LINE] = std::pair(lara_as_zip_line, lara_void_func);
		PlayerBehaviorStateRoutines[LS_CROUCH_IDLE] = std::pair(lara_as_crouch_idle, lara_col_crouch_idle);
		PlayerBehaviorStateRoutines[LS_CROUCH_ROLL] = std::pair(lara_as_crouch_roll, lara_col_crouch_roll);
		PlayerBehaviorStateRoutines[LS_SPRINT] = std::pair(lara_as_sprint, lara_col_sprint);
		PlayerBehaviorStateRoutines[LS_SPRINT_DIVE] = std::pair(lara_as_sprint_dive, lara_col_sprint_dive);
		PlayerBehaviorStateRoutines[LS_MONKEY_IDLE] = std::pair(lara_as_monkey_idle, lara_col_monkey_idle);
		PlayerBehaviorStateRoutines[LS_MONKEY_FORWARD] = std::pair(lara_as_monkey_forward, lara_col_monkey_forward);
		PlayerBehaviorStateRoutines[LS_MONKEY_SHIMMY_LEFT] = std::pair(lara_as_monkey_shimmy_left, lara_col_monkey_shimmy_left);
		PlayerBehaviorStateRoutines[LS_MONKEY_SHIMMY_RIGHT] = std::pair(lara_as_monkey_shimmy_right, lara_col_monkey_shimmy_right);
		PlayerBehaviorStateRoutines[LS_MONKEY_TURN_180] = std::pair(lara_as_monkey_turn_180, lara_col_monkey_turn_180);
		PlayerBehaviorStateRoutines[LS_CRAWL_IDLE] = std::pair(lara_as_crawl_idle, lara_col_crawl_idle);
		PlayerBehaviorStateRoutines[LS_CRAWL_FORWARD] = std::pair(lara_as_crawl_forward, lara_col_crawl_forward);
		PlayerBehaviorStateRoutines[LS_MONKEY_TURN_LEFT] = std::pair(lara_as_monkey_turn_left, lara_col_monkey_turn_left);
		PlayerBehaviorStateRoutines[LS_MONKEY_TURN_RIGHT] = std::pair(lara_as_monkey_turn_right, lara_col_monkey_turn_right);
		PlayerBehaviorStateRoutines[LS_CRAWL_TURN_LEFT] = std::pair(lara_as_crawl_turn_left, lara_col_crawl_turn_left);
		PlayerBehaviorStateRoutines[LS_CRAWL_TURN_RIGHT] = std::pair(lara_as_crawl_turn_right, lara_col_crawl_turn_right);
		PlayerBehaviorStateRoutines[LS_CRAWL_BACK] = std::pair(lara_as_crawl_back, lara_col_crawl_back);
		PlayerBehaviorStateRoutines[LS_HANG_TO_CRAWL] = std::pair(lara_as_controlled_no_look, lara_void_func);
		PlayerBehaviorStateRoutines[LS_CRAWL_TO_HANG] = std::pair(lara_as_controlled_no_look, lara_col_crawl_to_hang);
		PlayerBehaviorStateRoutines[LS_MISC_CONTROL] = std::pair(lara_as_controlled, lara_default_col);
		PlayerBehaviorStateRoutines[LS_ROPE_TURN_CLOCKWISE] = std::pair(lara_as_rope_turn_clockwise, lara_void_func);
		PlayerBehaviorStateRoutines[LS_ROPE_TURN_COUNTER_CLOCKWISE] = std::pair(lara_as_rope_turn_counter_clockwise, lara_void_func);
		PlayerBehaviorStateRoutines[LS_GIANT_BUTTON_PUSH] = std::pair(lara_as_controlled, lara_default_col);
		PlayerBehaviorStateRoutines[LS_TRAPDOOR_FLOOR_OPEN] = std::pair(lara_as_controlled, lara_void_func);
		PlayerBehaviorStateRoutines[LS_FREEFALL_BIS] = std::pair(lara_as_controlled, lara_void_func);
		PlayerBehaviorStateRoutines[LS_ROUND_HANDLE] = std::pair(lara_as_controlled_no_look, lara_col_turn_switch);
		PlayerBehaviorStateRoutines[LS_COGWHEEL] = std::pair(lara_as_controlled_no_look, lara_void_func);
		PlayerBehaviorStateRoutines[LS_LEVERSWITCH_PUSH] = std::pair(lara_as_controlled, lara_void_func);
		PlayerBehaviorStateRoutines[LS_HOLE] = std::pair(lara_as_pickup, lara_default_col);
		PlayerBehaviorStateRoutines[LS_POLE_IDLE] = std::pair(lara_as_pole_idle, lara_col_pole_idle);
		PlayerBehaviorStateRoutines[LS_POLE_UP] = std::pair(lara_as_pole_up, lara_col_pole_up);
		PlayerBehaviorStateRoutines[LS_POLE_DOWN] = std::pair(lara_as_pole_down, lara_col_pole_down);
		PlayerBehaviorStateRoutines[LS_POLE_TURN_CLOCKWISE] = std::pair(lara_as_pole_turn_clockwise, lara_col_pole_turn_clockwise);
		PlayerBehaviorStateRoutines[LS_POLE_TURN_COUNTER_CLOCKWISE] = std::pair(lara_as_pole_turn_counter_clockwise, lara_col_pole_turn_counter_clockwise);
		PlayerBehaviorStateRoutines[LS_PULLEY] = std::pair(lara_as_controlled_no_look, lara_default_col);
		PlayerBehaviorStateRoutines[LS_CROUCH_TURN_LEFT] = std::pair(lara_as_crouch_turn_left, lara_col_crouch_turn_left);
		PlayerBehaviorStateRoutines[LS_CROUCH_TURN_RIGHT] = std::pair(lara_as_crouch_turn_right, lara_col_crouch_turn_right);
		PlayerBehaviorStateRoutines[LS_SHIMMY_OUTER_LEFT] = std::pair(lara_as_shimmy_corner, lara_as_null);
		PlayerBehaviorStateRoutines[LS_SHIMMY_OUTER_RIGHT] = std::pair(lara_as_shimmy_corner, lara_as_null);
		PlayerBehaviorStateRoutines[LS_SHIMMY_INNER_LEFT] = std::pair(lara_as_shimmy_corner, lara_as_null);
		PlayerBehaviorStateRoutines[LS_SHIMMY_INNER_RIGHT] = std::pair(lara_as_shimmy_corner, lara_as_null);
		PlayerBehaviorStateRoutines[LS_ROPE_IDLE] = std::pair(lara_as_rope_idle, lara_col_rope_idle);
		PlayerBehaviorStateRoutines[LS_ROPE_UP] = std::pair(lara_as_rope_up, lara_void_func);
		PlayerBehaviorStateRoutines[LS_ROPE_DOWN] = std::pair(lara_as_rope_down, lara_void_func);
		PlayerBehaviorStateRoutines[LS_ROPE_SWING] = std::pair(lara_as_rope_idle, lara_col_rope_swing);
		PlayerBehaviorStateRoutines[LS_ROPE_UNKNOWN] = std::pair(lara_as_rope_idle, lara_col_rope_swing);
		PlayerBehaviorStateRoutines[LS_CORRECT_POSITION] = std::pair(lara_void_func, lara_void_func);
		PlayerBehaviorStateRoutines[LS_DOUBLEDOOR_PUSH] = std::pair(lara_as_controlled, lara_void_func);
		PlayerBehaviorStateRoutines[LS_FLY_CHEAT] = std::pair(lara_as_fly_cheat, lara_col_fly_cheat);
		PlayerBehaviorStateRoutines[LS_TIGHTROPE_IDLE] = std::pair(lara_as_tightrope_idle, lara_default_col);
		PlayerBehaviorStateRoutines[LS_TIGHTROPE_TURN_180] = std::pair(lara_as_controlled_no_look, lara_default_col);
		PlayerBehaviorStateRoutines[LS_TIGHTROPE_WALK] = std::pair(lara_as_tightrope_walk, lara_default_col);
		PlayerBehaviorStateRoutines[LS_TIGHTROPE_UNBALANCE_LEFT] = std::pair(lara_as_tightrope_fall, lara_default_col);
		PlayerBehaviorStateRoutines[LS_TIGHTROPE_UNBALANCE_RIGHT] = std::pair(lara_as_tightrope_fall, lara_default_col);
		PlayerBehaviorStateRoutines[LS_TIGHTROPE_ENTER] = std::pair(lara_as_null, lara_default_col);
		PlayerBehaviorStateRoutines[LS_TIGHTROPE_DISMOUNT] = std::pair(lara_as_tightrope_dismount, lara_default_col);
		PlayerBehaviorStateRoutines[LS_DOVE_SWITCH] = std::pair(lara_as_switch_on, lara_default_col);
		PlayerBehaviorStateRoutines[LS_TIGHTROPE_RECOVER_BALANCE] = std::pair(lara_as_null, lara_default_col);
		PlayerBehaviorStateRoutines[LS_HORIZONTAL_BAR_SWING] = std::pair(lara_as_horizontal_bar_swing, lara_default_col);
		PlayerBehaviorStateRoutines[LS_HORIZONTAL_BAR_LEAP] = std::pair(lara_as_horizontal_bar_leap, lara_default_col);
		PlayerBehaviorStateRoutines[LS_UNKNOWN_1] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[LS_RADIO_LISTENING] = std::pair(lara_as_controlled_no_look, lara_void_func);
		PlayerBehaviorStateRoutines[LS_RADIO_OFF] = std::pair(lara_as_controlled_no_look, lara_void_func);
		PlayerBehaviorStateRoutines[LS_UNKNOWN_2] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[LS_UNKNOWN_3] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[LS_UNKNOWN_4] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[LS_UNKNOWN_5] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[LS_PICKUP_FROM_CHEST] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[LS_LADDER_TO_CROUCH] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[LS_SHIMMY_45_OUTER_LEFT] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[LS_SHIMMY_45_OUTER_RIGHT] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[LS_SHIMMY_45_INNER_LEFT] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[LS_SHIMMY_45_INNER_RIGHT] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[LS_SLOPE_CLIMB_IDLE] = std::pair(lara_as_slopeclimb, lara_col_slopeclimb);
		PlayerBehaviorStateRoutines[LS_SLOPE_CLIMB_UP] = std::pair(lara_as_slopeclimbup, lara_default_col);
		PlayerBehaviorStateRoutines[LS_SLOPE_CLIMB_DOWN] = std::pair(lara_as_slopeclimbdown, lara_default_col);
		PlayerBehaviorStateRoutines[LS_COGWHEEL_UNGRAB] = std::pair(lara_as_controlled_no_look, lara_void_func);
		PlayerBehaviorStateRoutines[LS_STEP_UP] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[LS_STEP_DOWN] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[LS_SLOPE_CLIMB_FALL] = std::pair(lara_as_slopefall, lara_default_col);
		PlayerBehaviorStateRoutines[LS_LADDER_DISMOUNT_LEFT] = std::pair(lara_as_climb_stepoff_left, lara_default_col);
		PlayerBehaviorStateRoutines[LS_LADDER_DISMOUNT_RIGHT] = std::pair(lara_as_climb_stepoff_right, lara_default_col);
		PlayerBehaviorStateRoutines[LS_TURN_LEFT_FAST] = std::pair(lara_as_turn_fast, lara_col_turn_fast);
		PlayerBehaviorStateRoutines[LS_CRAWL_EXIT_STEP_DOWN] = std::pair(lara_as_controlled, lara_default_col);
		PlayerBehaviorStateRoutines[LS_CRAWL_EXIT_JUMP] = std::pair(lara_as_controlled, lara_default_col);
		PlayerBehaviorStateRoutines[LS_CRAWL_EXIT_FLIP] = std::pair(lara_as_controlled, lara_default_col);
		PlayerBehaviorStateRoutines[LS_SLOPE_CLIMB_HANG] = std::pair(lara_as_slopehang, lara_col_slopehang);
		PlayerBehaviorStateRoutines[LS_SLOPE_CLIMB_SHIMMY] = std::pair(lara_as_slopeshimmy, lara_col_slopeshimmy);
		PlayerBehaviorStateRoutines[LS_SLOPE_CLIMB_START] = std::pair(lara_as_sclimbstart, lara_default_col);
		PlayerBehaviorStateRoutines[LS_SLOPE_CLIMB_STOP] = std::pair(lara_as_sclimbstop, lara_default_col);
		PlayerBehaviorStateRoutines[LS_SLOPE_CLIMB_END] = std::pair(lara_as_sclimbend, lara_default_col);
		PlayerBehaviorStateRoutines[LS_CRAWL_STEP_UP] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[LS_CRAWL_STEP_DOWN] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[LS_MONKEY_BACK] = std::pair(lara_as_monkey_back, lara_col_monkey_back);
		PlayerBehaviorStateRoutines[LS_VAULT] = std::pair(lara_as_vault, lara_void_func);
		PlayerBehaviorStateRoutines[LS_VAULT_2_STEPS] = std::pair(lara_as_vault, lara_void_func);
		PlayerBehaviorStateRoutines[LS_VAULT_3_STEPS] = std::pair(lara_as_vault, lara_void_func);
		PlayerBehaviorStateRoutines[LS_VAULT_1_STEP_CROUCH] = std::pair(lara_as_vault, lara_void_func);
		PlayerBehaviorStateRoutines[LS_VAULT_2_STEPS_CROUCH] = std::pair(lara_as_vault, lara_void_func);
		PlayerBehaviorStateRoutines[LS_VAULT_3_STEPS_CROUCH] = std::pair(lara_as_vault, lara_void_func);
		PlayerBehaviorStateRoutines[LS_SOFT_SPLAT] = std::pair(lara_as_idle, lara_col_idle);
		PlayerBehaviorStateRoutines[LS_CROUCH_TURN_180] = std::pair(lara_as_crouch_turn_180, lara_col_crouch_turn_180);
		PlayerBehaviorStateRoutines[LS_CRAWL_TURN_180] = std::pair(lara_as_crawl_turn_180, lara_col_crawl_turn_180);
		PlayerBehaviorStateRoutines[LS_TURN_180] = std::pair(lara_as_turn_180, lara_col_turn_180);

		// Reserved for ladder object.
		PlayerBehaviorStateRoutines[174] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[175] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[176] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[177] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[178] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[179] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[180] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[181] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[182] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[183] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[184] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[185] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[186] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[187] = std::pair(lara_as_null, lara_void_func);
		PlayerBehaviorStateRoutines[188] = std::pair(lara_as_null, lara_void_func);

		PlayerBehaviorStateRoutines[LS_REMOVE_PUZZLE] = std::pair(lara_as_use_puzzle, lara_default_col);
		PlayerBehaviorStateRoutines[LS_PUSHABLE_EDGE_SLIP] = std::pair(lara_as_pushable_edge_slip, lara_void_func);
		PlayerBehaviorStateRoutines[LS_SPRINT_SLIDE] = std::pair(lara_as_sprint_slide, lara_col_sprint_slide);
		PlayerBehaviorStateRoutines[LS_TREAD_WATER_VAULT_1_STEP_DOWN_TO_STAND] = std::pair(lara_as_surface_climb_out, lara_void_func);
		PlayerBehaviorStateRoutines[LS_TREAD_WATER_VAULT_0_STEPS_TO_STAND] = std::pair(lara_as_surface_climb_out, lara_void_func);
		PlayerBehaviorStateRoutines[LS_TREAD_WATER_VAULT_1_STEP_UP_TO_STAND] = std::pair(lara_as_surface_climb_out, lara_void_func);
		PlayerBehaviorStateRoutines[LS_TREAD_WATER_VAULT_1_STEP_DOWN_TO_CROUCH] = std::pair(lara_as_surface_climb_out, lara_void_func);
		PlayerBehaviorStateRoutines[LS_TREAD_WATER_VAULT_0_STEPS_TO_CROUCH] = std::pair(lara_as_surface_climb_out, lara_void_func);
		PlayerBehaviorStateRoutines[LS_TREAD_WATER_VAULT_1_STEP_UP_TO_CROUCH] = std::pair(lara_as_surface_climb_out, lara_void_func);

		PlayerBehaviorStateRoutines[LS_PULLEY_UNGRAB] = std::pair(lara_as_controlled_no_look, lara_void_func);
	}

	void HandlePlayerBehaviorState(ItemInfo& item, CollisionInfo& coll, PlayerBehaviorStateRoutineType routineType)
	{
		// State ID out of range; return early.
		if (item.Animation.ActiveState < 0 ||
			item.Animation.ActiveState >= NUM_LARA_STATES)
		{
			TENLog("Error handling unregistered player behavior state " + std::to_string(item.Animation.ActiveState) + ".", LogLevel::Warning);
			return;
		}

		// Get behavior state routines.
		const auto& routinePair = PlayerBehaviorStateRoutines[item.Animation.ActiveState];
		const auto& controlRoutine = routinePair.first;
		const auto& collRoutine = routinePair.second;

		// Execute behavior state routine.
		switch (routineType)
		{
		case PlayerBehaviorStateRoutineType::Control:
			if (controlRoutine == nullptr)
				break;

			controlRoutine(&item, &coll);
			return;

		case PlayerBehaviorStateRoutineType::Collision:
			if (collRoutine == nullptr)
				break;

			collRoutine(&item, &coll);
			return;
		}

		TENLog("Error handling unregistered player behavior state " + std::to_string(item.Animation.ActiveState) + ".", LogLevel::Warning);
	}
}
