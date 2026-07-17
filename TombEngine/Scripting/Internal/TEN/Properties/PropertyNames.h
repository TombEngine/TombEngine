#pragma once

#include "Specific/trutils.h"

using namespace TEN::Utils;

// Common property names.

static const auto PropName_AttackType	= GetHash("AttackType");
static const auto PropName_Damage		= GetHash("Damage");
static const auto PropName_HitPoints	= GetHash("HitPoints");
static const auto PropName_MeshID		= GetHash("MeshID");
static const auto PropName_PrimaryColor = GetHash("PrimaryColor");

// Effect property names.

static const auto PropName_PointLightEnabled	= GetHash("PointLightEnabled");
static const auto PropName_PointLightCastShadow = GetHash("PointLightCastShadow");
static const auto PropName_PointLightColor		= GetHash("PointLightColor");
static const auto PropName_PointLightFalloff	= GetHash("PointLightFalloff");

static const auto PropName_SpotLightEnabled		= GetHash("SpotLightEnabled");
static const auto PropName_SpotLightCastShadow	= GetHash("SpotLightCastShadow");
static const auto PropName_SpotLightColor		= GetHash("SpotLightColor");
static const auto PropName_SpotLightDistance	= GetHash("SpotLightDistance");
static const auto PropName_SpotLightFalloff		= GetHash("SpotLightFalloff");
static const auto PropName_SpotLightIntensity	= GetHash("SpotLightIntensity");
static const auto PropName_SpotLightRadius		= GetHash("SpotLightRadius");

// Vehicle property names.

static const auto PropName_VehicleFoam				= GetHash("VehicleFoamEnable");

static const auto PropName_VehicleMist				= GetHash("VehicleMistEnable");
static const auto PropName_VehicleMistStartColor	= GetHash("VehicleMistStartColor");
static const auto PropName_VehicleMistEndColor		= GetHash("VehicleMistEndColor");

static const auto PropName_VehicleSmokeStartColor	= GetHash("VehicleSmokeStartColor");
static const auto PropName_VehicleSmokeEndColor		= GetHash("VehicleSmokeEndColor");

static const auto PropName_VehicleWake				= GetHash("VehicleWakeEnable");
static const auto PropName_VehicleWakeStartColor	= GetHash("VehicleWakeStartColor");
static const auto PropName_VehicleWakeEndColor		= GetHash("VehicleWakeEndColor");