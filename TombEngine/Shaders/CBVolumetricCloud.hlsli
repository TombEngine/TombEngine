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
    int   CloudType;	  // 0=None, 1=AltocumulusMid, 2=Aurora
    float CloudCompositeScale; // Composite alpha multiplier: 1.0 = normal, <1.0 = bleed-through pass.
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
    float  AltoThickness;		  // [50,15000] cloud slab thickness for Altocumulus
    //--
    // Row 14 — Altocumulus dark / shadow color + underside softness
    float3 AltoCloudColorDark; // [0,1] per channel — RGB tint for cloud base/shadow
    float  AltoBottomSoftness; // [0,1] 0=flat slab bottom, 1=organic irregular underside
    //--
    // Row 15 — Altocumulus sky-height redistribution
    // 0 = uniform distribution. (+) = more/larger toward horizon. (-) = more/larger toward zenith.
    float AltoZenithBias;   // [-1,1]  cloud distribution bias
    float AltoHorizonWidth; // [0,1]  0=wide (to near horizon), 1=zenith-only cap
    float AltoBleedDepth;  // [0,100] bleed clouds depth (0.01*val*CloudBottomHeight)
    float AltoHtPad5;
    //--
    // Row 16 — Altocumulus sky-height modulation + sun elevation
    float AltoHeightBlendPower;   // [0.25,4]  exponent on the skyHeight ramp
    float CloudSunElevation;      // sin(pitch): +1 = zenith, 0 = horizon, -1 = nadir.
    float CloudNightAmbient;      // [0,0.5] minimum ambient when sun is below horizon.
    float CloudTwilightAmbient;   // [0,1] ambient contribution during twilight.
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
    // Row 20 — Sun screen-space UV + bolt scale params.
    // (-1,-1) when no global lens flare is active or sun is behind/off-screen.
    float2 SunScreenUV;
    float  LightningBoltLengthScale;    // [0.1,5] bolt length multiplier
    float  LightningBoltThicknessScale; // [0.1,5] bolt radius multiplier
    //--
    // Row 21 — Atmospheric sun-lighting multipliers.
    float CloudSunLightIntensity;       // [0,5]   direct sun brightness multiplier.
    float CloudAmbientIntensity;        // [0,2]   ambient light multiplier.
    float CloudSilverliningStrength;    // [0,3]   silverlining / forward-scatter boost.
    float CloudForwardScatterStrength;  // [0,3]   broad HG scatter strength.
    //--
    // Row 22
    float CloudLightAbsorption;         // [0.1,5] Beer-Lambert absorption exponent (standard clouds).
    float CloudSunWarmthInfluence;      // [0,1]   sun color warmth blend.
    float CloudIsBleedPass;             // 0 = normal composite, 1 = bleed-through-mountains pass (un-fades horizon).
    float DriftOutProgress;             // [0,1] wind-directional dissolution progress (0 = normal, 1 = fully dissolved).
    //--
    // Row 23 — Compositor hybrid-blend thresholds
    float BlendThresholdHigh;           // [0,1]     luminance above which screen blend starts (bright side).
    float BlendThresholdHighWidth;      // [0.005,0.4] half-width of bright→alpha smoothstep.
    float BlendThresholdLow;            // [0,1]     luminance below which screen blend starts (dark side).
    float CloudMoonLightFactor;         // [0,1]     moonlight direct illumination factor (0 = day/no moon, >0 = moon contributing).
    //--
    // Row 24 — Sunset underside lighting
    float3 SunsetUndersideColor;        // Pre-computed sunset color (yellow→orange→red→magenta gradient).
    float  SunsetUndersideIntensity;    // [0,1] overall sunset underside activation (0 = off, 1 = full).
    //--
    // Row 25 — Sunset underside parameters
    float  SunsetUndersideSpread;       // [0.5,4] angular spread of sunset glow around sun direction.
    float  SunsetUndersideHeightFade;   // [0.5,4] exponent controlling how quickly glow fades from base to top.
    float  SunsetPad0;
    float  SunsetPad1;
};
