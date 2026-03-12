// CBVolumetricCloud.hlsli — Constant buffer for volumetric cloud rendering.
// Bound to register b9 (first free register after b8=Sky).

cbuffer CBVolumetricCloud : register(b9)
{
	// Row 0
	float CloudBottomHeight;
	float CloudTopHeight;
	float CloudThickness;
	float Coverage;
	//--
	// Row 1
	float CloudDensity;
	float ShapeScale;
	float DetailScale;
	float DetailStrength;
	//--
	// Row 2
	float WeatherScale;
	float Absorption;
	float AmbientContrib;
	float SilverliningStr;
	//--
	// Row 3
	float PhaseForward;
	float PhaseBackward;
	float WindSpeed;
	float EvolutionSpeed;
	//--
	// Row 4
	float2 WindDirection;
	float  CloudTime;
	float  JitterStrength;
	//--
	// Row 5
	float3 CloudLightDirection;
	float  CloudPadding0;
	//--
	// Row 6
	float3 CloudLightColor;
	float  CloudPadding1;
	//--
	// Row 7
	int PrimaryStepCount;
	int ShadowStepCount;
	int DetailNoiseEnabled;
	int CloudDebugView;
	//--
	// Row 8
	float2 CloudRenderSize;
	float2 InvCloudRenderSize;
	//--
	// Row 9
	int   TemporalEnabled;
	float FrameIndex;
	float EarthRadius;
	float PlanetCenterY;
};
