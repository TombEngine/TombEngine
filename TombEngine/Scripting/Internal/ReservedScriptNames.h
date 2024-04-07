#pragma once

// TODO: Clean up and organise this entire file.

// Tables
static constexpr char ScriptReserved_TEN[]				= "TEN";
static constexpr char ScriptReserved_Flow[]				= "Flow";
static constexpr char ScriptReserved_Logic[]			= "Logic";
static constexpr char ScriptReserved_Objects[]			= "Objects";
static constexpr char ScriptReserved_Strings[]			= "Strings";
static constexpr char ScriptReserved_Inventory[]		= "Inventory";
static constexpr char ScriptReserved_Input[]			= "Input";
static constexpr char ScriptReserved_Sound[]			= "Sound";
static constexpr char ScriptReserved_View[]				= "View";
static constexpr char ScriptReserved_Util[]				= "Util";
static constexpr char ScriptReserved_Effects[]			= "Effects";

// Classes
static constexpr char ScriptReserved_Static[]			= "Static";
static constexpr char ScriptReserved_Camera[]			= "Camera";
static constexpr char ScriptReserved_Sink[]				= "Sink";
static constexpr char ScriptReserved_SoundSource[]		= "SoundSource";
static constexpr char ScriptReserved_AIObject[]			= "AIObject";
static constexpr char ScriptReserved_Volume[]			= "Volume";
static constexpr char ScriptReserved_Room[]				= "Room";
static constexpr char ScriptReserved_Color[]			= "Color";
static constexpr char ScriptReserved_DisplayString[]	= "DisplayString";
static constexpr char ScriptReserved_Vec2[]				= "Vec2";
static constexpr char ScriptReserved_Rotation[]			= "Rotation";
static constexpr char ScriptReserved_LevelFunc[]		= "LevelFunc";

// Moveable object
static constexpr char ScriptReserved_Moveable[]			= "Moveable";
static constexpr char ScriptReserved_MoveableStatus[]	= "MoveableStatus";

// Player object
static constexpr char ScriptReserved_Lara[]							= "Lara";
static constexpr char ScriptReserved_GetPlayerInteractedMoveable[]	= "GetInteractedMoveable";

// DisplaySprite object
static constexpr char ScriptReserved_DisplaySprite[]				= "DisplaySprite";
static constexpr char ScriptReserved_DisplayStringGetObjectID[]		= "GetObjectID";
static constexpr char ScriptReserved_DisplayStringGetSpriteID[]		= "GetSpriteID";
static constexpr char ScriptReserved_DisplayStringGetPosition[]		= "GetPosition";
static constexpr char ScriptReserved_DisplayStringGetRotation[]		= "GetRotation";
static constexpr char ScriptReserved_DisplayStringGetScale[]		= "GetScale";
static constexpr char ScriptReserved_DisplayStringGetColor[]		= "GetColor";
static constexpr char ScriptReserved_DisplayStringSetObjectID[]		= "SetObjectID";
static constexpr char ScriptReserved_DisplayStringSetSpriteID[]		= "SetSpriteID";
static constexpr char ScriptReserved_DisplayStringSetPosition[]		= "SetPosition";
static constexpr char ScriptReserved_DisplayStringSetRotation[]		= "SetRotation";
static constexpr char ScriptReserved_DisplayStringSetScale[]		= "SetScale";
static constexpr char ScriptReserved_DisplayStringSetColor[]		= "SetColor";
static constexpr char ScriptReserved_DisplaySpriteDraw[]			= "Draw";

// Built-in LevelFuncs
static constexpr char ScriptReserved_OnStart[]			= "OnStart";
static constexpr char ScriptReserved_OnLoad[]			= "OnLoad";
static constexpr char ScriptReserved_OnLoop[]			= "OnLoop";
static constexpr char ScriptReserved_OnControlPhase[]	= "OnControlPhase"; // DEPRECATED
static constexpr char ScriptReserved_OnSave[]			= "OnSave";
static constexpr char ScriptReserved_OnEnd[]			= "OnEnd";

static constexpr char ScriptReserved_EndReasonExitToTitle[]		= "EXITTOTITLE";
static constexpr char ScriptReserved_EndReasonLevelComplete[]	= "LEVELCOMPLETE";
static constexpr char ScriptReserved_EndReasonLoadGame[]		= "LOADGAME";
static constexpr char ScriptReserved_EndReasonDeath[]			= "DEATH";
static constexpr char ScriptReserved_EndReasonOther[]			= "OTHER";

// Callback points
static constexpr char ScriptReserved_PreStart[]			= "PRESTART";
static constexpr char ScriptReserved_PostStart[]		= "POSTSTART";
static constexpr char ScriptReserved_PreEnd[]			= "PREEND";
static constexpr char ScriptReserved_PostEnd[]			= "POSTEND";
static constexpr char ScriptReserved_PreSave[]			= "PRESAVE";
static constexpr char ScriptReserved_PostSave[]			= "POSTSAVE";
static constexpr char ScriptReserved_PreLoad[]			= "PRELOAD";
static constexpr char ScriptReserved_PostLoad[]			= "POSTLOAD";
static constexpr char ScriptReserved_PreLoop[]			= "PRELOOP";
static constexpr char ScriptReserved_PostLoop[]			= "POSTLOOP";
static constexpr char ScriptReserved_PreControlPhase[]	= "PRECONTROLPHASE";
static constexpr char ScriptReserved_PostControlPhase[]	= "POSTCONTROLPHASE";

// Event types
static constexpr char ScriptReserved_EventOnEnter[]	= "ENTER";
static constexpr char ScriptReserved_EventOnInside[] = "INSIDE";
static constexpr char ScriptReserved_EventOnLeave[]	= "LEAVE";
static constexpr char ScriptReserved_EventOnLoad[] = "LOAD";
static constexpr char ScriptReserved_EventOnSave[] = "SAVE";
static constexpr char ScriptReserved_EventOnStart[] = "START";
static constexpr char ScriptReserved_EventOnEnd[] = "END";
static constexpr char ScriptReserved_EventOnLoop[] = "LOOP";

// Member functions
static constexpr char ScriptReserved_New[]					= "New";
static constexpr char ScriptReserved_Init[]					= "Init";
static constexpr char ScriptReserved_Enable[]				= "Enable";
static constexpr char ScriptReserved_Disable[]				= "Disable";
static constexpr char ScriptReserved_MakeInvisible[]		= "MakeInvisible";
static constexpr char ScriptReserved_SetVisible[]			= "SetVisible";
static constexpr char ScriptReserved_Explode[]				= "Explode";
static constexpr char ScriptReserved_Shatter[]				= "Shatter";
static constexpr char ScriptReserved_SetPoison[]			= "SetPoison";
static constexpr char ScriptReserved_GetPoison[]			= "GetPoison";
static constexpr char ScriptReserved_SetAir[]				= "SetAir";
static constexpr char ScriptReserved_GetAir[]				= "GetAir";
static constexpr char ScriptReserved_GetWet[]				= "GetWet";
static constexpr char ScriptReserved_SetWet[]				= "SetWet";
static constexpr char ScriptReserved_GetStamina[]			= "GetStamina";
static constexpr char ScriptReserved_SetStamina[]			= "SetStamina";
static constexpr char ScriptReserved_GetAirborne[]			= "GetAirborne";
static constexpr char ScriptReserved_SetAirborne[]			= "SetAirborne";
static constexpr char ScriptReserved_GetAmmoType[]			= "GetAmmoType";
static constexpr char ScriptReserved_GetAmmoCount[]			= "GetAmmoCount";
static constexpr char ScriptReserved_GetVehicle[]			= "GetVehicle";
static constexpr char ScriptReserved_GetTarget[]			= "GetTarget";
static constexpr char ScriptReserved_GetColor[]				= "GetColor";
static constexpr char ScriptReserved_SetColor[]				= "SetColor";
static constexpr char ScriptReserved_SetFlags[]				= "SetFlags";
static constexpr char ScriptReserved_SetTranslated[]		= "SetTranslated";
static constexpr char ScriptReserved_GetPosition[]			= "GetPosition";
static constexpr char ScriptReserved_GetJointPosition[]		= "GetJointPosition";
static constexpr char ScriptReserved_SetPosition[]			= "SetPosition";
static constexpr char ScriptReserved_GetRotation[]			= "GetRotation";
static constexpr char ScriptReserved_SetRotation[]			= "SetRotation";
static constexpr char ScriptReserved_GetRotationY[]			= "GetRotationY";
static constexpr char ScriptReserved_SetRotationY[]			= "SetRotationY";
static constexpr char ScriptReserved_GetScale[]				= "GetScale";
static constexpr char ScriptReserved_SetScale[]				= "SetScale";
static constexpr char ScriptReserved_GetSolid[]				= "GetSolid";
static constexpr char ScriptReserved_SetSolid[]				= "SetSolid";
static constexpr char ScriptReserved_GetName[]				= "GetName";
static constexpr char ScriptReserved_SetName[]				= "SetName";
static constexpr char ScriptReserved_GetSlot[]				= "GetSlot";
static constexpr char ScriptReserved_SetSlot[]				= "SetSlot";
static constexpr char ScriptReserved_GetObjectID[]			= "GetObjectID";
static constexpr char ScriptReserved_SetObjectID[]			= "SetObjectID";
static constexpr char ScriptReserved_GetSoundID[]			= "GetSoundID";
static constexpr char ScriptReserved_SetSoundID[]			= "SetSoundID";
static constexpr char ScriptReserved_GetHP[]				= "GetHP";
static constexpr char ScriptReserved_SetHP[]				= "SetHP";
static constexpr char ScriptReserved_GetSlotHP[]			= "GetSlotHP";
static constexpr char ScriptReserved_GetVelocity[]			= "GetVelocity";
static constexpr char ScriptReserved_SetVelocity[]			= "SetVelocity";
static constexpr char ScriptReserved_GetFrameNumber[]		= "GetFrame";
static constexpr char ScriptReserved_GetEndFrame[]			= "GetEndFrame";
static constexpr char ScriptReserved_SetFrameNumber[]		= "SetFrame";
static constexpr char ScriptReserved_GetAnimNumber[]		= "GetAnim";
static constexpr char ScriptReserved_SetAnimNumber[]		= "SetAnim";
static constexpr char ScriptReserved_GetStateNumber[]		= "GetState";
static constexpr char ScriptReserved_SetStateNumber[]		= "SetState";
static constexpr char ScriptReserved_GetOCB[]				= "GetOCB";
static constexpr char ScriptReserved_SetOCB[]				= "SetOCB";
static constexpr char ScriptReserved_GetStatus[]			= "GetStatus";
static constexpr char ScriptReserved_SetStatus[]			= "SetStatus";
static constexpr char ScriptReserved_GetAIBits[]			= "GetAIBits";
static constexpr char ScriptReserved_SetAIBits[]			= "SetAIBits";
static constexpr char ScriptReserved_GetItemFlags[]			= "GetItemFlags";
static constexpr char ScriptReserved_SetItemFlags[]			= "SetItemFlags";
static constexpr char ScriptReserved_GetLocationAI[]		= "GetLocationAI";
static constexpr char ScriptReserved_SetLocationAI[]		= "SetLocationAI";
static constexpr char ScriptReserved_GetMeshCount[]			= "GetMeshCount";
static constexpr char ScriptReserved_GetMeshVisible[]		= "GetMeshVisible";
static constexpr char ScriptReserved_SetMeshVisible[]		= "SetMeshVisible";
static constexpr char ScriptReserved_ShatterMesh[]			= "ShatterMesh";
static constexpr char ScriptReserved_GetMeshSwapped[]		= "GetMeshShapped";
static constexpr char ScriptReserved_SwapMesh[]				= "SwapMesh";
static constexpr char ScriptReserved_UnswapMesh[]			= "UnswapMesh";
static constexpr char ScriptReserved_GetHitStatus[]			= "GetHitStatus";
static constexpr char ScriptReserved_GetActive[]			= "GetActive";
static constexpr char ScriptReserved_GetRoom[]				= "GetRoom";
static constexpr char ScriptReserved_GetRoomNumber[]		= "GetRoomNumber";
static constexpr char ScriptReserved_SetRoomNumber[]		= "SetRoomNumber";
static constexpr char ScriptReserved_GetStrength[]			= "GetStrength";
static constexpr char ScriptReserved_SetStrength[]			= "SetStrength";
static constexpr char ScriptReserved_GetValid[]				= "GetValid";
static constexpr char ScriptReserved_SetEffect[]			= "SetEffect";
static constexpr char ScriptReserved_GetEffect[]			= "GetEffect";
static constexpr char ScriptReserved_SetCustomEffect[]		= "SetCustomEffect";
static constexpr char ScriptReserved_Destroy[]				= "Destroy";
static constexpr char ScriptReserved_GetKey[]				= "GetKey";
static constexpr char ScriptReserved_SetKey[]				= "SetKey";
static constexpr char ScriptReserved_GetOnKilled[]			= "GetOnKilled";
static constexpr char ScriptReserved_SetOnKilled[]			= "SetOnKilled";
static constexpr char ScriptReserved_GetOnHit[]				= "GetOnHit";
static constexpr char ScriptReserved_SetOnHit[]				= "SetOnHit";
static constexpr char ScriptReserved_GetOnCollidedWithObject[]	= "GetOnCollidedWithObject";
static constexpr char ScriptReserved_SetOnCollidedWithObject[]	= "SetOnCollidedWithObject";
static constexpr char ScriptReserved_GetOnCollidedWithRoom[]	= "GetOnCollidedWithRoom";
static constexpr char ScriptReserved_SetOnCollidedWithRoom[]	= "SetOnCollidedWithRoom";
static constexpr char ScriptReserved_ToVec2[]					= "ToVec2";
static constexpr char ScriptReserved_AttachObjCamera[]		= "AttachObjCamera";
static constexpr char ScriptReserved_AnimFromObject[]		= "AnimFromObject";
static constexpr char ScriptReserved_GetCreatureEnemy[]		= "GetCreatureEnemy";
static constexpr char ScriptReserved_ClearCreatureEnemy[]	= "ClearCreatureEnemy";
static constexpr char ScriptReserved_SetCreatureEnemy[]		= "SetCreatureEnemy";
static constexpr char ScriptReserved_GetCreatureTarget[]	= "GetCreatureTarget";
static constexpr char ScriptReserved_SetCreatureTarget[]	= "SetCreatureTarget";
static constexpr char ScriptReserved_ClearActivators[]		= "ClearActivators";
static constexpr char ScriptReserved_IsMoveableInside[]		= "IsMoveableInside";
static constexpr char ScriptReserved_GetFlag[]				= "GetFlag";
static constexpr char ScriptReserved_SetFlag[]				= "SetFlag";
static constexpr char ScriptReserved_IsTagPresent[]			= "IsTagPresent";
static constexpr char ScriptReserved_SetReverbType[]		= "SetReverbType";

// Flow Functions
static constexpr char ScriptReserved_AddLevel[]					= "AddLevel";
static constexpr char ScriptReserved_GetLevel[]					= "GetLevel";
static constexpr char ScriptReserved_GetCurrentLevel[]			= "GetCurrentLevel";
static constexpr char ScriptReserved_SetIntroImagePath[]		= "SetIntroImagePath";
static constexpr char ScriptReserved_SetTitleScreenImagePath[]	= "SetTitleScreenImagePath";
static constexpr char ScriptReserved_SetFarView[]				= "SetFarView";
static constexpr char ScriptReserved_SetSettings[]				= "SetSettings";
static constexpr char ScriptReserved_SetAnimations[]			= "SetAnimations";
static constexpr char ScriptReserved_EndLevel[]					= "EndLevel";
static constexpr char ScriptReserved_GetGameStatus[]			= "GetGameStatus";
static constexpr char ScriptReserved_SaveGame[]					= "SaveGame";
static constexpr char ScriptReserved_LoadGame[]					= "LoadGame";
static constexpr char ScriptReserved_DeleteSaveGame[]			= "DeleteSaveGame";
static constexpr char ScriptReserved_DoesSaveGameExist[]		= "DoesSaveGameExist";
static constexpr char ScriptReserved_GetSecretCount[]			= "GetSecretCount";
static constexpr char ScriptReserved_SetSecretCount[]			= "SetSecretCount";
static constexpr char ScriptReserved_SetTotalSecretCount[]		= "SetTotalSecretCount";
static constexpr char ScriptReserved_AddSecret[]				= "AddSecret";
static constexpr char ScriptReserved_EnableFlyCheat[]			= "EnableFlyCheat";
static constexpr char ScriptReserved_EnableMassPickup[]			= "EnableMassPickup";
static constexpr char ScriptReserved_EnableLaraInTitle[]		= "EnableLaraInTitle";
static constexpr char ScriptReserved_EnableLevelSelect[]		= "EnableLevelSelect";
static constexpr char ScriptReserved_EnableLoadSave[]			= "EnableLoadSave";
static constexpr char ScriptReserved_EnablePointFilter[]		= "EnablePointFilter";

// Flow Functions
static constexpr char ScriptReserved_SetStrings[]			= "SetStrings";
static constexpr char ScriptReserved_GetString[]			= "GetString";
static constexpr char ScriptReserved_SetLanguageNames[]		= "SetLanguageNames";

// Flow Tables
static constexpr char ScriptReserved_WeatherType[]	  = "WeatherType";
static constexpr char ScriptReserved_LaraType[]		  = "LaraType";
static constexpr char ScriptReserved_RotationAxis[]	  = "RotationAxis";
static constexpr char ScriptReserved_ItemAction[]	  = "ItemAction";
static constexpr char ScriptReserved_ErrorMode[]	  = "ErrorMode";
static constexpr char ScriptReserved_InventoryItem[]  = "InventoryItem";
static constexpr char ScriptReserved_LaraWeaponType[] = "LaraWeaponType";
static constexpr char ScriptReserved_PlayerAmmoType[] = "PlayerAmmoType";
static constexpr char ScriptReserved_HandStatus[]	  = "HandStatus";
static constexpr char ScriptReserved_GameStatus[]	  = "GameStatus";

// Functions
static constexpr char ScriptReserved_ShowString[]					= "ShowString";
static constexpr char ScriptReserved_HideString[]					= "HideString";
static constexpr char ScriptReserved_IsStringDisplaying[]			= "IsStringDisplaying";
static constexpr char ScriptReserved_SetAmbientTrack[]				= "SetAmbientTrack";
static constexpr char ScriptReserved_PlayAudioTrack[]				= "PlayAudioTrack";
static constexpr char ScriptReserved_StopAudioTrack[]				= "StopAudioTrack";
static constexpr char ScriptReserved_StopAudioTracks[]				= "StopAudioTracks";
static constexpr char ScriptReserved_GetAudioTrackLoudness[]		= "GetAudioTrackLoudness";
static constexpr char ScriptReserved_GetCurrentSubtitle[]			= "GetCurrentSubtitle";
static constexpr char ScriptReserved_PlaySound[]					= "PlaySound";
static constexpr char ScriptReserved_StopSound[]					= "StopSound";
static constexpr char ScriptReserved_IsSoundPlaying[]				= "IsSoundPlaying";
static constexpr char ScriptReserved_IsAudioTrackPlaying[]			= "IsAudioTrackPlaying";
static constexpr char ScriptReserved_GiveInvItem[]					= "GiveItem";
static constexpr char ScriptReserved_TakeInvItem[]					= "TakeItem";
static constexpr char ScriptReserved_GetInvItemCount[]				= "GetItemCount";
static constexpr char ScriptReserved_SetInvItemCount[]				= "SetItemCount";
static constexpr char ScriptReserved_GetMoveableByName[]			= "GetMoveableByName";
static constexpr char ScriptReserved_GetStaticByName[]				= "GetStaticByName";
static constexpr char ScriptReserved_GetMoveablesBySlot[]			= "GetMoveablesBySlot";
static constexpr char ScriptReserved_GetStaticsBySlot[]				= "GetStaticsBySlot";
static constexpr char ScriptReserved_GetCameraByName[]				= "GetCameraByName";
static constexpr char ScriptReserved_GetSinkByName[]				= "GetSinkByName";
static constexpr char ScriptReserved_GetAIObjectByName[]			= "GetAIObjectByName";
static constexpr char ScriptReserved_GetSoundSourceByName[]			= "GetSoundSourceByName";
static constexpr char ScriptReserved_GetVolumeByName[]				= "GetVolumeByName";
static constexpr char ScriptReserved_GetRoomsByTag[]				= "GetRoomsByTag";
static constexpr char ScriptReserved_GetRoomByName[]				= "GetRoomByName";
static constexpr char ScriptReserved_CalculateDistance[]			= "CalculateDistance";
static constexpr char ScriptReserved_CalculateHorizontalDistance[]	= "CalculateHorizontalDistance";
static constexpr char ScriptReserved_PercentToScreen[]				= "PercentToScreen";
static constexpr char ScriptReserved_ScreenToPercent[]				= "ScreenToPercent";
static constexpr char ScriptReserved_GetAspectRatio[]				= "GetAspectRatio";
static constexpr char ScriptReserved_HasLineOfSight[]				= "HasLineOfSight";

static constexpr char ScriptReserved_AddCallback[]					= "AddCallback";
static constexpr char ScriptReserved_RemoveCallback[]				= "RemoveCallback";
static constexpr char ScriptReserved_HandleEvent[]					= "HandleEvent";
static constexpr char ScriptReserved_EnableEvent[]					= "EnableEvent";
static constexpr char ScriptReserved_DisableEvent[]					= "DisableEvent";

static constexpr char ScriptReserved_EmitParticle[]					= "EmitParticle";
static constexpr char ScriptReserved_EmitLightningArc[]				= "EmitLightningArc";
static constexpr char ScriptReserved_EmitShockwave[]				= "EmitShockwave";
static constexpr char ScriptReserved_EmitLight[]					= "EmitLight";
static constexpr char ScriptReserved_EmitBlood[]					= "EmitBlood";
static constexpr char ScriptReserved_EmitFire[]						= "EmitFire";
static constexpr char ScriptReserved_MakeExplosion[]				= "MakeExplosion";
static constexpr char ScriptReserved_MakeEarthquake[]				= "MakeEarthquake";
static constexpr char ScriptReserved_Vibrate[]						= "Vibrate";
static constexpr char ScriptReserved_FlashScreen[]					= "FlashScreen";
static constexpr char ScriptReserved_FadeIn[]						= "FadeIn";
static constexpr char ScriptReserved_FadeOut[]						= "FadeOut";
static constexpr char ScriptReserved_FadeOutComplete[]				= "FadeOutComplete";
static constexpr char ScriptReserved_SetCineBars[]					= "SetCineBars";
static constexpr char ScriptReserved_SetFOV[]						= "SetFOV";
static constexpr char ScriptReserved_GetFOV[]						= "GetFOV";
static constexpr char ScriptReserved_GetCameraType[]				= "GetCameraType";
static constexpr char ScriptReserved_GetCameraRoom[]				= "GetCameraRoom";

static constexpr char ScriptReserved_SetPostProcessMode[]			= "SetPostProcessMode";
static constexpr char ScriptReserved_SetPostProcessStrength[]		= "SetPostProcessStrength";
static constexpr char ScriptReserved_SetPostProcessTint[]			= "SetPostProcessTint";

static constexpr char ScriptReserved_GetCameraPosition[]			= "GetCameraPosition";
static constexpr char ScriptReserved_GetCameraTarget[]				= "GetCameraTarget";

static constexpr char ScriptReserved_KeyIsHeld[]					= "KeyIsHeld";
static constexpr char ScriptReserved_KeyIsHit[]						= "KeyIsHit";
static constexpr char ScriptReserved_KeyPush[]						= "KeyPush";
static constexpr char ScriptReserved_KeyClear[]						= "KeyClear";

static constexpr char ScriptReserved_FlipMap[]						= "FlipMap";
static constexpr char ScriptReserved_GetFlipMapStatus[]				= "GetFlipMapStatus";
static constexpr char ScriptReserved_PlayFlyBy[]					= "PlayFlyBy";

static constexpr char ScriptReserved_PlayCamera[]					= "PlayCamera";
static constexpr char ScriptReserved_ResetObjCamera[]				= "ResetObjCamera";
static constexpr char ScriptReserved_UndrawWeapon[]					= "UndrawWeapon";
static constexpr char ScriptReserved_GetHandStatus[]				= "GetHandStatus";
static constexpr char ScriptReserved_GetWeaponType[]				= "GetWeaponType";
static constexpr char ScriptReserved_ThrowAwayTorch[]				= "ThrowAwayTorch";
static constexpr char ScriptReserved_SetWeaponType[]				= "SetWeaponType";
static constexpr char ScriptReserved_TorchIsLit[]					= "TorchIsLit";
static constexpr char ScriptReserved_PrintLog[]						= "PrintLog";
static constexpr char ScriptReserved_PickMoveable[]					= "PickMoveableByDisplayPosition";
static constexpr char ScriptReserved_PickStatic[]					= "PickStaticByDisplayPosition";
static constexpr char ScriptReserved_GetDisplayPosition[]			= "GetDisplayPosition";
static constexpr char ScriptReserved_GetCursorDisplayPosition[]		= "GetCursorDisplayPosition"; // Deprecated
static constexpr char ScriptReserved_GetMouseDisplayPosition[]		= "GetMouseDisplayPosition";

// Enums
static constexpr char ScriptReserved_ObjID[]					= "ObjID";
static constexpr char ScriptReserved_BlendID[]					= "BlendID";
static constexpr char ScriptReserved_EffectID[]					= "EffectID";
static constexpr char ScriptReserved_ActionID[]					= "ActionID";
static constexpr char ScriptReserved_CameraType[]				= "CameraType";
static constexpr char ScriptReserved_PostProcessMode[]			= "PostProcessMode";
static constexpr char ScriptReserved_SoundTrackType[]			= "SoundTrackType";
static constexpr char ScriptReserved_LogLevel[]					= "LogLevel";
static constexpr char ScriptReserved_RoomFlagID[]				= "RoomFlagID";
static constexpr char ScriptReserved_RoomReverb[]				= "RoomReverb";
static constexpr char ScriptReserved_DisplayStringOption[]		= "DisplayStringOption";
static constexpr char ScriptReserved_CallbackPoint[]			= "CallbackPoint";
static constexpr char ScriptReserved_EndReason[]				= "EndReason";
static constexpr char ScriptReserved_EventType[]				= "EventType";
static constexpr char ScriptReserved_AlignMode[]				= "AlignMode";
static constexpr char ScriptReserved_ScaleMode[]				= "ScaleMode";

static constexpr char ScriptReserved_LevelVars[]	= "LevelVars";
static constexpr char ScriptReserved_GameVars[]		= "GameVars";
static constexpr char ScriptReserved_LevelFuncs[]	= "LevelFuncs";
static constexpr char ScriptReserved_Engine[]		= "Engine";

// Constants
static constexpr char ScriptReserved_LogLevelInfo[]		= "INFO";
static constexpr char ScriptReserved_LogLevelWarning[]	= "WARNING";
static constexpr char ScriptReserved_LogLevelError[]	= "ERROR";

// Internal
static constexpr char ScriptReserved_LaraObject[] = "LaraObject";

// Vec2
constexpr char ScriptReserved_Vec2SetLength[]		= "ToLength";
constexpr char ScriptReserved_Vec2Normalize[]		= "Normalize";
constexpr char ScriptReserved_Vec2Rotate[]			= "Rotate";
constexpr char ScriptReserved_Vec2Lerp[]			= "Lerp";
constexpr char ScriptReserved_Vec2Cross[]			= "Cross";
constexpr char ScriptReserved_Vec2Dot[]				= "Dot";
constexpr char ScriptReserved_Vec2Distance[]		= "Distance";
constexpr char ScriptReserved_Vec2Length[]			= "Length";

// Vec3
constexpr char ScriptReserved_Vec3[]				= "Vec3";
constexpr char ScriptReserved_Vec3Normalize[]		= "Normalize";
constexpr char ScriptReserved_Vec3Rotate[]			= "Rotate";
constexpr char ScriptReserved_Vec3Lerp[]			= "Lerp";
constexpr char ScriptReserved_Vec3Cross[]			= "Cross";
constexpr char ScriptReserved_Vec3Dot[]				= "Dot";
constexpr char ScriptReserved_Vec3Distance[]		= "Distance";
constexpr char ScriptReserved_Vec3Length[]			= "Length";
