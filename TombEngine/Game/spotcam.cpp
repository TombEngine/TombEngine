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
#include "Math/Utils.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Input;
using namespace TEN::Math;
using namespace TEN::Renderer;
using namespace TEN::Control::Volumes;
using namespace TEN::Collision::Point;

namespace TEN::SpotCam
{
	enum class PausePhase
	{
		None,    // Normal playback, speed factor = 1.
		EaseOut, // Quadratic ease-out, speed factor transitions from 1 to 0.
		Hold,    // Fully stopped, speed factor = 0.
		EaseIn   // Quadratic ease-in, speed factor transitions from 0 to 1.
	};
	
	struct SplineCameraKnots
	{
		static const int TRACKING_KNOT_COUNT = 3;
		static const int SPLINE_KNOT_COUNT   = 6;
		static const int BLEND_KNOT_COUNT    = 5;

		std::vector<float> PosX    = {};
		std::vector<float> PosY    = {};
		std::vector<float> PosZ    = {};
		std::vector<float> TargetX = {};
		std::vector<float> TargetY = {};
		std::vector<float> TargetZ = {};
		std::vector<float> Roll    = {};
		std::vector<float> FOV     = {};
		std::vector<float> Speed   = {};

		void Resize(int count)
		{
			PosX.assign(count, 0.0f);
			PosY.assign(count, 0.0f);
			PosZ.assign(count, 0.0f);
			TargetX.assign(count, 0.0f);
			TargetY.assign(count, 0.0f);
			TargetZ.assign(count, 0.0f);
			Roll.assign(count, 0.0f);
			FOV.assign(count, 0.0f);
			Speed.assign(count, 0.0f);
		}
	
		void SetKnot(int index, const SpotCamInfo& cam)
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
	
	// Public globals.

	int  LastSpotCamSequence = 0;
	bool TrackCameraInit     = false;
	bool UseSpotCam          = false;
	bool SpotcamSwitched     = false;
	bool SpotcamDontDrawLara = false;
	bool SpotcamOverlay      = false;
	
	// Local state.
	
	static SplineCameraKnots Knots = {};

	static std::unordered_map<int, int> SequenceMap      = {};
	static std::vector<int>             SequenceCamCount = {};

	static float  SplineAlpha         = 0.0f; // Normalized spline position [0, 1].
	
	static int    FirstCameraIndex    = 0;
	static int    LastCameraIndex     = 0;
	static int    CurrentCameraIndex  = 0;
	static int    FadeCameraIndex     = NO_VALUE;
	
	static int    CurrentSequenceID   = 0;
	static int    SequenceCameraCount = 0;
	static int    LoopCount           = 0;
	static int    SplineFromOffset    = 0; // Number of leading knots sourced from initial camera.
	
	static bool   IsFirstLookPress    = false;
	static bool   IsTransitionToGame  = false;
	static bool   RunHeavyTriggers    = false;
	
	
	static int      SavedCameraRoom    = 0;
	static Vector3i SavedCameraPos     = Vector3i::Zero;
	static Vector3i SavedCameraTarget  = Vector3i::Zero;
	static int      SavedLaraHealth    = 0;
	static int      SavedLaraAir       = 0;
	
	// Pause state machine.
	
	static PausePhase CurrentPausePhase   = PausePhase::None;
	static float      PauseSpeedFactor    = 1.0f;  // Multiplier applied to per-frame speed advancement.
	static float      PauseEaseProgress   = 0.0f;  // Progress through current ease phase [0, 1].
	static float      PauseEaseStep       = 0.0f;  // Per-frame step, derived from segment speed at ease start.
	static float      PauseEaseStartAlpha = 0.0f;  // SplineAlpha when ease-out began.
	static int        PauseHoldTimer      = 0;     // Frames remaining in hold phase.
	static bool       IsPauseComplete     = false; // Prevents re-triggering pause for same segment.
	
	// Resets the pause state machine to idle.
	static void InitializePauseState()
	{
		CurrentPausePhase = PausePhase::None;
		PauseSpeedFactor = 1.0f;
		PauseEaseProgress = 0.0f;
		PauseEaseStep = 0.0f;
		PauseEaseStartAlpha = 0.0f;
		PauseHoldTimer = 0;
		IsPauseComplete = false;
	}

	bool HasSpotCamSequence(int sequence)
	{
		return SequenceMap.find(sequence) != SequenceMap.end();
	}

	int GetSequenceFirstCameraIndex(int sequence)
	{
		if (!HasSpotCamSequence(sequence))
			return NO_VALUE;

		int index = 0;
		for (int i = 0; i < SequenceMap[sequence]; i++)
			index += SequenceCamCount[i];

		return index;
	}

	static int GetSequenceCameraCount(int sequence)
	{
		if (!HasSpotCamSequence(sequence))
			return 0;

		return SequenceCamCount[SequenceMap[sequence]];
	}

	void ClearSpotCamSequences()
	{
		UseSpotCam = false;
		SpotcamDontDrawLara = false;
		SpotcamOverlay = false;
	
		g_Level.SpotCams.clear();
		SequenceMap.clear();
		SequenceCamCount.clear();
	}

	void InitializeSpotCamSequences(bool startFirstSequence)
	{
		TrackCameraInit = false;
	
		SequenceCamCount.clear();
		SequenceMap.clear();

		if (g_Level.SpotCams.empty())
			return;

		int currentSequence = g_Level.SpotCams[0].Sequence;
		int count = 0;
	
		for (const auto& cam : g_Level.SpotCams)
		{
			if (cam.Sequence != currentSequence)
			{
				SequenceMap[currentSequence] = (int)SequenceCamCount.size();
				SequenceCamCount.push_back(count);
				currentSequence = cam.Sequence;
				count = 0;
			}
	
			count++;
		}
	
		SequenceMap[currentSequence] = (int)SequenceCamCount.size();
		SequenceCamCount.push_back(count);

		if (startFirstSequence&& HasSpotCamSequence(0))
		{
			InitializeSpotCam(0);
			UseSpotCam = true;
		}
	}
	
	void InitializeSpotCam(short sequence)
	{
		if (g_Level.SpotCams.empty() || !HasSpotCamSequence(sequence))
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
		InitializePauseState();
	
		// Save player state.
		SavedLaraAir    = Lara.Status.Air;
		SavedLaraHealth = LaraItem->HitPoints;
	
		// Save camera state.
		SavedCameraPos    = Vector3i(Camera.pos.x, Camera.pos.y, Camera.pos.z);
		SavedCameraTarget = Vector3i(Camera.target.x, Camera.target.y, Camera.target.z);
		SavedCameraRoom   = Camera.pos.RoomNumber;
	
		// Compute first camera index for this sequence.
		CurrentSequenceID = sequence;
		CurrentCameraIndex = GetSequenceFirstCameraIndex(sequence);

		if (CurrentCameraIndex == NO_VALUE)
		{
			TENLog(fmt::format("Can't find proper first camera index for flyby sequence {}.", sequence), LogLevel::Warning);
			return;
		}

		SplineAlpha = 0.0f;
		IsTransitionToGame = false;

		FirstCameraIndex    = CurrentCameraIndex;
		SequenceCameraCount = GetSequenceCameraCount(sequence);
		LastCameraIndex     = FirstCameraIndex + SequenceCameraCount - 1;

		const auto& firstCam = g_Level.SpotCams[CurrentCameraIndex];

		if (firstCam.Flags & SCF_DISABLE_LARA_CONTROLS)
		{
			Lara.Control.IsLocked = true;
			SetCinematicBars(SPOTCAM_CINEMATIC_BARS_HEIGHT, SPOTCAM_CINEMATIC_BARS_SPEED);
		}
	
		// Populate spline knot arrays.
		if (firstCam.Flags & SCF_TRACKING_CAM)
		{
			// Tracking camera: pad with first camera, then all cameras, then pad with last.
			Knots.Resize(SequenceCameraCount + SplineCameraKnots::TRACKING_KNOT_COUNT);
			Knots.SetKnot(1, g_Level.SpotCams[FirstCameraIndex]);
			SplineFromOffset = 0;

			for (int i = 0; i < SequenceCameraCount; i++)
				Knots.SetKnot(i + 2, g_Level.SpotCams[FirstCameraIndex + i]);

			Knots.SetKnot(SequenceCameraCount + 2, g_Level.SpotCams[LastCameraIndex]);
		}
		else if (firstCam.Flags & SCF_CUT_PAN)
		{
			// Cut-pan: first knot is current camera, then fill 4 knots from sequence.
			Knots.Resize(SplineCameraKnots::SPLINE_KNOT_COUNT);
			Knots.SetKnot(1, g_Level.SpotCams[CurrentCameraIndex]);
			SplineFromOffset = 0;

			Camera.DisableInterpolation = true;
	
			int camIndex = CurrentCameraIndex;
			for (int i = 0; i < 4; i++)
			{
				if (camIndex > LastCameraIndex)
					camIndex = FirstCameraIndex;
	
				Knots.SetKnot(i + 2, g_Level.SpotCams[camIndex]);
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
			// Smooth pan: blend from current camera position to first spotcam (indices 0-4).
			Knots.Resize(SplineCameraKnots::BLEND_KNOT_COUNT);
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
			Knots.SetKnot(3, g_Level.SpotCams[CurrentCameraIndex]);
	
			// Knot [4] = next spotcam (or clamped to last).
			int nextIndex = CurrentCameraIndex + 1;
			if (nextIndex > LastCameraIndex)
				nextIndex = FirstCameraIndex;
	
			Knots.SetKnot(4, g_Level.SpotCams[nextIndex]);
		}
	
		if (firstCam.Flags & SCF_HIDE_LARA)
			SpotcamDontDrawLara = true;
	}
	
	// Runs heavy triggers at the camera's current position.
	static void TestVolumesAndTriggers()
	{
		if (!RunHeavyTriggers)
			return;
	
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
		RunHeavyTriggers = false;
	}
	
	// Advances the spline position by the given normalized speed and manages the pause state machine (ease-out, hold, ease-in).
	static bool AdvanceOrPauseSequence(float normalizedSpeed)
	{
		constexpr auto EASE_DISTANCE = 0.15f;
		constexpr auto MIN_SPEED = 0.001f;
	
		// Trigger ease-out when the camera is within PAUSE_EASE_DISTANCE of the segment end and a pause is pending.
		if (CurrentPausePhase == PausePhase::None)
		{
			bool hasPause = g_Level.SpotCams[CurrentCameraIndex].Timer > 0 && (g_Level.SpotCams[CurrentCameraIndex].Flags & SCF_STOP_MOVEMENT) && !IsPauseComplete;
	
			if (hasPause && (1.0f - SplineAlpha) <= EASE_DISTANCE)
			{
				IsPauseComplete = true;
				PauseEaseStartAlpha = SplineAlpha;
				PauseEaseProgress = 0.0f;
	
				// Derive step so the initial alpha velocity matches the current camera speed.
				float remainingAlpha = std::max(1.0f - SplineAlpha, MIN_SPEED);

				// Clamp to a minimum so the pause doesn't stall if speed is near zero.
				float clampedSpeed = std::max(normalizedSpeed, MIN_SPEED);

				PauseEaseStep = clampedSpeed / (2.0f * remainingAlpha);
				CurrentPausePhase = PausePhase::EaseOut;
			}
		}
	
		switch (CurrentPausePhase)
		{
		case PausePhase::EaseOut:
	
			PauseEaseProgress = std::min(PauseEaseProgress + PauseEaseStep, 1.0f);
			SplineAlpha = PauseEaseStartAlpha + (1.0f - PauseEaseStartAlpha) * PauseEaseProgress * (2.0f - PauseEaseProgress);
	
			if (PauseEaseProgress >= 1.0f)
			{
				PauseSpeedFactor = 0.0f;
				PauseHoldTimer = g_Level.SpotCams[CurrentCameraIndex].Timer >> 4;
				CurrentPausePhase = PausePhase::Hold;
			}
			return false;
	
		case PausePhase::Hold:
	
			PauseHoldTimer--;
	
			if (PauseHoldTimer <= 0)
			{
				PauseEaseProgress = 0.0f;
				PauseSpeedFactor = 0.0f;
				CurrentPausePhase = PausePhase::EaseIn;
				return true; // Signal caller to advance to next segment.
			}
			return false;
	
		case PausePhase::EaseIn:
	
			PauseEaseProgress = std::min(PauseEaseProgress + PauseEaseStep, 1.0f);
			PauseSpeedFactor = PauseEaseProgress * PauseEaseProgress;
	
			if (PauseEaseProgress >= 1.0f)
			{
				PauseSpeedFactor = 1.0f;
				PauseEaseProgress = 0.0f;
				IsPauseComplete = false;
				CurrentPausePhase = PausePhase::None;
			}
	
			// Advance alpha normally using the ramping speed factor.
			SplineAlpha = std::min(SplineAlpha + normalizedSpeed * PauseSpeedFactor, 1.0f);
			return false;
	
		default:
			// No pause active; normal advance.
			SplineAlpha = std::min(SplineAlpha + normalizedSpeed, 1.0f);
			return false;
		}
	}
	
	// Ends the spotcam sequence and restores normal camera.
	static void EndSequence(const SpotCamInfo& firstCam)
	{
		TestVolumesAndTriggers();
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
	
			Knots.SetKnot(startKnotIndex + i, g_Level.SpotCams[camIndex]);
			camIndex++;
		}
	}
	
	// Tracking camera: finds the closest spline position to Lara using a coarse-to-fine search.
	static float FindClosestSplineAlpha(int knotCount)
	{
		auto  laraPos = LaraItem->Pose.Position;
		float closestAlpha = 0.0f;
		float searchStep = 1.0f / 8.0f;
	
		float searchStart = 0.0f;
	
		for (int iteration = 0; iteration < 8; iteration++)
		{
			float closestDist = FLT_MAX;
	
			for (int sample = 0; sample < 8; sample++)
			{
				float sampleAlpha = searchStart + sample * searchStep;
				if (sampleAlpha > 1.0f)
					break;
	
				float cx = Spline(sampleAlpha, &Knots.PosX[1], knotCount);
				float cy = Spline(sampleAlpha, &Knots.PosY[1], knotCount);
				float cz = Spline(sampleAlpha, &Knots.PosZ[1], knotCount);
	
				float dist = Vector3::Distance(Vector3(cx, cy, cz), Vector3((float)laraPos.x, (float)laraPos.y, (float)laraPos.z));
	
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
	
	void CalculateSpotCam()
	{
		if (g_Level.SpotCams.empty() || FirstCameraIndex >= (int)g_Level.SpotCams.size())
		{
			TENLog(fmt::format("Flyby sequence {} refers to a camera {} that does not exist.", CurrentSequenceID, FirstCameraIndex), LogLevel::Warning);
			UseSpotCam = false;
			return;
		}
	
		if (Lara.Control.IsLocked)
		{
			LaraItem->HitPoints = SavedLaraHealth;
			Lara.Status.Air = SavedLaraAir;
		}
	
		const auto& firstCam = g_Level.SpotCams[FirstCameraIndex];
		int knotCount = (firstCam.Flags & SCF_TRACKING_CAM) ? (SequenceCameraCount + 2) : 4;

		// Spline() needs at least 4 knots to form a valid segment.
		if (knotCount < 4)
		{
			TENLog(fmt::format("Flyby sequence {} has too few cameras for spline interpolation.", CurrentSequenceID), LogLevel::Warning);
			UseSpotCam = false;
			return;
		}
	
		// Interpolate all camera properties at current spline position.
		float interpPosX    = Spline(SplineAlpha, &Knots.PosX[1],    knotCount);
		float interpPosY    = Spline(SplineAlpha, &Knots.PosY[1],    knotCount);
		float interpPosZ    = Spline(SplineAlpha, &Knots.PosZ[1],    knotCount);
		float interpTargetX = Spline(SplineAlpha, &Knots.TargetX[1], knotCount);
		float interpTargetY = Spline(SplineAlpha, &Knots.TargetY[1], knotCount);
		float interpTargetZ = Spline(SplineAlpha, &Knots.TargetZ[1], knotCount);
		float interpSpeed   = Spline(SplineAlpha, &Knots.Speed[1],   knotCount);
		float interpRoll    = Spline(SplineAlpha, &Knots.Roll[1],    knotCount);
		float interpFOV     = Spline(SplineAlpha, &Knots.FOV[1],     knotCount);
	
		// Handle screen fading.
		if ((g_Level.SpotCams[CurrentCameraIndex].Flags & SCF_SCREEN_FADE_IN) &&
			FadeCameraIndex != CurrentCameraIndex)
		{
			SetScreenFadeIn(FADE_SCREEN_SPEED);
			FadeCameraIndex = CurrentCameraIndex;
		}
	
		if ((g_Level.SpotCams[CurrentCameraIndex].Flags & SCF_SCREEN_FADE_OUT) &&
			FadeCameraIndex != CurrentCameraIndex)
		{
			SetScreenFadeOut(FADE_SCREEN_SPEED);
			FadeCameraIndex = CurrentCameraIndex;
		}
	
		// Advance spline position.
		bool advancedToNextSegment = false;
	
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
			// Non-tracking: advance alpha and manage pause state machine.
			float normalizedSpeed = interpSpeed / (float)USHRT_MAX;
			advancedToNextSegment = AdvanceOrPauseSequence(normalizedSpeed);
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
			int collRoomNumber = GetPointCollision(pos, g_Level.SpotCams[CurrentCameraIndex].RoomNumber).GetRoomNumber();
	
			if (collRoomNumber != Camera.pos.RoomNumber && !IsPointInRoom(pos, collRoomNumber))
				collRoomNumber = FindRoomNumber(pos, g_Level.SpotCams[CurrentCameraIndex].RoomNumber);
	
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
		if (g_Level.SpotCams[CurrentCameraIndex].Flags & SCF_OVERLAY)
			SpotcamOverlay = true;

		if (g_Level.SpotCams[CurrentCameraIndex].Flags & SCF_HIDE_LARA)
			SpotcamDontDrawLara = true;

		if (g_Level.SpotCams[CurrentCameraIndex].Flags & SCF_ACTIVATE_HEAVY_TRIGGERS)
			RunHeavyTriggers = true;
	
		TestVolumesAndTriggers();
	
		// Tracking camera just sets init flag and returns.
		if (firstCam.Flags & SCF_TRACKING_CAM)
		{
			TrackCameraInit = true;
			return;
		}
	
		// During active pause phases (ease-out, hold, ease-in active), skip
		// segment-advance logic unless the hold timer just expired.
		if (CurrentPausePhase != PausePhase::None && !advancedToNextSegment)
			return;
	
		// Non-tracking: check if the spline segment is complete.
		float normalizedSpeed = interpSpeed / (float)USHRT_MAX;
		if (!advancedToNextSegment && SplineAlpha <= 1.0f - normalizedSpeed)
			return;
	
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
			if (g_Level.SpotCams[CurrentCameraIndex].Flags & SCF_REENABLE_LARA_CONTROLS)
				Lara.Control.IsLocked = false;

			if (g_Level.SpotCams[CurrentCameraIndex].Flags & SCF_DISABLE_LARA_CONTROLS)
			{
				Lara.Control.IsLocked = true;

				if (CurrentLevel)
					SetCinematicBars(SPOTCAM_CINEMATIC_BARS_HEIGHT, SPOTCAM_CINEMATIC_BARS_SPEED);
			}
	
			// Handle cut-to-cam: jump to a specific camera in the sequence.
			if (g_Level.SpotCams[CurrentCameraIndex].Flags & SCF_CUT_TO_CAM)
			{
				int jumpTarget = FirstCameraIndex + g_Level.SpotCams[CurrentCameraIndex].Timer;

				if (jumpTarget < FirstCameraIndex || jumpTarget > LastCameraIndex)
				{
					TENLog(fmt::format("Flyby sequence {} has no camera with index {}. Check the flyby setup in the editor.", CurrentSequenceID, jumpTarget), LogLevel::Warning);
					jumpTarget = std::clamp(jumpTarget, FirstCameraIndex, LastCameraIndex);
				}
	
				Knots.SetKnot(1, g_Level.SpotCams[jumpTarget]);
				CurrentCameraIndex = jumpTarget;
				prevCamIndex = jumpTarget;

				knotStartIndex = 2;
				Camera.DisableInterpolation = true;
			}
	
			knotStartIndex++;
			Knots.SetKnot(knotStartIndex - 1, g_Level.SpotCams[prevCamIndex]);
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
			EndSequence(firstCam);
			return;
		}
	
		// No explicit end flag: smoothly blend back to gameplay camera.
		Knots.SetKnot(1, g_Level.SpotCams[CurrentCameraIndex - 1]);
		Knots.SetKnot(2, g_Level.SpotCams[CurrentCameraIndex - 1]);
	
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
	
	Pose GetSpotCamSequenceTransform(int sequence, float alpha, bool loop)
	{
		constexpr auto BLEND_RANGE = 0.1f;
		constexpr auto BLEND_START = BLEND_RANGE;
		constexpr auto BLEND_END   = 1.0f - BLEND_RANGE;
	
		alpha = std::clamp(alpha, 0.0f, 1.0f);
	
		if (sequence < 0 || !HasSpotCamSequence(sequence))
		{
			TENLog(fmt::format("Wrong flyby sequence number {} provided for getting camera coordinates.", sequence), LogLevel::Warning);
			return Pose::Zero;
		}

		int cameraCount = GetSequenceCameraCount(sequence);
		if (cameraCount < 2)
		{
			TENLog(fmt::format("Not enough cameras in flyby sequence {} to calculate the coordinates.", sequence), LogLevel::Warning);
			return Pose::Zero;
		}
	
		// Find first camera index for this sequence.
		int firstIndex = GetSequenceFirstCameraIndex(sequence);
		if (firstIndex == NO_VALUE)
		{
			TENLog(fmt::format("First camera index is incorrect in flyby sequence {}.", sequence), LogLevel::Warning);
			return Pose::Zero;
		}
	
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
	
			xOrigins.push_back((float)g_Level.SpotCams[seqID].Position.x);
			yOrigins.push_back((float)g_Level.SpotCams[seqID].Position.y);
			zOrigins.push_back((float)g_Level.SpotCams[seqID].Position.z);
			xTargets.push_back((float)g_Level.SpotCams[seqID].Target.x);
			yTargets.push_back((float)g_Level.SpotCams[seqID].Target.y);
			zTargets.push_back((float)g_Level.SpotCams[seqID].Target.z);
			rolls.push_back((float)g_Level.SpotCams[seqID].Roll);
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
			float blendFactor = (alpha < BLEND_START) ? (0.5f + (alpha / BLEND_RANGE) * 0.5f) : ((alpha - BLEND_END) / BLEND_START) * 0.5f;
	
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

} // namespace TEN::SpotCam
