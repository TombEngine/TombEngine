#include "framework.h"
#include "Scripting/Internal/TEN/View/ViewHandler.h"

#include "Game/camera.h"
#include "Game/effects/weather.h"
#include "Game/Lara/lara.h"
#include "Game/spotcam.h"
#include "Renderer/Renderer.h"
#include "Scripting/Internal/LuaHandler.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"
#include "Scripting/Internal/TEN/Objects/Room/RoomObject.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Scripting/Internal/TEN/View/AlignModes.h"
#include "Scripting/Internal/TEN/View/CameraTypes.h"
#include "Scripting/Internal/TEN/View/DisplaySprite/ScriptDisplaySprite.h"
#include "Scripting/Internal/TEN/View/ScaleModes.h"
#include "Scripting/Internal/TEN/View/PostProcessEffects.h"
#include "Specific/clock.h"
#include "Specific/Video/Video.h"

using namespace TEN::Effects::Environment;
using namespace TEN::Scripting::DisplaySprite;
using namespace TEN::Scripting::View;
using namespace TEN::Video;

using TEN::Renderer::g_Renderer;

/***
Functions to manage camera and game view.
@tentable View
@pragma nostrip
*/

namespace TEN::Scripting::View
{
	static void FadeOut(TypeOrNil<float> speed)
	{
		SetScreenFadeOut(ValueOr<float>(speed, 1.0f) / (float)FPS);
	}

	static void FadeIn(TypeOrNil<float> speed)
	{
		SetScreenFadeIn(ValueOr<float>(speed, 1.0f) / (float)FPS);
	}

	static bool FadeOutComplete()
	{
		return ScreenFadeCurrent == 0.0f;
	}

	static void SetCineBars(TypeOrNil<float> height, TypeOrNil<float> speed)
	{
		// divide by 200 so that a percentage of 100 means that each
		// bar takes up half the screen
		float heightProportion = ValueOr<float>(height, 30) / 200.0f;
		float speedProportion = ValueOr<float>(speed, 30) / 200.0f;
		SetCinematicBars(heightProportion, speedProportion / float(FPS));
	}

	static void SetFOV(float angle)
	{
		AlterFOV(ANGLE(std::clamp(abs(angle), 10.0f, 170.0f)));
	}

	static float GetFOV()
	{
		return TO_DEGREES(GetCurrentFOV());
	}

	static ScriptCameraType GetCameraType()
	{
		if (UseSpotCam)
			return ScriptCameraType::Flyby;

		if (Lara.Control.Look.IsUsingLasersight)
			return ScriptCameraType::Lasersight;

		if (Lara.Control.Look.IsUsingBinoculars)
			return ScriptCameraType::Binoculars;

		if (Camera.oldType == CameraType::Heavy)
			return ScriptCameraType::Fixed;

		return (ScriptCameraType)Camera.oldType;
	}
	
	static Vec3 GetCameraPosition()
	{
		return Vec3(Camera.pos.ToVector3());
	}

	static Vec3 GetCameraTarget()
	{
		return Vec3(Camera.target.ToVector3());
	}

	static std::unique_ptr<Room> GetCameraRoom()
	{
		return std::make_unique<Room>(g_Level.Rooms[Camera.pos.RoomNumber]);
	}

	static void ResetObjCamera()
	{
		ObjCamera(LaraItem, 0, LaraItem, 0, false);
	}

	static void PlayFlyby(int seqID)
	{
		UseSpotCam = true;
		InitializeSpotCam(seqID);
	}

	static void PlayVideo(const std::string& fileName, TypeOrNil<bool> background, TypeOrNil<bool> silent, TypeOrNil<bool> loop)
	{
		auto mode = ValueOr<bool>(background, false) ? VideoPlaybackMode::Background : VideoPlaybackMode::Exclusive;
		g_VideoPlayer.Play(fileName, mode, ValueOr<bool>(silent, false), ValueOr<bool>(loop, false));
	}

	static void StopVideo()
	{
		g_VideoPlayer.Stop();
	}

	static Vec3 GetFlybyPosition(int seqID, float progress, TypeOrNil<bool> loop)
	{
		constexpr auto PROGRESS_MAX = 100.0f;

		return Vec3(GetCameraTransform(seqID, progress / PROGRESS_MAX, ValueOr<bool>(loop, false)).Position);
	}

	static Rotation GetFlybyRotation(int seqID, float progress, TypeOrNil<bool> loop)
	{
		constexpr auto PROGRESS_MAX = 100.0f;

		return Rotation(GetCameraTransform(seqID, progress / PROGRESS_MAX, ValueOr<bool>(loop, false)).Orientation);
	}

	static void FlashScreen(TypeOrNil<ScriptColor> col, TypeOrNil<float> speed)
	{
		auto color = ValueOr<ScriptColor>(col, ScriptColor(255, 255, 255));
		Weather.Flash(color.GetR(), color.GetG(), color.GetB(), (ValueOr<float>(speed, 1.0)) / (float)FPS);
	}

	static float GetAspectRatio()
	{
		auto screenRes = g_Renderer.GetScreenResolution().ToVector2();
		return (screenRes.x / screenRes.y);
	}

	static void SetPostProcessMode(PostProcessMode mode)
	{
		g_Renderer.SetPostProcessMode(mode);
	}

	static void SetPostProcessStrength(TypeOrNil<float> strength)
	{
		g_Renderer.SetPostProcessStrength(std::clamp((float)ValueOr<float>(strength, 1.0), 0.0f, 1.0f));
	}

	static void SetPostProcessTint(const ScriptColor& color)
	{
		// Tint value must be normalized, because overbright color values cause postprocessing to fail and
		// flood the screen with a single color channel that is overflown.

		auto vec = (Vector3)color;
		vec.x = std::clamp(vec.x, 0.0f, 1.0f);
		vec.y = std::clamp(vec.y, 0.0f, 1.0f);
		vec.z = std::clamp(vec.z, 0.0f, 1.0f);
		g_Renderer.SetPostProcessTint(vec);
	}

	void Register(sol::state* state, sol::table& parent)
	{
		auto tableView = sol::table(state->lua_state(), sol::create);
		parent.set(ScriptReserved_View, tableView);

		///Do a full-screen fade-in from black.
		//@function FadeIn
		//@tparam float speed (default 1.0). Speed in units per second. A value of 1 will make the fade take one second.
		tableView.set_function(ScriptReserved_FadeIn, &FadeIn);

		///Do a full-screen fade-to-black. The screen will remain black until a call to FadeIn.
		//@function FadeOut
		//@tparam float speed (default 1.0). Speed in units per second. A value of 1 will make the fade take one second.
		tableView.set_function(ScriptReserved_FadeOut, &FadeOut);

		///Check if fade out is complete and screen is completely black.
		//@treturn bool state of the fade out
		tableView.set_function(ScriptReserved_FadeOutComplete, &FadeOutComplete);

		///Move black cinematic bars in from the top and bottom of the game window.
		//@function SetCineBars
		//@tparam float height (default 30). Percentage of the screen to be covered
		//@tparam float speed (default 30). Coverage percent per second
		tableView.set_function(ScriptReserved_SetCineBars, &SetCineBars);

		///Set field of view.
		//@function SetFOV
		//@tparam float angle in degrees (clamped to [10, 170])
		tableView.set_function(ScriptReserved_SetFOV, &SetFOV);

		///Get field of view.
		//@function GetFOV
		//@treturn float current FOV angle in degrees
		tableView.set_function(ScriptReserved_GetFOV, &GetFOV);

		///Shows the mode of the game camera.
		//@function GetCameraType
		//@treturn View.CameraType value used by the Main Camera.
		//@usage
		//LevelFuncs.OnLoop = function() 
		//	if (View.GetCameraType() == CameraType.COMBAT) then
		//		--Do your Actions here.
		//	end
		//end
		tableView.set_function(ScriptReserved_GetCameraType, &GetCameraType);

		///Gets current camera position.
		//@function GetCameraPosition
		//@treturn Vec3 current camera position
		tableView.set_function(ScriptReserved_GetCameraPosition, &GetCameraPosition);

		///Gets current camera target.
		//@function GetCameraTarget
		//@treturn Vec3 current camera target
		tableView.set_function(ScriptReserved_GetCameraTarget, &GetCameraTarget);

		///Gets current room where camera is positioned.
		//@function GetCameraRoom
		//@treturn Objects.Room current room of the camera
		tableView.set_function(ScriptReserved_GetCameraRoom, &GetCameraRoom);

		///Sets the post-process effect mode, like negative or monochrome.
		//@function SetPostProcessMode
		//@tparam View.PostProcessMode effect type to set.
		tableView.set_function(ScriptReserved_SetPostProcessMode, &SetPostProcessMode);

		///Sets the post-process effect strength.
		//@function SetPostProcessStrength
		//@tparam float strength (default 1.0). How strong the effect is.
		tableView.set_function(ScriptReserved_SetPostProcessStrength, &SetPostProcessStrength);

		///Sets the post-process tint.
		//@function SetPostProcessTint
		//@tparam Color tint value to use.
		tableView.set_function(ScriptReserved_SetPostProcessTint, &SetPostProcessTint);

		/// Play a video file. Should be placed in the `FMV` folder.
		// @function PlayVideo
		// @tparam string fileName Video file name.  Can be provided without extension, if type is mp4, mkv or avi.
		// @tparam[opt] bool background (default: false). Play video in the background mode. In such case, video must be shown using @{View.DisplaySprite}.
		// @tparam[opt] bool silent (default: false). Play video without sound.
		// @tparam[opt] bool loop (default: false). Play video in a loop.
		tableView.set_function(ScriptReserved_PlayVideo, &PlayVideo);

		/// Stop the currently playing video. Only possible if video is playing in the background mode.
		// @function StopVideo
		tableView.set_function(ScriptReserved_StopVideo, &StopVideo);

		/// Play a flyby sequence.
		// @function PlayFlyby
		// @tparam int seqID Flyby sequence ID.
		tableView.set_function(ScriptReserved_PlayFlyby, &PlayFlyby);

		/// Get a flyby sequence's position at a specified progress point in percent.
		// @function GetFlybyPosition
		// @tparam int seqID Flyby sequence ID.
		// @tparam float progress Progress point in percent. Clamped to [0, 100].
		// @tparam[opt] bool loop Smooth the position near start and end points, as if the sequence is looped.
		// @treturn Vec3 Position at the given progress point.
		tableView.set_function(ScriptReserved_GetFlybyPosition, &GetFlybyPosition);

		/// Get a flyby sequence's rotation at a specified progress point in percent.
		// @function GetFlybyRotation
		// @tparam int seqID Flyby sequence ID.
		// @tparam float progress Progress point in percent. Clamped to [0, 100].
		// @tparam[opt] bool loop Smooth the position near start and end points, as if the sequence is looped.
		// @treturn Rotation Rotation at the given progress point.
		tableView.set_function(ScriptReserved_GetFlybyRotation, &GetFlybyRotation);

		/// Reset object camera back to Lara and deactivate object camera.
		//@function ResetObjCamera
		tableView.set_function(ScriptReserved_ResetObjCamera, &ResetObjCamera);

		/// Flash screen.
		//@function FlashScreen
		//@tparam Color color (default Color(255, 255, 255))
		//@tparam float speed (default 1.0). Speed in units per second. Value of 1 will make flash take one second. Clamped to [0.005, 1.0].
		tableView.set_function(ScriptReserved_FlashScreen, &FlashScreen);

		/// Get the display resolution's aspect ratio.
		// @function GetAspectRatio
		// @treturn float Display resolution's aspect ratio.
		tableView.set_function(ScriptReserved_GetAspectRatio, &GetAspectRatio);

		// COMPATIBILITY
		tableView.set_function("PlayFlyBy", &PlayFlyby);

		// Register types.
		ScriptDisplaySprite::Register(*state, parent);

		// Register enums.
		auto handler = LuaHandler(state);
		handler.MakeReadOnlyTable(tableView, ScriptReserved_CameraType, CAMERA_TYPE);
		handler.MakeReadOnlyTable(tableView, ScriptReserved_AlignMode, ALIGN_MODES);
		handler.MakeReadOnlyTable(tableView, ScriptReserved_ScaleMode, SCALE_MODES);
		handler.MakeReadOnlyTable(tableView, ScriptReserved_PostProcessMode, POSTPROCESS_MODES);
	}
};
