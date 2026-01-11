#pragma once
#include "Math/Math.h"
#include "Specific/clock.h"

constexpr auto MAX_SPOTCAMS = 256;
constexpr auto SPOTCAM_CINEMATIC_BARS_HEIGHT = 1.0f / 16;
constexpr auto SPOTCAM_CINEMATIC_BARS_SPEED = 1.0f / FPS;

class Pose;

struct SPOTCAM
{
	int x;
	int y;
	int z;
	int tx;
	int ty;
	int tz;
	unsigned char sequence;
	unsigned char camera;
	short fov;
	short roll;
	short timer;
	short speed;
	short flags;
	short roomNumber;
	short pad;
};

enum SPOTCAM_FLAGS
{
	SCF_CUT_PAN					= (1 << 0),	 // Cut without panning smoothly.
	SCF_OVERLAY					= (1 << 1),  // TODO: Add vignette.
	SCF_LOOP_SEQUENCE			= (1 << 2),
	SCF_TRACKING_CAM			= (1 << 3),
	SCF_HIDE_LARA				= (1 << 4),
	SCF_FOCUS_LARA_HEAD			= (1 << 5),
	SCF_CUT_TO_LARA_CAM			= (1 << 6),
	SCF_CUT_TO_CAM				= (1 << 7),
	SCF_STOP_MOVEMENT			= (1 << 8),  // Stop movement for a given time (cf. `Timer` field).
	SCF_DISABLE_BREAKOUT		= (1 << 9),  // Disable breaking out from cutscene using LOOK key.
	SCF_DISABLE_LARA_CONTROLS	= (1 << 10), // Also add widescreen bars.
	SCF_REENABLE_LARA_CONTROLS	= (1 << 11), // Used with 0x0400, keeps widescreen bars.
	SCF_SCREEN_FADE_IN			= (1 << 12),
	SCF_SCREEN_FADE_OUT			= (1 << 13),
	SCF_ACTIVATE_HEAVY_TRIGGERS = (1 << 14), // When camera is moving above heavy trigger sector, it will be activated.
	SCF_CAMERA_ONE_SHOT			= (1 << 15),
};

extern SPOTCAM SpotCam[MAX_SPOTCAMS];
extern int SpotCamRemap[MAX_SPOTCAMS];
extern int CameraCnt[MAX_SPOTCAMS];
extern int LastSpotCamSequence;
extern int NumberSpotcams;
extern bool UseSpotCam;
extern bool SpotcamSwitched;
extern bool SpotcamDontDrawLara;
extern bool SpotcamOverlay;
extern bool TrackCameraInit;

void ClearSpotCamSequences();
void InitializeSpotCamSequences(bool startFirstSequence);
void InitializeSpotCam(short sequence);
void CalculateSpotCameras();
int Spline(int x, int* knots, int nk);

Pose GetCameraTransform(int sequence, float alpha, bool loop);
