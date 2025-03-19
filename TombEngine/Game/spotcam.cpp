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

constexpr auto MAX_CAMERA = 18;

bool TrackCameraInit;
int SpotcamTimer;
bool SpotcamPaused;
int SpotcamLoopCnt;
int CameraFade;
Vector3i LaraFixedPosition;
int InitialCameraRoom;
Vector3i InitialCameraPosition;
Vector3i InitialCameraTarget;
int CurrentSplinePosition;
int SplineToCamera;
int FirstCamera;
int LastCamera;
int CurrentCameraCnt;
int CameraXposition[MAX_CAMERA];
int CameraYposition[MAX_CAMERA];
int CameraZposition[MAX_CAMERA];
int CameraXtarget[MAX_CAMERA];
int CameraYtarget[MAX_CAMERA];
int CameraZtarget[MAX_CAMERA];
int CameraRoll[MAX_CAMERA];
int CameraFOV[MAX_CAMERA];
int CameraSpeed[MAX_CAMERA];
int SplineFromCamera;
bool SpotCamFirstLook;
short CurrentSplineCamera;
int LastSpotCamSequence;
int LaraHealth;
int LaraAir;
int CurrentSpotcamSequence;
SPOTCAM SpotCam[MAX_SPOTCAMS];
int SpotCamRemap[MAX_SPOTCAMS];
int CameraCnt[MAX_SPOTCAMS];
int NumberSpotcams;

bool CheckTrigger = false;
bool UseSpotCam = false;
bool SpotcamSwitched = false;
bool SpotcamDontDrawLara = false;
bool SpotcamOverlay = false;

void ClearSpotCamSequences()
{
	UseSpotCam = false;
	SpotcamDontDrawLara = false;
	SpotcamOverlay = false;


	for (int i = 0; i < MAX_SPOTCAMS; i++)
		SpotCam[i] = {};
}

void InitializeSpotCamSequences(bool startFirstSequence)
{
	TrackCameraInit = false;

	int n = NumberSpotcams;
	int cc = 1;

	if (n != 0)
	{
		int ce = 0;
		int s = SpotCam[0].sequence;

		if (cc < n)
		{
			for (n = 1; n < NumberSpotcams; n++)
			{
				// Same sequence.
				if (SpotCam[n].sequence == s)
					cc++;
				// New sequence.
				else
				{
					CameraCnt[ce] = cc;
					cc = 1;
					SpotCamRemap[s] = ce;
					ce++;
					s = SpotCam[n].sequence;
				}
			}
		}

		CameraCnt[ce] = cc;
		SpotCamRemap[s] = ce;
	}

	if (startFirstSequence)
	{
		InitializeSpotCam(0);
		UseSpotCam = true;
	}
}

void InitializeSpotCam(short Sequence)
{
	if (TrackCameraInit != 0 && LastSpotCamSequence == Sequence)
	{
		TrackCameraInit = false;
		return;
	}

	Lara.Control.Look.OpticRange = 0;
	Lara.Control.Look.IsUsingLasersight = false;

	AlterFOV(ANGLE(DEFAULT_FOV), false);

	LaraItem->MeshBits = ALL_JOINT_BITS;

	ResetPlayerFlex(LaraItem);

	Camera.bounce = 0;

	Lara.Inventory.IsBusy = 0;

	CameraFade = -1;
	LastSpotCamSequence = Sequence;
	TrackCameraInit = false;
	SpotcamTimer = 0;
	SpotcamPaused = false;
	SpotcamLoopCnt = 0;
	Lara.Control.IsLocked = false;

	LaraAir = Lara.Status.Air;

	InitialCameraPosition.x = Camera.pos.x;
	InitialCameraPosition.y = Camera.pos.y;
	InitialCameraPosition.z = Camera.pos.z;

	InitialCameraTarget.x = Camera.target.x;
	InitialCameraTarget.y = Camera.target.y;
	InitialCameraTarget.z = Camera.target.z;

	LaraHealth = LaraItem->HitPoints;
	InitialCameraRoom = Camera.pos.RoomNumber;

	LaraFixedPosition.x = LaraItem->Pose.Position.x;
	LaraFixedPosition.y = LaraItem->Pose.Position.y;
	LaraFixedPosition.z = LaraItem->Pose.Position.z;

	CurrentSpotcamSequence = Sequence;
	CurrentSplineCamera = 0;

	for (int i = 0; i < SpotCamRemap[Sequence]; i++)
		CurrentSplineCamera += CameraCnt[i];

	CurrentSplinePosition = 0;
	SplineToCamera = 0;

	FirstCamera = CurrentSplineCamera;

	auto* spotcam = &SpotCam[CurrentSplineCamera];

	LastCamera = CurrentSplineCamera + (CameraCnt[SpotCamRemap[Sequence]] - 1);
	CurrentCameraCnt = CameraCnt[SpotCamRemap[Sequence]];

	if ((spotcam->flags & SCF_DISABLE_LARA_CONTROLS))
	{
		Lara.Control.IsLocked = true;
		SetCinematicBars(1.0f, SPOTCAM_CINEMATIC_BARS_SPEED);
	}

	if (spotcam->flags & SCF_TRACKING_CAM)
	{
		CameraXposition[1] = SpotCam[FirstCamera].x;
		CameraYposition[1] = SpotCam[FirstCamera].y;
		CameraZposition[1] = SpotCam[FirstCamera].z;
		CameraXtarget[1] = SpotCam[FirstCamera].tx;
		CameraYtarget[1] = SpotCam[FirstCamera].ty;
		CameraZtarget[1] = SpotCam[FirstCamera].tz;
		CameraRoll[1] = SpotCam[FirstCamera].roll;
		CameraFOV[1] = SpotCam[FirstCamera].fov;
		CameraSpeed[1] = SpotCam[FirstCamera].speed;

		SplineFromCamera = 0;

		if (CurrentCameraCnt > 0)
		{
			spotcam = &SpotCam[FirstCamera];

			for (int i = 0; i < CurrentCameraCnt; i++, spotcam++)
			{
				CameraXposition[i + 2] = spotcam->x;
				CameraYposition[i + 2] = spotcam->y;
				CameraZposition[i + 2] = spotcam->z;
				CameraXtarget[i + 2] = spotcam->tx;
				CameraYtarget[i + 2] = spotcam->ty;
				CameraZtarget[i + 2] = spotcam->tz;
				CameraRoll[i + 2] = spotcam->roll;
				CameraFOV[i + 2] = spotcam->fov;
				CameraSpeed[i + 2] = spotcam->speed;
			}
		}

		CameraXposition[CurrentCameraCnt + 2] = SpotCam[LastCamera].x;
		CameraYposition[CurrentCameraCnt + 2] = SpotCam[LastCamera].y;
		CameraZposition[CurrentCameraCnt + 2] = SpotCam[LastCamera].z;
		CameraXtarget[CurrentCameraCnt + 2] = SpotCam[LastCamera].tx;
		CameraYtarget[CurrentCameraCnt + 2] = SpotCam[LastCamera].ty;
		CameraZtarget[CurrentCameraCnt + 2] = SpotCam[LastCamera].tz;
		CameraFOV[CurrentCameraCnt + 2] = SpotCam[LastCamera].fov;
		CameraRoll[CurrentCameraCnt + 2] = SpotCam[LastCamera].roll;
		CameraSpeed[CurrentCameraCnt + 2] = SpotCam[LastCamera].speed;
	}
	else
	{
		int sp = 0;
		if ((spotcam->flags & SCF_CUT_PAN))
		{
			CameraXposition[1] = SpotCam[CurrentSplineCamera].x;
			CameraYposition[1] = SpotCam[CurrentSplineCamera].y;
			CameraZposition[1] = SpotCam[CurrentSplineCamera].z;
			CameraXtarget[1] = SpotCam[CurrentSplineCamera].tx;
			CameraYtarget[1] = SpotCam[CurrentSplineCamera].ty;
			CameraZtarget[1] = SpotCam[CurrentSplineCamera].tz;
			CameraRoll[1] = SpotCam[CurrentSplineCamera].roll;
			CameraFOV[1] = SpotCam[CurrentSplineCamera].fov;
			CameraSpeed[1] = SpotCam[CurrentSplineCamera].speed;

			Camera.DisableInterpolation = true;

			SplineFromCamera = 0;

			int cn = CurrentSplineCamera;
			while (sp < 4)
			{
				if (LastCamera < CurrentSplineCamera)
					cn = FirstCamera;

				CameraXposition[sp + 2] = SpotCam[cn].x;
				CameraYposition[sp + 2] = SpotCam[cn].y;
				CameraZposition[sp + 2] = SpotCam[cn].z;
				CameraXtarget[sp + 2] = SpotCam[cn].tx;
				CameraYtarget[sp + 2] = SpotCam[cn].ty;
				CameraZtarget[sp + 2] = SpotCam[cn].tz;
				CameraRoll[sp + 2] = SpotCam[cn].roll;
				CameraFOV[sp + 2] = SpotCam[cn].fov;
				CameraSpeed[sp + 2] = SpotCam[cn].speed;
				cn++;
				sp++;
			}

			CurrentSplineCamera++;

			if (CurrentSplineCamera > LastCamera)
				CurrentSplineCamera = FirstCamera;

			if (spotcam->flags & SCF_ACTIVATE_HEAVY_TRIGGERS)
				CheckTrigger = true;

			if (spotcam->flags & SCF_HIDE_LARA)
				SpotcamDontDrawLara = true;
		}
		else
		{
			int cn = CurrentSplineCamera;

			CameraXposition[1] = InitialCameraPosition.x;
			CameraYposition[1] = InitialCameraPosition.y;
			CameraZposition[1] = InitialCameraPosition.z;
			CameraXtarget[1] = InitialCameraTarget.x;
			CameraYtarget[1] = InitialCameraTarget.y;
			CameraZtarget[1] = InitialCameraTarget.z;
			CameraFOV[1] = CurrentFOV;
			CameraRoll[1] = 0;
			CameraSpeed[1] = spotcam->speed;

			CameraXposition[2] = InitialCameraPosition.x;
			CameraYposition[2] = InitialCameraPosition.y;
			CameraZposition[2] = InitialCameraPosition.z;
			CameraXtarget[2] = InitialCameraTarget.x;
			CameraYtarget[2] = InitialCameraTarget.y;
			CameraZtarget[2] = InitialCameraTarget.z;
			CameraFOV[2] = CurrentFOV;
			CameraRoll[2] = 0;
			CameraSpeed[2] = spotcam->speed;

			CameraXposition[3] = SpotCam[CurrentSplineCamera].x;
			CameraYposition[3] = SpotCam[CurrentSplineCamera].y;
			CameraZposition[3] = SpotCam[CurrentSplineCamera].z;
			CameraXtarget[3] = SpotCam[CurrentSplineCamera].tx;
			CameraYtarget[3] = SpotCam[CurrentSplineCamera].ty;
			CameraZtarget[3] = SpotCam[CurrentSplineCamera].tz;
			CameraRoll[3] = SpotCam[CurrentSplineCamera].roll;
			CameraFOV[3] = SpotCam[CurrentSplineCamera].fov;
			CameraSpeed[3] = SpotCam[CurrentSplineCamera].speed;

			SplineFromCamera = 1;

			cn++;

			if (LastCamera < cn)
				cn = FirstCamera;

			CameraXposition[4] = SpotCam[cn].x;
			CameraYposition[4] = SpotCam[cn].y;
			CameraZposition[4] = SpotCam[cn].z;

			CameraXtarget[4] = SpotCam[cn].tx;
			CameraYtarget[4] = SpotCam[cn].ty;
			CameraZtarget[4] = SpotCam[cn].tz;

			CameraRoll[4] = SpotCam[cn].roll;
			CameraFOV[4] = SpotCam[cn].fov;
			CameraSpeed[4] = SpotCam[cn].speed;
		}
	}

	if (spotcam->flags & SCF_HIDE_LARA)
		SpotcamDontDrawLara = true;
}

void CalculateSpotCameras()
{
	int cpx; // stack offset -96
	int cpy; // stack offset -92
	int cpz; // stack offset -88
	int ctx; // stack offset -84
	int cty; // stack offset -80
	int ctz; // stack offset -76
	int cspeed; // stack offset -72
	int cfov; // stack offset -68
	int croll; // stack offset -64
	SPOTCAM* s; // stack offset -60
	short spline_cnt; // $s3
	int dx; // $v1
	int dy; // $s0
	int dz; // $s1

	//{ // line 76, offset 0x38114
	int sp; // $s2
	int cp; // $fp
	int clen; // $s4
	int tlen; // $v1
	int cx; // $s1
	int cy; // $s0
	int cz; // $v0
	int lx; // stack offset -56
	int lz; // stack offset -52
	int ly; // stack offset -48
	int cn; // $s0


	CAMERA_INFO backup;

	if (Lara.Control.IsLocked)
	{
		LaraItem->HitPoints = LaraHealth;
		Lara.Status.Air = LaraAir;
	}

	s = &SpotCam[FirstCamera];
	spline_cnt = 4;

	if (s->flags & SCF_TRACKING_CAM)
		spline_cnt = CurrentCameraCnt + 2;

	//loc_37F64
	cpx = Spline(CurrentSplinePosition, &CameraXposition[1], spline_cnt);
	cpy = Spline(CurrentSplinePosition, &CameraYposition[1], spline_cnt);
	cpz = Spline(CurrentSplinePosition, &CameraZposition[1], spline_cnt);
	ctx = Spline(CurrentSplinePosition, &CameraXtarget[1], spline_cnt);
	cty = Spline(CurrentSplinePosition, &CameraYtarget[1], spline_cnt);
	ctz = Spline(CurrentSplinePosition, &CameraZtarget[1], spline_cnt);
	cspeed = Spline(CurrentSplinePosition, &CameraSpeed[1], spline_cnt);
	croll = Spline(CurrentSplinePosition, &CameraRoll[1], spline_cnt);
	cfov = Spline(CurrentSplinePosition, &CameraFOV[1], spline_cnt);

	if ((SpotCam[CurrentSplineCamera].flags & SCF_SCREEN_FADE_IN) &&
		CameraFade != CurrentSplineCamera)
	{
		SetScreenFadeIn(FADE_SCREEN_SPEED);
		CameraFade = CurrentSplineCamera;
	}

	if ((SpotCam[CurrentSplineCamera].flags & SCF_SCREEN_FADE_OUT) &&
		CameraFade != CurrentSplineCamera)
	{
		SetScreenFadeOut(FADE_SCREEN_SPEED);
		CameraFade = CurrentSplineCamera;
	}

	sp = 0;
	tlen = 0;
	clen = 0;
	cp = 0;
	int temp = 0x2000;

	if (s->flags & SCF_TRACKING_CAM)
	{
		lx = LaraItem->Pose.Position.x;
		ly = LaraItem->Pose.Position.y;
		lz = LaraItem->Pose.Position.z;

		for (int i = 0; i < 8; i++)
		{
			clen = 0x10000;

			for (int j = 0; j < 8; j++)
			{
				cx = Spline(sp, &CameraXposition[1], spline_cnt);
				cy = Spline(sp, &CameraYposition[1], spline_cnt);
				cz = Spline(sp, &CameraZposition[1], spline_cnt);

				dx = SQUARE(cx - lx);
				dy = SQUARE(cy - ly);
				dz = SQUARE(cz - lz);

				tlen = sqrt(dx + dy + dz);

				if (tlen <= clen)
				{
					cp = sp;
					clen = tlen;
				}

				sp += temp;

				if (sp > 0x10000)
					break;
			}

			temp >>= 1;
			sp = cp - 2 * (temp & 0xFE); // << 2 ?

			if (sp < 0)
				sp = 0;
		}

		CurrentSplinePosition += (cp - CurrentSplinePosition) >> 5;

		if ((s->flags & SCF_CUT_PAN))
		{
			if (abs(cp - CurrentSplinePosition) > 0x8000)
				CurrentSplinePosition = cp;
		}

		if (CurrentSplinePosition > 0x10000)
			CurrentSplinePosition = 0x10000;
		else if (CurrentSplinePosition < 0)
			CurrentSplinePosition = 0;
	}
	else if (!SpotcamTimer)
		CurrentSplinePosition += cspeed;

	bool lookPressed = (IsHeld(In::Look)) != 0;

	if (!lookPressed)
		SpotCamFirstLook = false;

	if ((s->flags & SCF_DISABLE_BREAKOUT) || !lookPressed)
	{
		// Disable interpolation if camera traveled too far.
		auto origin = Vector3(Camera.pos.x, Camera.pos.y, Camera.pos.z);
		auto target = Vector3(cpx, cpy, cpz);
		float dist = Vector3::Distance(origin, target);

		if (dist > BLOCK(0.25f))
			Camera.DisableInterpolation = true;

		Camera.pos.x = cpx;
		Camera.pos.y = cpy;
		Camera.pos.z = cpz;

		if ((s->flags & SCF_FOCUS_LARA_HEAD) || (s->flags & SCF_TRACKING_CAM))
		{
			Camera.target.x = LaraItem->Pose.Position.x;
			Camera.target.y = LaraItem->Pose.Position.y;
			Camera.target.z = LaraItem->Pose.Position.z;
		}
		else
		{
			Camera.target.x = ctx;
			Camera.target.y = cty;
			Camera.target.z = ctz;
			CalculateBounce(false);
		}

		int outsideRoom = IsRoomOutside(cpx, cpy, cpz);
		if (outsideRoom == NO_VALUE)
		{
			// HACK: Sometimes actual camera room number desyncs from room number derived using floordata functions.
			// If such case is identified, we do a brute-force search for coherrent room number.
			// This issue is only present in sub-click floor height setups after TE 1.7.0. -- Lwmte, 02.11.2024
		
			auto pos = Vector3i(Camera.pos.x, Camera.pos.y, Camera.pos.z);
			int collRoomNumber = GetPointCollision(pos, SpotCam[CurrentSplineCamera].roomNumber).GetRoomNumber();

			if (collRoomNumber != Camera.pos.RoomNumber)
				collRoomNumber = FindRoomNumber(pos, SpotCam[CurrentSplineCamera].roomNumber);

			Camera.pos.RoomNumber = collRoomNumber;
		}
		else
		{
			Camera.pos.RoomNumber = outsideRoom;
		}

		AlterFOV(cfov, false);

		LookAt(&Camera, croll);
		UpdateMikePos(*LaraItem);

		if (SpotCam[CurrentSplineCamera].flags & SCF_OVERLAY)
			SpotcamOverlay = true;

		if (SpotCam[CurrentSplineCamera].flags & SCF_HIDE_LARA)
			SpotcamDontDrawLara = true;

		if (SpotCam[CurrentSplineCamera].flags & SCF_ACTIVATE_HEAVY_TRIGGERS)
			CheckTrigger = true;

		if (CheckTrigger)
		{
			CameraType oldType = Camera.type;
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
			CheckTrigger = false;
		}

		if (s->flags & SCF_TRACKING_CAM)
		{
			TrackCameraInit = true;
		}
		else if (CurrentSplinePosition > 0x10000 - cspeed)
		{
			if (SpotCam[CurrentSplineCamera].timer > 0 &&
				SpotCam[CurrentSplineCamera].flags & SCF_STOP_MOVEMENT)
			{
				if (!SpotcamTimer && !SpotcamPaused)
					SpotcamTimer = SpotCam[CurrentSplineCamera].timer >> 3;
			}
			
			if (!SpotcamTimer)
			{
				CurrentSplinePosition = 0;

				if (CurrentSplineCamera != FirstCamera)
					cn = CurrentSplineCamera - 1;
				else
					cn = LastCamera;

				sp = 1;

				if (SplineFromCamera != 0)
				{
					SplineFromCamera = 0;
					cn = FirstCamera - 1;
				}
				else
				{
					if (SpotCam[CurrentSplineCamera].flags & SCF_REENABLE_LARA_CONTROLS)
						Lara.Control.IsLocked = false;

					if (SpotCam[CurrentSplineCamera].flags & SCF_DISABLE_LARA_CONTROLS)
					{						
						if (CurrentLevel)
							SetCinematicBars(1.0f, SPOTCAM_CINEMATIC_BARS_SPEED);

						Lara.Control.IsLocked = true;
					}

					int sp2 = 0;
					if (SpotCam[CurrentSplineCamera].flags & SCF_CUT_TO_CAM)
					{
						cn = FirstCamera + SpotCam[CurrentSplineCamera].timer;

						Camera.DisableInterpolation = true;

						CameraXposition[1] = SpotCam[cn].x;
						CameraYposition[1] = SpotCam[cn].y;
						CameraZposition[1] = SpotCam[cn].z;
						CameraXtarget[1] = SpotCam[cn].tx;
						CameraYtarget[1] = SpotCam[cn].ty;
						CameraZtarget[1] = SpotCam[cn].tz;
						CameraRoll[1] = SpotCam[cn].roll;
						CameraFOV[1] = SpotCam[cn].fov;
						CameraSpeed[1] = SpotCam[cn].speed;
						sp2 = 1;
						CurrentSplineCamera = cn;
					}

					sp = sp2 + 1;

					CameraXposition[sp] = SpotCam[cn].x;
					CameraYposition[sp] = SpotCam[cn].y;
					CameraZposition[sp] = SpotCam[cn].z;
					CameraXtarget[sp] = SpotCam[cn].tx;
					CameraYtarget[sp] = SpotCam[cn].ty;
					CameraZtarget[sp] = SpotCam[cn].tz;
					CameraRoll[sp] = SpotCam[cn].roll;
					CameraFOV[sp] = SpotCam[cn].fov;
					CameraSpeed[sp] = SpotCam[cn].speed;
				}

				cn++;
				if (sp < 4)
				{
					while (sp < 4)
					{
						if (s->flags & SCF_LOOP_SEQUENCE)
						{
							if (LastCamera < cn)
								cn = FirstCamera;
						}
						else
						{
							if (LastCamera < cn)
								cn = LastCamera;
						}

						CameraXposition[sp + 1] = SpotCam[cn].x;
						CameraYposition[sp + 1] = SpotCam[cn].y;
						CameraZposition[sp + 1] = SpotCam[cn].z;
						CameraXtarget[sp + 1] = SpotCam[cn].tx;
						CameraYtarget[sp + 1] = SpotCam[cn].ty;
						CameraZtarget[sp + 1] = SpotCam[cn].tz;
						CameraRoll[sp + 1] = SpotCam[cn].roll;
						CameraFOV[sp + 1] = SpotCam[cn].fov;
						CameraSpeed[sp + 1] = SpotCam[cn].speed;
						cn++;
						sp++;
					}
				}

				CurrentSplineCamera++;
				SpotcamPaused = 0;

				if (LastCamera >= CurrentSplineCamera)
					return;

				if (s->flags & SCF_LOOP_SEQUENCE)
				{
					CurrentSplineCamera = FirstCamera;
					SpotcamLoopCnt++;
				}
				else if (s->flags & SCF_CUT_TO_LARA_CAM || SplineToCamera)
				{
					if (CheckTrigger)
					{
						CameraType oldType = Camera.type;
						Camera.type = CameraType::Heavy;
						if (CurrentLevel)
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
						CheckTrigger = false;
					}

					SetCinematicBars(0.0f, SPOTCAM_CINEMATIC_BARS_SPEED);

					UseSpotCam = false;
					CheckTrigger = false;
					Lara.Control.IsLocked = false;
					Lara.Control.Look.IsUsingBinoculars = false;
					Camera.oldType = CameraType::Fixed;
					Camera.type = CameraType::Chase;
					Camera.speed = 1;
					Camera.DisableInterpolation = true;

					if (s->flags & SCF_CUT_TO_LARA_CAM)
					{
						Camera.pos.x = InitialCameraPosition.x;
						Camera.pos.y = InitialCameraPosition.y;
						Camera.pos.z = InitialCameraPosition.z;
						Camera.pos.RoomNumber = InitialCameraRoom;
						Camera.target.x = InitialCameraTarget.x;
						Camera.target.y = InitialCameraTarget.y;
						Camera.target.z = InitialCameraTarget.z;
					}

					SpotcamOverlay = false;
					SpotcamDontDrawLara = false;
					AlterFOV(LastFOV);
				}
				else
				{
					CameraXposition[1] = SpotCam[CurrentSplineCamera - 1].x;
					CameraYposition[1] = SpotCam[CurrentSplineCamera - 1].y;
					CameraZposition[1] = SpotCam[CurrentSplineCamera - 1].z;
					CameraXtarget[1] = SpotCam[CurrentSplineCamera - 1].tx;
					CameraYtarget[1] = SpotCam[CurrentSplineCamera - 1].ty;
					CameraZtarget[1] = SpotCam[CurrentSplineCamera - 1].tz;
					CameraRoll[1] = SpotCam[CurrentSplineCamera - 1].roll;
					CameraFOV[1] = SpotCam[CurrentSplineCamera - 1].fov;
					CameraSpeed[1] = SpotCam[CurrentSplineCamera - 1].speed;

					CameraXposition[2] = SpotCam[CurrentSplineCamera - 1].x;
					CameraYposition[2] = SpotCam[CurrentSplineCamera - 1].y;
					CameraZposition[2] = SpotCam[CurrentSplineCamera - 1].z;
					CameraXtarget[2] = SpotCam[CurrentSplineCamera - 1].tx;
					CameraYtarget[2] = SpotCam[CurrentSplineCamera - 1].ty;
					CameraZtarget[2] = SpotCam[CurrentSplineCamera - 1].tz;
					CameraRoll[2] = SpotCam[CurrentSplineCamera - 1].roll;
					CameraFOV[2] = SpotCam[CurrentSplineCamera - 1].fov;
					CameraSpeed[2] = SpotCam[CurrentSplineCamera - 1].speed;

					memcpy((char*)& backup, (char*)& Camera, sizeof(CAMERA_INFO));
					Camera.oldType = CameraType::Fixed;
					Camera.type = CameraType::Chase;
					Camera.speed = 1;

					int elevation = Camera.targetElevation;

					CalculateCamera(LaraCollision);

					CameraRoll[2] = 0;
					CameraRoll[3] = 0;
					CameraSpeed[2] = CameraSpeed[1];

					InitialCameraPosition.x = Camera.pos.x;
					InitialCameraPosition.y = Camera.pos.y;
					InitialCameraPosition.z = Camera.pos.z;

					InitialCameraTarget.x = Camera.target.x;
					InitialCameraTarget.y = Camera.target.y;
					InitialCameraTarget.z = Camera.target.z;

					CameraXposition[3] = Camera.pos.x;
					CameraYposition[3] = Camera.pos.y;
					CameraZposition[3] = Camera.pos.z;
					CameraXtarget[3] = Camera.target.x;
					CameraYtarget[3] = Camera.target.y;
					CameraZtarget[3] = Camera.target.z;
					CameraFOV[3] = LastFOV;
					CameraSpeed[3] = CameraSpeed[2];
					CameraRoll[3] = 0;

					CameraXposition[4] = Camera.pos.x;
					CameraYposition[4] = Camera.pos.y;
					CameraZposition[4] = Camera.pos.z;
					CameraXtarget[4] = Camera.target.x;
					CameraYtarget[4] = Camera.target.y;
					CameraZtarget[4] = Camera.target.z;
					CameraFOV[4] = LastFOV;
					CameraSpeed[4] = CameraSpeed[2] >> 1;
					CameraRoll[4] = 0;

					memcpy((char*)& Camera, (char*)& backup, sizeof(CAMERA_INFO));

					Camera.targetElevation = elevation;

					LookAt(&Camera, croll);
					UpdateMikePos(*LaraItem);

					SplineToCamera = 1;
				}

				if (CurrentSplineCamera > LastCamera)
					CurrentSplineCamera = LastCamera;
			}
			else
			{
				SpotcamTimer--;
				if (!SpotcamTimer)
					SpotcamPaused = 1;
			}
		}
	}
	else if (s->flags & SCF_TRACKING_CAM)
	{
		if (!SpotCamFirstLook)
		{
			Camera.oldType = CameraType::Fixed;
			SpotCamFirstLook = true;
		}

		CalculateCamera(LaraCollision);
	}
	else
	{
		SetScreenFadeIn(FADE_SCREEN_SPEED);
		SetCinematicBars(0.0f, SPOTCAM_CINEMATIC_BARS_SPEED);
		UseSpotCam = false;
		Lara.Control.IsLocked = false;
		Camera.speed = 1;
		AlterFOV(LastFOV);
		CalculateCamera(LaraCollision);
		CheckTrigger = false;
	}
}

// Core's version. Proper decompilation by ChocolateFan
// TODO: Replace with float-based version.
int Spline(int x, int* knots, int nk)
{
	int span = x * (nk - 3) >> 16;
	if (span >= nk - 3)
		span = nk - 4;

	int* k = &knots[span];
	x = x * (nk - 3) - span * 65536;

	int c1 = (k[1] >> 1) - (k[2] >> 1) - k[2] + k[1] + (k[3] >> 1) + ((-k[0] - 1) >> 1);
	int c2 = 2 * k[2] - 2 * k[1] - (k[1] >> 1) - (k[3] >> 1) + k[0];

	return ((__int64)x * (((__int64)x * (((__int64)x * c1 >> 16) + c2) >> 16) + (k[2] >> 1) + ((-k[0] - 1) >> 1)) >> 16) + k[1];
}

Pose GetCameraTransform(int sequence, float alpha)
{
	alpha = std::clamp(alpha, 0.0f, 1.0f);

	// Retrieve camera count in sequence.
	int cameraCount = CameraCnt[SpotCamRemap[sequence]];
	if (cameraCount < 2)
		return Pose::Zero; // Not enough cameras to interpolate.

	// Find first ID for sequence.
	int firstSeqID = 0;
	for (int i = 0; i < SpotCamRemap[sequence]; i++)
		firstSeqID += CameraCnt[i];

	// Determine number of spline points and spline position.
	int splinePoints = cameraCount + 2;
	int splineAlpha = int(alpha * (float)USHRT_MAX);

	// Extract camera properties into separate vectors for interpolation.
	std::vector<int> xOrigins, yOrigins, zOrigins, xTargets, yTargets, zTargets, rolls;
	for (int i = -1; i < (cameraCount + 1); i++)
	{
		int seqID = std::clamp(firstSeqID + i, firstSeqID, (firstSeqID + cameraCount) - 1);

		xOrigins.push_back(SpotCam[seqID].x);
		yOrigins.push_back(SpotCam[seqID].y);
		zOrigins.push_back(SpotCam[seqID].z);
		xTargets.push_back(SpotCam[seqID].tx);
		yTargets.push_back(SpotCam[seqID].ty);
		zTargets.push_back(SpotCam[seqID].tz);
		rolls.push_back(SpotCam[seqID].roll);
	}

	// Compute spline interpolation of main flyby camera parameters.
	auto origin = Vector3(Spline(splineAlpha, xOrigins.data(), splinePoints),
						  Spline(splineAlpha, yOrigins.data(), splinePoints),
						  Spline(splineAlpha, zOrigins.data(), splinePoints));

	auto target = Vector3(Spline(splineAlpha, xTargets.data(), splinePoints),
						  Spline(splineAlpha, yTargets.data(), splinePoints),
						  Spline(splineAlpha, zTargets.data(), splinePoints));

	short orientZ = Spline(splineAlpha, rolls.data(), splinePoints);

	auto pose = Pose(origin, EulerAngles(target - origin));
	pose.Orientation.z = orientZ;
	return pose;
}
