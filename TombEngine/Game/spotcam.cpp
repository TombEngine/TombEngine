#include "framework.h"
#include "Game/spotcam.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/control/control.h"
#include "Game/control/volume.h"
#include "Game/collision/Point.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Specific/Input/Input.h"

using namespace TEN::Animation;
using namespace TEN::Input;
using namespace TEN::Renderer;
using namespace TEN::Control::Volumes;
using namespace TEN::Collision::Point;

constexpr auto MAX_SPLINE_KNOTS = 18;

// Duration in seconds for smooth ease-in and ease-out during camera pauses.
constexpr auto PAUSE_EASE_DURATION = 0.5f;
constexpr auto PAUSE_EASE_STEP     = 1.0f / (PAUSE_EASE_DURATION * FPS);

enum class PausePhase
{
	None,    // Normal playback; speed factor = 1.
	EaseOut, // Decelerating; speed factor transitions from 1 to 0.
	Hold,    // Fully stopped; speed factor = 0.
	EaseIn   // Accelerating; speed factor transitions from 0 to 1.
};

// Spline knot arrays for camera interpolation. Each component is stored
// as a contiguous float array so it can be passed directly to Spline().
struct SplineCameraKnots
{
	float PosX[MAX_SPLINE_KNOTS]    = {};
	float PosY[MAX_SPLINE_KNOTS]    = {};
	float PosZ[MAX_SPLINE_KNOTS]    = {};
	float TargetX[MAX_SPLINE_KNOTS] = {};
	float TargetY[MAX_SPLINE_KNOTS] = {};
	float TargetZ[MAX_SPLINE_KNOTS] = {};
	float Roll[MAX_SPLINE_KNOTS]    = {};
	float FOV[MAX_SPLINE_KNOTS]     = {};
	float Speed[MAX_SPLINE_KNOTS]   = {};

	void SetKnot(int index, const SpotCam& cam)
	{
		PosX[index]    = (float)cam.Position.x;
		PosY[index]    = (float)cam.Position.y;
		PosZ[index]    = (float)cam.Position.z;
		TargetX[index] = (float)cam.Target.x;
		TargetY[index] = (float)cam.Target.y;
		TargetZ[index] = (float)cam.Target.z;
		Roll[index]    = (float)cam.Roll;
		FOV[index]     = (float)cam.FOV;
		Speed[index]   = (float)cam.Speed;
	}
};

// Public globals (declared extern in header).
std::vector<SpotCam>         SpotCams;
std::unordered_map<int, int> SpotCamRemap;
std::vector<int>             CameraCnt;

int  LastSpotCamSequence = 0;
bool TrackCameraInit     = false;
bool UseSpotCam          = false;
bool SpotcamSwitched     = false;
bool SpotcamDontDrawLara = false;
bool SpotcamOverlay      = false;

// File-local state.
static SplineCameraKnots Knots = {};

static float  SplineAlpha         = 0.0f;  // Normalized spline position [0, 1].
static int    FirstCameraIndex    = 0;
static int    LastCameraIndex     = 0;
static int    SequenceCameraCount = 0;
static int    SplineFromOffset    = 0;     // Number of leading knots sourced from initial camera.
static short  CurrentCameraIndex  = 0;
static int    CurrentSequenceID   = 0;

static bool   IsFirstLookPress    = false;
static bool   IsTransitionToGame  = false; // Transitioning back to gameplay camera.
static bool   RunHeavyTriggers    = false;

// Pause state machine.
static PausePhase CurrentPausePhase   = PausePhase::None;
static float      PauseSpeedFactor    = 1.0f;  // Multiplier applied to camera speed.
static float      PauseEaseProgress   = 0.0f;  // Progress through current ease phase [0, 1].
static int        PauseHoldTimer      = 0;     // Frames remaining in hold phase.
static bool       IsPauseComplete     = false; // Prevents re-triggering pause for same segment.

static int    LoopCount       = 0;
static int    FadeCameraIndex = NO_VALUE;

static Vector3i SavedLaraPos       = Vector3i::Zero;
static int      SavedCameraRoom    = 0;
static Vector3i SavedCameraPos     = Vector3i::Zero;
static Vector3i SavedCameraTarget  = Vector3i::Zero;
static int      SavedLaraHealth    = 0;
static int      SavedLaraAir       = 0;

// Updates the pause state machine each frame.
static void UpdatePause()
{
	switch (CurrentPausePhase)
	{
	case PausePhase::EaseOut:
		PauseEaseProgress += PAUSE_EASE_STEP;

		if (PauseEaseProgress >= 1.0f)
		{
			PauseEaseProgress = 1.0f;
			PauseSpeedFactor = 0.0f;
			CurrentPausePhase = PausePhase::Hold;
		}
		else
		{
			PauseSpeedFactor = 1.0f - Smoothstep(PauseEaseProgress);
		}
		break;

	case PausePhase::Hold:
		PauseHoldTimer--;

		if (PauseHoldTimer <= 0)
		{
			PauseEaseProgress = 0.0f;
			CurrentPausePhase = PausePhase::EaseIn;
		}
		break;

	case PausePhase::EaseIn:
		PauseEaseProgress += PAUSE_EASE_STEP;

		if (PauseEaseProgress >= 1.0f)
		{
			PauseEaseProgress = 0.0f;
			PauseSpeedFactor = 1.0f;
			CurrentPausePhase = PausePhase::None;
			IsPauseComplete = true;
		}
		else
		{
			PauseSpeedFactor = Smoothstep(PauseEaseProgress);
		}
		break;

	default:
		break;
	}
}

// Begins a smooth pause: ease-out, hold, then ease-in.
static void BeginPause(int holdFrames)
{
	PauseHoldTimer = holdFrames;
	PauseEaseProgress = 0.0f;
	CurrentPausePhase = PausePhase::EaseOut;
}

// Resets the pause state machine to idle.
static void ResetPause()
{
	CurrentPausePhase = PausePhase::None;
	PauseSpeedFactor  = 1.0f;
	PauseEaseProgress = 0.0f;
	PauseHoldTimer    = 0;
	IsPauseComplete   = false;
}

void ClearSpotCamSequences()
{
	UseSpotCam = false;
	SpotcamDontDrawLara = false;
	SpotcamOverlay = false;

	SpotCams.clear();
	SpotCamRemap.clear();
	CameraCnt.clear();
}

void InitializeSpotCamSequences(bool startFirstSequence)
{
	TrackCameraInit = false;

	CameraCnt.clear();
	SpotCamRemap.clear();

	if (SpotCams.empty())
		return;

	unsigned int currentSequence = SpotCams[0].Sequence;
	int count = 0;

	for (const auto& cam : SpotCams)
	{
		if (cam.Sequence != currentSequence)
		{
			SpotCamRemap[currentSequence] = (int)CameraCnt.size();
			CameraCnt.push_back(count);
			currentSequence = cam.Sequence;
			count = 0;
		}

		count++;
	}

	SpotCamRemap[currentSequence] = (int)CameraCnt.size();
	CameraCnt.push_back(count);

	if (startFirstSequence && SpotCamRemap.count(0))
	{
		InitializeSpotCam(0);
		UseSpotCam = true;
	}
}

void InitializeSpotCam(short sequence)
{
	if (SpotCams.empty() || SpotCamRemap.find(sequence) == SpotCamRemap.end())
	{
		TENLog(fmt::format("Initializing flyby sequence {} failed, sequence not found.", sequence), LogLevel::Warning);
		return;
	}

	if (TrackCameraInit && LastSpotCamSequence == sequence)
	{
		TrackCameraInit = false;
		return;
	}

	// Reset player data.
	LaraItem->MeshBits = ALL_JOINT_BITS;
	ResetPlayerFlex(LaraItem);

	Lara.Control.Look.OpticRange = 0;
	Lara.Control.Look.IsUsingLasersight = false;
	Lara.Control.IsLocked = false;
	Lara.Inventory.IsBusy = 0;

	AlterFOV(ANGLE(DEFAULT_FOV), false);
	Camera.bounce = 0;

	// Reset spotcam state.
	FadeCameraIndex      = NO_VALUE;
	LastSpotCamSequence  = sequence;
	TrackCameraInit      = false;
	LoopCount            = 0;
	ResetPause();

	// Save player state.
	SavedLaraAir    = Lara.Status.Air;
	SavedLaraHealth = LaraItem->HitPoints;
	SavedLaraPos    = LaraItem->Pose.Position;

	// Save camera state.
	SavedCameraPos    = Vector3i(Camera.pos.x, Camera.pos.y, Camera.pos.z);
	SavedCameraTarget = Vector3i(Camera.target.x, Camera.target.y, Camera.target.z);
	SavedCameraRoom   = Camera.pos.RoomNumber;

	// Compute first camera index for this sequence.
	CurrentSequenceID = sequence;
	CurrentCameraIndex = 0;

	for (int i = 0; i < SpotCamRemap[sequence]; i++)
		CurrentCameraIndex += CameraCnt[i];

	SplineAlpha = 0.0f;
	IsTransitionToGame = false;

	FirstCameraIndex = CurrentCameraIndex;
	LastCameraIndex  = CurrentCameraIndex + CameraCnt[SpotCamRemap[sequence]] - 1;

	SequenceCameraCount = CameraCnt[SpotCamRemap[sequence]];

	const auto& firstCam = SpotCams[CurrentCameraIndex];

	if (firstCam.Flags & SCF_DISABLE_LARA_CONTROLS)
	{
		Lara.Control.IsLocked = true;
		SetCinematicBars(SPOTCAM_CINEMATIC_BARS_HEIGHT, SPOTCAM_CINEMATIC_BARS_SPEED);
	}

	// Populate spline knot arrays.
	Knots = {};

	if (firstCam.Flags & SCF_TRACKING_CAM)
	{
		// Tracking camera: pad with first camera, then all cameras, then pad with last.
		Knots.SetKnot(1, SpotCams[FirstCameraIndex]);
		SplineFromOffset = 0;

		for (int i = 0; i < SequenceCameraCount; i++)
			Knots.SetKnot(i + 2, SpotCams[FirstCameraIndex + i]);

		Knots.SetKnot(SequenceCameraCount + 2, SpotCams[LastCameraIndex]);
	}
	else if (firstCam.Flags & SCF_CUT_PAN)
	{
		// Cut-pan: first knot is current camera, then fill 4 knots from sequence.
		Knots.SetKnot(1, SpotCams[CurrentCameraIndex]);
		Camera.DisableInterpolation = true;
		SplineFromOffset = 0;

		int camIndex = CurrentCameraIndex;
		for (int i = 0; i < 4; i++)
		{
			if (camIndex > LastCameraIndex)
				camIndex = FirstCameraIndex;

			Knots.SetKnot(i + 2, SpotCams[camIndex]);
			camIndex++;
		}

		CurrentCameraIndex++;
		if (CurrentCameraIndex > LastCameraIndex)
			CurrentCameraIndex = FirstCameraIndex;

		if (firstCam.Flags & SCF_ACTIVATE_HEAVY_TRIGGERS)
			RunHeavyTriggers = true;

		if (firstCam.Flags & SCF_HIDE_LARA)
			SpotcamDontDrawLara = true;
	}
	else
	{
		// Smooth pan: blend from current camera position to first spotcam.
		SplineFromOffset = 1;

		// Knots [1] and [2] = current camera position (for smooth approach).
		auto setInitialKnot = [&](int index)
		{
			Knots.PosX[index]    = (float)SavedCameraPos.x;
			Knots.PosY[index]    = (float)SavedCameraPos.y;
			Knots.PosZ[index]    = (float)SavedCameraPos.z;
			Knots.TargetX[index] = (float)SavedCameraTarget.x;
			Knots.TargetY[index] = (float)SavedCameraTarget.y;
			Knots.TargetZ[index] = (float)SavedCameraTarget.z;
			Knots.FOV[index]     = (float)CurrentFOV;
			Knots.Roll[index]    = 0.0f;
			Knots.Speed[index]   = (float)firstCam.Speed;
		};

		setInitialKnot(1);
		setInitialKnot(2);

		// Knot [3] = first spotcam in sequence.
		Knots.SetKnot(3, SpotCams[CurrentCameraIndex]);

		// Knot [4] = next spotcam (or clamped to last).
		int nextIndex = CurrentCameraIndex + 1;
		if (nextIndex > LastCameraIndex)
			nextIndex = FirstCameraIndex;

		Knots.SetKnot(4, SpotCams[nextIndex]);
	}

	if (firstCam.Flags & SCF_HIDE_LARA)
		SpotcamDontDrawLara = true;
}

// Runs heavy triggers at the camera's current position.
static void RunCameraHeavyTriggers()
{
	auto oldType = Camera.type;
	Camera.type = CameraType::Heavy;

	if (CurrentLevel != 0)
	{
		TestTriggers(Camera.pos.x, Camera.pos.y, Camera.pos.z, Camera.pos.RoomNumber, true);
		TestVolumes(&Camera);
	}
	else
	{
		TestTriggers(Camera.pos.x, Camera.pos.y, Camera.pos.z, Camera.pos.RoomNumber, false);
		TestTriggers(Camera.pos.x, Camera.pos.y, Camera.pos.z, Camera.pos.RoomNumber, true);
		TestVolumes(&Camera);
	}

	Camera.type = oldType;
}

// Ends the spotcam sequence and restores normal camera.
static void EndSpotCamSequence(const SpotCam& firstCam)
{
	if (RunHeavyTriggers)
	{
		RunCameraHeavyTriggers();
		RunHeavyTriggers = false;
	}

	SetCinematicBars(0.0f, SPOTCAM_CINEMATIC_BARS_SPEED);

	UseSpotCam = false;
	RunHeavyTriggers = false;
	Lara.Control.IsLocked = false;
	Lara.Control.Look.IsUsingBinoculars = false;
	Camera.oldType = CameraType::Fixed;
	Camera.type = CameraType::Chase;
	Camera.speed = 1;
	Camera.DisableInterpolation = true;

	if (firstCam.Flags & SCF_CUT_TO_LARA_CAM)
	{
		Camera.pos.x = SavedCameraPos.x;
		Camera.pos.y = SavedCameraPos.y;
		Camera.pos.z = SavedCameraPos.z;
		Camera.pos.RoomNumber = SavedCameraRoom;
		Camera.target.x = SavedCameraTarget.x;
		Camera.target.y = SavedCameraTarget.y;
		Camera.target.z = SavedCameraTarget.z;
	}

	SpotcamOverlay = false;
	SpotcamDontDrawLara = false;
	AlterFOV(LastFOV);
}

// Fills 4 spline knots starting from the given camera index, wrapping or clamping as needed.
static void FillSplineKnots(int startKnotIndex, int startCamIndex, int count, bool loop)
{
	int camIndex = startCamIndex;
	for (int i = 0; i < count; i++)
	{
		if (loop)
		{
			if (camIndex > LastCameraIndex)
				camIndex = FirstCameraIndex;
		}
		else
		{
			if (camIndex > LastCameraIndex)
				camIndex = LastCameraIndex;
		}

		Knots.SetKnot(startKnotIndex + i, SpotCams[camIndex]);
		camIndex++;
	}
}

// Tracking camera: finds the closest spline position to Lara using a coarse-to-fine search.
static float FindClosestSplineAlpha(int knotCount)
{
	auto laraPos = LaraItem->Pose.Position;
	float closestAlpha = 0.0f;
	float searchStep = 1.0f / 8.0f;

	float searchStart = 0.0f;

	for (int iteration = 0; iteration < 8; iteration++)
	{
		float closestDist = 65536.0f;

		for (int sample = 0; sample < 8; sample++)
		{
			float sampleAlpha = searchStart + sample * searchStep;
			if (sampleAlpha > 1.0f)
				break;

			float cx = Spline(sampleAlpha, &Knots.PosX[1], knotCount);
			float cy = Spline(sampleAlpha, &Knots.PosY[1], knotCount);
			float cz = Spline(sampleAlpha, &Knots.PosZ[1], knotCount);

			float dist = Vector3::Distance(
				Vector3(cx, cy, cz),
				Vector3((float)laraPos.x, (float)laraPos.y, (float)laraPos.z));

			if (dist <= closestDist)
			{
				closestAlpha = sampleAlpha;
				closestDist = dist;
			}
		}

		float halfStep = searchStep / 2.0f;
		searchStart = closestAlpha - 2.0f * halfStep;
		if (searchStart < 0.0f)
			searchStart = 0.0f;

		searchStep = halfStep;
	}

	return closestAlpha;
}

void CalculateSpotCameras()
{
	if (SpotCams.empty() || FirstCameraIndex >= (int)SpotCams.size())
	{
		UseSpotCam = false;
		return;
	}

	if (Lara.Control.IsLocked)
	{
		LaraItem->HitPoints = SavedLaraHealth;
		Lara.Status.Air = SavedLaraAir;
	}

	const auto& firstCam = SpotCams[FirstCameraIndex];
	int knotCount = (firstCam.Flags & SCF_TRACKING_CAM) ? (SequenceCameraCount + 2) : 4;

	// Interpolate all camera properties at current spline position.
	float interpPosX    = Spline(SplineAlpha, &Knots.PosX[1], knotCount);
	float interpPosY    = Spline(SplineAlpha, &Knots.PosY[1], knotCount);
	float interpPosZ    = Spline(SplineAlpha, &Knots.PosZ[1], knotCount);
	float interpTargetX = Spline(SplineAlpha, &Knots.TargetX[1], knotCount);
	float interpTargetY = Spline(SplineAlpha, &Knots.TargetY[1], knotCount);
	float interpTargetZ = Spline(SplineAlpha, &Knots.TargetZ[1], knotCount);
	float interpSpeed   = Spline(SplineAlpha, &Knots.Speed[1], knotCount);
	float interpRoll    = Spline(SplineAlpha, &Knots.Roll[1], knotCount);
	float interpFOV     = Spline(SplineAlpha, &Knots.FOV[1], knotCount);

	// Handle screen fading.
	if ((SpotCams[CurrentCameraIndex].Flags & SCF_SCREEN_FADE_IN) &&
		FadeCameraIndex != CurrentCameraIndex)
	{
		SetScreenFadeIn(FADE_SCREEN_SPEED);
		FadeCameraIndex = CurrentCameraIndex;
	}

	if ((SpotCams[CurrentCameraIndex].Flags & SCF_SCREEN_FADE_OUT) &&
		FadeCameraIndex != CurrentCameraIndex)
	{
		SetScreenFadeOut(FADE_SCREEN_SPEED);
		FadeCameraIndex = CurrentCameraIndex;
	}

	// Tracking camera: advance spline position to track Lara.
	if (firstCam.Flags & SCF_TRACKING_CAM)
	{
		float closestAlpha = FindClosestSplineAlpha(knotCount);

		// Smoothly approach the closest position.
		SplineAlpha += (closestAlpha - SplineAlpha) / 32.0f;

		if ((firstCam.Flags & SCF_CUT_PAN) && std::abs(closestAlpha - SplineAlpha) > 0.5f)
			SplineAlpha = closestAlpha;

		SplineAlpha = std::clamp(SplineAlpha, 0.0f, 1.0f);
	}
	else
	{
		// Non-tracking: advance by interpolated speed (normalize from [0, 65536] to [0, 1]).
		// Apply pause speed factor for smooth easing.
		SplineAlpha = std::min(SplineAlpha + interpSpeed / 65536.0f * PauseSpeedFactor, 1.0f);
	}

	bool lookPressed = IsHeld(In::Look);
	if (!lookPressed)
		IsFirstLookPress = false;

	// Handle look-key breakout for non-tracking cameras.
	if (!(firstCam.Flags & SCF_DISABLE_BREAKOUT) && lookPressed)
	{
		if (firstCam.Flags & SCF_TRACKING_CAM)
		{
			if (!IsFirstLookPress)
			{
				Camera.oldType = CameraType::Fixed;
				IsFirstLookPress = true;
			}

			CalculateCamera(LaraCollision);
		}
		else
		{
			// Break out of spotcam entirely.
			SetScreenFadeIn(FADE_SCREEN_SPEED);
			SetCinematicBars(0.0f, SPOTCAM_CINEMATIC_BARS_SPEED);
			UseSpotCam = false;
			Lara.Control.IsLocked = false;
			Camera.speed = 1;
			AlterFOV(LastFOV);
			CalculateCamera(LaraCollision);
			RunHeavyTriggers = false;
		}

		return;
	}

	// Disable interpolation if camera jumped too far.
	auto origin = Vector3((float)Camera.pos.x, (float)Camera.pos.y, (float)Camera.pos.z);
	auto target = Vector3(interpPosX, interpPosY, interpPosZ);
	if (Vector3::Distance(origin, target) > BLOCK(0.25f))
		Camera.DisableInterpolation = true;

	// Apply interpolated camera position.
	Camera.pos.x = (int)interpPosX;
	Camera.pos.y = (int)interpPosY;
	Camera.pos.z = (int)interpPosZ;

	if ((firstCam.Flags & SCF_FOCUS_LARA_HEAD) || (firstCam.Flags & SCF_TRACKING_CAM))
	{
		Camera.target.x = LaraItem->Pose.Position.x;
		Camera.target.y = LaraItem->Pose.Position.y;
		Camera.target.z = LaraItem->Pose.Position.z;
	}
	else
	{
		Camera.target.x = (int)interpTargetX;
		Camera.target.y = (int)interpTargetY;
		Camera.target.z = (int)interpTargetZ;
		CalculateBounce(false);
	}

	// Resolve camera room number.
	int outsideRoom = IsRoomOutside(Camera.pos.x, Camera.pos.y, Camera.pos.z);
	if (outsideRoom == NO_VALUE)
	{
		// HACK: Sometimes actual camera room number desyncs from room number derived using floordata functions.
		// If such case is identified, we do a brute-force search for coherent room number.
		// This issue is only present in sub-click floor height setups after TE 1.7.0. -- Lwmte, 02.11.2024

		auto pos = Vector3i(Camera.pos.x, Camera.pos.y, Camera.pos.z);
		int collRoomNumber = GetPointCollision(pos, SpotCams[CurrentCameraIndex].RoomNumber).GetRoomNumber();

		if (collRoomNumber != Camera.pos.RoomNumber && !IsPointInRoom(pos, collRoomNumber))
			collRoomNumber = FindRoomNumber(pos, SpotCams[CurrentCameraIndex].RoomNumber);

		Camera.pos.RoomNumber = collRoomNumber;
	}
	else
	{
		Camera.pos.RoomNumber = outsideRoom;
	}

	AlterFOV((short)interpFOV, false);
	LookAt(&Camera, (short)interpRoll);
	UpdateMikePos(*LaraItem);

	// Apply per-camera flags.
	if (SpotCams[CurrentCameraIndex].Flags & SCF_OVERLAY)
		SpotcamOverlay = true;

	if (SpotCams[CurrentCameraIndex].Flags & SCF_HIDE_LARA)
		SpotcamDontDrawLara = true;

	if (SpotCams[CurrentCameraIndex].Flags & SCF_ACTIVATE_HEAVY_TRIGGERS)
		RunHeavyTriggers = true;

	if (RunHeavyTriggers)
	{
		RunCameraHeavyTriggers();
		RunHeavyTriggers = false;
	}

	// Tracking camera just sets init flag and returns.
	if (firstCam.Flags & SCF_TRACKING_CAM)
	{
		TrackCameraInit = true;
		return;
	}

	// While in any pause phase, update the state machine and skip segment transitions.
	if (CurrentPausePhase != PausePhase::None)
	{
		UpdatePause();
		return;
	}

	// Non-tracking: check if the spline segment is complete.
	float normalizedSpeed = interpSpeed / 65536.0f;
	if (SplineAlpha <= 1.0f - normalizedSpeed)
		return;

	// Handle pause at end of segment.
	if (SpotCams[CurrentCameraIndex].Timer > 0 &&
		(SpotCams[CurrentCameraIndex].Flags & SCF_STOP_MOVEMENT) &&
		!IsPauseComplete)
	{
		BeginPause(SpotCams[CurrentCameraIndex].Timer >> 3);
		return;
	}

	// Segment complete: advance to next camera.
	SplineAlpha = 0.0f;
	IsPauseComplete = false;

	int prevCamIndex = (CurrentCameraIndex != FirstCameraIndex) ? (CurrentCameraIndex - 1) : LastCameraIndex;
	int knotStartIndex = 1;

	if (SplineFromOffset != 0)
	{
		// First segment was from initial camera; now switch to spotcam-only spline.
		SplineFromOffset = 0;
		prevCamIndex = FirstCameraIndex - 1;
		knotStartIndex = 2; // Leave knot[1] unchanged.
	}
	else
	{
		if (SpotCams[CurrentCameraIndex].Flags & SCF_REENABLE_LARA_CONTROLS)
			Lara.Control.IsLocked = false;

		if (SpotCams[CurrentCameraIndex].Flags & SCF_DISABLE_LARA_CONTROLS)
		{
			if (CurrentLevel)
				SetCinematicBars(SPOTCAM_CINEMATIC_BARS_HEIGHT, SPOTCAM_CINEMATIC_BARS_SPEED);
			Lara.Control.IsLocked = true;
		}

		// Handle cut-to-cam: jump to a specific camera in the sequence.
		if (SpotCams[CurrentCameraIndex].Flags & SCF_CUT_TO_CAM)
		{
			int jumpTarget = FirstCameraIndex + SpotCams[CurrentCameraIndex].Timer;
			Camera.DisableInterpolation = true;

			Knots.SetKnot(1, SpotCams[jumpTarget]);
			knotStartIndex = 2;
			CurrentCameraIndex = jumpTarget;
			prevCamIndex = jumpTarget;
		}

		knotStartIndex++;
		Knots.SetKnot(knotStartIndex - 1, SpotCams[prevCamIndex]);
	}

	// Fill remaining knots from subsequent cameras.
	int nextCamIndex = prevCamIndex + 1;
	bool isLooping = (firstCam.Flags & SCF_LOOP_SEQUENCE) != 0;
	FillSplineKnots(knotStartIndex, nextCamIndex, 4 - (knotStartIndex - 1), isLooping);

	CurrentCameraIndex++;
	IsPauseComplete = false;

	if (CurrentCameraIndex <= LastCameraIndex)
		return;

	// Sequence ended.
	if (firstCam.Flags & SCF_LOOP_SEQUENCE)
	{
		CurrentCameraIndex = FirstCameraIndex;
		LoopCount++;
		return;
	}

	if ((firstCam.Flags & SCF_CUT_TO_LARA_CAM) || IsTransitionToGame)
	{
		EndSpotCamSequence(firstCam);
		return;
	}

	// No explicit end flag: smoothly blend back to gameplay camera.
	Knots.SetKnot(1, SpotCams[CurrentCameraIndex - 1]);
	Knots.SetKnot(2, SpotCams[CurrentCameraIndex - 1]);

	CAMERA_INFO backup;
	memcpy(&backup, &Camera, sizeof(CAMERA_INFO));

	Camera.oldType = CameraType::Fixed;
	Camera.type = CameraType::Chase;
	Camera.speed = 1;

	int savedElevation = Camera.targetElevation;
	CalculateCamera(LaraCollision);

	Knots.Roll[2]  = 0.0f;
	Knots.Roll[3]  = 0.0f;
	Knots.Speed[2] = Knots.Speed[1];

	SavedCameraPos    = Vector3i(Camera.pos.x, Camera.pos.y, Camera.pos.z);
	SavedCameraTarget = Vector3i(Camera.target.x, Camera.target.y, Camera.target.z);

	Knots.PosX[3]    = (float)Camera.pos.x;
	Knots.PosY[3]    = (float)Camera.pos.y;
	Knots.PosZ[3]    = (float)Camera.pos.z;
	Knots.TargetX[3] = (float)Camera.target.x;
	Knots.TargetY[3] = (float)Camera.target.y;
	Knots.TargetZ[3] = (float)Camera.target.z;
	Knots.FOV[3]     = (float)LastFOV;
	Knots.Speed[3]   = Knots.Speed[2];
	Knots.Roll[3]    = 0.0f;

	Knots.PosX[4]    = (float)Camera.pos.x;
	Knots.PosY[4]    = (float)Camera.pos.y;
	Knots.PosZ[4]    = (float)Camera.pos.z;
	Knots.TargetX[4] = (float)Camera.target.x;
	Knots.TargetY[4] = (float)Camera.target.y;
	Knots.TargetZ[4] = (float)Camera.target.z;
	Knots.FOV[4]     = (float)LastFOV;
	Knots.Speed[4]   = Knots.Speed[2] / 2.0f;
	Knots.Roll[4]    = 0.0f;

	memcpy(&Camera, &backup, sizeof(CAMERA_INFO));
	Camera.targetElevation = savedElevation;

	LookAt(&Camera, (short)interpRoll);
	UpdateMikePos(*LaraItem);

	IsTransitionToGame = true;

	if (CurrentCameraIndex > LastCameraIndex)
		CurrentCameraIndex = LastCameraIndex;
}

// Catmull-Rom spline interpolation.
// @param alpha  Normalized parameter in [0, 1] across all segments.
// @param knots  Array of control point values.
// @param knotCount  Number of control points (must be >= 4).
float Spline(float alpha, const float* knots, int knotCount)
{
	int segments = knotCount - 3;
	int span = (int)(alpha * segments);

	if (span >= segments)
		span = segments - 1;

	const float* k = &knots[span];
	float t = alpha * segments - (float)span;

	// Catmull-Rom coefficients:
	// c1 = (-k0 + 3*k1 - 3*k2 + k3) / 2 (cubic)
	// c2 = (2*k0 - 5*k1 + 4*k2 - k3) / 2 (quadratic)
	// c3 = (k2 - k0) / 2 (linear)
	// c0 = k1 (constant)

	float c1 = (-k[0] + 3.0f * k[1] - 3.0f * k[2] + k[3]) * 0.5f;
	float c2 = k[0] - 2.5f * k[1] + 2.0f * k[2] - 0.5f * k[3];
	float c3 = (k[2] - k[0]) * 0.5f;

	return t * (t * (t * c1 + c2) + c3) + k[1];
}

Pose GetCameraTransform(int sequence, float alpha, bool loop)
{
	constexpr auto BLEND_RANGE = 0.1f;
	constexpr auto BLEND_START = BLEND_RANGE;
	constexpr auto BLEND_END   = 1.0f - BLEND_RANGE;

	alpha = std::clamp(alpha, 0.0f, 1.0f);

	if (sequence < 0 || SpotCamRemap.find(sequence) == SpotCamRemap.end())
	{
		TENLog("Wrong flyby sequence number provided for getting camera coordinates.", LogLevel::Warning);
		return Pose::Zero;
	}

	int cameraCount = CameraCnt[SpotCamRemap[sequence]];
	if (cameraCount < 2)
	{
		TENLog("Not enough cameras in flyby sequence to calculate the coordinates.", LogLevel::Warning);
		return Pose::Zero;
	}

	// Find first camera index for this sequence.
	int firstIndex = 0;
	for (int i = 0; i < SpotCamRemap[sequence]; i++)
		firstIndex += CameraCnt[i];

	int splinePoints = cameraCount + 2;

	// Build float arrays for spline interpolation.
	std::vector<float> xOrigins, yOrigins, zOrigins, xTargets, yTargets, zTargets, rolls;
	xOrigins.reserve(splinePoints);
	yOrigins.reserve(splinePoints);
	zOrigins.reserve(splinePoints);
	xTargets.reserve(splinePoints);
	yTargets.reserve(splinePoints);
	zTargets.reserve(splinePoints);
	rolls.reserve(splinePoints);

	for (int i = -1; i < (cameraCount + 1); i++)
	{
		int seqID = std::clamp(firstIndex + i, firstIndex, (firstIndex + cameraCount) - 1);

		xOrigins.push_back((float)SpotCams[seqID].Position.x);
		yOrigins.push_back((float)SpotCams[seqID].Position.y);
		zOrigins.push_back((float)SpotCams[seqID].Position.z);
		xTargets.push_back((float)SpotCams[seqID].Target.x);
		yTargets.push_back((float)SpotCams[seqID].Target.y);
		zTargets.push_back((float)SpotCams[seqID].Target.z);
		rolls.push_back((float)SpotCams[seqID].Roll);
	}

	auto getInterpolatedPoint = [&](float t, std::vector<float>& x, std::vector<float>& y, std::vector<float>& z)
	{
		return Vector3(Spline(t, x.data(), splinePoints),
		               Spline(t, y.data(), splinePoints),
		               Spline(t, z.data(), splinePoints));
	};

	auto getInterpolatedRoll = [&](float t)
	{
		return Spline(t, rolls.data(), splinePoints);
	};

	Vector3 originPos, targetPos;
	short orientZ = 0;

	// If looping and alpha is near sequence boundaries, blend between end and start.
	if (loop && (alpha < BLEND_START || alpha >= BLEND_END))
	{
		float blendFactor = (alpha < BLEND_START)
			? (0.5f + (alpha / BLEND_RANGE) * 0.5f)
			: ((alpha - BLEND_END) / BLEND_START) * 0.5f;

		originPos = Vector3::Lerp(
			getInterpolatedPoint(BLEND_END, xOrigins, yOrigins, zOrigins),
			getInterpolatedPoint(BLEND_START, xOrigins, yOrigins, zOrigins),
			blendFactor);
		targetPos = Vector3::Lerp(
			getInterpolatedPoint(BLEND_END, xTargets, yTargets, zTargets),
			getInterpolatedPoint(BLEND_START, xTargets, yTargets, zTargets),
			blendFactor);
		orientZ = (short)Lerp(getInterpolatedRoll(BLEND_END), getInterpolatedRoll(BLEND_START), blendFactor);
	}
	else
	{
		originPos = getInterpolatedPoint(alpha, xOrigins, yOrigins, zOrigins);
		targetPos = getInterpolatedPoint(alpha, xTargets, yTargets, zTargets);
		orientZ   = (short)getInterpolatedRoll(alpha);
	}

	auto pose = Pose(originPos, EulerAngles(targetPos - originPos));
	pose.Orientation.z = orientZ;
	return pose;
}
