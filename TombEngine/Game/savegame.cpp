#include "framework.h"
#include "Game/savegame.h"

#include <filesystem>

#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/control/box.h"
#include "Game/control/event.h"
#include "Game/control/flipeffect.h"
#include "Game/control/lot.h"
#include "Game/control/volume.h"
#include "Objects/Effects/Fireflies.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/effects.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_initialise.h"
#include "Game/misc.h"
#include "Game/spotcam.h"
#include "Game/room.h"
#include "Game/Setup.h"
#include "Objects/Generic/Object/rope.h"
#include "Objects/Generic/Switches/fullblock_switch.h"
#include "Objects/Generic/puzzles_keys.h"
#include "Objects/Sink.h"
#include "Objects/TR3/Entity/FishSwarm.h"
#include "Objects/TR4/Entity/tr4_beetle_swarm.h"
#include "Objects/TR5/Emitter/tr5_rats_emitter.h"
#include "Objects/TR5/Emitter/tr5_bats_emitter.h"
#include "Objects/TR5/Emitter/tr5_spider_emitter.h"
#include "Renderer/Renderer.h"
#include "Scripting/Internal/TEN/Flow//Level/FlowLevel.h"
#include "Scripting/Include/ScriptInterfaceGame.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Scripting/Include/Objects/ScriptInterfaceObjectsHandler.h"
#include "Sound/sound.h"
#include "Specific/clock.h"
#include "Specific/level.h"
#include "Specific/savegame/flatbuffers/ten_savegame_generated.h"

using namespace flatbuffers;
using namespace TEN::Collision::Floordata;
using namespace TEN::Control::Volumes;
using namespace TEN::Effects::Environment;
using namespace TEN::Effects::Fireflies;
using namespace TEN::Effects::Items;
using namespace TEN::Entities::Creatures::TR3;
using namespace TEN::Entities::Generic;
using namespace TEN::Entities::Switches;
using namespace TEN::Entities::TR4;
using namespace TEN::Gui;
using namespace TEN::Renderer;

namespace Save = TEN::Save;

constexpr auto SAVEGAME_MAX_SLOT  = 99;
constexpr auto SAVEGAME_PATH	  = "Save//";
constexpr auto SAVEGAME_FILE_MASK = "savegame.";

GameStats SaveGame::Statistics;
SaveGameHeader SaveGame::Infos[SAVEGAME_MAX];
std::map<int, std::vector<byte>> SaveGame::Hub;

int SaveGame::LastSaveGame;
std::string SaveGame::FullSaveDirectory;

void SaveGame::LoadHeaders()
{
	for (int i = 0; i < SAVEGAME_MAX; i++)
		Infos[i].Present = false;

	if (!std::filesystem::is_directory(FullSaveDirectory))
		return;

	// Reset overall savegame count.
	LastSaveGame = 0;

	// Try loading savegame.
	for (int i = 0; i < SAVEGAME_MAX; i++)
	{
		if (!DoesSaveGameExist(i, true))
			continue;

		if (!SaveGame::LoadHeader(i, &Infos[i]))
			continue;

		Infos[i].Present = true;

		if (Infos[i].Count > LastSaveGame)
			LastSaveGame = Infos[i].Count;
	}
}

static Save::EulerAngles FromEulerAngles(const EulerAngles& eulers)
{
	return Save::EulerAngles(eulers.x, eulers.y, eulers.z);
}

static Save::Vector2 FromVector2(const Vector2& vec)
{
	return Save::Vector2(vec.x, vec.y);
}

static Save::Vector2 FromVector2i(const Vector2i& vec)
{
	return Save::Vector2(vec.x, vec.y);
}

static Save::Vector3 FromVector3(const Vector3& vec)
{
	return Save::Vector3(vec.x, vec.y, vec.z);
}

static Save::Vector3 FromVector3i(const Vector3i& vec)
{
	return Save::Vector3(vec.x, vec.y, vec.z);
}

static Save::Vector4 FromVector4(const Vector4& vec)
{
	return Save::Vector4(vec.x, vec.y, vec.z, vec.w);
}

static Save::GameVector FromGameVector(const GameVector& vec)
{
	return Save::GameVector(vec.x, vec.y, vec.z, (int)vec.RoomNumber);
}

static Save::Pose FromPose(const Pose& pose)
{
	return Save::Pose(FromVector3i(pose.Position), FromEulerAngles(pose.Orientation), FromVector3(pose.Scale));
}

static EulerAngles ToEulerAngles(const Save::EulerAngles* eulers)
{
	return EulerAngles((short)round(eulers->x()), (short)round(eulers->y()), (short)round(eulers->z()));
}

static Vector2 ToVector2(const Save::Vector2* vec)
{
	return Vector2(vec->x(), vec->y());
}

static Vector2i ToVector2i(const Save::Vector2* vec)
{
	return Vector2i((int)round(vec->x()), (int)round(vec->y()));
}

static Vector3i ToVector3i(const Save::Vector3* vec)
{
	return Vector3i((int)round(vec->x()), (int)round(vec->y()), (int)round(vec->z()));
}

static Vector3 ToVector3(const Save::Vector3* vec)
{
	return Vector3(vec->x(), vec->y(), vec->z());
}

static Vector4 ToVector4(const Save::Vector3* vec)
{
	return Vector4(vec->x(), vec->y(), vec->z(), 1.0f);
}

static Vector4 ToVector4(const Save::Vector4* vec)
{
	return Vector4(vec->x(), vec->y(), vec->z(), vec->w());
}

static GameVector ToGameVector(const Save::GameVector* vec)
{
	return GameVector(vec->x(), vec->y(), vec->z(), (short)vec->room_number());
}

static Pose ToPose(const Save::Pose& pose)
{
	return Pose(ToVector3i(&pose.position()), ToEulerAngles(&pose.orientation()), ToVector3(&pose.scale()));
}

bool SaveGame::IsSaveGameSlotValid(int slot)
{
	if (slot < 0 || slot > SAVEGAME_MAX_SLOT)
	{
		TENLog("Attempted to access invalid savegame slot " + std::to_string(slot), LogLevel::Warning);
		return false;
	}

	return true;
}

bool SaveGame::DoesSaveGameExist(int slot, bool silent)
{
	if (!std::filesystem::is_regular_file(GetSavegameFilename(slot)))
	{
		if (!silent)
			TENLog("Attempted to access missing savegame slot " + std::to_string(slot), LogLevel::Warning);

		return false;
	}

	return true;
}

bool SaveGame::IsSaveGameValid(int slot)
{
	SaveGameHeader header;
	if (!LoadHeader(slot, &header))
		return false;

	// Hash mismatch between savegame and level file means that level version has changed.
	if (header.LevelHash != LastLevelHash)
		return false;

	return true;
}

bool SaveGame::IsLoadGamePossible()
{
	for (int i = 0; i < SAVEGAME_MAX; i++)
	{
		if (Infos[i].Present)
			return true;
	}

	return false;
}

std::string SaveGame::GetSavegameFilename(int slot)
{
	return (FullSaveDirectory + SAVEGAME_FILE_MASK + std::to_string(slot));
}

#define SaveVec(Type, Data, TableBuilder, UnionType, SaveType, ConversionFunc) \
				auto data = std::get<(int)Type>(Data); \
				TableBuilder vtb{ fbb }; \
				SaveType saveVec = ConversionFunc(data); \
				vtb.add_vec(&saveVec); \
				auto vecOffset = vtb.Finish(); \
				putDataInVec(UnionType, vecOffset);

void SaveGame::Init(const std::string& gameDirectory)
{
	FullSaveDirectory = gameDirectory + SAVEGAME_PATH;
}

const std::vector<byte> SaveGame::Build()
{
	ItemInfo itemToSerialize{};
	FlatBufferBuilder fbb{};

	std::vector<flatbuffers::Offset<Save::Item>> serializedItems{};

	// Savegame header
	auto levelNameOffset = fbb.CreateString(g_GameFlow->GetString(g_GameFlow->GetLevel(CurrentLevel)->NameStringKey.c_str()));

	Save::SaveGameHeaderBuilder sghb{ fbb };
	sghb.add_level_name(levelNameOffset);
	sghb.add_level_hash(LastLevelHash);

	const auto& gameTime = SaveGame::Statistics.Game.TimeTaken;
	sghb.add_hours(gameTime.GetHours());
	sghb.add_minutes(gameTime.GetMinutes());
	sghb.add_seconds(gameTime.GetSeconds());

	sghb.add_level(CurrentLevel);
	sghb.add_timer(GlobalCounter);
	sghb.add_count(++LastSaveGame);
	auto headerOffset = sghb.Finish();

	Save::SaveGameStatisticsBuilder sgLevelStatisticsBuilder{ fbb };
	sgLevelStatisticsBuilder.add_ammo_hits(Statistics.Level.AmmoHits);
	sgLevelStatisticsBuilder.add_ammo_used(Statistics.Level.AmmoUsed);
	sgLevelStatisticsBuilder.add_kills(Statistics.Level.Kills);
	sgLevelStatisticsBuilder.add_medipacks_used(Statistics.Level.HealthUsed);
	sgLevelStatisticsBuilder.add_damage_taken(Statistics.Level.DamageTaken);
	sgLevelStatisticsBuilder.add_distance(Statistics.Level.Distance);
	sgLevelStatisticsBuilder.add_pickups(Statistics.Level.Pickups);
	sgLevelStatisticsBuilder.add_secrets(Statistics.Level.Secrets);
	sgLevelStatisticsBuilder.add_timer(SaveGame::Statistics.Level.TimeTaken);
	auto levelStatisticsOffset = sgLevelStatisticsBuilder.Finish();

	Save::SaveGameStatisticsBuilder sgGameStatisticsBuilder{ fbb };
	sgGameStatisticsBuilder.add_ammo_hits(Statistics.Game.AmmoHits);
	sgGameStatisticsBuilder.add_ammo_used(Statistics.Game.AmmoUsed);
	sgGameStatisticsBuilder.add_kills(Statistics.Game.Kills);
	sgGameStatisticsBuilder.add_medipacks_used(Statistics.Game.HealthUsed);
	sgGameStatisticsBuilder.add_damage_taken(Statistics.Game.DamageTaken);
	sgGameStatisticsBuilder.add_distance(Statistics.Game.Distance);
	sgGameStatisticsBuilder.add_pickups(Statistics.Game.Pickups);
	sgGameStatisticsBuilder.add_secrets(Statistics.Game.Secrets);
	sgGameStatisticsBuilder.add_timer(SaveGame::Statistics.Game.TimeTaken);
	auto gameStatisticsOffset = sgGameStatisticsBuilder.Finish();

	// Lara
	std::vector<int> puzzles;
	for (int i = 0; i < NUM_PUZZLES; i++)
		puzzles.push_back(Lara.Inventory.Puzzles[i]);
	auto puzzlesOffset = fbb.CreateVector(puzzles);

	std::vector<int> puzzlesCombo;
	for (int i = 0; i < NUM_PUZZLES * 2; i++)
		puzzlesCombo.push_back(Lara.Inventory.PuzzlesCombo[i]);
	auto puzzlesComboOffset = fbb.CreateVector(puzzlesCombo);

	std::vector<int> keys;
	for (int i = 0; i < NUM_KEYS; i++)
		keys.push_back(Lara.Inventory.Keys[i]);
	auto keysOffset = fbb.CreateVector(keys);

	std::vector<int> keysCombo;
	for (int i = 0; i < NUM_KEYS * 2; i++)
		keysCombo.push_back(Lara.Inventory.KeysCombo[i]);
	auto keysComboOffset = fbb.CreateVector(keysCombo);

	std::vector<int> pickups;
	for (int i = 0; i < NUM_PICKUPS; i++)
		pickups.push_back(Lara.Inventory.Pickups[i]);
	auto pickupsOffset = fbb.CreateVector(pickups);

	std::vector<int> pickupsCombo;
	for (int i = 0; i < NUM_PICKUPS * 2; i++)
		pickupsCombo.push_back(Lara.Inventory.PickupsCombo[i]);
	auto pickupsComboOffset = fbb.CreateVector(pickupsCombo);

	std::vector<int> examines;
	for (int i = 0; i < NUM_EXAMINES; i++)
		examines.push_back(Lara.Inventory.Examines[i]);
	auto examinesOffset = fbb.CreateVector(examines);

	std::vector<int> examinesCombo;
	for (int i = 0; i < NUM_EXAMINES * 2; i++)
		examinesCombo.push_back(Lara.Inventory.ExaminesCombo[i]);
	auto examinesComboOffset = fbb.CreateVector(examinesCombo);

	std::vector<float> bubbleNodes;
	for (int i = 0; i < Lara.Effect.BubbleNodes.size(); i++)
		bubbleNodes.push_back(Lara.Effect.BubbleNodes[i]);
	auto bubbleNodesOffset = fbb.CreateVector(bubbleNodes);
	
	std::vector<float> dripNodes;
	for (int i = 0; i < Lara.Effect.DripNodes.size(); i++)
		dripNodes.push_back(Lara.Effect.DripNodes[i]);
	auto dripNodesOffset = fbb.CreateVector(dripNodes);

	std::vector<int> subsuitVelocity{};
	subsuitVelocity.push_back(Lara.Control.Subsuit.Velocity[0]);
	subsuitVelocity.push_back(Lara.Control.Subsuit.Velocity[1]);
	auto subsuitVelocityOffset = fbb.CreateVector(subsuitVelocity);

	Save::PlayerSkinBuilder playerSkin{ fbb };
	playerSkin.add_skin((int)Lara.Skin.Skin);
	playerSkin.add_skin_joints((int)Lara.Skin.SkinJoints);
	playerSkin.add_hair_primary((int)Lara.Skin.HairPrimary);
	playerSkin.add_hair_secondary((int)Lara.Skin.HairSecondary);
	auto playerSkinOffset = playerSkin.Finish();

	Save::HolsterInfoBuilder holsterInfo{ fbb };
	holsterInfo.add_back_holster((int)Lara.Control.Weapon.HolsterInfo.BackHolster);
	holsterInfo.add_left_holster((int)Lara.Control.Weapon.HolsterInfo.LeftHolster);
	holsterInfo.add_right_holster((int)Lara.Control.Weapon.HolsterInfo.RightHolster);
	auto holsterInfoOffset = holsterInfo.Finish();

	Save::ArmInfoBuilder leftArm{ fbb };
	leftArm.add_anim_number(Lara.LeftArm.AnimNumber);
	leftArm.add_gun_flash(Lara.LeftArm.GunFlash);
	leftArm.add_gun_smoke(Lara.LeftArm.GunSmoke);
	leftArm.add_frame_base(Lara.LeftArm.FrameBase);
	leftArm.add_frame_number(Lara.LeftArm.FrameNumber);
	leftArm.add_locked(Lara.LeftArm.Locked);
	leftArm.add_rotation(&FromEulerAngles(Lara.LeftArm.Orientation));
	auto leftArmOffset = leftArm.Finish();

	Save::ArmInfoBuilder rightArm{ fbb };
	rightArm.add_anim_number(Lara.RightArm.AnimNumber);
	rightArm.add_gun_flash(Lara.RightArm.GunFlash);
	rightArm.add_gun_smoke(Lara.RightArm.GunSmoke);
	rightArm.add_frame_base(Lara.RightArm.FrameBase);
	rightArm.add_frame_number(Lara.RightArm.FrameNumber);
	rightArm.add_locked(Lara.RightArm.Locked);
	rightArm.add_rotation(&FromEulerAngles(Lara.RightArm.Orientation));
	auto rightArmOffset = rightArm.Finish();

	Save::FlareDataBuilder flare{ fbb };
	flare.add_control_left(Lara.Flare.ControlLeft);
	flare.add_frame(Lara.Flare.Frame);
	flare.add_life(Lara.Flare.Life);
	auto flareOffset = flare.Finish();

	Save::TorchState currentTorchState{ (int)Lara.Torch.State };

	Save::TorchDataBuilder torch{ fbb };
	torch.add_state(currentTorchState);
	torch.add_is_lit(Lara.Torch.IsLit);
	auto torchOffset = torch.Finish();

	Save::LaraInventoryDataBuilder inventory{ fbb };
	inventory.add_beetle_life(Lara.Inventory.BeetleLife);
	inventory.add_big_waterskin(Lara.Inventory.BigWaterskin);
	inventory.add_examines(examinesOffset);
	inventory.add_examines_combo(examinesComboOffset);
	inventory.add_beetle_components(Lara.Inventory.BeetleComponents);
	inventory.add_has_binoculars(Lara.Inventory.HasBinoculars);
	inventory.add_has_diary(Lara.Inventory.HasDiary);
	inventory.add_has_load(Lara.Inventory.HasLoad);
	inventory.add_has_save(Lara.Inventory.HasSave);
	inventory.add_has_compass(Lara.Inventory.HasCompass);
	inventory.add_has_stopwatch(Lara.Inventory.HasStopwatch);
	inventory.add_has_crowbar(Lara.Inventory.HasCrowbar);
	inventory.add_has_lasersight(Lara.Inventory.HasLasersight);
	inventory.add_has_silencer(Lara.Inventory.HasSilencer);
	inventory.add_has_torch(Lara.Inventory.HasTorch);
	inventory.add_is_busy(Lara.Inventory.IsBusy);
	inventory.add_keys(keysOffset);
	inventory.add_keys_combo(keysComboOffset);
	inventory.add_old_busy(Lara.Inventory.OldBusy);
	inventory.add_puzzles(puzzlesOffset);
	inventory.add_puzzles_combo(puzzlesComboOffset);
	inventory.add_pickups(pickupsOffset);
	inventory.add_pickups_combo(pickupsComboOffset);
	inventory.add_small_waterskin(Lara.Inventory.SmallWaterskin);
	inventory.add_total_flares(Lara.Inventory.TotalFlares);
	inventory.add_total_small_medipacks(Lara.Inventory.TotalSmallMedipacks);
	inventory.add_total_large_medipacks(Lara.Inventory.TotalLargeMedipacks);
	auto inventoryOffset = inventory.Finish();

	Save::LaraCountDataBuilder count{ fbb };
	count.add_death(Lara.Control.Count.Death);
	count.add_pose(Lara.Control.Count.Pose);
	count.add_position_adjust(Lara.Control.Count.PositionAdjust);
	count.add_run_jump(Lara.Control.Count.Run);
	auto countOffset = count.Finish();

	Save::LookControlDataBuilder lookControl{ fbb };
	lookControl.add_is_using_binoculars(Lara.Control.Look.IsUsingBinoculars);
	lookControl.add_is_using_lasersight(Lara.Control.Look.IsUsingLasersight);
	lookControl.add_mode((int)Lara.Control.Look.Mode);
	lookControl.add_optic_range(Lara.Control.Look.OpticRange);
	lookControl.add_orientation(&FromEulerAngles(Lara.Control.Look.Orientation));
	lookControl.add_turn_rate(&FromEulerAngles(Lara.Control.Look.TurnRate));
	auto lookControlOffset = lookControl.Finish();

	Save::RopeControlDataBuilder ropeControl{ fbb };
	ropeControl.add_segment(Lara.Control.Rope.Segment);
	ropeControl.add_direction(Lara.Control.Rope.Direction);
	ropeControl.add_arc_front(Lara.Control.Rope.ArcFront);
	ropeControl.add_arc_back(Lara.Control.Rope.ArcBack);
	ropeControl.add_last_x(Lara.Control.Rope.LastX);
	ropeControl.add_max_x_forward(Lara.Control.Rope.MaxXForward);
	ropeControl.add_max_x_backward(Lara.Control.Rope.MaxXBackward);
	ropeControl.add_dframe(Lara.Control.Rope.DFrame);
	ropeControl.add_frame(Lara.Control.Rope.Frame);
	ropeControl.add_frame_rate(Lara.Control.Rope.FrameRate);
	ropeControl.add_y(Lara.Control.Rope.Y);
	ropeControl.add_ptr(Lara.Control.Rope.Ptr);
	ropeControl.add_offset(Lara.Control.Rope.Offset);
	ropeControl.add_down_vel(Lara.Control.Rope.DownVel);
	ropeControl.add_flag(Lara.Control.Rope.Flag);
	ropeControl.add_count(Lara.Control.Rope.Count);
	auto ropeControlOffset = ropeControl.Finish();

	Save::SubsuitControlDataBuilder subsuitControl{ fbb };
	subsuitControl.add_x_rot(Lara.Control.Subsuit.XRot);
	subsuitControl.add_d_x_rot(Lara.Control.Subsuit.DXRot);
	subsuitControl.add_velocity(subsuitVelocityOffset);
	subsuitControl.add_vertical_velocity(Lara.Control.Subsuit.VerticalVelocity);
	subsuitControl.add_x_rot_vel(Lara.Control.Subsuit.XRotVel);
	subsuitControl.add_hit_count(Lara.Control.Subsuit.HitCount);
	auto subsuitControlOffset = subsuitControl.Finish();

	Save::TightropeControlDataBuilder tightropeControl{ fbb };
	tightropeControl.add_balance(Lara.Control.Tightrope.Balance);
	tightropeControl.add_can_dismount(Lara.Control.Tightrope.CanDismount);
	tightropeControl.add_tightrope_item(Lara.Control.Tightrope.TightropeItem);
	tightropeControl.add_time_on_tightrope(Lara.Control.Tightrope.TimeOnTightrope);
	auto tightropeControlOffset = tightropeControl.Finish();

	Save::WeaponControlDataBuilder weaponControl{ fbb };
	weaponControl.add_weapon_item(Lara.Control.Weapon.WeaponItem);
	weaponControl.add_has_fired(Lara.Control.Weapon.HasFired);
	weaponControl.add_fired(Lara.Control.Weapon.Fired);
	weaponControl.add_uzi_left(Lara.Control.Weapon.UziLeft);
	weaponControl.add_uzi_right(Lara.Control.Weapon.UziRight);
	weaponControl.add_gun_type((int)Lara.Control.Weapon.GunType);
	weaponControl.add_request_gun_type((int)Lara.Control.Weapon.RequestGunType);
	weaponControl.add_last_gun_type((int)Lara.Control.Weapon.LastGunType);
	weaponControl.add_holster_info(holsterInfoOffset);
	weaponControl.add_interval(Lara.Control.Weapon.Interval);
	weaponControl.add_timer(Lara.Control.Weapon.Timer);
	weaponControl.add_num_shots_fired(Lara.Control.Weapon.NumShotsFired);
	auto weaponControlOffset = weaponControl.Finish();

	Save::PlayerContextDataBuilder context{ fbb };
	context.add_calc_jump_velocity(Lara.Context.CalcJumpVelocity);
	context.add_interacted_item_number(Lara.Context.InteractedItem);
	context.add_next_corner_pose(&FromPose(Lara.Context.NextCornerPos));
	context.add_projected_floor_height(Lara.Context.ProjectedFloorHeight);
	context.add_target_orient(&FromEulerAngles(Lara.Context.TargetOrientation));
	context.add_vehicle_item_number(Lara.Context.Vehicle);
	context.add_water_current_active(Lara.Context.WaterCurrentActive);
	context.add_water_current_pull(&FromVector3i(Lara.Context.WaterCurrentPull));
	context.add_water_surface_dist(Lara.Context.WaterSurfaceDist);
	auto contextOffset = context.Finish();

	Save::LaraControlDataBuilder control{ fbb };
	control.add_can_climb_ladder(Lara.Control.CanClimbLadder);
	control.add_can_monkey_swing(Lara.Control.CanMonkeySwing);
	control.add_count(countOffset);
	control.add_hand_status((int)Lara.Control.HandStatus);
	control.add_is_climbing_ladder(Lara.Control.IsClimbingLadder);
	control.add_is_locked(Lara.Control.IsLocked);
	control.add_is_low(Lara.Control.IsLow);
	control.add_is_moving(Lara.Control.IsMoving);
	control.add_is_run_jump_queued(Lara.Control.IsRunJumpQueued);
	control.add_jump_direction((int)Lara.Control.JumpDirection);
	control.add_keep_low(Lara.Control.KeepLow);
	control.add_look(lookControlOffset);
	control.add_move_angle(Lara.Control.MoveAngle);
	control.add_rope(ropeControlOffset);
	control.add_subsuit(subsuitControlOffset);
	control.add_tightrope(tightropeControlOffset);
	control.add_turn_rate(Lara.Control.TurnRate);
	control.add_water_status((int)Lara.Control.WaterStatus);
	control.add_weapon(weaponControlOffset);
	auto controlOffset = control.Finish();

	Save::PlayerEffectDataBuilder effect{ fbb };
	effect.add_bubble_nodes(bubbleNodesOffset);
	effect.add_drip_nodes(dripNodesOffset);
	auto effectOffset = effect.Finish();

	Save::PlayerStatusDataBuilder status{ fbb };
	status.add_air(Lara.Status.Air);
	status.add_exposure(Lara.Status.Exposure);
	status.add_poison(Lara.Status.Poison);
	status.add_stamina(Lara.Status.Stamina);
	auto statusOffset = status.Finish();

	Save::CollisionInfoDataBuilder collision{ fbb };
	collision.add_last_bridge_item_number(LaraCollision.LastBridgeItemNumber);
	collision.add_last_bridge_item_pose(&FromPose(LaraCollision.LastBridgeItemPose));
	auto collisionOffset = collision.Finish();

	Save::CameraBuilder camera{ fbb };
	camera.add_position(&FromGameVector(Camera.pos));
	camera.add_target(&FromGameVector(Camera.target));
	auto cameraOffset = camera.Finish();


	std::vector<flatbuffers::Offset<Save::CarriedWeaponInfo>> carriedWeapons;
	for (int i = 0; i < (int)LaraWeaponType::NumWeapons; i++)
	{
		CarriedWeaponInfo* info = &Lara.Weapons[i];
		
		std::vector<flatbuffers::Offset<Save::AmmoInfo>> ammos;
		for (int j = 0; j < (int)WeaponAmmoType::Count; j++)
		{
			Save::AmmoInfoBuilder ammo{ fbb };
			ammo.add_count(info->Ammo[j].GetCount());
			ammo.add_is_infinite(info->Ammo[j].HasInfinite());
			auto ammoOffset = ammo.Finish();
			ammos.push_back(ammoOffset);
		}
		auto ammosOffset = fbb.CreateVector(ammos);
		
		Save::CarriedWeaponInfoBuilder serializedInfo{ fbb };
		serializedInfo.add_ammo(ammosOffset);
		serializedInfo.add_has_lasersight(info->HasLasersight);
		serializedInfo.add_has_silencer(info->HasSilencer);
		serializedInfo.add_present(info->Present);
		serializedInfo.add_selected_ammo((int)info->SelectedAmmo);
		serializedInfo.add_weapon_mode((int)info->WeaponMode);
		auto serializedInfoOffset = serializedInfo.Finish();

		carriedWeapons.push_back(serializedInfoOffset);
	}
	auto carriedWeaponsOffset = fbb.CreateVector(carriedWeapons);

	Save::LaraBuilder lara{ fbb };
	lara.add_context(contextOffset);
	lara.add_control(controlOffset);
	lara.add_effect(effectOffset);
	lara.add_extra_anim(Lara.ExtraAnim);
	lara.add_extra_head_rot(&FromEulerAngles(Lara.ExtraHeadRot));
	lara.add_extra_torso_rot(&FromEulerAngles(Lara.ExtraTorsoRot));
	lara.add_flare(flareOffset);
	lara.add_highest_location(Lara.HighestLocation);
	lara.add_hit_direction(Lara.HitDirection);
	lara.add_hit_frame(Lara.HitFrame);
	lara.add_inventory(inventoryOffset);
	lara.add_left_arm(leftArmOffset);
	lara.add_location(Lara.Location);
	lara.add_location_pad(Lara.LocationPad);
	lara.add_right_arm(rightArmOffset);
	lara.add_skin(playerSkinOffset);
	lara.add_status(statusOffset);
	lara.add_collision(collisionOffset);
	lara.add_target_arm_orient(&FromEulerAngles(Lara.TargetArmOrient));
	lara.add_target_entity_number(Lara.TargetEntity == nullptr ? -1 : Lara.TargetEntity->Index);
	lara.add_torch(torchOffset);
	lara.add_weapons(carriedWeaponsOffset);
	auto laraOffset = lara.Finish();

	std::vector<flatbuffers::Offset<Save::Room>> rooms;
	for (auto& room : g_Level.Rooms)
	{
		auto nameOffset = fbb.CreateString(room.Name);

		Save::RoomBuilder serializedInfo{ fbb };
		serializedInfo.add_name(nameOffset);
		serializedInfo.add_index(room.RoomNumber);
		serializedInfo.add_reverb_type((int)room.reverbType);
		serializedInfo.add_flags(room.flags);
		auto serializedInfoOffset = serializedInfo.Finish();

		rooms.push_back(serializedInfoOffset);
	}
	auto roomOffset = fbb.CreateVector(rooms);

	int currentItemIndex = 0;
	for (auto& itemToSerialize : g_Level.Items) 
	{
		auto luaNameOffset = fbb.CreateString(itemToSerialize.Name);
		auto luaOnKilledNameOffset = fbb.CreateString(itemToSerialize.Callbacks.OnKilled);
		auto luaOnHitNameOffset = fbb.CreateString(itemToSerialize.Callbacks.OnHit);
		auto luaOnCollidedObjectNameOffset = fbb.CreateString(itemToSerialize.Callbacks.OnObjectCollided);
		auto luaOnCollidedRoomNameOffset = fbb.CreateString(itemToSerialize.Callbacks.OnRoomCollided);

		std::vector<int> itemFlags;
		for (int i = 0; i < 7; i++)
			itemFlags.push_back(itemToSerialize.ItemFlags[i]);
		auto itemFlagsOffset = fbb.CreateVector(itemFlags);

		std::vector<int> meshPointers;
		for (auto p : itemToSerialize.Model.MeshIndex)
			meshPointers.push_back(p);
		auto meshPointerOffset = fbb.CreateVector(meshPointers);
				
		flatbuffers::Offset<Save::Creature> creatureOffset;
		flatbuffers::Offset<Save::QuadBike> quadOffset;
		flatbuffers::Offset<Save::Minecart> mineOffset;
		flatbuffers::Offset<Save::UPV> upvOffset;
		flatbuffers::Offset<Save::Kayak> kayakOffset;
		flatbuffers::Offset<Save::Pushable> pushableOffset;

		flatbuffers::Offset<Save::Short> shortOffset;
		flatbuffers::Offset<Save::Int> intOffset;

		if (Objects.CheckID(itemToSerialize.ObjectNumber, true) && 
			Objects[itemToSerialize.ObjectNumber].intelligent && itemToSerialize.IsCreature())
		{
			auto creature = GetCreatureInfo(&itemToSerialize);

			std::vector<int> jointRotations;
			for (int i = 0; i < 4; i++)
				jointRotations.push_back(creature->JointRotation[i]);
			auto jointRotationsOffset = fbb.CreateVector(jointRotations);

			Save::CreatureBuilder creatureBuilder{ fbb };
			creatureBuilder.add_alerted(creature->Alerted);
			creatureBuilder.add_can_jump(creature->LOT.CanJump);
			creatureBuilder.add_can_monkey(creature->LOT.CanMonkey);
			creatureBuilder.add_enemy(creature->Enemy == nullptr ? -1 : creature->Enemy->Index);
			creatureBuilder.add_flags(creature->Flags);
			creatureBuilder.add_friendly(creature->Friendly);
			creatureBuilder.add_head_left(creature->HeadLeft);
			creatureBuilder.add_head_right(creature->HeadRight);
			creatureBuilder.add_hurt_by_lara(creature->HurtByLara);
			creatureBuilder.add_is_amphibious(creature->LOT.IsAmphibious);
			creatureBuilder.add_is_jumping(creature->LOT.IsJumping);
			creatureBuilder.add_is_monkeying(creature->LOT.IsMonkeying);
			creatureBuilder.add_joint_rotation(jointRotationsOffset);
			creatureBuilder.add_jump_ahead(creature->JumpAhead);
			creatureBuilder.add_location_ai(creature->LocationAI);
			creatureBuilder.add_weapon_delay1(creature->MuzzleFlash[0].Delay);
			creatureBuilder.add_weapon_delay2(creature->MuzzleFlash[1].Delay);
			creatureBuilder.add_maximum_turn(creature->MaxTurn);
			creatureBuilder.add_monkey_swing_ahead(creature->MonkeySwingAhead);
			creatureBuilder.add_mood((int)creature->Mood);
			creatureBuilder.add_patrol(creature->Patrol);
			creatureBuilder.add_poisoned(creature->Poisoned);
			creatureBuilder.add_reached_goal(creature->ReachedGoal);
			creatureBuilder.add_tosspad(creature->Tosspad);
			creatureBuilder.add_ai_target_number(creature->AITargetNumber);
			creatureOffset = creatureBuilder.Finish();
		}
		else if (itemToSerialize.Data.is<QuadBikeInfo>())
		{
			auto quad = (QuadBikeInfo*)itemToSerialize.Data;

			Save::QuadBikeBuilder quadBuilder{ fbb };

			quadBuilder.add_can_start_drift(quad->CanStartDrift);
			quadBuilder.add_drift_starting(quad->DriftStarting);
			quadBuilder.add_engine_revs(quad->EngineRevs);
			quadBuilder.add_extra_rotation(quad->ExtraRotation);
			quadBuilder.add_flags(quad->Flags);
			quadBuilder.add_front_rot(quad->FrontRot);
			quadBuilder.add_left_vertical_velocity(quad->LeftVerticalVelocity);
			quadBuilder.add_momentum_angle(quad->MomentumAngle);
			quadBuilder.add_no_dismount(quad->NoDismount);
			quadBuilder.add_pitch(quad->Pitch);
			quadBuilder.add_rear_rot(quad->RearRot);
			quadBuilder.add_revs(quad->Revs);
			quadBuilder.add_right_vertical_velocity(quad->RightVerticalVelocity);
			quadBuilder.add_smoke_start(quad->SmokeStart);
			quadBuilder.add_turn_rate(quad->TurnRate);
			quadBuilder.add_velocity(quad->Velocity);
			quadOffset = quadBuilder.Finish();
		}
		else if (itemToSerialize.Data.is<UPVInfo>())
		{
			auto upv = (UPVInfo*)itemToSerialize.Data;

			Save::UPVBuilder upvBuilder{ fbb };

			upvBuilder.add_fan_rot(upv->TurbineRotation);
			upvBuilder.add_flags(upv->Flags);
			upvBuilder.add_harpoon_left(upv->HarpoonLeft);
			upvBuilder.add_harpoon_timer(upv->HarpoonTimer);
			upvBuilder.add_rot(upv->TurnRate.y);
			upvBuilder.add_velocity(upv->Velocity);
			upvBuilder.add_x_rot(upv->TurnRate.x);
			upvOffset = upvBuilder.Finish();
		}
		else if (itemToSerialize.Data.is<MinecartInfo>())
		{
			auto mine = (MinecartInfo*)itemToSerialize.Data;

			Save::MinecartBuilder mineBuilder{ fbb };

			mineBuilder.add_flags(mine->Flags);
			mineBuilder.add_floor_height_front(mine->FloorHeightFront);
			mineBuilder.add_floor_height_middle(mine->FloorHeightMiddle);
			mineBuilder.add_gradient(mine->Gradient);
			mineBuilder.add_stop_delay(mine->StopDelayTime);
			mineBuilder.add_turn_len(mine->TurnLen);
			mineBuilder.add_turn_rot(mine->TurnRot);
			mineBuilder.add_turn_x(mine->TurnX);
			mineBuilder.add_turn_z(mine->TurnZ);
			mineBuilder.add_velocity(mine->Velocity);
			mineBuilder.add_vertical_velocity(mine->VerticalVelocity);
			mineOffset = mineBuilder.Finish();
		}
		else if (itemToSerialize.Data.is<KayakInfo>())
		{
			auto kayak = (KayakInfo*)itemToSerialize.Data;

			Save::KayakBuilder kayakBuilder{ fbb };

			kayakBuilder.add_flags(kayak->Flags);
			kayakBuilder.add_forward(kayak->Forward);
			kayakBuilder.add_front_vertical_velocity(kayak->FrontVerticalVelocity);
			kayakBuilder.add_left_right_count(kayak->LeftRightPaddleCount);
			kayakBuilder.add_left_vertical_velocity(kayak->LeftVerticalVelocity);
			kayakBuilder.add_old_pos(&FromPose(kayak->OldPose));
			kayakBuilder.add_right_vertical_velocity(kayak->RightVerticalVelocity);
			kayakBuilder.add_true_water(kayak->TrueWater);
			kayakBuilder.add_turn(kayak->Turn);
			kayakBuilder.add_turn_rate(kayak->TurnRate);
			kayakBuilder.add_velocity(kayak->Velocity);
			kayakBuilder.add_water_height(kayak->WaterHeight);
			kayakOffset = kayakBuilder.Finish();
		}
		else if (itemToSerialize.Data.is<PushableInfo>())
		{
			auto pushable = (PushableInfo*)itemToSerialize.Data;

			Save::PushableBuilder pushableBuilder{ fbb };

			pushableBuilder.add_pushable_behaviour_state((int)pushable->BehaviorState);
			pushableBuilder.add_pushable_gravity(pushable->Gravity);
			pushableBuilder.add_pushable_water_force(pushable->Oscillation);

			pushableBuilder.add_pushable_stack_limit(pushable->Stack.Limit);
			pushableBuilder.add_pushable_stack_upper(pushable->Stack.ItemNumberAbove);
			pushableBuilder.add_pushable_stack_lower(pushable->Stack.ItemNumberBelow);

			pushableBuilder.add_pushable_start_x(pushable->StartPos.x);
			pushableBuilder.add_pushable_start_z(pushable->StartPos.z);
			pushableBuilder.add_pushable_room_number(pushable->StartPos.RoomNumber);

			pushableBuilder.add_pushable_collider_flag(pushable->UseBridgeCollision);

			pushableBuilder.add_pushable_north_pullable(pushable->EdgeAttribs[0].IsPullable);
			pushableBuilder.add_pushable_north_pushable(pushable->EdgeAttribs[0].IsPushable);
			pushableBuilder.add_pushable_north_climbable(pushable->EdgeAttribs[0].IsClimbable);

			pushableBuilder.add_pushable_east_pullable(pushable->EdgeAttribs[1].IsPullable);
			pushableBuilder.add_pushable_east_pushable(pushable->EdgeAttribs[1].IsPushable);
			pushableBuilder.add_pushable_east_climbable(pushable->EdgeAttribs[1].IsClimbable);

			pushableBuilder.add_pushable_south_pullable(pushable->EdgeAttribs[2].IsPullable);
			pushableBuilder.add_pushable_south_pushable(pushable->EdgeAttribs[2].IsPushable);
			pushableBuilder.add_pushable_south_climbable(pushable->EdgeAttribs[2].IsClimbable);

			pushableBuilder.add_pushable_west_pullable(pushable->EdgeAttribs[3].IsPullable);
			pushableBuilder.add_pushable_west_pushable(pushable->EdgeAttribs[3].IsPushable);
			pushableBuilder.add_pushable_west_climbable(pushable->EdgeAttribs[3].IsClimbable);

			pushableOffset = pushableBuilder.Finish();
		}
		else if (itemToSerialize.Data.is<short>())
		{
			Save::ShortBuilder sb{ fbb };
			sb.add_scalar(short(itemToSerialize.Data));
			shortOffset = sb.Finish();
		}
		else if (itemToSerialize.Data.is<int>())
		{
			Save::IntBuilder ib{ fbb };
			ib.add_scalar(int(itemToSerialize.Data));
			intOffset = ib.Finish();
		}

		Save::ItemBuilder serializedItem{ fbb };

		if (Objects.CheckID(itemToSerialize.ObjectNumber, true))
			serializedItem.add_anim_number(itemToSerialize.Animation.AnimNumber - Objects[itemToSerialize.ObjectNumber].animIndex);

		serializedItem.add_next_item(itemToSerialize.NextItem);
		serializedItem.add_next_item_active(itemToSerialize.NextActive);
		serializedItem.add_after_death(itemToSerialize.AfterDeath);
		serializedItem.add_box_number(itemToSerialize.BoxNumber);
		serializedItem.add_carried_item(itemToSerialize.CarriedItem);
		serializedItem.add_anim_object_id(itemToSerialize.Animation.AnimObjectID);
		serializedItem.add_active_state(itemToSerialize.Animation.ActiveState);
		serializedItem.add_flags(itemToSerialize.Flags);
		serializedItem.add_floor(itemToSerialize.Floor);
		serializedItem.add_frame_number(itemToSerialize.Animation.FrameNumber);
		serializedItem.add_target_state(itemToSerialize.Animation.TargetState);
		serializedItem.add_hit_points(itemToSerialize.HitPoints);
		serializedItem.add_item_flags(itemFlagsOffset);
		serializedItem.add_mesh_bits(itemToSerialize.MeshBits.ToPackedBits());
		serializedItem.add_mesh_pointers(meshPointerOffset);
		serializedItem.add_base_mesh(itemToSerialize.Model.BaseMesh);
		serializedItem.add_object_id(itemToSerialize.ObjectNumber);
		serializedItem.add_pose(&FromPose(itemToSerialize.Pose));
		serializedItem.add_required_state(itemToSerialize.Animation.RequiredState);
		serializedItem.add_room_number(itemToSerialize.RoomNumber);
		serializedItem.add_velocity(&FromVector3(itemToSerialize.Animation.Velocity));
		serializedItem.add_timer(itemToSerialize.Timer);
		serializedItem.add_color(&FromVector4(itemToSerialize.Model.Color));
		serializedItem.add_touch_bits(itemToSerialize.TouchBits.ToPackedBits());
		serializedItem.add_trigger_flags(itemToSerialize.TriggerFlags);
		serializedItem.add_active(itemToSerialize.Active);
		serializedItem.add_status(itemToSerialize.Status);
		serializedItem.add_is_airborne(itemToSerialize.Animation.IsAirborne);
		serializedItem.add_hit_stauts(itemToSerialize.HitStatus);
		serializedItem.add_ai_bits(itemToSerialize.AIBits);
		serializedItem.add_collidable(itemToSerialize.Collidable);
		serializedItem.add_looked_at(itemToSerialize.LookedAt);
		serializedItem.add_effect_type((int)itemToSerialize.Effect.Type);
		serializedItem.add_effect_light_colour(&FromVector3(itemToSerialize.Effect.LightColor));
		serializedItem.add_effect_primary_colour(&FromVector3(itemToSerialize.Effect.PrimaryEffectColor));
		serializedItem.add_effect_secondary_colour(&FromVector3(itemToSerialize.Effect.SecondaryEffectColor));
		serializedItem.add_effect_count(itemToSerialize.Effect.Count);

		if (Objects.CheckID(itemToSerialize.ObjectNumber, true) && 
			Objects[itemToSerialize.ObjectNumber].intelligent && itemToSerialize.Data.is<CreatureInfo>())
		{
			serializedItem.add_data_type(Save::ItemData::Creature);
			serializedItem.add_data(creatureOffset.Union());
		}
		else if (itemToSerialize.Data.is<QuadBikeInfo>())
		{
			serializedItem.add_data_type(Save::ItemData::QuadBike);
			serializedItem.add_data(quadOffset.Union());
		}
		else if (itemToSerialize.Data.is<UPVInfo>())
		{
			serializedItem.add_data_type(Save::ItemData::UPV);
			serializedItem.add_data(upvOffset.Union());
		}
		else if (itemToSerialize.Data.is<MinecartInfo>())
		{
			serializedItem.add_data_type(Save::ItemData::Minecart);
			serializedItem.add_data(mineOffset.Union());
		}
		else if (itemToSerialize.Data.is<KayakInfo>())
		{
			serializedItem.add_data_type(Save::ItemData::Kayak);
			serializedItem.add_data(kayakOffset.Union());
		}
		else if (itemToSerialize.Data.is<PushableInfo>())
		{
			serializedItem.add_data_type(Save::ItemData::Pushable);
			serializedItem.add_data(pushableOffset.Union());
		}
		else if (itemToSerialize.Data.is<short>())
		{
			serializedItem.add_data_type(Save::ItemData::Short);
			serializedItem.add_data(shortOffset.Union());
		}
		else if (itemToSerialize.Data.is<int>())
		{
			serializedItem.add_data_type(Save::ItemData::Int);
			serializedItem.add_data(intOffset.Union());
		}

		serializedItem.add_lua_name(luaNameOffset);
		serializedItem.add_lua_on_killed_name(luaOnKilledNameOffset);
		serializedItem.add_lua_on_hit_name(luaOnHitNameOffset);
		serializedItem.add_lua_on_collided_with_object_name(luaOnCollidedObjectNameOffset);
		serializedItem.add_lua_on_collided_with_room_name(luaOnCollidedRoomNameOffset);

		auto serializedItemOffset = serializedItem.Finish();
		serializedItems.push_back(serializedItemOffset);

		currentItemIndex++;
	}
	auto serializedItemsOffset = fbb.CreateVector(serializedItems);

	std::vector<flatbuffers::Offset<Save::FishData>> fishSwarm;
	for (const auto& fish : FishSwarm)
	{
		Save::FishDataBuilder fishSave{ fbb };
		fishSave.add_is_lethal(fish.IsLethal);
		fishSave.add_is_patrolling(fish.IsPatrolling);
		fishSave.add_leader_item_number((fish.LeaderItemPtr == nullptr) ? -1 : fish.LeaderItemPtr->Index);
		fishSave.add_life(fish.Life);
		fishSave.add_mesh_index(fish.MeshIndex);
		fishSave.add_orientation(&FromEulerAngles(fish.Orientation));
		fishSave.add_position(&FromVector3(fish.Position));
		fishSave.add_position_target(&FromVector3(fish.PositionTarget));
		fishSave.add_room_number(fish.RoomNumber);
		fishSave.add_target_item_number((fish.TargetItemPtr == nullptr) ? -1 : fish.TargetItemPtr->Index);
		fishSave.add_undulation(fish.Undulation);
		fishSave.add_velocity(fish.Velocity);

		auto fishSaveOffset = fishSave.Finish();
		fishSwarm.push_back(fishSaveOffset);
	}
	auto fishSwarmOffset = fbb.CreateVector(fishSwarm);

	std::vector<flatbuffers::Offset<Save::FireflyData>> fireflySwarm;
	for (const auto& firefly : FireflySwarm)
	{
		Save::FireflyDataBuilder fireflySave{ fbb };
		fireflySave.add_sprite_index(firefly.SpriteSeqID);
		fireflySave.add_sprite_id(firefly.SpriteID);
		fireflySave.add_blend_mode((int)firefly.blendMode);
		fireflySave.add_scalar(firefly.scalar);
		fireflySave.add_position(&FromVector3(firefly.Position));
		fireflySave.add_room_number(firefly.RoomNumber);
		fireflySave.add_position_target(&FromVector3(firefly.PositionTarget));
		fireflySave.add_orientation(&FromEulerAngles(firefly.Orientation));
		fireflySave.add_velocity(firefly.Velocity);
		fireflySave.add_target_item_number((firefly.TargetItemPtr == nullptr) ? -1 : firefly.TargetItemPtr->Index);
		fireflySave.add_z_vel(firefly.zVel);
		fireflySave.add_life(firefly.Life);
		fireflySave.add_number(firefly.Number);
		fireflySave.add_d_r(firefly.rB);
		fireflySave.add_d_g(firefly.gB);
		fireflySave.add_d_b(firefly.bB);
		fireflySave.add_r(firefly.r);
		fireflySave.add_g(firefly.g);
		fireflySave.add_b(firefly.b);
		fireflySave.add_on(firefly.on);
		fireflySave.add_size(firefly.size);
		fireflySave.add_rot_Ang(firefly.rotAng);

		auto fireflySaveOffset = fireflySave.Finish();
		fireflySwarm.push_back(fireflySaveOffset);
	}
	auto fireflySwarmOffset = fbb.CreateVector(fireflySwarm);

	// TODO: In future, we should save only active FX, not whole array.
	// This may come together with Monty's branch merge -- Lwmte, 10.07.22

	std::vector<flatbuffers::Offset<Save::FXInfo>> serializedEffects{};
	for (auto& effectToSerialize : EffectList)
	{
		Save::FXInfoBuilder serializedEffect{ fbb };

		serializedEffect.add_pose(&FromPose(effectToSerialize.pos));
		serializedEffect.add_room_number(effectToSerialize.roomNumber);
		serializedEffect.add_object_number(effectToSerialize.objectNumber);
		serializedEffect.add_next_fx(effectToSerialize.nextFx);
		serializedEffect.add_next_active(effectToSerialize.nextActive);
		serializedEffect.add_speed(effectToSerialize.speed);
		serializedEffect.add_fall_speed(effectToSerialize.fallspeed);
		serializedEffect.add_frame_number(effectToSerialize.frameNumber);
		serializedEffect.add_counter(effectToSerialize.counter);
		serializedEffect.add_color(&FromVector4(effectToSerialize.color));
		serializedEffect.add_flag1(effectToSerialize.flag1);
		serializedEffect.add_flag2(effectToSerialize.flag2);

		auto serializedEffectOffset = serializedEffect.Finish();
		serializedEffects.push_back(serializedEffectOffset);
	}
	auto serializedEffectsOffset = fbb.CreateVector(serializedEffects);

	// Soundtrack playheads
	std::vector<flatbuffers::Offset<Save::Soundtrack>> soundtracks;
	for (int j = 0; j < (int)SoundTrackType::Count; j++)
	{
		auto trackData = GetSoundTrackNameAndPosition((SoundTrackType)j);
		auto nameOffset = fbb.CreateString(trackData.first);

		Save::SoundtrackBuilder track{ fbb };
		track.add_name(nameOffset);
		track.add_position(trackData.second);
		soundtracks.push_back(track.Finish());
	}
	auto soundtrackOffset = fbb.CreateVector(soundtracks);

	// Legacy soundtrack map
	std::vector<int> soundTrackMap;
	for (auto& track : SoundTracks) 
	{
		soundTrackMap.push_back(track.first);
		soundTrackMap.push_back(track.second.Mask); 
	}
	auto soundtrackMapOffset = fbb.CreateVector(soundTrackMap);

	// Action queue
	std::vector<int> actionQueue;
	for (int i = 0; i < ActionQueueMap.size(); i++)
		actionQueue.push_back((int)ActionQueueMap[(InputActionID)i]);
	auto actionQueueOffset = fbb.CreateVector(actionQueue);

	// Flipmaps
	std::vector<int> flipMaps;
	for (int i = 0; i < MAX_FLIPMAP; i++)
		flipMaps.push_back(FlipMap[i] >> 8);
	auto flipMapsOffset = fbb.CreateVector(flipMaps);

	std::vector<int> flipStats;
	for (int i = 0; i < MAX_FLIPMAP; i++)
		flipStats.push_back(FlipStats[i]);
	auto flipStatsOffset = fbb.CreateVector(flipStats);

	std::vector<int> roomItems;
	for (auto const& r : g_Level.Rooms)
		roomItems.push_back(r.itemNumber);
	auto roomItemsOffset = fbb.CreateVector(roomItems);

	// Cameras
	std::vector<flatbuffers::Offset<Save::FixedCamera>> cameras;
	for (int i = 0; i < g_Level.Cameras.size(); i++)
	{
		Save::FixedCameraBuilder camera{ fbb };
		camera.add_flags(g_Level.Cameras[i].Flags);
		cameras.push_back(camera.Finish());
	}
	auto camerasOffset = fbb.CreateVector(cameras);

	// Sinks
	std::vector<flatbuffers::Offset<Save::Sink>> sinks;
	for (int i = 0; i < g_Level.Sinks.size(); i++)
	{
		Save::SinkBuilder sink{ fbb };
		sink.add_flags(g_Level.Sinks[i].Strength);
		sinks.push_back(sink.Finish());
	}
	auto sinksOffset = fbb.CreateVector(sinks);

	// Flyby cameras
	std::vector<flatbuffers::Offset<Save::FlyByCamera>> flybyCameras;
	for (int i = 0; i < NumberSpotcams; i++)
	{
		Save::FlyByCameraBuilder flyby{ fbb };
		flyby.add_flags(SpotCam[i].flags);
		flybyCameras.push_back(flyby.Finish());
	}
	auto flybyCamerasOffset = fbb.CreateVector(flybyCameras);

	// Static meshes and volumes
	std::vector<flatbuffers::Offset<Save::StaticMeshInfo>> staticMeshes;
	std::vector<flatbuffers::Offset<Save::Volume>> volumes;
	for (int i = 0; i < g_Level.Rooms.size(); i++)
	{
		auto* room = &g_Level.Rooms[i];

		for (int j = 0; j < room->mesh.size(); j++)
		{
			Save::StaticMeshInfoBuilder staticMesh{ fbb };

			staticMesh.add_pose(&FromPose(room->mesh[j].pos));
			staticMesh.add_scale(room->mesh[j].scale);
			staticMesh.add_color(&FromVector4(room->mesh[j].color));

			staticMesh.add_flags(room->mesh[j].flags);
			staticMesh.add_hit_points(room->mesh[j].HitPoints);
			staticMesh.add_room_number(room->RoomNumber);
			staticMesh.add_number(j);
			staticMeshes.push_back(staticMesh.Finish());
		}

		for (int j = 0; j < room->TriggerVolumes.size(); j++)
		{
			auto& currVolume = room->TriggerVolumes[j];

			auto queue = std::vector<flatbuffers::Offset<Save::VolumeState>>{};
			for (int k = 0; k < currVolume.StateQueue.size(); k++)
			{
				auto& entry = currVolume.StateQueue[k];

				int activator = NO_VALUE;
				if (std::holds_alternative<int>(entry.Activator))
				{
					activator = std::get<int>(entry.Activator);
				}
				else
				{
					continue;
				}

				Save::VolumeStateBuilder volstate{ fbb };
				volstate.add_status((int)entry.Status);
				volstate.add_activator(activator);
				volstate.add_timestamp(entry.Timestamp);
				queue.push_back(volstate.Finish());
			}

			auto queueOffset = fbb.CreateVector(queue);
			auto nameOffset = fbb.CreateString(currVolume.Name);

			Save::VolumeBuilder volume{ fbb };
			volume.add_room_number(room->RoomNumber);
			volume.add_number(j);
			volume.add_name(nameOffset);
			volume.add_enabled(currVolume.Enabled);
			volume.add_position(&FromVector3(currVolume.Box.Center));
			volume.add_rotation(&FromVector4(currVolume.Box.Orientation));
			volume.add_scale(&FromVector3(currVolume.Box.Extents));
			volume.add_queue(queueOffset);
			volumes.push_back(volume.Finish());
		}
	}
	auto staticMeshesOffset = fbb.CreateVector(staticMeshes);
	auto volumesOffset = fbb.CreateVector(volumes);

	// Level state
	auto* level = (Level*)g_GameFlow->GetLevel(CurrentLevel);
	Save::LevelDataBuilder levelData { fbb };

	levelData.add_level_far_view(level->LevelFarView);

	levelData.add_fog_color(level->Fog.GetColor());
	levelData.add_fog_min_distance(level->Fog.MinDistance);
	levelData.add_fog_max_distance(level->Fog.MaxDistance);

	levelData.add_sky_layer_1_enabled(level->GetSkyLayerEnabled(0));
	levelData.add_sky_layer_1_color(level->GetSkyLayerColor(0));
	levelData.add_sky_layer_1_speed(level->GetSkyLayerSpeed(0));

	levelData.add_sky_layer_2_enabled(level->GetSkyLayerEnabled(1));
	levelData.add_sky_layer_2_color(level->GetSkyLayerColor(1));
	levelData.add_sky_layer_2_speed(level->GetSkyLayerSpeed(1));

	levelData.add_lensflare_color(level->LensFlare.GetColor());
	levelData.add_lensflare_pitch(level->LensFlare.GetPitch());
	levelData.add_lensflare_yaw(level->LensFlare.GetYaw());
	levelData.add_lensflare_sprite_id(level->LensFlare.GetSunSpriteID());

	levelData.add_starfield_meteor_count(level->Starfield.GetMeteorCount());
	levelData.add_starfield_meteor_spawn_density(level->Starfield.GetMeteorSpawnDensity());
	levelData.add_starfield_meteor_velocity(level->Starfield.GetMeteorVelocity());
	levelData.add_starfield_star_count(level->Starfield.GetStarCount());

	levelData.add_horizon1_enabled(level->Horizon1.GetEnabled());
	levelData.add_horizon1_object_id(level->Horizon1.GetObjectID());
	levelData.add_horizon1_position(&FromVector3(level->GetHorizonPosition(0)));
	levelData.add_horizon1_orientation(&FromEulerAngles(level->GetHorizonOrientation(0)));
	levelData.add_horizon1_transparency(level->Horizon1.GetTransparency());

	levelData.add_horizon2_enabled(level->Horizon2.GetEnabled());
	levelData.add_horizon2_object_id(level->Horizon2.GetObjectID());
	levelData.add_horizon2_position(&FromVector3(level->GetHorizonPosition(1)));
	levelData.add_horizon2_orientation(&FromEulerAngles(level->GetHorizonOrientation(1)));
	levelData.add_horizon2_transparency(level->Horizon2.GetTransparency());

	levelData.add_storm_enabled(level->Storm);
	levelData.add_rumble_enabled(level->Rumble);
	levelData.add_weather_type((int)level->Weather);
	levelData.add_weather_strength(level->WeatherStrength);

	auto levelDataOffset = levelData.Finish();

	// Global event sets
	std::vector<flatbuffers::Offset<Save::EventSet>> globalEventSets{};
	for (int j = 0; j < g_Level.GlobalEventSets.size(); j++)
	{
		std::vector<bool> statuses = {};
		std::vector<int> callCounters = {};

		for (int k = 0; k < g_Level.GlobalEventSets[j].Events.size(); k++)
		{
			statuses.push_back(g_Level.GlobalEventSets[j].Events[k].Enabled);
			callCounters.push_back(g_Level.GlobalEventSets[j].Events[k].CallCounter);
		}

		auto vecStatuses = fbb.CreateVector(statuses);
		auto vecCounters = fbb.CreateVector(callCounters);

		Save::EventSetBuilder eventSet{ fbb };

		eventSet.add_index(j);
		eventSet.add_statuses(vecStatuses);
		eventSet.add_call_counters(vecCounters);

		globalEventSets.push_back(eventSet.Finish());
	}
	auto globalEventSetsOffset = fbb.CreateVector(globalEventSets);

	// Volume event sets
	std::vector<flatbuffers::Offset<Save::EventSet>> volumeEventSets{};
	for (int j = 0; j < g_Level.VolumeEventSets.size(); j++)
	{
		std::vector<bool> statuses = {};
		std::vector<int> callCounters = {};

		for (int k = 0; k < g_Level.VolumeEventSets[j].Events.size(); k++)
		{
			statuses.push_back(g_Level.VolumeEventSets[j].Events[k].Enabled);
			callCounters.push_back(g_Level.VolumeEventSets[j].Events[k].CallCounter);
		}

		auto vecStatuses = fbb.CreateVector(statuses);
		auto vecCounters = fbb.CreateVector(callCounters);

		Save::EventSetBuilder eventSet{ fbb };

		eventSet.add_index(j);
		eventSet.add_statuses(vecStatuses);
		eventSet.add_call_counters(vecCounters);

		volumeEventSets.push_back(eventSet.Finish());
	}
	auto volumeEventSetsOffset = fbb.CreateVector(volumeEventSets);

	// Particles
	std::vector<flatbuffers::Offset<Save::ParticleInfo>> particles;
	for (int i = 0; i < MAX_PARTICLES; i++)
	{
		auto* particle = &Particles[i];

		if (!particle->on)
			continue;

		Save::ParticleInfoBuilder particleInfo{ fbb };

		particleInfo.add_animation_type(particle->animationType);
		particleInfo.add_b(particle->b);
		particleInfo.add_col_fade_speed(particle->colFadeSpeed);
		particleInfo.add_d_b(particle->dB);
		particleInfo.add_sprite_index(particle->SpriteSeqID);
		particleInfo.add_sprite_id(particle->SpriteID);
		particleInfo.add_damage(particle->damage);
		particleInfo.add_d_g(particle->dG);
		particleInfo.add_d_r(particle->dR);
		particleInfo.add_d_size(particle->dSize);
		particleInfo.add_dynamic(particle->dynamic);
		particleInfo.add_extras(particle->extras);
		particleInfo.add_fade_to_black(particle->fadeToBlack);
		particleInfo.add_flags(particle->flags);
		particleInfo.add_framerate(particle->framerate);
		particleInfo.add_friction(particle->friction);
		particleInfo.add_fx_obj(particle->fxObj);
		particleInfo.add_g(particle->g);
		particleInfo.add_gravity(particle->gravity);
		particleInfo.add_life(particle->life);
		particleInfo.add_light_radius(particle->lightRadius);
		particleInfo.add_light_flicker(particle->lightFlicker);
		particleInfo.add_light_flicker_s(particle->lightFlickerS);
		particleInfo.add_max_y_vel(particle->maxYvel);
		particleInfo.add_node_number(particle->nodeNumber);
		particleInfo.add_on(particle->on);
		particleInfo.add_r(particle->r);
		particleInfo.add_room_number(particle->roomNumber);
		particleInfo.add_rot_add(particle->rotAdd);
		particleInfo.add_rot_ang(particle->rotAng);
		particleInfo.add_s_b(particle->sB);
		particleInfo.add_scalar(particle->scalar);
		particleInfo.add_s_g(particle->sG);
		particleInfo.add_size(particle->size);
		particleInfo.add_s_life(particle->sLife);
		particleInfo.add_s_r(particle->sR);
		particleInfo.add_s_size(particle->sSize);
		particleInfo.add_sound(particle->sound);
		particleInfo.add_blend_mode((int)particle->blendMode);
		particleInfo.add_x(particle->x);
		particleInfo.add_x_vel(particle->xVel);
		particleInfo.add_y(particle->y);
		particleInfo.add_y_vel(particle->yVel);
		particleInfo.add_z(particle->z);
		particleInfo.add_z_vel(particle->zVel);
		particleInfo.add_target_pos(&FromVector3(particle->targetPos));

		particles.push_back(particleInfo.Finish());
	}
	auto particleOffset = fbb.CreateVector(particles);

	// Swarm enemies
	std::vector<flatbuffers::Offset<Save::SwarmObjectInfo>> bats;
	for (int i = 0; i < NUM_BATS; i++)
	{
		auto* bat = &Bats[i];

		Save::SwarmObjectInfoBuilder batInfo{ fbb };

		batInfo.add_flags(bat->Counter);
		batInfo.add_on(bat->On);
		batInfo.add_room_number(bat->RoomNumber);
		batInfo.add_pose(&FromPose(bat->Pose));

		bats.push_back(batInfo.Finish());
	}
	auto batsOffset = fbb.CreateVector(bats);

	std::vector<flatbuffers::Offset<Save::SwarmObjectInfo>> spiders;
	for (int i = 0; i < NUM_SPIDERS; i++)
	{
		auto* spider = &Spiders[i];

		Save::SwarmObjectInfoBuilder spiderInfo{ fbb };

		spiderInfo.add_flags(spider->Flags);
		spiderInfo.add_on(spider->On);
		spiderInfo.add_room_number(spider->RoomNumber);
		spiderInfo.add_pose(&FromPose(spider->Pose));

		spiders.push_back(spiderInfo.Finish());
	}
	auto spidersOffset = fbb.CreateVector(spiders);

	std::vector<flatbuffers::Offset<Save::SwarmObjectInfo>> rats;
	for (int i = 0; i < NUM_RATS; i++)
	{
		auto* rat = &Rats[i];

		Save::SwarmObjectInfoBuilder ratInfo{ fbb };

		ratInfo.add_flags(rat->Flags);
		ratInfo.add_on(rat->On);
		ratInfo.add_room_number(rat->RoomNumber);
		ratInfo.add_pose(&FromPose(rat->Pose));

		rats.push_back(ratInfo.Finish());
	}
	auto ratsOffset = fbb.CreateVector(rats);

	std::vector<flatbuffers::Offset<Save::SwarmObjectInfo>> scarabs;
	for (int i = 0; i < NUM_BATS; i++)
	{
		auto* beetle = &BeetleSwarm[i];

		Save::SwarmObjectInfoBuilder scarabInfo{ fbb };

		scarabInfo.add_flags(beetle->Flags);
		scarabInfo.add_on(beetle->On);
		scarabInfo.add_room_number(beetle->RoomNumber);
		scarabInfo.add_pose(&FromPose(beetle->Pose));

		scarabs.push_back(scarabInfo.Finish());
	}
	auto scarabsOffset = fbb.CreateVector(scarabs);

	// Rope
	flatbuffers::Offset<Save::Rope> ropeOffset;
	flatbuffers::Offset<Save::Pendulum> pendulumOffset;
	flatbuffers::Offset<Save::Pendulum> alternatePendulumOffset;

	if (Lara.Control.Rope.Ptr != -1)
	{
		ROPE_STRUCT* rope = &Ropes[Lara.Control.Rope.Ptr];

		std::vector<flatbuffers::Offset<Save::RopeSegment>> segments;
		for (int i = 0; i < ROPE_SEGMENTS; i++)
		{
			Save::RopeSegmentBuilder segment{ fbb };

			segment.add_segment(&FromVector3i(rope->segment[i]));
			segment.add_velocity(&FromVector3i(rope->velocity[i]));
			segment.add_normalised_segment(&FromVector3i(rope->normalisedSegment[i]));
			segment.add_mesh_segment(&FromVector3i(rope->meshSegment[i]));
			segment.add_coord(&FromVector3i(rope->coords[i]));

			segments.push_back(segment.Finish());
		}
		auto ropeSegmentsOffset = fbb.CreateVector(segments);

		Save::RopeBuilder ropeInfo{ fbb };

		ropeInfo.add_segments(ropeSegmentsOffset);
		ropeInfo.add_coiled(rope->coiled);
		ropeInfo.add_position(&FromVector3i(rope->position));
		ropeInfo.add_segment_length(rope->segmentLength);

		ropeOffset = ropeInfo.Finish();

		Save::PendulumBuilder pendulumInfo{ fbb };
		pendulumInfo.add_node(CurrentPendulum.node);
		pendulumInfo.add_position(&FromVector3i(CurrentPendulum.position));
		pendulumInfo.add_velocity(&FromVector3i(CurrentPendulum.velocity));
		pendulumOffset = pendulumInfo.Finish();

		Save::PendulumBuilder alternatePendulumInfo{ fbb };
		alternatePendulumInfo.add_node(AlternatePendulum.node);
		alternatePendulumInfo.add_position(&FromVector3i(AlternatePendulum.position));
		alternatePendulumInfo.add_velocity(&FromVector3i(AlternatePendulum.velocity));
		alternatePendulumOffset = alternatePendulumInfo.Finish();
	}

	std::vector<SavedVar> savedVars;

	g_GameScript->GetVariables(savedVars);
	
	std::vector<flatbuffers::Offset<Save::UnionTable>> varsVec;
	for (auto const& s : savedVars)
	{
		auto putDataInVec = [&varsVec, &fbb](Save::VarUnion type, auto const & offsetVar)
		{
			Save::UnionTableBuilder ut{ fbb };
			ut.add_u_type(type);
			ut.add_u(offsetVar.Union());
			varsVec.push_back(ut.Finish());
		};

		if (std::holds_alternative<std::string>(s))
		{
			auto strOffset2 = fbb.CreateString(std::get<std::string>(s));
			Save::stringTableBuilder stb{ fbb };
			stb.add_str(strOffset2);
			auto strOffset = stb.Finish();

			putDataInVec(Save::VarUnion::str, strOffset);
		}
		else if (std::holds_alternative<double>(s))
		{
			Save::doubleTableBuilder dtb{ fbb };
			dtb.add_scalar(std::get<double>(s));
			auto doubleOffset = dtb.Finish();

			putDataInVec(Save::VarUnion::num, doubleOffset);
		}
		else if (std::holds_alternative<bool>(s))
		{
			Save::boolTableBuilder btb{ fbb };
			btb.add_scalar(std::get<bool>(s));
			auto boolOffset = btb.Finish();

			putDataInVec(Save::VarUnion::boolean, boolOffset);
		}
		else if (std::holds_alternative<IndexTable>(s))
		{
			std::vector<Save::KeyValPair> keyValVec;
			auto& vec = std::get<IndexTable>(s);
			for (auto& id : vec)
			{
				keyValVec.push_back(Save::KeyValPair(id.first, id.second));
			}

			auto vecOffset = fbb.CreateVectorOfStructs(keyValVec);
			Save::ScriptTableBuilder stb{ fbb };
			stb.add_keys_vals(vecOffset);
			auto scriptTableOffset = stb.Finish();

			putDataInVec(Save::VarUnion::tab, scriptTableOffset);
		}
		else if (std::holds_alternative<FuncName>(s))
		{
			std::string data = std::get<FuncName>(s).name;
			auto strOffset = fbb.CreateString(data);
			Save::funcNameTableBuilder ftb{ fbb };
			ftb.add_str(strOffset);
			auto funcNameOffset = ftb.Finish();

			putDataInVec(Save::VarUnion::funcName, funcNameOffset);
		}
		else
		{
			switch (SavedVarType(s.index()))
			{
			case SavedVarType::Vec2:
				{
					SaveVec(SavedVarType::Vec2, s, Save::vec2TableBuilder, Save::VarUnion::vec2, Save::Vector2, FromVector2);
					break;
				}
				
			case SavedVarType::Vec3:
				{
					SaveVec(SavedVarType::Vec3, s, Save::vec3TableBuilder, Save::VarUnion::vec3, Save::Vector3, FromVector3);
					break;
				}

			case SavedVarType::Rotation:
				{
					SaveVec(SavedVarType::Rotation, s, Save::rotationTableBuilder, Save::VarUnion::rotation, Save::Vector3, FromVector3);
					break;
				}

			case SavedVarType::Time:
				{
					Save::timeTableBuilder ttb{ fbb };
					ttb.add_scalar(std::get<int>(s));
					auto timeOffset = ttb.Finish();

					putDataInVec(Save::VarUnion::time, timeOffset);
					break;
				}

			case SavedVarType::Color:
				{
					Save::colorTableBuilder ctb{ fbb };
					ctb.add_color(std::get<(int)SavedVarType::Color>(s));
					auto offset = ctb.Finish();

					putDataInVec(Save::VarUnion::color, offset);
					break;
				}
			}
		}
	}

	auto unionVec = fbb.CreateVector(varsVec);
	Save::UnionVecBuilder uvb{ fbb };
	uvb.add_members(unionVec);
	auto unionVecOffset = uvb.Finish();

	std::vector<std::string> callbackVecPreStart;
	std::vector<std::string> callbackVecPostStart;

	std::vector<std::string> callbackVecPreEnd;
	std::vector<std::string> callbackVecPostEnd;

	std::vector<std::string> callbackVecPreSave;
	std::vector<std::string> callbackVecPostSave;

	std::vector<std::string> callbackVecPreLoad;
	std::vector<std::string> callbackVecPostLoad;

	std::vector<std::string> callbackVecPreLoop;
	std::vector<std::string> callbackVecPostLoop;

	std::vector<std::string> callbackVecPreUseItem;
	std::vector<std::string> callbackVecPostUseItem;

	std::vector<std::string> callbackVecPreFreeze;
	std::vector<std::string> callbackVecPostFreeze;

	g_GameScript->GetCallbackStrings(
		callbackVecPreStart,
		callbackVecPostStart,
		callbackVecPreEnd,
		callbackVecPostEnd,
		callbackVecPreSave,
		callbackVecPostSave,
		callbackVecPreLoad,
		callbackVecPostLoad,
		callbackVecPreLoop,
		callbackVecPostLoop,
		callbackVecPreUseItem,
		callbackVecPostUseItem,
		callbackVecPreFreeze,
		callbackVecPostFreeze);

	auto stringsCallbackPreStart = fbb.CreateVectorOfStrings(callbackVecPreStart);
	auto stringsCallbackPostStart = fbb.CreateVectorOfStrings(callbackVecPostStart);
	auto stringsCallbackPreEnd = fbb.CreateVectorOfStrings(callbackVecPreEnd);
	auto stringsCallbackPostEnd = fbb.CreateVectorOfStrings(callbackVecPostEnd);
	auto stringsCallbackPreSave = fbb.CreateVectorOfStrings(callbackVecPreSave);
	auto stringsCallbackPostSave = fbb.CreateVectorOfStrings(callbackVecPostSave);
	auto stringsCallbackPreLoad = fbb.CreateVectorOfStrings(callbackVecPreLoad);
	auto stringsCallbackPostLoad = fbb.CreateVectorOfStrings(callbackVecPostLoad);
	auto stringsCallbackPreLoop = fbb.CreateVectorOfStrings(callbackVecPreLoop);
	auto stringsCallbackPostLoop = fbb.CreateVectorOfStrings(callbackVecPostLoop);
	auto stringsCallbackPreUseItem = fbb.CreateVectorOfStrings(callbackVecPreUseItem);
	auto stringsCallbackPostUseItem = fbb.CreateVectorOfStrings(callbackVecPostUseItem);
	auto stringsCallbackPreFreeze = fbb.CreateVectorOfStrings(callbackVecPreFreeze);
	auto stringsCallbackPostFreeze = fbb.CreateVectorOfStrings(callbackVecPostFreeze);

	Save::SaveGameBuilder sgb{ fbb };

	sgb.add_header(headerOffset);
	sgb.add_level(levelStatisticsOffset);
	sgb.add_game(gameStatisticsOffset);
	sgb.add_level_data(levelDataOffset);
	sgb.add_secret_bits(SaveGame::Statistics.SecretBits);
	sgb.add_camera(cameraOffset);
	sgb.add_lara(laraOffset);
	sgb.add_rooms(roomOffset);
	sgb.add_next_item_free(NextItemFree);
	sgb.add_next_item_active(NextItemActive);
	sgb.add_items(serializedItemsOffset);
	sgb.add_fish_swarm(fishSwarmOffset);
	sgb.add_firefly_swarm(fireflySwarmOffset);
	sgb.add_fxinfos(serializedEffectsOffset);
	sgb.add_next_fx_free(NextFxFree);
	sgb.add_next_fx_active(NextFxActive);
	sgb.add_postprocess_mode((int)g_Renderer.GetPostProcessMode());
	sgb.add_postprocess_strength(g_Renderer.GetPostProcessStrength());
	sgb.add_postprocess_tint(&FromVector3(g_Renderer.GetPostProcessTint()));
	sgb.add_soundtracks(soundtrackOffset);

	sgb.add_cd_flags(soundtrackMapOffset);
	sgb.add_action_queue(actionQueueOffset);
	sgb.add_flip_maps(flipMapsOffset);
	sgb.add_flip_stats(flipStatsOffset);
	sgb.add_room_items(roomItemsOffset);
	sgb.add_flip_effect(FlipEffect);
	sgb.add_flip_status(FlipStatus);
	sgb.add_current_fov(LastFOV);
	sgb.add_last_inv_item(g_Gui.GetLastInventoryItem());
	sgb.add_static_meshes(staticMeshesOffset);
	sgb.add_volumes(volumesOffset);
	sgb.add_fixed_cameras(camerasOffset);
	sgb.add_particles(particleOffset);
	sgb.add_bats(batsOffset);
	sgb.add_rats(ratsOffset);
	sgb.add_spiders(spidersOffset);
	sgb.add_scarabs(scarabsOffset);
	sgb.add_sinks(sinksOffset);
	sgb.add_flyby_cameras(flybyCamerasOffset);
	sgb.add_global_event_sets(globalEventSetsOffset);
	sgb.add_volume_event_sets(volumeEventSetsOffset);

	if (Lara.Control.Rope.Ptr != -1)
	{
		sgb.add_rope(ropeOffset);
		sgb.add_pendulum(pendulumOffset);
		sgb.add_alternate_pendulum(alternatePendulumOffset);
	}

	sgb.add_script_vars(unionVecOffset);

	sgb.add_callbacks_pre_start(stringsCallbackPreStart);
	sgb.add_callbacks_post_start(stringsCallbackPostStart);

	sgb.add_callbacks_pre_end(stringsCallbackPreEnd);
	sgb.add_callbacks_post_end(stringsCallbackPostEnd);

	sgb.add_callbacks_pre_save(stringsCallbackPreSave);
	sgb.add_callbacks_post_save(stringsCallbackPostSave);

	sgb.add_callbacks_pre_load(stringsCallbackPreLoad);
	sgb.add_callbacks_post_load(stringsCallbackPostLoad);

	sgb.add_callbacks_pre_loop(stringsCallbackPreLoop);
	sgb.add_callbacks_post_loop(stringsCallbackPostLoop);

	sgb.add_callbacks_pre_useitem(stringsCallbackPreUseItem);
	sgb.add_callbacks_post_useitem(stringsCallbackPostUseItem);

	sgb.add_callbacks_pre_freeze(stringsCallbackPreFreeze);
	sgb.add_callbacks_post_freeze(stringsCallbackPostFreeze);

	auto sg = sgb.Finish();
	fbb.Finish(sg);

	auto buffer = fbb.GetBufferPointer();
	auto size   = fbb.GetSize();

	auto result = std::vector<byte>(buffer, buffer + size);
	return result;
}

void SaveGame::SaveHub(int index)
{
	// Don't save title level to a hub.
	if (index == 0)
		return;

	// Build hub data.
	TENLog("Saving hub data for level #" + std::to_string(index) + (IsOnHub(index) ? " (overwrite)" : " (new)"), LogLevel::Info);
	Hub[index] = Build();
}

void SaveGame::LoadHub(int index)
{
	// Don't attempt to load hub data if level is a title level.
	if (index == 0)
		return;

	if (IsOnHub(index))
	{
		// Load hub data.
		TENLog("Loading hub data for level #" + std::to_string(index), LogLevel::Info);
		Parse(Hub[index], true);
	}

	// Restore vehicle (also for cases when no hub data yet exists).
	InitializePlayerVehicle(*LaraItem);
}

bool SaveGame::IsOnHub(int index)
{
	return (Hub.count(index) > 0);
}

void SaveGame::ResetHub()
{
	if (Hub.empty())
		return;

	TENLog("Clearing hub data", LogLevel::Info);
	Hub.clear();
}

bool SaveGame::Save(int slot)
{
	if (!IsSaveGameSlotValid(slot))
		return false;

	g_GameScript->OnSave();
	HandleAllGlobalEvents(EventType::Save, (Activator)short(LaraItem->Index));

	// Savegame infos need to be reloaded so that last savegame counter properly increases.
	LoadHeaders();

	auto fileName = GetSavegameFilename(slot);
	TENLog("Saving to savegame: " + fileName, LogLevel::Info);

	if (!std::filesystem::is_directory(FullSaveDirectory))
		std::filesystem::create_directory(FullSaveDirectory);

	std::ofstream fileOut{};
	fileOut.open(fileName, std::ios_base::binary | std::ios_base::out);

	// Write current level save data.
	auto currentLevelState = SaveGame::Build();
	int size = (int)currentLevelState.size();
	fileOut.write(reinterpret_cast<const char*>(&size), sizeof(size));
	fileOut.write(reinterpret_cast<const char*>(currentLevelState.data()), size);

	// Write hub data.
	int hubCount = (int)Hub.size();
	fileOut.write(reinterpret_cast<const char*>(&hubCount), sizeof(hubCount));

	for (auto& level : Hub)
	{
		fileOut.write(reinterpret_cast<const char*>(&level.first), sizeof(level.first));

		size = (int)level.second.size();
		fileOut.write(reinterpret_cast<const char*>(&size), sizeof(size));
		fileOut.write(reinterpret_cast<const char*>(level.second.data()), size);
	}

	fileOut.close();

	return true;
}

bool SaveGame::Load(int slot)
{
	if (!IsSaveGameValid(slot))
	{
		TENLog("Loading from savegame in slot " + std::to_string(slot) + " is impossible, data is missing or level has changed.", LogLevel::Error);
		return false;
	}

	auto fileName = GetSavegameFilename(slot);
	TENLog("Loading from savegame: " + fileName, LogLevel::Info);

	auto file = std::ifstream();
	try
	{
		file.open(fileName, std::ios_base::app | std::ios_base::binary);

		int size = 0;
		file.read(reinterpret_cast<char*>(&size), sizeof(size));

		// Read current level save data.
		auto saveData = std::vector<byte>(size);
		file.read(reinterpret_cast<char*>(saveData.data()), size);

		// Reset hub data, as it's about to be replaced with saved one.
		ResetHub();

		// Read hub data from savegame.
		int hubCount = 0;
		file.read(reinterpret_cast<char*>(&hubCount), sizeof(hubCount));

		TENLog("Hub count: " + std::to_string(hubCount), LogLevel::Info);

		for (int i = 0; i < hubCount; i++)
		{
			int index = 0;
			file.read(reinterpret_cast<char*>(&index), sizeof(index));

			file.read(reinterpret_cast<char*>(&size), sizeof(size));
			auto hubBuffer = std::vector<byte>(size);
			file.read(reinterpret_cast<char*>(hubBuffer.data()), size);

			Hub[index] = hubBuffer;
		}

		file.close();

		// Load save data for current level.
		Parse(saveData, false);
		return true;
	}
	catch (std::exception& ex)
	{
		TENLog("Error while loading savegame: " + std::string(ex.what()), LogLevel::Error);

		if (file.is_open())
			file.close();
	}

	return false;
}

static void ParseStatistics(const Save::SaveGame* s, bool isHub)
{
	SaveGame::Statistics.SecretBits = s->secret_bits();

	SaveGame::Statistics.Level.AmmoHits = s->level()->ammo_hits();
	SaveGame::Statistics.Level.AmmoUsed = s->level()->ammo_used();
	SaveGame::Statistics.Level.Distance = s->level()->distance();
	SaveGame::Statistics.Level.HealthUsed = s->level()->medipacks_used();
	SaveGame::Statistics.Level.DamageTaken = s->level()->damage_taken();
	SaveGame::Statistics.Level.Kills = s->level()->kills();
	SaveGame::Statistics.Level.Pickups = s->level()->pickups();
	SaveGame::Statistics.Level.Secrets = s->level()->secrets();
	SaveGame::Statistics.Level.TimeTaken = s->level()->timer();

	// Don't touch game statistics if data is parsed in hub mode.
	if (isHub)
		return;

	SaveGame::Statistics.Game.AmmoHits = s->game()->ammo_hits();
	SaveGame::Statistics.Game.AmmoUsed = s->game()->ammo_used();
	SaveGame::Statistics.Game.Distance = s->game()->distance();
	SaveGame::Statistics.Game.HealthUsed = s->game()->medipacks_used();
	SaveGame::Statistics.Game.DamageTaken = s->game()->damage_taken();
	SaveGame::Statistics.Game.Kills = s->game()->kills();
	SaveGame::Statistics.Game.Pickups = s->game()->pickups();
	SaveGame::Statistics.Game.Secrets = s->game()->secrets();
	SaveGame::Statistics.Game.TimeTaken = s->game()->timer();
}

static void ParseLua(const Save::SaveGame* s, bool hubMode)
{
	// Global level data

	auto* level = (Level*)g_GameFlow->GetLevel(CurrentLevel);

	level->Fog.MaxDistance = s->level_data()->fog_max_distance();
	level->Fog.MinDistance = s->level_data()->fog_min_distance();
	level->Fog.SetColor(s->level_data()->fog_color());

	level->Layer1.Enabled = s->level_data()->sky_layer_1_enabled();
	level->Layer1.CloudSpeed = s->level_data()->sky_layer_1_speed();
	level->Layer1.SetColor(s->level_data()->sky_layer_1_color());

	level->Layer2.Enabled = s->level_data()->sky_layer_2_enabled();
	level->Layer2.CloudSpeed = s->level_data()->sky_layer_2_speed();
	level->Layer2.SetColor(s->level_data()->sky_layer_2_color());

	level->LensFlare.SetSunSpriteID(s->level_data()->lensflare_sprite_id());
	level->LensFlare.SetPitch(s->level_data()->lensflare_pitch());
	level->LensFlare.SetYaw(s->level_data()->lensflare_yaw());
	level->LensFlare.SetColor(s->level_data()->lensflare_color());

	level->Starfield.SetStarCount(s->level_data()->starfield_star_count());
	level->Starfield.SetMeteorCount(s->level_data()->starfield_meteor_count());
	level->Starfield.SetMeteorSpawnDensity(s->level_data()->starfield_meteor_spawn_density());
	level->Starfield.SetMeteorVelocity(s->level_data()->starfield_meteor_velocity());

	level->Horizon1.SetEnabled(s->level_data()->horizon1_enabled());
	level->Horizon1.SetObjectID((GAME_OBJECT_ID)s->level_data()->horizon1_object_id());
	level->Horizon1.SetPosition(ToVector3(s->level_data()->horizon1_position()), true);
	level->Horizon1.SetRotation(ToEulerAngles(s->level_data()->horizon1_orientation()), true);
	level->Horizon1.SetTransparency(s->level_data()->horizon1_transparency());

	level->Horizon2.SetEnabled(s->level_data()->horizon2_enabled());
	level->Horizon2.SetObjectID((GAME_OBJECT_ID)s->level_data()->horizon2_object_id());
	level->Horizon2.SetPosition(ToVector3(s->level_data()->horizon2_position()), true);
	level->Horizon2.SetRotation(ToEulerAngles(s->level_data()->horizon2_orientation()), true);
	level->Horizon2.SetTransparency(s->level_data()->horizon2_transparency());

	level->Storm = s->level_data()->storm_enabled();
	level->Rumble = s->level_data()->rumble_enabled();
	level->Weather = (WeatherType)s->level_data()->weather_type();
	level->WeatherStrength = s->level_data()->weather_strength();

	// Event sets

	if (g_Level.VolumeEventSets.size() == s->volume_event_sets()->size())
	{
		for (int i = 0; i < s->volume_event_sets()->size(); ++i)
		{
			auto setSaved = s->volume_event_sets()->Get(i);
			for (int j = 0; j < setSaved->call_counters()->size(); ++j)
			{
				g_Level.VolumeEventSets[setSaved->index()].Events[j].Enabled = setSaved->statuses()->Get(j);
				g_Level.VolumeEventSets[setSaved->index()].Events[j].CallCounter = setSaved->call_counters()->Get(j);
			}
		}
	}

	if (g_Level.GlobalEventSets.size() == s->global_event_sets()->size())
	{
		for (int i = 0; i < s->global_event_sets()->size(); ++i)
		{
			auto setSaved = s->global_event_sets()->Get(i);
			for (int j = 0; j < setSaved->call_counters()->size(); ++j)
			{
				g_Level.GlobalEventSets[setSaved->index()].Events[j].Enabled = setSaved->statuses()->Get(j);
				g_Level.GlobalEventSets[setSaved->index()].Events[j].CallCounter = setSaved->call_counters()->Get(j);
			}
		}
	}

	// Variables

	auto loadedVars = std::vector<SavedVar>{};

	auto unionVec = s->script_vars();
	if (unionVec)
	{
		for (const auto& var : *(unionVec->members()))
		{
			auto varType = var->u_type();
			switch (varType)
			{
			case Save::VarUnion::num:
				loadedVars.push_back(var->u_as_num()->scalar());
				break;

			case Save::VarUnion::boolean:
				loadedVars.push_back(var->u_as_boolean()->scalar());
				break;

			case Save::VarUnion::str:
				loadedVars.push_back(var->u_as_str()->str()->str());
				break;

			case Save::VarUnion::tab:
			{
				auto tab = var->u_as_tab()->keys_vals();
				auto& loadedTab = loadedVars.emplace_back(IndexTable{});

				for (const auto& pair : *tab)
					std::get<IndexTable>(loadedTab).push_back(std::make_pair(pair->key(), pair->val()));

				break;
			}

			case Save::VarUnion::vec2:
			{
				auto stored = var->u_as_vec2()->vec();
				SavedVar var;
				var.emplace<(int)SavedVarType::Vec2>(ToVector2(stored));
				loadedVars.push_back(var);
				break;
			}

			case Save::VarUnion::vec3:
			{
				auto stored = var->u_as_vec3()->vec();
				SavedVar var;
				var.emplace<(int)SavedVarType::Vec3>(ToVector3(stored));
				loadedVars.push_back(var);
				break;
			}

			case Save::VarUnion::rotation:
			{
				auto stored = var->u_as_rotation()->vec();
				SavedVar var;
				var.emplace<(int)SavedVarType::Rotation>(ToVector3(stored));
				loadedVars.push_back(var);
				break;
			}

			case Save::VarUnion::time:
			{
				auto stored = var->u_as_time()->scalar();
				SavedVar var;
				var.emplace<(int)SavedVarType::Time>(stored);
				loadedVars.push_back(var);
				break;
			}

			case Save::VarUnion::color:
				loadedVars.push_back((D3DCOLOR)var->u_as_color()->color());
				break;

			case Save::VarUnion::funcName:
				loadedVars.push_back(FuncName{ var->u_as_funcName()->str()->str() });
				break;

			default:
				break;
			}
		}
	}

	g_GameScript->SetVariables(loadedVars, hubMode);

	// Callbacks

	auto populateCallbackVecs = [&s](auto callbackFunc)
	{
		auto callbacksVec = std::vector<std::string>{};
		auto callbacksOffsetVec = std::invoke(callbackFunc, s);

		for (const auto& e : *callbacksOffsetVec)
			callbacksVec.push_back(e->str());

		return callbacksVec;
	};

	auto callbacksPreStartVec = populateCallbackVecs(&Save::SaveGame::callbacks_pre_start);
	auto callbacksPostStartVec = populateCallbackVecs(&Save::SaveGame::callbacks_post_start);

	auto callbacksPreEndVec = populateCallbackVecs(&Save::SaveGame::callbacks_pre_end);
	auto callbacksPostEndVec = populateCallbackVecs(&Save::SaveGame::callbacks_post_end);

	auto callbacksPreSaveVec = populateCallbackVecs(&Save::SaveGame::callbacks_pre_save);
	auto callbacksPostSaveVec = populateCallbackVecs(&Save::SaveGame::callbacks_post_save);

	auto callbacksPreLoadVec = populateCallbackVecs(&Save::SaveGame::callbacks_pre_load);
	auto callbacksPostLoadVec = populateCallbackVecs(&Save::SaveGame::callbacks_post_load);

	auto callbacksPreLoopVec = populateCallbackVecs(&Save::SaveGame::callbacks_pre_loop);
	auto callbacksPostLoopVec = populateCallbackVecs(&Save::SaveGame::callbacks_post_loop);

	auto callbacksPreUseItemVec = populateCallbackVecs(&Save::SaveGame::callbacks_pre_useitem);
	auto callbacksPostUseItemVec = populateCallbackVecs(&Save::SaveGame::callbacks_post_useitem);

	auto callbacksPreFreezeVec = populateCallbackVecs(&Save::SaveGame::callbacks_pre_freeze);
	auto callbacksPostFreezeVec = populateCallbackVecs(&Save::SaveGame::callbacks_post_freeze);

	g_GameScript->SetCallbackStrings(
		callbacksPreStartVec,
		callbacksPostStartVec,
		callbacksPreEndVec,
		callbacksPostEndVec,
		callbacksPreSaveVec,
		callbacksPostSaveVec,
		callbacksPreLoadVec,
		callbacksPostLoadVec,
		callbacksPreLoopVec,
		callbacksPostLoopVec,
		callbacksPreUseItemVec,
		callbacksPostUseItemVec,
		callbacksPreFreezeVec,
		callbacksPostFreezeVec);
}

static void ParsePlayer(const Save::SaveGame* s)
{
	// Restore current inventory item.
	g_Gui.SetLastInventoryItem(s->last_inv_item());

	ZeroMemory(&Lara, sizeof(LaraInfo));

	// Player
	ZeroMemory(Lara.Inventory.Puzzles, NUM_PUZZLES * sizeof(int));
	for (int i = 0; i < s->lara()->inventory()->puzzles()->size(); i++)
		Lara.Inventory.Puzzles[i] = s->lara()->inventory()->puzzles()->Get(i);

	ZeroMemory(Lara.Inventory.PuzzlesCombo, NUM_PUZZLES * 2 * sizeof(int));
	for (int i = 0; i < s->lara()->inventory()->puzzles_combo()->size(); i++)
		Lara.Inventory.PuzzlesCombo[i] = s->lara()->inventory()->puzzles_combo()->Get(i);

	ZeroMemory(Lara.Inventory.Keys, NUM_KEYS * sizeof(int));
	for (int i = 0; i < s->lara()->inventory()->keys()->size(); i++)
		Lara.Inventory.Keys[i] = s->lara()->inventory()->keys()->Get(i);

	ZeroMemory(Lara.Inventory.KeysCombo, NUM_KEYS * 2 * sizeof(int));
	for (int i = 0; i < s->lara()->inventory()->keys_combo()->size(); i++)
		Lara.Inventory.KeysCombo[i] = s->lara()->inventory()->keys_combo()->Get(i);

	ZeroMemory(Lara.Inventory.Pickups, NUM_PICKUPS * sizeof(int));
	for (int i = 0; i < s->lara()->inventory()->pickups()->size(); i++)
		Lara.Inventory.Pickups[i] = s->lara()->inventory()->pickups()->Get(i);

	ZeroMemory(Lara.Inventory.PickupsCombo, NUM_PICKUPS * 2 * sizeof(int));
	for (int i = 0; i < s->lara()->inventory()->pickups_combo()->size(); i++)
		Lara.Inventory.PickupsCombo[i] = s->lara()->inventory()->pickups_combo()->Get(i);

	ZeroMemory(Lara.Inventory.Examines, NUM_EXAMINES * sizeof(int));
	for (int i = 0; i < s->lara()->inventory()->examines()->size(); i++)
		Lara.Inventory.Examines[i] = s->lara()->inventory()->examines()->Get(i);

	ZeroMemory(Lara.Inventory.ExaminesCombo, NUM_EXAMINES * 2 * sizeof(int));
	for (int i = 0; i < s->lara()->inventory()->examines_combo()->size(); i++)
		Lara.Inventory.ExaminesCombo[i] = s->lara()->inventory()->examines_combo()->Get(i);

	for (int i = 0; i < Lara.Effect.BubbleNodes.size(); i++)
		Lara.Effect.BubbleNodes[i] = s->lara()->effect()->bubble_nodes()->Get(i);

	for (int i = 0; i < Lara.Effect.DripNodes.size(); i++)
		Lara.Effect.DripNodes[i] = s->lara()->effect()->drip_nodes()->Get(i);

	Lara.Context.CalcJumpVelocity = s->lara()->context()->calc_jump_velocity();
	Lara.Context.WaterCurrentActive = s->lara()->context()->water_current_active();
	Lara.Context.WaterCurrentPull.x = s->lara()->context()->water_current_pull()->x();
	Lara.Context.WaterCurrentPull.y = s->lara()->context()->water_current_pull()->y();
	Lara.Context.WaterCurrentPull.z = s->lara()->context()->water_current_pull()->z();
	Lara.Context.InteractedItem = s->lara()->context()->interacted_item_number();
	Lara.Context.NextCornerPos = ToPose(*s->lara()->context()->next_corner_pose());
	Lara.Context.ProjectedFloorHeight = s->lara()->context()->projected_floor_height();
	Lara.Context.TargetOrientation = ToEulerAngles(s->lara()->context()->target_orient());
	Lara.Context.Vehicle = s->lara()->context()->vehicle_item_number();
	Lara.Context.WaterSurfaceDist = s->lara()->context()->water_surface_dist();
	Lara.Control.CanMonkeySwing = s->lara()->control()->can_monkey_swing();
	Lara.Control.CanClimbLadder = s->lara()->control()->is_climbing_ladder();
	Lara.Control.Count.Death = s->lara()->control()->count()->death();
	Lara.Control.Count.Pose = s->lara()->control()->count()->pose();
	Lara.Control.Count.PositionAdjust = s->lara()->control()->count()->position_adjust();
	Lara.Control.Count.Run = s->lara()->control()->count()->run_jump();
	Lara.Control.Count.Death = s->lara()->control()->count()->death();
	Lara.Control.IsClimbingLadder = s->lara()->control()->is_climbing_ladder();
	Lara.Control.IsLow = s->lara()->control()->is_low();
	Lara.Control.IsMoving = s->lara()->control()->is_moving();
	Lara.Control.JumpDirection = (JumpDirection)s->lara()->control()->jump_direction();
	Lara.Control.KeepLow = s->lara()->control()->keep_low();
	Lara.Control.Look.IsUsingBinoculars = s->lara()->control()->look()->is_using_binoculars();
	Lara.Control.Look.IsUsingLasersight = s->lara()->control()->look()->is_using_lasersight();
	Lara.Control.Look.Mode = (LookMode)s->lara()->control()->look()->mode();
	Lara.Control.Look.OpticRange = s->lara()->control()->look()->optic_range();
	Lara.Control.Look.Orientation = ToEulerAngles(s->lara()->control()->look()->orientation());
	Lara.Control.Look.TurnRate = ToEulerAngles(s->lara()->control()->look()->turn_rate());
	Lara.Control.MoveAngle = s->lara()->control()->move_angle();
	Lara.Control.IsRunJumpQueued = s->lara()->control()->is_run_jump_queued();
	Lara.Control.TurnRate = s->lara()->control()->turn_rate();
	Lara.Control.IsLocked = s->lara()->control()->is_locked();
	Lara.Control.HandStatus = (HandStatus)s->lara()->control()->hand_status();
	Lara.Control.Weapon.GunType = (LaraWeaponType)s->lara()->control()->weapon()->gun_type();
	Lara.Control.Weapon.HasFired = s->lara()->control()->weapon()->has_fired();
	Lara.Control.Weapon.Interval = s->lara()->control()->weapon()->interval();
	Lara.Control.Weapon.Fired = s->lara()->control()->weapon()->fired();
	Lara.Control.Weapon.LastGunType = (LaraWeaponType)s->lara()->control()->weapon()->last_gun_type();
	Lara.Control.Weapon.RequestGunType = (LaraWeaponType)s->lara()->control()->weapon()->request_gun_type();
	Lara.Control.Weapon.HolsterInfo.BackHolster = (HolsterSlot)s->lara()->control()->weapon()->holster_info()->back_holster();
	Lara.Control.Weapon.HolsterInfo.LeftHolster = (HolsterSlot)s->lara()->control()->weapon()->holster_info()->left_holster();
	Lara.Control.Weapon.HolsterInfo.RightHolster = (HolsterSlot)s->lara()->control()->weapon()->holster_info()->right_holster();
	Lara.Control.Weapon.NumShotsFired = s->lara()->control()->weapon()->num_shots_fired();
	Lara.Control.Weapon.Timer = s->lara()->control()->weapon()->timer();
	Lara.Control.Weapon.UziLeft = s->lara()->control()->weapon()->uzi_left();
	Lara.Control.Weapon.UziRight = s->lara()->control()->weapon()->uzi_right();
	Lara.Control.Weapon.WeaponItem = s->lara()->control()->weapon()->weapon_item();
	Lara.ExtraAnim = s->lara()->extra_anim();
	Lara.ExtraHeadRot = ToEulerAngles(s->lara()->extra_head_rot());
	Lara.ExtraTorsoRot = ToEulerAngles(s->lara()->extra_torso_rot());
	Lara.Flare.Life = s->lara()->flare()->life();
	Lara.Flare.ControlLeft = s->lara()->flare()->control_left();
	Lara.Flare.Frame = s->lara()->flare()->frame();
	Lara.HighestLocation = s->lara()->highest_location();
	Lara.HitDirection = s->lara()->hit_direction();
	Lara.HitFrame = s->lara()->hit_frame();
	Lara.Inventory.BeetleComponents = s->lara()->inventory()->beetle_components();
	Lara.Inventory.BeetleLife = s->lara()->inventory()->beetle_life();
	Lara.Inventory.BigWaterskin = s->lara()->inventory()->big_waterskin();
	Lara.Inventory.HasBinoculars = s->lara()->inventory()->has_binoculars();
	Lara.Inventory.HasDiary = s->lara()->inventory()->has_diary();
	Lara.Inventory.HasLoad = s->lara()->inventory()->has_load();
	Lara.Inventory.HasSave = s->lara()->inventory()->has_save();
	Lara.Inventory.HasStopwatch = s->lara()->inventory()->has_stopwatch();
	Lara.Inventory.HasCompass = s->lara()->inventory()->has_compass();
	Lara.Inventory.HasCrowbar = s->lara()->inventory()->has_crowbar();
	Lara.Inventory.HasLasersight = s->lara()->inventory()->has_lasersight();
	Lara.Inventory.HasSilencer = s->lara()->inventory()->has_silencer();
	Lara.Inventory.HasTorch = s->lara()->inventory()->has_torch();
	Lara.Inventory.IsBusy = s->lara()->inventory()->is_busy();
	Lara.Inventory.OldBusy = s->lara()->inventory()->old_busy();
	Lara.Inventory.SmallWaterskin = s->lara()->inventory()->small_waterskin();
	Lara.Inventory.TotalFlares = s->lara()->inventory()->total_flares();
	Lara.Inventory.TotalLargeMedipacks = s->lara()->inventory()->total_large_medipacks();
	Lara.Inventory.TotalSmallMedipacks = s->lara()->inventory()->total_small_medipacks();
	Lara.LeftArm.AnimNumber = s->lara()->left_arm()->anim_number();
	Lara.LeftArm.GunFlash = s->lara()->left_arm()->gun_flash();
	Lara.LeftArm.GunSmoke = s->lara()->left_arm()->gun_smoke();
	Lara.LeftArm.FrameBase = s->lara()->left_arm()->frame_base();
	Lara.LeftArm.FrameNumber = s->lara()->left_arm()->frame_number();
	Lara.LeftArm.Locked = s->lara()->left_arm()->locked();
	Lara.LeftArm.Orientation = ToEulerAngles(s->lara()->left_arm()->rotation());
	Lara.Location = s->lara()->location();
	Lara.LocationPad = s->lara()->location_pad();
	Lara.RightArm.AnimNumber = s->lara()->right_arm()->anim_number();
	Lara.RightArm.GunFlash = s->lara()->right_arm()->gun_flash();
	Lara.RightArm.GunSmoke = s->lara()->right_arm()->gun_smoke();
	Lara.RightArm.FrameBase = s->lara()->right_arm()->frame_base();
	Lara.RightArm.FrameNumber = s->lara()->right_arm()->frame_number();
	Lara.RightArm.Locked = s->lara()->right_arm()->locked();
	Lara.RightArm.Orientation = ToEulerAngles(s->lara()->right_arm()->rotation());
	Lara.Torch.IsLit = s->lara()->torch()->is_lit();
	Lara.Torch.State = (TorchState)s->lara()->torch()->state();
	Lara.Control.Rope.Segment = s->lara()->control()->rope()->segment();
	Lara.Control.Rope.Direction = s->lara()->control()->rope()->direction();
	Lara.Control.Rope.ArcFront = s->lara()->control()->rope()->arc_front();
	Lara.Control.Rope.ArcBack = s->lara()->control()->rope()->arc_back();
	Lara.Control.Rope.LastX = s->lara()->control()->rope()->last_x();
	Lara.Control.Rope.MaxXForward = s->lara()->control()->rope()->max_x_forward();
	Lara.Control.Rope.MaxXBackward = s->lara()->control()->rope()->max_x_backward();
	Lara.Control.Rope.DFrame = s->lara()->control()->rope()->dframe();
	Lara.Control.Rope.Frame = s->lara()->control()->rope()->frame();
	Lara.Control.Rope.FrameRate = s->lara()->control()->rope()->frame_rate();
	Lara.Control.Rope.Y = s->lara()->control()->rope()->y();
	Lara.Control.Rope.Ptr = s->lara()->control()->rope()->ptr();
	Lara.Control.Rope.Offset = s->lara()->control()->rope()->offset();
	Lara.Control.Rope.DownVel = s->lara()->control()->rope()->down_vel();
	Lara.Control.Rope.Flag = s->lara()->control()->rope()->flag();
	Lara.Control.Rope.Count = s->lara()->control()->rope()->count();
	Lara.Control.Subsuit.XRot = s->lara()->control()->subsuit()->x_rot();
	Lara.Control.Subsuit.DXRot = s->lara()->control()->subsuit()->d_x_rot();
	Lara.Control.Subsuit.Velocity[0] = s->lara()->control()->subsuit()->velocity()->Get(0);
	Lara.Control.Subsuit.Velocity[1] = s->lara()->control()->subsuit()->velocity()->Get(1);
	Lara.Control.Subsuit.VerticalVelocity = s->lara()->control()->subsuit()->vertical_velocity();
	Lara.Control.Subsuit.XRotVel = s->lara()->control()->subsuit()->x_rot_vel();
	Lara.Control.Subsuit.HitCount = s->lara()->control()->subsuit()->hit_count();
	Lara.Control.Tightrope.Balance = s->lara()->control()->tightrope()->balance();
	Lara.Control.Tightrope.CanDismount = s->lara()->control()->tightrope()->can_dismount();
	Lara.Control.Tightrope.TightropeItem = s->lara()->control()->tightrope()->tightrope_item();
	Lara.Control.Tightrope.TimeOnTightrope = s->lara()->control()->tightrope()->time_on_tightrope();
	Lara.Control.WaterStatus = (WaterStatus)s->lara()->control()->water_status();
	Lara.Status.Air = s->lara()->status()->air();
	Lara.Status.Exposure = s->lara()->status()->exposure();
	Lara.Status.Poison = s->lara()->status()->poison();
	Lara.Status.Stamina = s->lara()->status()->stamina();
	Lara.TargetEntity = (s->lara()->target_entity_number() >= 0) ? &g_Level.Items[s->lara()->target_entity_number()] : nullptr;
	Lara.TargetArmOrient = ToEulerAngles(s->lara()->target_arm_orient());
	Lara.Skin.Skin = static_cast<GAME_OBJECT_ID>(s->lara()->skin()->skin());
	Lara.Skin.SkinJoints = static_cast<GAME_OBJECT_ID>(s->lara()->skin()->skin_joints());
	Lara.Skin.HairPrimary = static_cast<GAME_OBJECT_ID>(s->lara()->skin()->hair_primary());
	Lara.Skin.HairSecondary = static_cast<GAME_OBJECT_ID>(s->lara()->skin()->hair_secondary());

	for (int i = 0; i < s->lara()->weapons()->size(); i++)
	{
		auto* info = s->lara()->weapons()->Get(i);

		for (int j = 0; j < info->ammo()->size(); j++)
		{
			Lara.Weapons[i].Ammo[j].SetInfinite(info->ammo()->Get(j)->is_infinite());
			Lara.Weapons[i].Ammo[j] = info->ammo()->Get(j)->count();
		}

		Lara.Weapons[i].HasLasersight = info->has_lasersight();
		Lara.Weapons[i].HasSilencer = info->has_silencer();
		Lara.Weapons[i].Present = info->present();
		Lara.Weapons[i].SelectedAmmo = (WeaponAmmoType)info->selected_ammo();
		Lara.Weapons[i].WeaponMode = (LaraWeaponTypeCarried)info->weapon_mode();
	}

	// Rope
	if (Lara.Control.Rope.Ptr >= 0)
	{
		auto* rope = &Ropes[Lara.Control.Rope.Ptr];

		for (int i = 0; i < s->rope()->segments()->size(); i++)
		{
			auto ropeSegment = s->rope()->segments()->Get(i);
			rope->segment[i] = ToVector3i(ropeSegment->segment());
			rope->normalisedSegment[i] = ToVector3i(ropeSegment->normalised_segment());
			rope->meshSegment[i] = ToVector3i(ropeSegment->mesh_segment());
			rope->coords[i] = ToVector3i(ropeSegment->coord());
			rope->velocity[i] = ToVector3i(ropeSegment->velocity());
		}

		rope->coiled = s->rope()->coiled();
		rope->active = s->rope()->active();

		rope->position = ToVector3i(s->rope()->position());
		CurrentPendulum.position = ToVector3i(s->pendulum()->position());
		CurrentPendulum.velocity = ToVector3i(s->pendulum()->velocity());

		CurrentPendulum.node = s->pendulum()->node();
		CurrentPendulum.rope = rope;

		AlternatePendulum.position = ToVector3i(s->alternate_pendulum()->position());
		AlternatePendulum.velocity = ToVector3i(s->alternate_pendulum()->velocity());

		AlternatePendulum.node = s->alternate_pendulum()->node();
		AlternatePendulum.rope = rope;
	}

	// Collision
	LaraCollision.LastBridgeItemNumber = s->lara()->collision()->last_bridge_item_number();
	LaraCollision.LastBridgeItemPose = ToPose(*s->lara()->collision()->last_bridge_item_pose());

	// Camera
	Camera.pos = ToGameVector(s->camera()->position());
	Camera.target = ToGameVector(s->camera()->target());

	for (auto& item : g_Level.Items)
	{
		if (item.ObjectNumber != ID_LARA || item.Index >= g_Level.NumItems)
			continue;

		LaraItem->Data = nullptr;
		LaraItem = &item;
		LaraItem->Location.RoomNumber = item.RoomNumber;
		LaraItem->Location.Height = item.Pose.Position.y;
		LaraItem->Data = &Lara;
		break;
	}
}

static void ParseEffects(const Save::SaveGame* s)
{
	// Restore camera FOV.
	AlterFOV(s->current_fov());

	// Restore postprocess effects.
	g_Renderer.SetPostProcessMode((PostProcessMode)s->postprocess_mode());
	g_Renderer.SetPostProcessStrength(s->postprocess_strength());
	g_Renderer.SetPostProcessTint(ToVector3(s->postprocess_tint()));

	// Restore soundtracks.
	for (int i = 0; i < s->soundtracks()->size(); i++)
	{
		TENAssert(i < (int)SoundTrackType::Count, "Soundtrack type count was changed");

		auto track = s->soundtracks()->Get(i);
		PlaySoundTrack(track->name()->str(), (SoundTrackType)i, track->position(), SOUND_XFADETIME_LEVELJUMP);
	}

	// Load fish swarm.
	for (int i = 0; i < s->fish_swarm()->size(); i++)
	{
		const auto& fishSave = s->fish_swarm()->Get(i);
		auto fish = FishData{};

		fish.IsLethal = fishSave->is_lethal();
		fish.IsPatrolling = fishSave->is_patrolling();
		fish.LeaderItemPtr = (fishSave->leader_item_number() == -1) ? nullptr : &g_Level.Items[fishSave->leader_item_number()];
		fish.Life = fishSave->life();
		fish.MeshIndex = fishSave->mesh_index();
		fish.Orientation = ToEulerAngles(fishSave->orientation());
		fish.Position = ToVector3(fishSave->position());
		fish.PositionTarget = ToVector3(fishSave->position_target());
		fish.RoomNumber = fishSave->room_number();
		fish.TargetItemPtr = (fishSave->target_item_number() == -1) ? nullptr : &g_Level.Items[fishSave->target_item_number()];
		fish.Undulation = fishSave->undulation();
		fish.Velocity = fishSave->velocity();

		FishSwarm.push_back(fish);
	}

	// Load firefly swarm.
	for (int i = 0; i < s->firefly_swarm()->size(); i++)
	{
		const auto& fireflySave = s->firefly_swarm()->Get(i);
		auto firefly = FireflyData{};

		firefly.SpriteSeqID = fireflySave->sprite_index();
		firefly.SpriteID = fireflySave->sprite_id();
		firefly.blendMode = (BlendMode)fireflySave->blend_mode();
		firefly.scalar = fireflySave->scalar();
		firefly.Position = ToVector3(fireflySave->position());
		firefly.RoomNumber = fireflySave->room_number();
		firefly.PositionTarget = ToVector3(fireflySave->position_target());
		firefly.Orientation = ToEulerAngles(fireflySave->orientation());
		firefly.Velocity = fireflySave->velocity();
		firefly.TargetItemPtr = (fireflySave->target_item_number() == -1) ? nullptr : &g_Level.Items[fireflySave->target_item_number()];
		firefly.zVel = fireflySave->z_vel();
		firefly.Life = fireflySave->life();
		firefly.Number = fireflySave->number();
		firefly.rB = fireflySave->d_r();
		firefly.gB = fireflySave->d_g();
		firefly.bB = fireflySave->d_b();
		firefly.r = fireflySave->r();
		firefly.g = fireflySave->g();
		firefly.b = fireflySave->b();
		firefly.on = fireflySave->on();
		firefly.size = fireflySave->size();
		firefly.rotAng = fireflySave->rot_Ang();

		FireflySwarm.push_back(firefly);
	}

	// Load particles.
	for (int i = 0; i < s->particles()->size(); i++)
	{
		auto* particleInfo = s->particles()->Get(i);
		auto* particle = &Particles[i];

		particle->x = particleInfo->x();
		particle->y = particleInfo->y();
		particle->z = particleInfo->z();
		particle->xVel = particleInfo->x_vel();
		particle->yVel = particleInfo->y_vel();
		particle->zVel = particleInfo->z_vel();
		particle->gravity = particleInfo->gravity();
		particle->rotAng = particleInfo->rot_ang();
		particle->flags = particleInfo->flags();
		particle->sSize = particleInfo->s_size();
		particle->dSize = particleInfo->d_size();
		particle->size = particleInfo->size();
		particle->friction = particleInfo->friction();
		particle->scalar = particleInfo->scalar();
		particle->SpriteSeqID = (GAME_OBJECT_ID)particleInfo->sprite_index();
		particle->SpriteID = particleInfo->sprite_id();
		particle->rotAdd = particleInfo->rot_add();
		particle->maxYvel = particleInfo->max_y_vel();
		particle->on = particleInfo->on();
		particle->sR = particleInfo->s_r();
		particle->sG = particleInfo->s_g();
		particle->sB = particleInfo->s_b();
		particle->dR = particleInfo->d_r();
		particle->dG = particleInfo->d_g();
		particle->dB = particleInfo->d_b();
		particle->r = particleInfo->r();
		particle->g = particleInfo->g();
		particle->b = particleInfo->b();
		particle->colFadeSpeed = particleInfo->col_fade_speed();
		particle->fadeToBlack = particleInfo->fade_to_black();
		particle->sLife = particleInfo->s_life();
		particle->life = particleInfo->life();
		particle->blendMode = (BlendMode)particleInfo->blend_mode();
		particle->extras = particleInfo->extras();
		particle->dynamic = particleInfo->dynamic();
		particle->fxObj = particleInfo->fx_obj();
		particle->roomNumber = particleInfo->room_number();
		particle->nodeNumber = particleInfo->node_number();
		particle->targetPos = ToVector3(particleInfo->target_pos());
		particle->animationType = (ParticleAnimType)particleInfo->animation_type();
		particle->damage = particleInfo->damage();
		particle->framerate = particleInfo->framerate();
		particle->lightRadius = particleInfo->light_radius();
		particle->lightFlicker = particleInfo->light_flicker();
		particle->lightFlickerS = particleInfo->light_flicker_s();
		particle->sound = particleInfo->sound();

	}

	for (int i = 0; i < s->bats()->size(); i++)
	{
		auto* batInfo = s->bats()->Get(i);
		auto* bat = &Bats[i];

		bat->On = batInfo->on();
		bat->Counter = batInfo->flags();
		bat->RoomNumber = batInfo->room_number();
		bat->Pose = ToPose(*batInfo->pose());
	}

	for (int i = 0; i < s->rats()->size(); i++)
	{
		auto ratInfo = s->rats()->Get(i);
		auto* rat = &Rats[i];

		rat->On = ratInfo->on();
		rat->Flags = ratInfo->flags();
		rat->RoomNumber = ratInfo->room_number();
		rat->Pose = ToPose(*ratInfo->pose());
	}

	for (int i = 0; i < s->spiders()->size(); i++)
	{
		auto* spiderInfo = s->spiders()->Get(i);
		auto* spider = &Spiders[i];

		spider->On = spiderInfo->on();
		spider->Flags = spiderInfo->flags();
		spider->RoomNumber = spiderInfo->room_number();
		spider->Pose = ToPose(*spiderInfo->pose());
	}

	for (int i = 0; i < s->scarabs()->size(); i++)
	{
		auto beetleInfo = s->scarabs()->Get(i);
		auto* beetle = &BeetleSwarm[i];

		beetle->On = beetleInfo->on();
		beetle->Flags = beetleInfo->flags();
		beetle->RoomNumber = beetleInfo->room_number();
		beetle->Pose = ToPose(*beetleInfo->pose());
	}

	NextFxFree = s->next_fx_free();
	NextFxActive = s->next_fx_active();

	for (int i = 0; i < s->fxinfos()->size(); ++i)
	{
		auto& fx = EffectList[i];
		auto fx_saved = s->fxinfos()->Get(i);
		fx.pos = ToPose(*fx_saved->pose());
		fx.roomNumber = fx_saved->room_number();
		fx.objectNumber = fx_saved->object_number();
		fx.nextFx = fx_saved->next_fx();
		fx.nextActive = fx_saved->next_active();
		fx.speed = fx_saved->speed();
		fx.fallspeed = fx_saved->fall_speed();
		fx.frameNumber = fx_saved->frame_number();
		fx.counter = fx_saved->counter();
		fx.color = ToVector4(fx_saved->color());
		fx.flag1 = fx_saved->flag1();
		fx.flag2 = fx_saved->flag2();
	}
}

static void ParseLevel(const Save::SaveGame* s, bool hubMode)
{
	// Rooms
	for (int i = 0; i < s->rooms()->size(); i++)
	{
		auto room = s->rooms()->Get(i);
		g_Level.Rooms[room->index()].Name = room->name()->str();
		g_Level.Rooms[room->index()].flags = room->flags();
		g_Level.Rooms[room->index()].reverbType = (ReverbType)room->reverb_type();
	}

	// Static objects.
	for (int i = 0; i < s->static_meshes()->size(); i++)
	{
		auto staticMesh = s->static_meshes()->Get(i);
		auto room = &g_Level.Rooms[staticMesh->room_number()];
		int number = staticMesh->number();

		room->mesh[number].pos = ToPose(*staticMesh->pose());
		room->mesh[number].roomNumber = staticMesh->room_number();
		room->mesh[number].scale = staticMesh->scale();
		room->mesh[number].color = ToVector4(staticMesh->color());

		room->mesh[number].flags = staticMesh->flags();
		room->mesh[number].HitPoints = staticMesh->hit_points();

		room->mesh[number].Dirty = true;
		
		if (!room->mesh[number].flags)
		{
			short roomNumber = staticMesh->room_number();
			FloorInfo* floor = GetFloor(room->mesh[number].pos.Position.x, room->mesh[number].pos.Position.y, room->mesh[number].pos.Position.z, &roomNumber);
			TestTriggers(room->mesh[number].pos.Position.x, room->mesh[number].pos.Position.y, room->mesh[number].pos.Position.z, staticMesh->room_number(), true, 0);
			floor->Stopper = false;
		}
	}

	// Volumes
	for (int i = 0; i < s->volumes()->size(); i++)
	{
		auto volume = s->volumes()->Get(i);
		auto room = &g_Level.Rooms[volume->room_number()];
		int number = volume->number();

		room->TriggerVolumes[number].Enabled = volume->enabled();
		room->TriggerVolumes[number].Name = volume->name()->str();
		room->TriggerVolumes[number].Box.Center =
		room->TriggerVolumes[number].Sphere.Center = ToVector3(volume->position());
		room->TriggerVolumes[number].Box.Orientation = ToVector4(volume->rotation());
		room->TriggerVolumes[number].Box.Extents = ToVector3(volume->scale());
		room->TriggerVolumes[number].Sphere.Radius = room->TriggerVolumes[number].Box.Extents.x;

		for (int j = 0; j < volume->queue()->size(); j++)
		{
			auto state = volume->queue()->Get(j);
			room->TriggerVolumes[number].StateQueue.push_back(
				VolumeState
				{
					(VolumeStateStatus)state->status(),
					(short)state->activator(),
					state->timestamp()
				});
		}
	}

	// Flipmaps (should be applied after statics and volumes are loaded)
	for (int i = 0; i < s->flip_stats()->size(); i++)
	{
		if (s->flip_stats()->Get(i) != 0)
			DoFlipMap(i);

		FlipMap[i] = s->flip_maps()->Get(i) << 8;
	}

	// Flipeffects
	FlipEffect = s->flip_effect();
	FlipStatus = s->flip_status();

	// Restore action queue.
	for (int i = 0; i < s->action_queue()->size(); i++)
	{
		TENAssert(i < ActionQueueMap.size(), "Action queue size was changed.");
		ActionQueueMap[(InputActionID)i] = (ActionQueueState)s->action_queue()->Get(i);
	}

	// Legacy soundtrack map.
	for (int i = 0; i < s->cd_flags()->size(); i++)
	{
		int index = s->cd_flags()->Get(i);
		int mask = s->cd_flags()->Get(++i);
		SoundTracks[index].Mask = mask;
	}

	// Cameras 
	for (int i = 0; i < s->fixed_cameras()->size(); i++)
	{
		if (i < g_Level.Cameras.size())
			g_Level.Cameras[i].Flags = s->fixed_cameras()->Get(i)->flags();
	}

	// Sinks 
	for (int i = 0; i < s->sinks()->size(); i++)
	{
		if (i < g_Level.Sinks.size())
			g_Level.Sinks[i].Strength = s->sinks()->Get(i)->flags();
	}

	// Flyby cameras 
	for (int i = 0; i < s->flyby_cameras()->size(); i++)
	{
		if (i < NumberSpotcams)
			SpotCam[i].flags = s->flyby_cameras()->Get(i)->flags();
	}

	// Items

	NextItemFree = s->next_item_free();
	NextItemActive = s->next_item_active();

	for(int i = 0; i < s->room_items()->size(); ++i)
		g_Level.Rooms[i].itemNumber = s->room_items()->Get(i);

	for (int i = 0; i < s->items()->size(); i++)
	{
		const Save::Item* savedItem = s->items()->Get(i);

		bool dynamicItem = i >= g_Level.NumItems;

		ItemInfo* item = &g_Level.Items[i];
		item->ObjectNumber = GAME_OBJECT_ID(savedItem->object_id());

		item->NextItem = savedItem->next_item();
		item->NextActive = savedItem->next_item_active();

		if (item->ObjectNumber == GAME_OBJECT_ID::ID_NO_OBJECT)
			continue;

		ObjectInfo* obj = &Objects[item->ObjectNumber];
		
		item->Name = savedItem->lua_name()->str();
		if (!item->Name.empty())
			g_GameScriptEntities->AddName(item->Name, (short)i);

		item->Callbacks.OnKilled = savedItem->lua_on_killed_name()->str();
		item->Callbacks.OnHit = savedItem->lua_on_hit_name()->str();
		item->Callbacks.OnObjectCollided = savedItem->lua_on_collided_with_object_name()->str();
		item->Callbacks.OnRoomCollided = savedItem->lua_on_collided_with_room_name()->str();

		g_GameScriptEntities->TryAddColliding(i);

		// Don't load player data in hub mode.
		if (item->ObjectNumber == ID_LARA && hubMode)
		{
			item->RoomNumber = savedItem->room_number();
			item->Floor = savedItem->floor();
			item->BoxNumber = savedItem->box_number();
			continue;
		}

		if (item->Index == Lara.Context.Vehicle && hubMode)
		{
			//item->Pose = ToPose(*savedItem->pose());
			item->RoomNumber = savedItem->room_number();
			item->Floor = savedItem->floor();
			item->BoxNumber = savedItem->box_number();
			continue;
		}

		// If object is bridge - remove it from existing sectors.
		if (item->IsBridge())
			UpdateBridgeItem(g_Level.Items[i], BridgeUpdateType::Remove);

		// Position
		item->Pose = ToPose(*savedItem->pose());
		item->RoomNumber = savedItem->room_number();
		item->Floor = savedItem->floor();
		item->BoxNumber = savedItem->box_number();

		// Animations
		item->Animation.AnimObjectID = GAME_OBJECT_ID(savedItem->anim_object_id());
		item->Animation.ActiveState = savedItem->active_state();
		item->Animation.RequiredState = savedItem->required_state();
		item->Animation.TargetState = savedItem->target_state();
		item->Animation.AnimNumber = obj->animIndex + savedItem->anim_number();
		item->Animation.FrameNumber = savedItem->frame_number();
		item->Animation.Velocity = ToVector3(savedItem->velocity());

		// Hit points
		item->HitPoints = savedItem->hit_points();

		// Mesh stuff
		item->MeshBits = savedItem->mesh_bits();
		item->Model.BaseMesh = savedItem->base_mesh();
		item->Model.MeshIndex.resize(savedItem->mesh_pointers()->size());
		for (int j = 0; j < savedItem->mesh_pointers()->size(); j++)
			item->Model.MeshIndex[j] = savedItem->mesh_pointers()->Get(j);

		// Flags and timers
		for (int j = 0; j < 7; j++)
			item->ItemFlags[j] = savedItem->item_flags()->Get(j);

		item->Timer = savedItem->timer();
		item->TriggerFlags = savedItem->trigger_flags();
		item->Flags = savedItem->flags();

		// Color
		item->Model.Color = ToVector4(savedItem->color());

		// Carried item
		item->CarriedItem = savedItem->carried_item();

		item->Active = savedItem->active();
		item->HitStatus = savedItem->hit_stauts();
		item->Status = (ItemStatus)savedItem->status();
		item->AIBits = savedItem->ai_bits();
		item->Animation.IsAirborne = savedItem->is_airborne();
		item->Collidable = savedItem->collidable();
		item->LookedAt = savedItem->looked_at();

		item->Effect.Type = (EffectType)savedItem->effect_type();
		item->Effect.PrimaryEffectColor = ToVector3(savedItem->effect_primary_colour());
		item->Effect.SecondaryEffectColor = ToVector3(savedItem->effect_secondary_colour());
		item->Effect.LightColor = ToVector3(savedItem->effect_light_colour());
		item->Effect.Count = savedItem->effect_count();

		if (item->ObjectNumber >= ID_SMASH_OBJECT1 && item->ObjectNumber <= ID_SMASH_OBJECT8 &&
			(item->Flags & ONESHOT))
		{
			item->MeshBits = 0x00100;
		}

		// Post-load specific hacks for objects.
		if (item->ObjectNumber >= ID_PUZZLE_HOLE1 && item->ObjectNumber <= ID_PUZZLE_HOLE16 &&
			(item->Status == ITEM_ACTIVE || item->Status == ITEM_DEACTIVATED))
		{
			item->ObjectNumber = (GAME_OBJECT_ID)((int)item->ObjectNumber + ID_PUZZLE_DONE1 - ID_PUZZLE_HOLE1);
			item->Animation.AnimNumber = Objects[item->ObjectNumber].animIndex + savedItem->anim_number();
		}

		// Re-add bridges at new position.
		if (item->IsBridge())
			UpdateBridgeItem(g_Level.Items[i], BridgeUpdateType::Initialize);

		// Creature data for intelligent items.
		if (item->ObjectNumber != ID_LARA && item->Status == ITEM_ACTIVE && obj->intelligent)
		{
			EnableEntityAI(i, true, false);

			auto creature = GetCreatureInfo(item);
			auto data = savedItem->data();
			auto savedCreature = (Save::Creature*)data;

			if (savedCreature == nullptr)
				continue;

			creature->Alerted = savedCreature->alerted();
			creature->LOT.CanJump = savedCreature->can_jump();
			creature->LOT.CanMonkey = savedCreature->can_monkey();

			if (savedCreature->enemy() >= 0)
				creature->Enemy = &g_Level.Items[savedCreature->enemy()];

			creature->Flags = savedCreature->flags();
			creature->Friendly = savedCreature->friendly();
			creature->HeadLeft = savedCreature->head_left();
			creature->HeadRight = savedCreature->head_right();
			creature->HurtByLara = savedCreature->hurt_by_lara();
			creature->LocationAI = savedCreature->location_ai();
			creature->MuzzleFlash[0].Delay = savedCreature->weapon_delay1();
			creature->MuzzleFlash[1].Delay = savedCreature->weapon_delay2();
			creature->LOT.IsAmphibious = savedCreature->is_amphibious();
			creature->LOT.IsJumping = savedCreature->is_jumping();
			creature->LOT.IsMonkeying = savedCreature->is_monkeying();

			for (int j = 0; j < 4; j++)
				creature->JointRotation[j] = savedCreature->joint_rotation()->Get(j);

			creature->JumpAhead = savedCreature->jump_ahead();
			creature->MaxTurn = savedCreature->maximum_turn();
			creature->MonkeySwingAhead = savedCreature->monkey_swing_ahead();
			creature->Mood = (MoodType)savedCreature->mood();
			creature->Patrol = savedCreature->patrol();
			creature->Poisoned = savedCreature->poisoned();
			creature->ReachedGoal = savedCreature->reached_goal();
			creature->Tosspad = savedCreature->tosspad();
			SetEntityTarget(i, savedCreature->ai_target_number());
		}
		else if (item->Data.is<QuadBikeInfo>())
		{
			auto* quadBike = (QuadBikeInfo*)item->Data;
			auto* savedQuad = (Save::QuadBike*)savedItem->data();

			quadBike->CanStartDrift = savedQuad->can_start_drift();
			quadBike->DriftStarting = savedQuad->drift_starting();
			quadBike->EngineRevs = savedQuad->engine_revs();
			quadBike->ExtraRotation = savedQuad->extra_rotation();
			quadBike->Flags = savedQuad->flags();
			quadBike->FrontRot = savedQuad->front_rot();
			quadBike->LeftVerticalVelocity = savedQuad->left_vertical_velocity();
			quadBike->MomentumAngle = savedQuad->momentum_angle();
			quadBike->NoDismount = savedQuad->no_dismount();
			quadBike->Pitch = savedQuad->pitch();
			quadBike->RearRot = savedQuad->rear_rot();
			quadBike->Revs = savedQuad->revs();
			quadBike->RightVerticalVelocity = savedQuad->right_vertical_velocity();
			quadBike->SmokeStart = savedQuad->smoke_start();
			quadBike->TurnRate = savedQuad->turn_rate();
			quadBike->Velocity = savedQuad->velocity();
		}
		else if (item->Data.is<UPVInfo>())
		{
			auto* upv = (UPVInfo*)item->Data;
			auto* savedUpv = (Save::UPV*)savedItem->data();

			upv->TurbineRotation = savedUpv->fan_rot();
			upv->Flags = savedUpv->flags();
			upv->HarpoonLeft = savedUpv->harpoon_left();
			upv->HarpoonTimer = savedUpv->harpoon_timer();
			upv->TurnRate.y = savedUpv->rot();
			upv->Velocity = savedUpv->velocity();
			upv->TurnRate.x = savedUpv->x_rot();
		}
		else if (item->Data.is<MinecartInfo>())
		{
			auto* minecart = (MinecartInfo*)item->Data;
			auto* savedMine = (Save::Minecart*)savedItem->data();

			minecart->Flags = savedMine->flags();
			minecart->FloorHeightFront = savedMine->floor_height_front();
			minecart->FloorHeightMiddle = savedMine->floor_height_middle();
			minecart->Gradient = savedMine->gradient();
			minecart->StopDelayTime = savedMine->stop_delay();
			minecart->TurnLen = savedMine->turn_len();
			minecart->TurnRot = savedMine->turn_rot();
			minecart->TurnX = savedMine->turn_x();
			minecart->TurnZ = savedMine->turn_z();
			minecart->Velocity = savedMine->velocity();
			minecart->VerticalVelocity = savedMine->vertical_velocity();
		}
		else if (item->Data.is<KayakInfo>())
		{
			auto* kayak = (KayakInfo*)item->Data;
			auto* savedKayak = (Save::Kayak*)savedItem->data();

			kayak->Flags = savedKayak->flags();
			kayak->Forward = savedKayak->forward();
			kayak->FrontVerticalVelocity = savedKayak->front_vertical_velocity();
			kayak->LeftRightPaddleCount = savedKayak->left_right_count();
			kayak->LeftVerticalVelocity = savedKayak->left_vertical_velocity();
			kayak->OldPose = ToPose(*savedKayak->old_pos());
			kayak->RightVerticalVelocity = savedKayak->right_vertical_velocity();
			kayak->TrueWater = savedKayak->true_water();
			kayak->Turn = savedKayak->turn();
			kayak->TurnRate = savedKayak->turn_rate();
			kayak->Velocity = savedKayak->velocity();
			kayak->WaterHeight = savedKayak->water_height();
		}
		else if (item->Data.is <PushableInfo>())
		{
			auto* pushable = (PushableInfo*)item->Data;
			auto* savedPushable = (Save::Pushable*)savedItem->data();

			pushable->BehaviorState = (PushableBehaviourState)savedPushable->pushable_behaviour_state();
			pushable->Gravity = savedPushable->pushable_gravity();
			pushable->Oscillation = savedPushable->pushable_water_force();

			pushable->Stack.Limit = savedPushable->pushable_stack_limit();
			pushable->Stack.ItemNumberAbove = savedPushable->pushable_stack_upper();
			pushable->Stack.ItemNumberBelow = savedPushable->pushable_stack_lower();

			pushable->StartPos.x = savedPushable->pushable_start_x();
			pushable->StartPos.z = savedPushable->pushable_start_z();
			pushable->StartPos.RoomNumber = savedPushable->pushable_room_number();

			pushable->UseBridgeCollision = savedPushable->pushable_collider_flag();

			pushable->EdgeAttribs[0].IsPullable = savedPushable->pushable_north_pullable();
			pushable->EdgeAttribs[0].IsPushable = savedPushable->pushable_north_pushable();
			pushable->EdgeAttribs[0].IsClimbable = savedPushable->pushable_north_climbable();

			pushable->EdgeAttribs[1].IsPullable = savedPushable->pushable_east_pullable();
			pushable->EdgeAttribs[1].IsPushable = savedPushable->pushable_east_pushable();
			pushable->EdgeAttribs[1].IsClimbable = savedPushable->pushable_east_climbable();

			pushable->EdgeAttribs[2].IsPullable = savedPushable->pushable_south_pullable();
			pushable->EdgeAttribs[2].IsPushable = savedPushable->pushable_south_pushable();
			pushable->EdgeAttribs[2].IsClimbable = savedPushable->pushable_south_climbable();

			pushable->EdgeAttribs[3].IsPullable = savedPushable->pushable_west_pullable();
			pushable->EdgeAttribs[3].IsPushable = savedPushable->pushable_west_pushable();
			pushable->EdgeAttribs[3].IsClimbable = savedPushable->pushable_west_climbable();
		}
		else if (savedItem->data_type() == Save::ItemData::Short)
		{
			auto* data = savedItem->data();
			auto* savedData = (Save::Short*)data;
			item->Data = savedData->scalar();
		}
		else if (savedItem->data_type() == Save::ItemData::Int)
		{
			auto* data = savedItem->data();
			auto* savedData = (Save::Int*)data;
			item->Data = savedData->scalar();
		}
	}
}

void SaveGame::Parse(const std::vector<byte>& buffer, bool hubMode)
{
	if (!Save::VerifySaveGameBuffer(flatbuffers::Verifier(buffer.data(), buffer.size())))
	{
		TENLog("Savegame data is incorrect and was not loaded! Incorrect flatbuffer format or memory corruption?", LogLevel::Error);
		return;
	}

	JustLoaded = true;

	const Save::SaveGame* s = Save::GetSaveGame(buffer.data());

	ParseLevel(s, hubMode);
	ParseLua(s, hubMode);
	ParseStatistics(s, hubMode);

	// Effects and player data is ignored when loading hub.
	// Effects are commonly believed to be non-preservable, while player data is transfered from previous level.

	if (hubMode)
		return;

	ParseEffects(s);
	ParsePlayer(s);
}

bool SaveGame::LoadHeader(int slot, SaveGameHeader* header)
{
	if (!IsSaveGameSlotValid(slot))
		return false;

	if (!DoesSaveGameExist(slot))
		return false;

	auto fileName = GetSavegameFilename(slot);

	std::ifstream file;
	file.open(fileName, std::ios_base::app | std::ios_base::binary);

	file.seekg(0, std::ios::end);
	size_t length = file.tellg();
	file.seekg(0, std::ios::beg);

	if (length == 0)
	{
		TENLog("Savegame #" + std::to_string(slot) + " has no data!", LogLevel::Warning);
		return false;
	}

	try
	{
		int size;
		file.read(reinterpret_cast<char*>(&size), sizeof(size));

		std::unique_ptr<char[]> buffer = std::make_unique<char[]>(size);
		file.read(buffer.get(), size);
		file.close();

		bool bufferIsValid = Save::VerifySaveGameBuffer(flatbuffers::Verifier(reinterpret_cast<const unsigned char*>(buffer.get()), size));

		if (size <= 0 || size >= length || !bufferIsValid)
		{
			TENLog("Incorrect data in savegame #" + std::to_string(slot) + ". Old format?", LogLevel::Warning);
			return false;
		}

		const Save::SaveGame* s = Save::GetSaveGame(buffer.get());

		header->Level = s->header()->level();
		header->LevelName = s->header()->level_name()->str();
		header->LevelHash = s->header()->level_hash();
		header->Hours = s->header()->hours();
		header->Minutes = s->header()->minutes();
		header->Seconds = s->header()->seconds();
		header->Level = s->header()->level();
		header->Timer = s->header()->timer();
		header->Count = s->header()->count();

		return true;
	}
	catch (std::exception& ex)
	{
		TENLog("Error reading savegame #" + std::to_string(slot) + ", Exception: " + ex.what(), LogLevel::Error);
		return false;
	}
}

void SaveGame::Delete(int slot)
{
	if (!IsSaveGameSlotValid(slot))
		return;

	if (!DoesSaveGameExist(slot))
		return;

	std::filesystem::remove(GetSavegameFilename(slot));
}
