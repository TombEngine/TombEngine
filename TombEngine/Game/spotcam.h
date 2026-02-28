#pragma once

#include "Math/Math.h"
#include "Specific/clock.h"

class Pose;

namespace TEN::SpotCam
{
	constexpr auto SPOTCAM_CINEMATIC_BARS_HEIGHT = 1.0f / 16;
	constexpr auto SPOTCAM_CINEMATIC_BARS_SPEED  = 1.0f;

	struct SpotCamInfo
	{
		Vector3i     Position   = Vector3i::Zero;
		Vector3i     Target     = Vector3i::Zero;
		int          RoomNumber = 0;

		int          Sequence   = 0;
		int          Camera     = 0;

		short        FOV        = 0;
		short        Roll       = 0;
		short        Timer      = 0;
		short        Speed      = 0;
		short        Flags      = 0;
	};

	enum SpotCamFlags
	{
		SCF_CUT_PAN                 = (1 << 0),  // Cut without panning smoothly.
		SCF_OVERLAY                 = (1 << 1),  // TODO: Add vignette.
		SCF_LOOP_SEQUENCE           = (1 << 2),
		SCF_TRACKING_CAM            = (1 << 3),
		SCF_HIDE_LARA               = (1 << 4),
		SCF_FOCUS_LARA_HEAD         = (1 << 5),
		SCF_CUT_TO_LARA_CAM         = (1 << 6),
		SCF_CUT_TO_CAM              = (1 << 7),
		SCF_STOP_MOVEMENT           = (1 << 8),  // Stop movement for a given time (cf. `Timer` field).
		SCF_DISABLE_BREAKOUT        = (1 << 9),  // Disable breaking out from cutscene using LOOK key.
		SCF_DISABLE_LARA_CONTROLS   = (1 << 10), // Also add widescreen bars.
		SCF_REENABLE_LARA_CONTROLS  = (1 << 11), // Used with 0x0400, keeps widescreen bars.
		SCF_SCREEN_FADE_IN          = (1 << 12),
		SCF_SCREEN_FADE_OUT         = (1 << 13),
		SCF_ACTIVATE_HEAVY_TRIGGERS = (1 << 14), // When camera is moving above heavy trigger sector, it will be activated.
		SCF_CAMERA_ONE_SHOT         = (1 << 15),
	};

	extern int  LastSpotCamSequence;
	extern bool UseSpotCam;
	extern bool SpotcamSwitched;
	extern bool SpotcamDontDrawLara;
	extern bool SpotcamOverlay;
	extern bool TrackCameraInit;

	bool HasSpotCamSequence(int sequence);
	int  GetSequenceFirstCameraIndex(int sequence);

	void ClearSpotCamSequences();
	void InitializeSpotCamSequences(bool startFirstSequence);
	void InitializeSpotCam(short sequence);
	void CalculateSpotCam();
	Pose GetSpotCamSequenceTransform(int sequence, float alpha, bool loop);
}
