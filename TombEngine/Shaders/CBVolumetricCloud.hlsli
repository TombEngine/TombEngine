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
    float HorizonFade;	// Multiplier on horizon atmospheric fade (0 = none, 1 = full).
    float DistanceFade;   // Multiplier on distance-based opacity falloff (0 = none, 1 = full).
    int   CloudType;	  // 0=None, 1=CirrusHigh, 2=AltocumulusMid, 3=StratocumulusLow, 4=CumulonimbusVertical
    float CloudPadding2;
    //--
    // Row 11 — Altocumulus-specific appearance parameters (only used when CloudType == 2)
    float AltoBillowStrength;  // [0,1]	  blend toward billow (abs-value) FBM noise
    float AltoCovSoftWidth;	// [0,0.25]   self-referential coverage soft-threshold width
    float AltoAbsorption;	  // [0.1,5.0]  absorption coefficient for Altocumulus
    float AltoCloudSize;	   // [0.2,5.0]  feature scale multiplier
    //--
    // Row 12 — Altocumulus extended parameters
    float AltoCloudAmount;	 // [0.0,1.0]  coverage/fill control
    float AltoCloudBrightness; // [0.1,4.0]  brightness multiplier
    float AltoFbmLacunarity;   // [1.5,4.0]  FBM frequency ratio per octave
    float AltoFbmGain;		 // [0.1,0.9]  FBM amplitude scaling per octave
    //--
    // Row 13 — Altocumulus cloud color
    float3 AltoCloudColor;	 // [0,1] per channel — RGB tint for cloud top/highlights
    float  AltoThickness;	  // [50,5000]  cloud slab thickness for Altocumulus
    //--
    // Row 14 — Altocumulus dark / shadow color + underside softness
    float3 AltoCloudColorDark; // [0,1] per channel — RGB tint for cloud base/shadow
    float  AltoBottomSoftness; // [0,1] 0=flat slab bottom, 1=organic irregular underside
    //--
    // Row 15 — Altocumulus sky-height redistribution
    // 0 = uniform distribution. (+) = more/larger toward horizon. (-) = more/larger toward zenith.
    float AltoZenithBias;   // [-1,1]  cloud distribution bias
    float AltoHtPad3;
    float AltoHtPad4;
    float AltoHtPad5;
    //--
    // Row 16 — Altocumulus sky-height modulation (continued)
    float AltoHeightBlendPower;   // [0.25,4]  exponent on the skyHeight ramp
    float AltoHtPad0;
    float AltoHtPad1;
    float AltoHtPad2;
    //--
    // Row 17 — Lightning enable + frequencies
    int   LightningEnabled;	  // 0 or 1
    float LightningStrikeFreq;   // [0,1]
    float LightningInternalFreq; // [0,1]
    float LightningPad;
    //--
    // Row 18 — Lightning speed + glow
    float  LightningSpeed;		// [0.5,10]
    float  LightningInternalSpeed;// [1,20]
    float  LightningGlowIntensity;// [0.5,10]
    float  LightningFlashIntensity;// [0.5,15]
    //--
    // Row 19 — Lightning bolt color + ambient
    float3 LightningBoltColor;	// [0,1] per channel
    float  LightningAmbientContrib;// [0,1]
    //--
    // Row 20 — Sun screen-space UV for cloud-coverage occlusion sampling.
    // (-1,-1) when no global lens flare is active or sun is behind/off-screen.
    float2 SunScreenUV;
    float2 SunOccPad;
};
