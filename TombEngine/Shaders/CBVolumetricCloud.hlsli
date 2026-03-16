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
	//--
	// Row 10
	float HorizonFade;    // Multiplier on horizon atmospheric fade (0 = none, 1 = full).
	float DistanceFade;   // Multiplier on distance-based opacity falloff (0 = none, 1 = full).
	int   CloudType;      // 0=None, 1=CirrusHigh, 2=AltocumulusMid, 3=StratocumulusLow, 4=CumulonimbusVertical
	float CloudPadding2;
	//--
	// Row 11 — Altocumulus-specific appearance parameters (only used when CloudType == 2)
	float AltoBillowStrength;  // [0,1]      blend toward billow (abs-value) FBM noise
	float AltoCovSoftWidth;    // [0,0.25]   self-referential coverage soft-threshold width
	float AltoAbsorption;      // [0.1,5.0]  absorption coefficient for Altocumulus
	float AltoCloudSize;       // [0.2,5.0]  feature scale multiplier
	//--
	// Row 12 — Altocumulus extended parameters
	float AltoCloudAmount;     // [0.0,1.0]  coverage/fill control
	float AltoCloudBrightness; // [0.1,4.0]  brightness multiplier
	float AltoFbmLacunarity;   // [1.5,4.0]  FBM frequency ratio per octave
	float AltoFbmGain;         // [0.1,0.9]  FBM amplitude scaling per octave
	//--
	// Row 13 — Altocumulus cloud color
	float3 AltoCloudColor;     // [0,1] per channel — RGB tint for cloud top/highlights
	float  AltoThickness;      // [50,5000]  cloud slab thickness for Altocumulus
	//--
	// Row 14 — Altocumulus dark / shadow color
	float3 AltoCloudColorDark; // [0,1] per channel — RGB tint for cloud base/shadow
	float  AltoRow14Pad;
};
