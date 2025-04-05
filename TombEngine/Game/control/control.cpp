#include "framework.h"
#include "Game/control/control.h"

#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/control/flipeffect.h"
#include "Game/control/lot.h"
#include "Game/control/volume.h"
#include "Game/effects/debris.h"
#include "Game/effects/Blood.h"
#include "Game/effects/Bubble.h"
#include "Game/effects/DisplaySprite.h"
#include "Game/effects/Drip.h"
#include "Game/effects/effects.h"
#include "Game/effects/Electricity.h"
#include "Game/effects/explosion.h"
#include "Game/effects/Footprint.h"
#include "Game/effects/Hair.h"
#include "Game/effects/Ripple.h"
#include "Game/effects/simple_particle.h"
#include "Game/effects/smoke.h"
#include "Game/effects/spark.h"
#include "Game/effects/Splash.h"
#include "Game/effects/Streamer.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/Gui.h"
#include "Game/Hud/Hud.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_cheat.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_one_gun.h"
#include "Game/items.h"
#include "Game/pickup/pickup.h"
#include "Game/room.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Game/spotcam.h"
#include "Math/Math.h"
#include "Objects/Effects/LensFlare.h"
#include "Objects/Effects/tr4_locusts.h"
#include "Objects/Effects/Fireflies.h"
#include "Objects/Generic/Object/objects.h"
#include "Objects/Generic/Object/rope.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Objects/TR3/Entity/FishSwarm.h"
#include "Objects/TR4/Entity/tr4_beetle_swarm.h"
#include "Objects/TR5/Emitter/tr5_bats_emitter.h"
#include "Objects/TR5/Emitter/tr5_rats_emitter.h"
#include "Objects/TR5/Emitter/tr5_spider_emitter.h"
#include "Objects/TR5/Trap/LaserBarrier.h"
#include "Objects/TR5/Trap/LaserBeam.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/Objects/ScriptInterfaceObjectsHandler.h"
#include "Scripting/Include/ScriptInterfaceGame.h"
#include "Scripting/Include/Strings/ScriptInterfaceStringsHandler.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"
#include "Sound/sound.h"
#include "Specific/clock.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"
#include "Specific/winmain.h"
#include "Specific/Video/Video.h"

using namespace std::chrono;
using namespace TEN::Effects;
using namespace TEN::Effects::Blood;
using namespace TEN::Effects::Bubble;
using namespace TEN::Effects::DisplaySprite;
using namespace TEN::Effects::Drip;
using namespace TEN::Effects::Electricity;
using namespace TEN::Effects::Environment;
using namespace TEN::Effects::Explosion;
using namespace TEN::Effects::Footprint;
using namespace TEN::Effects::Hair;
using namespace TEN::Effects::Ripple;
using namespace TEN::Effects::Smoke;
using namespace TEN::Effects::Spark;
using namespace TEN::Effects::Splash;
using namespace TEN::Effects::Streamer;
using namespace TEN::Entities::Creatures::TR3;
using namespace TEN::Entities::Generic;
using namespace TEN::Entities::Switches;
using namespace TEN::Entities::Traps;
using namespace TEN::Entities::TR4;
using namespace TEN::Collision::Floordata;
using namespace TEN::Control::Volumes;
using namespace TEN::Hud;
using namespace TEN::Input;
using namespace TEN::Math;
using namespace TEN::Renderer;
using namespace TEN::Entities::Creatures::TR3;
using namespace TEN::Entities::Effects;
using namespace TEN::Effects::Fireflies;
using namespace TEN::Video;

constexpr auto DEATH_NO_INPUT_TIMEOUT = 10 * FPS;
constexpr auto DEATH_INPUT_TIMEOUT	  = 3 * FPS;

int GlobalCounter = 0;

bool InitializeGame	= false;
bool DoTheGame		= false;
bool JustLoaded		= false;
bool ThreadEnded	= false;

int RequiredStartPos;
int CurrentLevel;
int NextLevel;

bool  InItemControlLoop;
short ItemNewRoomNo;
short ItemNewRooms[MAX_ROOMS];
short NextItemActive;
short NextItemFree;
short NextFxActive;
short NextFxFree;

int ControlPhaseTime;

void DrawPhase(bool isTitle, float interpolationFactor)
{
	if (isTitle)
	{
		g_Renderer.RenderTitle(interpolationFactor);
	}
	else if (g_GameFlow->CurrentFreezeMode == FreezeMode::None)
	{
		g_Renderer.Render(interpolationFactor);
	}
	else
	{
		g_Renderer.RenderFreezeMode(interpolationFactor, g_GameFlow->CurrentFreezeMode == FreezeMode::Full);
	}

	g_Renderer.Lock();
}

GameStatus GamePhase(bool insideMenu)
{
	auto time1 = std::chrono::high_resolution_clock::now();
	bool isTitle = (CurrentLevel == 0);

	g_Renderer.PrepareScene();
	g_Renderer.SaveOldState();

	ClearFires();
	ClearLensFlares();
	ClearAllDisplaySprites();

	SetupInterpolation();
	PrepareCamera();

	RegeneratePickups();

	g_GameStringsHandler->ProcessDisplayStrings(DELTA_TIME);

	// Controls are polled before OnLoop to allow input data to be overwritten by script API methods.
	HandleControls(isTitle);

	// Pre-loop script and event handling.
	g_GameScript->OnLoop(DELTA_TIME, false); // TODO: Don't use DELTA_TIME constant with high framerate.
	HandleAllGlobalEvents(EventType::Loop, (Activator)short(LaraItem->Index));

	// Queued input actions are read again after OnLoop, so that remaining control loop can immediately register
	// emulated keypresses from the script.
	ApplyActionQueue();

	// Control lock is processed after handling scripts because builder may want to process input externally
	// while locking player from input.
	if (!isTitle && Lara.Control.IsLocked)
		ClearAllActions();

	// Item update should happen before camera update, so potential flyby/track camera triggers
	// are processed correctly.
	UpdateAllItems();
	UpdateAllEffects();
	UpdateLara(LaraItem, isTitle);
	g_GameScriptEntities->TestCollidingObjects();

	// Smash shatters and clear stopper flags under them.
	UpdateShatters();

	// Clear last selected item in inventory (must be after on loop event handling, so they can detect that).
	g_Gui.CancelInventorySelection();

	// Update weather.
	Weather.Update();

	// Update effects.
	StreamerEffect.Update();
	UpdateWibble();
	UpdateSparks();
	UpdateFireSparks();
	UpdateSmoke();
	UpdateBlood();
	UpdateBubbles();
	UpdateDebris();
	UpdateGunShells();
	UpdateFootprints();
	UpdateSplashes();
	UpdateElectricityArcs();
	UpdateHelicalLasers();
	UpdateDrips();
	UpdateRats();
	UpdateRipples();
	UpdateBats();
	UpdateSpiders();
	UpdateSparkParticles();
	UpdateSmokeParticles();
	UpdateSimpleParticles();
	UpdateExplosionParticles();
	UpdateShockwaves();
	UpdateBeetleSwarm();
	UpdateLocusts();
	UpdateUnderwaterBloodParticles();
	UpdateFishSwarm();
	UpdateFireflySwarm();
	UpdateGlobalLensFlare();

	// Update HUD.
	g_Hud.Update(*LaraItem);
	UpdateFadeScreenAndCinematicBars();

	// Rumble screen (like in submarine level of TRC).
	if (g_GameFlow->GetLevel(CurrentLevel)->GetRumbleEnabled())
		RumbleScreen();

	DoFlipEffect(FlipEffect, LaraItem);

	PlaySoundSources();
	Sound_UpdateScene();

	auto gameStatus = GameStatus::Normal;

	if (!insideMenu)
	{
		// Handle inventory, pause, load, save screens.
		gameStatus = HandleMenuCalls(isTitle);

		// Handle global input events.
		if (gameStatus == GameStatus::Normal)
			gameStatus = HandleGlobalInputEvents(isTitle);
	}

	if (gameStatus != GameStatus::Normal)
	{
		// Call post-loop callbacks last time and end level.
		g_GameScript->OnLoop(DELTA_TIME, true);
		g_GameScript->OnEnd(gameStatus);
		HandleAllGlobalEvents(EventType::End, (Activator)short(LaraItem->Index));
	}
	else
	{
		// Post-loop script and event handling.
		g_GameScript->OnLoop(DELTA_TIME, true);
	}

	UpdateCamera();

	// Clear savegame loaded flag.
	JustLoaded = false;

	// Update timers.
	SaveGame::Statistics.Game.TimeTaken++;
	SaveGame::Statistics.Level.TimeTaken++;
	GlobalCounter++;

	auto time2 = std::chrono::high_resolution_clock::now();
	ControlPhaseTime = (std::chrono::duration_cast<std::chrono::nanoseconds>(time2 - time1)).count() / 1000000;

	return gameStatus;
}

GameStatus FreezePhase()
{
	// If needed when first entering freeze mode, do initialization.
	if (g_GameFlow->LastFreezeMode == FreezeMode::None)
	{
		// Capture the screen for drawing it as a background.
		if (g_GameFlow->CurrentFreezeMode == FreezeMode::Full)
			g_Renderer.DumpGameScene(SceneRenderMode::NoHud);

		StopRumble();
	}
	
	// Update last freeze mode here, so that items won't update inside freeze loop.
	g_GameFlow->LastFreezeMode = g_GameFlow->CurrentFreezeMode;

	g_Renderer.PrepareScene();
	g_Renderer.SaveOldState();

	ClearLensFlares();
	ClearAllDisplaySprites();

	SetupInterpolation();
	PrepareCamera();

	g_GameStringsHandler->ProcessDisplayStrings(DELTA_TIME);

	// Track previous player animation to queue hair update if needed.
	int lastAnimNumber = LaraItem->Animation.AnimNumber;

	// Poll controls and call scripting events.
	HandleControls(false);
	g_GameScript->OnFreeze();
	HandleAllGlobalEvents(EventType::Freeze, (Activator)short(LaraItem->Index));

	// Partially update scene if not using full freeze mode.
	if (g_GameFlow->LastFreezeMode != FreezeMode::Full)
	{
		if (g_GameFlow->LastFreezeMode == FreezeMode::Player)
			UpdateLara(LaraItem, false);

		UpdateAllItems();
		UpdateGlobalLensFlare();

		UpdateCamera();

		PlaySoundSources();
		Sound_UpdateScene();
	}

	// HACK: Update player hair if animation was switched in spectator mode.
	// Needed for photo mode and other similar functionality.
	if (g_GameFlow->LastFreezeMode == FreezeMode::Spectator &&
		lastAnimNumber != LaraItem->Animation.AnimNumber)
	{
		lastAnimNumber = LaraItem->Animation.AnimNumber;
		for (int i = 0; i < FPS; i++)
			HairEffect.Update(*LaraItem);
	}

	// Update last freeze mode again, as it may have been changed in a script.
	g_GameFlow->LastFreezeMode = g_GameFlow->CurrentFreezeMode;

	return GameStatus::Normal;
}

GameStatus ControlPhase(bool insideMenu)
{
	// For safety, only allow to break game loop in non-title levels.
	if (g_GameFlow->CurrentFreezeMode == FreezeMode::None || CurrentLevel == 0)
	{
		return GamePhase(insideMenu);
	}
	else
	{
		return FreezePhase();
	}
}

unsigned CALLBACK GameMain(void *)
{
	TENLog("Starting GameMain()...", LogLevel::Info);

	TimeInit();

	// Do fixed-time title image.
	if (!g_GameFlow->IntroImagePath.empty())
		g_Renderer.RenderTitleImage();

	// Play intro video.
	if (!g_GameFlow->IntroVideoPath.empty())
	{
		g_VideoPlayer.Play(g_GameFlow->IntroVideoPath);
		while (g_VideoPlayer.Update());
	}

	// Execute Lua gameflow and play game.
	g_GameFlow->DoFlow();

	DoTheGame = false;

	// Finish thread.
	PostMessage(WindowsHandle, WM_CLOSE, NULL, NULL);
	EndThread();

	return true;
}

GameStatus DoLevel(int levelIndex, bool loadGame)
{
	bool isTitle = !levelIndex;

	TENLog(isTitle ? "DoTitle" : "DoLevel", LogLevel::Info);

	// Load level. Fall back to title if unsuccessful.
	if (!LoadLevelFile(levelIndex))
		return (isTitle ? GameStatus::ExitGame : GameStatus::ExitToTitle);

	// Initialize items, effects, lots, and cameras.
	HairEffect.Initialize();
	InitializeFXArray();
	InitializeCamera();
	InitializeSpotCamSequences(isTitle);
	InitializeItemBoxData();
	InitializeSpecialEffects();

	// Initialize scripting.
	InitializeScripting(levelIndex, loadGame);
	InitializeNodeScripts();

	// Initialize menu and inventory state.
	g_Gui.Initialize();

	// Initialize game variables and optionally load game.
	InitializeOrLoadGame(loadGame);

	// DoGameLoop() returns only when level has ended.
	return DoGameLoop(levelIndex);
}

void UpdateShatters()
{
	if (!SmashedMeshCount)
		return;

	do
	{
		SmashedMeshCount--;

		auto* floor = GetFloor(
			SmashedMesh[SmashedMeshCount]->pos.Position.x,
			SmashedMesh[SmashedMeshCount]->pos.Position.y,
			SmashedMesh[SmashedMeshCount]->pos.Position.z,
			&SmashedMeshRoom[SmashedMeshCount]);

		TestTriggers(SmashedMesh[SmashedMeshCount]->pos.Position.x,
			SmashedMesh[SmashedMeshCount]->pos.Position.y,
			SmashedMesh[SmashedMeshCount]->pos.Position.z,
			SmashedMeshRoom[SmashedMeshCount], true);

		TestVolumes(SmashedMeshRoom[SmashedMeshCount], SmashedMesh[SmashedMeshCount]);

		floor->Stopper = false;
		SmashedMesh[SmashedMeshCount] = 0;
	}
	while (SmashedMeshCount != 0);
}

void KillMoveItems()
{
	if (ItemNewRoomNo > 0)
	{
		for (int i = 0; i < ItemNewRoomNo; i++)
		{
			int itemNumber = ItemNewRooms[i * 2];
			if (itemNumber >= 0)
			{
				ItemNewRoom(itemNumber, ItemNewRooms[(i * 2) + 1]);
			}
			else
			{
				KillItem(itemNumber & 0x7FFF);
			}
		}
	}

	ItemNewRoomNo = 0;
}

void KillMoveEffects()
{
	if (ItemNewRoomNo > 0)
	{
		for (int i = 0; i < ItemNewRoomNo; i++)
		{
			int itemNumber = ItemNewRooms[i * 2];
			if (itemNumber >= 0)
			{
				EffectNewRoom(itemNumber, ItemNewRooms[(i * 2) + 1]);
			}
			else
			{
				KillEffect(itemNumber & 0x7FFF);
			}
		}
	}

	ItemNewRoomNo = 0;
}

// NOTE: No one should use this ever again.
int GetRandomControl()
{
	return Random::GenerateInt();
}

int GetRandomDraw()
{
	return Random::GenerateInt();
}

void CleanUp()
{
	// Reset oscillator seed.
	Wibble = 0;

	// Clear player lock, otherwise controls will lock if user exits to title while playing flyby with locked controls.
	Lara.Control.IsLocked = false;

	// Resets lightning and wind parameters to avoid holding over previous weather to new level.
	Weather.Clear();

	// Clear creatures, otherwise list of active creatures from previous level will spill into new level.
	ActiveCreatures.clear();

	// Clear ropes.
	Ropes.clear();

	// Clear camera data.
	ClearSpotCamSequences();
	ClearCinematicBars();

	// Clear effects.
	StreamerEffect.Clear();
	ClearUnderwaterBloodParticles();
	ClearBubbles();
	ClearAllDisplaySprites();
	ClearFootprints();
	ClearDrips();
	ClearRipples();
	ClearLaserBarrierEffects();
	ClearLaserBeamEffects();
	DisableSmokeParticles();
	DisableSparkParticles();
	DisableDebris();

	// Clear swarm enemies.
	ClearSwarmEnemies(nullptr);

	// Clear HUD.
	g_Hud.Clear();

	// Clear soundtrack masks.
	ClearSoundTrackMasks();

	// Clear all remaining renderer data.
	g_Renderer.ClearScene();
	g_Renderer.SetPostProcessMode(PostProcessMode::None);
	g_Renderer.SetPostProcessStrength(1.0f);
	g_Renderer.SetPostProcessTint(Vector3::One);

	// Reset Itemcamera
	ClearObjCamera();
}

void InitializeScripting(int levelIndex, bool loadGame)
{
	TENLog("Loading level script...", LogLevel::Info);

	g_GameStringsHandler->ClearDisplayStrings();
	g_GameScript->ResetScripts(!levelIndex || loadGame);

	const auto& level = *g_GameFlow->GetLevel(levelIndex);

	// Run level script if it exists.
	if (!level.ScriptFileName.empty())
	{
		auto levelScriptName = g_GameFlow->GetGameDir() + level.ScriptFileName;
		if (std::filesystem::is_regular_file(levelScriptName))
		{
			g_GameScript->ExecuteScriptFile(levelScriptName);
		}
		else
		{
			TENLog("Level script not found: " + levelScriptName, LogLevel::Warning);
		}

		g_GameScript->InitCallbacks();
		g_GameStringsHandler->SetCallbackDrawString([](const std::string& key, D3DCOLOR color, const Vec2& pos, float scale, int flags)
		{
			g_Renderer.AddString(
				key,
				Vector2(
					(pos.x / g_Configuration.ScreenWidth) * DISPLAY_SPACE_RES.x,
					(pos.y / g_Configuration.ScreenHeight) * DISPLAY_SPACE_RES.y),
				Color(color), scale, flags);
		});
	}

	// Play default background music.
	if (!loadGame)
		PlaySoundTrack(level.GetAmbientTrack(), SoundTrackType::BGM, 0, SOUND_XFADETIME_LEVELJUMP);
}

void DeInitializeScripting(int levelIndex, GameStatus reason)
{
	// Reload gameflow script to clear level script variables.
	g_GameFlow->LoadFlowScript();

	g_GameScript->FreeLevelScripts();
	g_GameScriptEntities->FreeEntities();

	// If level index is 0, it means we are in a title level and game variables should be cleared.
	if (levelIndex == 0)
		g_GameScript->ResetScripts(true);
}

void InitializeOrLoadGame(bool loadGame)
{
	g_Gui.SetInventoryItemChosen(NO_VALUE);
	g_Gui.SetEnterInventory(NO_VALUE);

	// Restore game?
	if (loadGame)
	{
		if (!SaveGame::Load(g_GameFlow->SelectedSaveGame))
		{
			NextLevel = g_GameFlow->GetNumLevels();
			return;
		}

		InitializeGame = false;

		g_GameFlow->SelectedSaveGame = 0;
		g_GameScript->OnLoad();
		HandleAllGlobalEvents(EventType::Load, (Activator)short(LaraItem->Index));
	}
	else
	{
		// If not loading savegame, clear all info.
		SaveGame::Statistics.Level = {};
		SaveGame::Statistics.SecretBits = 0;

		if (InitializeGame)
		{
			// Clear all game info as well.
			SaveGame::Statistics.Game = {};
			InitializeGame = false;

			SaveGame::ResetHub();
			TENLog("Starting new game.", LogLevel::Info);
		}
		else
		{
			SaveGame::LoadHub(CurrentLevel);
			TENLog("Starting new level.", LogLevel::Info);
		}

		g_GameScript->OnStart();
		HandleAllGlobalEvents(EventType::Start, (Activator)short(LaraItem->Index));
	}
}

GameStatus DoGameLoop(int levelIndex)
{
	int frameCount = LOOP_FRAME_COUNT;
	auto& status = g_GameFlow->LastGameStatus;

	// Before entering actual game loop, GamePhase() must be called once to sort out
	// various runtime shenanigangs (e.g. hair or freeze mode initialization).
	status = GamePhase(false);

	g_Synchronizer.Init();
	bool legacy30FpsDoneDraw = false;

	while (DoTheGame)
	{
		g_Synchronizer.Sync();

		if (g_VideoPlayer.Update())
			continue;

		while (g_Synchronizer.Synced())
		{
			status = ControlPhase(false);
			g_Synchronizer.Step();

			legacy30FpsDoneDraw = false;
		}

		if (status != GameStatus::Normal)
			break;

		if (g_GameFlow->LastFreezeMode != g_GameFlow->CurrentFreezeMode)
			continue;

		if (!g_Configuration.EnableHighFramerate)
		{
			if (!legacy30FpsDoneDraw)
			{
				DrawPhase(!levelIndex, 0.0f);
				legacy30FpsDoneDraw = true;
			}
		}
		else
		{
			DrawPhase(!levelIndex, g_Synchronizer.GetInterpolationFactor());
		}
	}

	EndGameLoop(levelIndex, status);

	return status;
}

void EndGameLoop(int levelIndex, GameStatus reason)
{
	// Save last screenshot for loading screen.
	g_Renderer.DumpGameScene();

	SaveGame::SaveHub(levelIndex);
	DeInitializeScripting(levelIndex, reason);

	g_VideoPlayer.Stop();
	StopAllSounds();
	StopSoundTracks(SOUND_XFADETIME_LEVELJUMP, true);
	StopRumble();
}

void SetupInterpolation()
{
	for (auto& item : g_Level.Items)
		item.DisableInterpolation = false;

	// HACK: Remove after ScriptInterfaceFlowHandler is deprecated.
	auto* level = (Level*)g_GameFlow->GetLevel(CurrentLevel);
	level->Horizon1.SetPosition(level->Horizon1.GetPosition(), true);
	level->Horizon2.SetPosition(level->Horizon2.GetPosition(), true);
	level->Horizon1.SetRotation(level->Horizon1.GetRotation(), true);
	level->Horizon2.SetRotation(level->Horizon2.GetRotation(), true);
}

void HandleControls(bool isTitle)
{
	// Poll input devices and update input variables.
	// TODO: To allow cutscene skipping later, don't clear Deselect action.
	UpdateInputActions(false, true);

	if (isTitle)
		ClearAction(In::Look);
}

GameStatus HandleMenuCalls(bool isTitle)
{
	auto gameStatus = GameStatus::Normal;

	if (ScreenFading)
		return gameStatus;

	if (isTitle)
	{
		auto invStatus = g_Gui.TitleOptions(LaraItem);

		switch (invStatus)
		{
		case InventoryResult::NewGame:
		case InventoryResult::NewGameSelectedLevel:
			return GameStatus::NewGame;

		case InventoryResult::HomeLevel:
			return GameStatus::HomeLevel;
			break;

		case InventoryResult::LoadGame:
			return GameStatus::LoadGame;

		case InventoryResult::ExitGame:
			return GameStatus::ExitGame;
		}

		return gameStatus;
	}

	bool playerAlive = LaraItem->HitPoints > 0;
	
	bool doLoad      = IsClicked(In::Load) || 
					   (!IsClicked(In::Inventory) && !NoAction() && SaveGame::IsLoadGamePossible() && Lara.Control.Count.Death > DEATH_INPUT_TIMEOUT);
	bool doSave      = IsClicked(In::Save) && playerAlive;
	bool doPause     = IsClicked(In::Pause) && playerAlive;
	bool doInventory = (IsClicked(In::Inventory) || g_Gui.GetEnterInventory() != NO_VALUE) && playerAlive;

	// Handle inventory.
	if (doSave && g_GameFlow->IsLoadSaveEnabled() && Lara.Inventory.HasSave && g_Gui.GetInventoryMode() != InventoryMode::Save)
	{
		SaveGame::LoadHeaders();
		g_Gui.SetInventoryMode(InventoryMode::Save);
		g_Gui.CallInventory(LaraItem, false);
	}
	else if (doLoad && g_GameFlow->IsLoadSaveEnabled() && Lara.Inventory.HasLoad && g_Gui.GetInventoryMode() != InventoryMode::Load)
	{
		SaveGame::LoadHeaders();
		g_Gui.SetInventoryMode(InventoryMode::Load);
		if (g_Gui.CallInventory(LaraItem, false))
			gameStatus = GameStatus::LoadGame;
	}
	else if (doPause && g_Gui.GetInventoryMode() != InventoryMode::Pause)
	{
		if (g_Gui.CallPause())
			gameStatus = GameStatus::ExitToTitle;
	}
	else if (doInventory && LaraItem->HitPoints > 0 && !Lara.Control.Look.IsUsingBinoculars)
	{
		if (g_Gui.CallInventory(LaraItem, true))
			gameStatus = GameStatus::LoadGame;
	}

	if (gameStatus != GameStatus::Normal)
	{
		StopAllSounds();
		StopRumble();
	}

	return gameStatus;
}

GameStatus HandleGlobalInputEvents(bool isTitle)
{
	if (isTitle)
		return GameStatus::Normal;

	// Check if player dead.
	if (Lara.Control.Count.Death > DEATH_NO_INPUT_TIMEOUT ||
		Lara.Control.Count.Death > DEATH_INPUT_TIMEOUT && !NoAction())
	{
		// TODO: Maybe do game over menu like some PSX versions have?
		return GameStatus::LaraDead;
	}

	// Check if level has been completed.
	// Negative NextLevel indicates that a savegame must be loaded from corresponding slot.
	if (NextLevel > 0)
	{
		return GameStatus::LevelComplete;
	}
	else if (NextLevel < 0)
	{
		g_GameFlow->SelectedSaveGame = -(NextLevel + 1);
		return GameStatus::LoadGame;
	}

	return GameStatus::Normal;
}
