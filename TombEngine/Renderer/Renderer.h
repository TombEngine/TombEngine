#pragma once
#include <SimpleMath.h>
#include "Math/Math.h"
#include "Game/control/box.h"
#include "Game/items.h"
#include "Game/Animation/Animation.h"
#include "Game/Gui.h"
#include "Game/Hud/DrawItems/DrawItems.h"
#include "Game/Hud/Hud.h"
#include "Game/Hud/PickupSummary.h"
#include "Game/effects/effects.h"
#include "Game/effects/Electricity.h"
#include "Game/Setup.h"
#include "Specific/level.h"
#include "Specific/fast_vector.h"
#include "Renderer/Frustum.h"
#include "Renderer/RendererEnums.h"
#include "Renderer/RenderView.h"
#include "Renderer/Structures/RendererLight.h"
#include "Renderer/ConstantBuffers/HUDBarBuffer.h"
#include "Renderer/ConstantBuffers/HUDBuffer.h"
#include "Renderer/ConstantBuffers/ShadowLightBuffer.h"
#include "Renderer/ConstantBuffers/RoomBuffer.h"
#include "Renderer/ConstantBuffers/ItemBuffer.h"
#include "Renderer/ConstantBuffers/AnimatedBuffer.h"
#include "Renderer/ConstantBuffers/BlendingBuffer.h"
#include "Renderer/ConstantBuffers/CameraMatrixBuffer.h"
#include "Renderer/ConstantBuffers/MaterialBuffer.h"
#include "Renderer/ConstantBuffers/InstancedStaticBuffer.h"
#include "Renderer/ConstantBuffers/InstancedSpriteBuffer.h"
#include "Renderer/ConstantBuffers/PostProcessBuffer.h"
#include "Renderer/ConstantBuffers/SMAABuffer.h"
#include "Renderer/ConstantBuffers/SkyBuffer.h"
#include "Renderer/Structures/RendererBone.h"
#include "Renderer/Structures/RendererDoor.h"
#include "Renderer/Structures/RendererStringToDraw.h"
#include "Renderer/Structures/RendererRoom.h"
#include "Renderer/Structures/RendererSprite.h"
#include "Renderer/Structures/RendererAnimatedTexture.h"
#include "Renderer/Structures/RendererAnimatedTextureSet.h"
#include "Renderer/Graphics/Vertices/PostProcessVertex.h"
#include "Renderer/ShaderManager/ShaderManager.h"
#include "Renderer/Structures/RendererItem.h"
#include "Renderer/Structures/RendererEffect.h"
#include "Renderer/Structures/RendererLine3D.h"
#include "Renderer/Structures/RendererTriangle3D.h"
#include "Renderer/Structures/RendererMesh.h"
#include "Renderer/Structures/RendererSpriteSequence.h"
#include "Renderer/Structures/RendererSpriteBucket.h"
#include "Renderer/Structures/RendererLine2D.h"
#include "Renderer/Structures/RendererHudBar.h"
#include "Renderer/Structures/RendererRoomAmbientMap.h"
#include "Renderer/Structures/RendererObject.h"
#include "Renderer/Structures/RendererStar.h"
#include "Renderer/Graphics/IGraphicsDevice.h"

using namespace TEN::Animation;

enum GAME_OBJECT_ID : short;
enum class SphereSpaceType;
class EulerAngles;
struct AnimFrameInterpData;
struct CAMERA_INFO;

namespace TEN::Renderer
{
	using namespace TEN::Effects::Electricity;
	using namespace TEN::Gui;
	using namespace TEN::Hud;
	using namespace TEN::Renderer::ConstantBuffers;
	using namespace TEN::Renderer::Graphics;
	using namespace TEN::Renderer::Structures;
	using namespace TEN::Renderer::Utils;
	using namespace DirectX::SimpleMath;

	using AtlasTexturesSet = std::tuple<
		std::unique_ptr<ITexture2D>, 
		std::unique_ptr<ITexture2D>, 
		std::unique_ptr<ITexture2D>, 
		std::unique_ptr<ITexture2D>>;

	class Renderer
	{
	private:
		std::unique_ptr<IGraphicsDevice> _graphicsDevice;

		std::unique_ptr<IInputLayout> _vertexInputLayout;
		std::unique_ptr<IInputLayout> _fullScreenVertexInputLayout;

		// Adapter info
		AdapterInfo _adapterInfo = {};

		// Render targets

		std::unique_ptr<IRenderSurface2D> _normalsAndMaterialIndexRenderTarget;
		std::unique_ptr<IRenderSurface2D> _depthRenderTarget;
		std::unique_ptr<IRenderSurface2D> _emissiveAndRoughnessRenderTarget;
		std::unique_ptr<IRenderSurface2D> _dumpScreenRenderTarget;
		std::unique_ptr<IRenderSurface2D> _renderTarget;
		std::unique_ptr<IRenderSurface2D> _postProcessRenderTarget[2];
		std::unique_ptr<IRenderSurface2D> _glowRenderTarget[2];
		std::unique_ptr<IRenderSurface2D> _tempRoomAmbientRenderTarget1;
		std::unique_ptr<IRenderSurface2D> _tempRoomAmbientRenderTarget2;
		std::unique_ptr<IRenderSurface2D> _tempRoomAmbientRenderTarget3;
		std::unique_ptr<IRenderSurface2D> _tempRoomAmbientRenderTarget4;
		std::unique_ptr<IRenderSurface2D> _shadowMap;
		std::unique_ptr<IRenderSurface2D> _legacyReflectionsRenderTarget;
		std::unique_ptr<IRenderSurface2D> _SSAORenderTarget;
		std::unique_ptr<IRenderSurface2D> _SSAOBlurredRenderTarget;
		std::unique_ptr<IRenderSurface2D> _SMAASceneRenderTarget;
		std::unique_ptr<IRenderSurface2D> _SMAASceneSRGBRenderTarget;
		std::unique_ptr<IRenderSurface2D> _SMAADepthRenderTarget;
		std::unique_ptr<IRenderSurface2D> _SMAAEdgesRenderTarget;
		std::unique_ptr<IRenderSurface2D> _SMAABlendRenderTarget;
		std::unique_ptr<IRenderSurface2D> _skyboxRenderTarget;
		std::unique_ptr<IRenderSurface2D> _backBuffer;

		// Constant buffers

		RenderView _gameCamera;
		RenderView _oldGameCamera;
		RenderView _currentGameCamera;
		std::unique_ptr<IConstantBuffer> _cbCameraMatrices;
		CItemBuffer _stItem;
		std::unique_ptr<IConstantBuffer> _cbItem;
		CRoomBuffer _stRoom;
		std::unique_ptr<IConstantBuffer> _cbRoom;
		CAnimatedBuffer _stAnimated;
		std::unique_ptr<IConstantBuffer> _cbAnimated;
		CShadowLightBuffer _stShadowMap;
		std::unique_ptr<IConstantBuffer> _cbShadowMap;
		CHUDBuffer _stHUD;
		std::unique_ptr<IConstantBuffer> _cbHUD;
		CHUDBarBuffer _stHUDBar;
		std::unique_ptr<IConstantBuffer> _cbHUDBar;
		CPostProcessBuffer _stPostProcessBuffer;
		std::unique_ptr<IConstantBuffer> _cbPostProcessBuffer;
		CInstancedSpriteBuffer _stInstancedSpriteBuffer;
		std::unique_ptr<IConstantBuffer> _cbInstancedSpriteBuffer;
		CBlendingBuffer _stBlending;
		std::unique_ptr<IConstantBuffer> _cbBlending;
		CInstancedStaticMeshBuffer _stInstancedStaticMeshBuffer;
		std::unique_ptr<IConstantBuffer> _cbInstancedStaticMeshBuffer;
		CSMAABuffer _stSMAABuffer;
		std::unique_ptr<IConstantBuffer> _cbSMAABuffer;
		CSkyBuffer _stSky;
		std::unique_ptr<IConstantBuffer> _cbSky;
		CMaterialBuffer _stMaterial;
		std::unique_ptr<IConstantBuffer> _cbMaterial;

		// Primitive batches

		RendererViewport _viewport;
		RendererViewport _shadowMapViewport;

		// Text

		std::unique_ptr<ISpriteFont> _gameFont;
		std::vector<RendererStringToDraw> _stringsToDraw;
		Vector4 _blinkColorValue = Vector4::Zero;
		float _blinkTime = 0.0f;
		float _oldBlinkTime = 0.0f;

		// Sprites
		std::vector<Vertex> _spriteVertices;
		std::unique_ptr<IVertexBuffer> _spriteVertexBuffer;

		// Graphics resources

		std::unique_ptr<ITexture2D> _logo;
		std::unique_ptr<ITexture2D> _skyTexture;
		std::unique_ptr<ITexture2D> _whiteTexture;
		RendererSprite _whiteSprite;
		std::unique_ptr<ITexture2D> _loadingBarBorder;
		std::unique_ptr<ITexture2D> _loadingBarInner;
		std::unique_ptr<ITexture2D> _loadingScreenTexture;

		std::unique_ptr<IVertexBuffer> _roomsVertexBuffer;
		std::unique_ptr<IIndexBuffer> _roomsIndexBuffer;
		std::unique_ptr<IVertexBuffer> _moveablesVertexBuffer;
		std::unique_ptr<IIndexBuffer> _moveablesIndexBuffer;
		std::unique_ptr<IVertexBuffer> _staticsVertexBuffer;
		std::unique_ptr<IIndexBuffer> _staticsIndexBuffer;
		std::unique_ptr<IVertexBuffer> _skyVertexBuffer;
		std::unique_ptr<IIndexBuffer> _skyIndexBuffer;
		std::unique_ptr<IVertexBuffer> _quadVertexBuffer;

		std::vector<Vertex> _roomsVertices;
		std::vector<int> _roomsIndices;
		std::vector<Vertex> _moveablesVertices;
		std::vector<int> _moveablesIndices;
		std::vector<Vertex> _staticsVertices;
		std::vector<int> _staticsIndices;

		// Rooms and collector

		std::vector<RendererRoom> _rooms;
		bool _invalidateCache;
		std::vector<short> _visitedRoomsStack;

		// Lights

		int _dynamicLightList = 0;
		std::vector<RendererLight> _dynamicLights[2];
		RendererLight* _shadowLight;

		// Lines

		std::vector<RendererLine2D>		_lines2DToDraw = {};
		std::vector<RendererLine3D>		_lines3DToDraw = {};
		std::vector<RendererTriangle3D> _triangles3DToDraw = {};

		// Textures, objects and sprites

		std::vector<std::optional<RendererObject>>			   _moveableObjects;
		std::vector<std::optional<RendererObject>>			   _staticObjects; // Key = static ID, value = renderer object.
		std::vector<RendererSprite>							   _sprites;
		std::vector<RendererSpriteSequence>					   _spriteSequences;
		std::vector<RendererAnimatedTextureSet>				   _animatedTextureSets;
		std::vector<RendererMesh*>							   _meshes;
		std::vector<AtlasTexturesSet>						   _roomTextures;
		std::vector<AtlasTexturesSet>						   _animatedTextures;
		std::vector<AtlasTexturesSet>						   _moveablesTextures;
		std::vector<AtlasTexturesSet>						   _staticTextures;
		std::vector<std::unique_ptr<ITexture2D>>			   _spritesTextures;
		RendererSprite										   _videoSprite; // Video texture is an unique case

		Matrix _playerWorldMatrix;

		// Preallocated pools of objects for avoiding new/delete.
		// Items and effects are safe (can't be more than 1024 items in TR), 
		// lights should be oversized (eventually ignore lights more than MAX_LIGHTS)

		std::vector<RendererItem> _items;
		std::vector<RendererEffect> _effects;

		// Debug variables

		int _numDrawCalls = 0;

		int _numRoomsDrawCalls = 0;
		int _numSortedRoomsDrawCalls = 0;
		int _numMoveablesDrawCalls = 0;
		int _numSortedMoveablesDrawCalls = 0;
		int _numStaticsDrawCalls = 0;
		int _numInstancedStaticsDrawCalls = 0;
		int _numSortedStaticsDrawCalls = 0;
		int _numSpritesDrawCalls = 0;
		int _numInstancedSpritesDrawCalls = 0;
		int _numSortedSpritesDrawCalls = 0;

		int _numLinesDrawCalls = 0;

		int _numTriangles = 0;
		int _numSortedTriangles = 0;

		int _numShadowMapDrawCalls = 0;
		int _numDebrisDrawCalls = 0;
		int _numEffectsDrawCalls = 0;

		int _numDotProducts = 0;
		int _numCheckPortalCalls = 0;
		int _numGetVisibleRoomsCalls = 0;

		int _numConstantBufferUpdates = 0;

		int _numExecutedMaterialsUpdates = 0;
		int _numRequestedMaterialsUpdates = 0;

		float _currentLineHeight = 0.0f;

		RendererDebugPage _debugPage = RendererDebugPage::None;

		// Times for debug

		int _timeUpdate;
		int _timeRoomsCollector;
		int _timeDraw;
		int _timeFrame;
		float _fps;
		int _currentCausticsFrame;

		// Screen settings
		float _farView = DEFAULT_FAR_VIEW;
		bool _isWindowed;

		// A flag to prevent extra renderer object additions

		bool _isLocked = false;

		// Caching state changes

		ITextureBase* _lastTexture;
		BlendMode _lastBlendMode;
		DepthState _lastDepthState;
		CullMode _lastCullMode;
		int _lastMaterialIndex;

		std::vector<RendererSpriteBucket> _spriteBuckets;

		// Antialiasing

		std::unique_ptr<ITexture2D> _SMAAAreaTexture;
		std::unique_ptr<ITexture2D> _SMAASearchTexture;

		// Post-process

		PostProcessMode _postProcessMode = PostProcessMode::None;
		float _postProcessStrength = 1.0f;
		Vector3 _postProcessTint = (Vector3)NEUTRAL_COLOR;

		std::unique_ptr<IVertexBuffer> _fullscreenTriangleVertexBuffer;

		bool _doingFullscreenPass = false;

		// SSAO

		std::unique_ptr<ITexture2D> _SSAONoiseTexture;
		std::vector<Vector4> _SSAOKernel;

		// Special effects
		RendererMirror* _currentMirror = nullptr;

		// Transparency

		fast_vector<Vertex> _sortedPolygonsVertices;
		fast_vector<int> _sortedPolygonsIndices;
		std::unique_ptr<IVertexBuffer> _sortedPolygonsVertexBuffer;
		std::unique_ptr<IIndexBuffer> _sortedPolygonsIndexBuffer;

		// High framerate

		float _interpolationFactor = 0.0f;
		bool  _graphicsSettingsChanged = false;

		// Shader manager

		ShaderManager _shaders;

		std::unique_ptr<IPrimitiveBatch> _primitiveBatch;
		std::unique_ptr<ISpriteBatch> _spriteBatch;

		void ApplySMAA(IRenderSurface2D* renderTarget, RenderView& view);
		void ApplyFXAA(IRenderSurface2D* renderTarget, RenderView& view);
		void ApplyAntialiasing(IRenderSurface2D* renderTarget, RenderView& view);
		void BindTexture(TextureRegister registerType, ITextureBase* texture, SamplerStateRegister samplerType);
		int  BindLight(RendererLight& light, ShaderLight* lights, int index);
		void BindRoomLights(std::vector<RendererLight*>& lights);
		void BindInstancedStaticLights(std::vector<RendererLight*>& lights, int instanceID);
		void BindMoveableLights(std::vector<RendererLight*>& lights, int roomNumber, int prevRoomNumber, float fade, bool shadow);
		void BindRoomDecals(const std::vector<RendererDecal>& decals);
		void BindRenderTargetAsTexture(TextureRegister registerType, IRenderTarget2D* target, SamplerStateRegister samplerType);
		void BindConstantBuffer(ShaderStage shaderStage, ConstantBufferRegister constantBufferType, IConstantBuffer* buffer);
		void BindMaterial(int materialIndex, bool force);
		void BuildHierarchy(RendererObject* obj);
		void BuildHierarchyRecursive(RendererObject* obj, RendererBone* node, RendererBone* parentNode);
		void UpdateAnimation(RendererItem* item, RendererObject& obj, const KeyframeInterpolationData& interpData, int mask, bool useObjectWorldRotation = false);
		bool CheckPortal(short parentRoomNumber, RendererDoor* door, Vector4 viewPort, Vector4* clipPort, RenderView& renderView);
		void GetVisibleRooms(short from, short to, Vector4 viewPort, bool water, int count, bool onlyRooms, RenderView& renderView);
		void CollectMirrors(RenderView& renderView);
		void CollectRooms(RenderView& renderView, bool onlyRooms);
		void CollectItems(short roomNumber, RenderView& renderView);
		void CollectStatics(short roomNumber, RenderView& renderView);
		void CollectLights(const Vector3& pos, float radius, int roomNumber, int prevRoomNumber, bool prioritizeShadowLight, bool useCachedRoomLights, std::vector<RendererLightNode>* roomsLights, std::vector<RendererLight*>* outputLights);
		void CollectLightsForItem(RendererItem* item);
		void CollectLightsForEffect(short roomNumber, RendererEffect* effect);
		void CollectLightsForRoom(short roomNumber, RenderView& renderView);
		void CollectLightsForCamera();
		void CalculateLightFades(RendererItem* item);
		void CollectDecalsForRoom(short roomNumber, RenderView& renderView);
		void CollectEffects(short roomNumber);
		void ClearShadowMap();
		void CalculateSSAO(RenderView& view);
		void UpdateItemAnimations(RenderView& view);
		void InitializeScreen(int w, int h, bool reset);
		void InitializeCommonTextures();
		void InitializeGameBars();
		void InitializeMenuBars(int y);
		void InitializeSky();
		void DrawAllStrings();
		void PrepareDynamicLight(RendererLight& light);
		void PrepareLaserBarriers(RenderView& view);
		void PrepareSingleLaserBeam(RenderView& view);
		void DrawHorizonAndSky(IDepthTarget* depthTarget, RenderView& renderView, int arrayIndex = 0, bool reflectionPass = false);
		void DrawHorizonAndSkyForReflections(RenderView& renderView);
		void DrawRooms(RenderView& view, RendererPass rendererPass);
		void DrawItems(RenderView& view, RendererPass rendererPass, bool onlyPlayer = false);
		void DrawAnimatingItem(RendererItem* item, RenderView& view, RendererPass rendererPass);
		void DrawWaterfalls(RendererItem* item, RenderView& view, float speed, RendererPass rendererPass);
		void DrawBaddyGunflashes(RenderView& view);
		void DrawStatics(RenderView& view, RendererPass rendererPass);
		void DrawLara(RenderView& view, RendererPass rendererPass);
		void PrepareFires(RenderView& view);
		void PrepareParticles(RenderView& view);
		void PrepareSmokes(RenderView& view);
		void PrepareFireflies(RenderView& view);
		void PrepareElectricity(RenderView& view);
		void PrepareHelicalLasers(RenderView& view);
		void PrepareBlood(RenderView& view);
		void PrepareWeatherParticles(RenderView& view);
		void PrepareDrips(RenderView& view);
		void PrepareBubbles(RenderView& view);
		void DoRenderPass(RendererPass pass, RenderView& view, bool drawMirrors);
		void DrawObjects(RendererPass pass, RenderView& view, bool player, bool moveables, bool statics, bool sprites);
		void DrawEffects(RenderView& view, RendererPass rendererPass);
		void DrawEffect(RenderView& view, RendererEffect* effect, RendererPass rendererPass);
		void PrepareSplashes(RenderView& view);
		void DrawSprites(RenderView& view, RendererPass rendererPass);
		void DrawDisplaySprites(RenderView& view, bool negativePriority);
		void DrawDisplayItems();
		void DrawSortedFaces(RenderView& view);
		void DrawSingleSprite(RendererSortableObject* object, RendererObjectType lastObjectType, RenderView& view);
		void DrawRoomSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view);
		void DrawItemSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view);
		void DrawStaticSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view);
		void DrawSpriteSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view);
		void DrawMoveableAsStaticSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view);
		void DrawHairSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view, int index);
		void DrawLines2D();
		void DrawLines3D(RenderView& view);
		void DrawTriangles3D(RenderView& view);
		void DrawOverlays(RenderView& view);
		void PrepareRopes(RenderView& view);
		void DrawFishSwarm(RenderView& view, RendererPass rendererPass);
		void DrawBats(RenderView& view, RendererPass rendererPass);
		void DrawRats(RenderView& view, RendererPass rendererPass);
		void DrawScarabs(RenderView& view, RendererPass rendererPass);
		void DrawSpiders(RenderView& view, RendererPass rendererPass);
		bool DrawGunFlashes(RenderView& view);
		void DrawGunShells(RenderView& view, RendererPass rendererPass);
		void DrawLocusts(RenderView& view, RendererPass rendererPass);
		void DrawStatistics();
		void DrawExamines();
		void DrawDebris(RenderView& view, RendererPass rendererPass);
		void DrawFullScreenImage(ITextureBase* texture, float fade, IRenderTarget2D* target,
			IDepthTarget* depthTarget);
		void PrepareShockwaves(RenderView& view);
		void PrepareRipples(RenderView& view);
		void PrepareUnderwaterBloodParticles(RenderView& view);
		void DrawFullScreenQuad(ITextureBase* texture, Vector3 color, bool fit = true, float customAspect = 0.0f);
		void DrawFullScreenSprite(RendererSprite* sprite, DirectX::SimpleMath::Vector3 color, bool fit = true);
		void PrepareSmokeParticles(RenderView& view);
		void PrepareSparkParticles(RenderView& view);
		void PrepareExplosionParticles(RenderView& view);
		void DrawLaraHolsters(RendererItem* itemToDraw, RendererRoom* room, RenderView& view, RendererPass rendererPass);
		void DrawLaraJoints(RendererItem* itemToDraw, RendererRoom* room, RenderView& view, RendererPass rendererPass);
		void DrawLaraHair(RendererItem* itemToDraw, RendererRoom* room, RenderView& view, RendererPass rendererPass);
		void DrawMesh(RendererItem* itemToDraw, RendererMesh* mesh, RendererObjectType type, int boneIndex, bool skinned, RenderView& view, RendererPass rendererPass);
		void PrepareSimpleParticles(RenderView& view);
		void PrepareStreamers(RenderView& view);
		void PrepareFootprints(RenderView& view);
		void DrawLoadingBar(float percent);
		void DrawPostprocess(IRenderSurface2D* renderTarget, RenderView& view, SceneRenderMode renderMode);
		void RenderInventoryScene(IRenderSurface2D* renderTarget, ITextureBase* background, float backgroundFade);
		void RenderTitleMenu(Menu menu);
		void RenderPauseMenu(Menu menu);
		void RenderLoadSaveMenu();
		void RenderOptionsMenu(Menu menu, int initialY);
		void RenderNewInventory();
		void RenderBlobShadows(RenderView& renderView);
		void RenderShadowMap(RendererItem* item, RenderView& view);
		void RenderItemShadows(RenderView& renderView);
		void SetBlendMode(BlendMode blendMode, bool force = false);
		void SetDepthState(DepthState depthState, bool force = false);
		void SetCullMode(CullMode cullMode, bool force = false);
		void SetAlphaTest(AlphaTestMode mode, float threshold, bool force = false);
		void SetScissor(RendererRectangle rectangle);
		bool SetupBlendModeAndAlphaTest(BlendMode blendMode, RendererPass rendererPass, int drawPass);
		void SortAndPrepareSprites(RenderView& view);
		void SortTransparentFaces(RenderView& view);
		void ResetItems();
		void ResetScissor();
		void ResetDebugVariables();
		float CalculateFrameRate();
		void InterpolateCamera(float interpFactor);
		void CopyRenderTarget(IRenderSurface2D* source, IRenderSurface2D* dest, RenderView& view);
		void CopyRenderTargetAndDownscale(IRenderSurface2D* source, IRenderSurface2D* dest, float factor, RenderView& view);
		void BindBucketTextures(const RendererBucket& bucket, TextureSource textureSource, bool animated);
		void BindAtlasTextures(const RendererBucket& bucket, TextureSource textureSource);
		void PackSpriteTextureCoordinates(int instanceId, RendererSprite* sprite);
		void ApplyGlow(IRenderSurface2D* renderTarget, RenderView& view);

		void AddSpriteBillboard(RendererSprite* sprite, const Vector3& pos, const Vector4& color, float orient2D, float scale,
			Vector2 size, BlendMode blendMode, bool isSoftParticle, RenderView& view, SpriteRenderType renderType = SpriteRenderType::Default);
		void AddSpriteBillboardConstrained(RendererSprite* sprite, const Vector3& pos, const Vector4& color, float orient2D,
			float scale, Vector2 size, BlendMode blendMode, const Vector3& constrainAxis,
			bool isSoftParticle, RenderView& view, SpriteRenderType renderType = SpriteRenderType::Default);
		void AddSpriteBillboardConstrainedLookAt(RendererSprite* sprite, const Vector3& pos, const Vector4& color, float orient2D,
			float scale, Vector2 size, BlendMode blendMode, const Vector3& lookAtAxis,
			bool isSoftParticle, RenderView& view, SpriteRenderType renderType = SpriteRenderType::Default);
		void AddQuad(RendererSprite* sprite, const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2, const Vector3& vertex3,
			const Vector4 color, float orient2D, float scale, Vector2 size, BlendMode blendMode, bool softParticles,
			RenderView& view);
		void AddQuad(RendererSprite* sprite, const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2, const Vector3& vertex3,
			const Vector4& color0, const Vector4& color1, const Vector4& color2, const Vector4& color3, float orient2D,
			float scale, Vector2 size, BlendMode blendMode, bool isSoftParticle, RenderView& view, SpriteRenderType renderType = SpriteRenderType::Default);
		void AddColoredQuad(const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2, const Vector3& vertex3,
			const Vector4& color, BlendMode blendMode, RenderView& view);
		void AddColoredQuad(const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2, const Vector3& vertex3,
			const Vector4& color0, const Vector4& color1, const Vector4& color2, const Vector4& color3,
			BlendMode blendMode, RenderView& view, SpriteRenderType renderType = SpriteRenderType::Default);

		Matrix GetWorldMatrixForSprite(const RendererSpriteToDraw& sprite, RenderView& view);
		RendererObject& GetRendererObject(GAME_OBJECT_ID id);
		RendererMesh* GetMesh(int meshIndex);
		Vector4 GetPortalRect(Vector4 v, Vector4 vp);
		bool SphereBoxIntersection(BoundingBox box, Vector3 sphereCentre, float sphereRadius);
		void InitializeSpriteQuad();
		void InitializePostProcess();
		void CreateSSAONoiseTexture();
		void InitializeSMAA();
		void SetupAnimatedTextures(const RendererBucket& bucket);
		std::unique_ptr<ITexture2D> CreateDefaultTexture(std::vector<unsigned char> color);
		std::optional<Vector2> ProjectDisplayItemPointToScreen(const Vector3& worldPos) const;
		bool IsRoomReflected(RenderView& renderView, int roomNumber);

		inline bool IgnoreReflectionPassForRoom(int roomNumber)
		{
			return (_currentMirror != nullptr && roomNumber != _currentMirror->RoomNumber);
		}

		inline void ReflectVectorOptionally(Vector3& vector)
		{
			if (_currentMirror == nullptr)
				return;

			vector = Vector3::Transform(vector, _currentMirror->ReflectionMatrix);
		}

		inline void ReflectMatrixOptionally(Matrix& matrix)
		{
			if (_currentMirror == nullptr)
				return;

			matrix = matrix * _currentMirror->ReflectionMatrix;
		}

		inline void DrawIndexedTriangles(int count, int baseIndex, int baseVertex)
		{
			_graphicsDevice->DrawIndexedTriangles(count, baseIndex, baseVertex);
			_numTriangles += count / 3;
			_numDrawCalls++;
		}

		inline void DrawIndexedInstancedTriangles(int count, int instances, int baseIndex, int baseVertex)
		{
			_graphicsDevice->DrawIndexedInstancedTriangles(count, instances, baseIndex, baseVertex);
			_numTriangles += (count / 3 * instances) * (count % 4 == 0 ? 2 : 1);
			_numDrawCalls++;
		}

		inline void DrawInstancedTriangles(int count, int instances, int baseVertex)
		{
			_graphicsDevice->DrawInstancedTriangles(count, instances, baseVertex);
			_numTriangles += (count / 3 * instances) * (count % 4 == 0 ? 2 : 1);
			_numDrawCalls++;
		}

		inline void DrawTriangles(int count, int baseVertex)
		{
			_graphicsDevice->DrawTriangles(count, baseVertex);
			_numTriangles += count / 3;
			_numDrawCalls++;
		}

		inline void UpdateConstantBuffer(void* data, IConstantBuffer* cb) noexcept
		{
			_graphicsDevice->UpdateConstantBuffer(cb, data);
			_numConstantBufferUpdates++;
		}

		template <typename CBuff>
		std::unique_ptr<IConstantBuffer> CreateConstantBuffer()
		{
			return std::move(_graphicsDevice->CreateConstantBuffer(sizeof(CBuff), L"")); //TEN::Utils::ToString( typeid(CBuff).name()
		}

		static inline bool IsSortedBlendMode(BlendMode blendMode)
		{
			return !(blendMode == BlendMode::Opaque ||
				blendMode == BlendMode::AlphaTest ||
				blendMode == BlendMode::Additive ||
				blendMode == BlendMode::FastAlphaBlend);
		}

		static inline BlendMode GetBlendModeFromAlpha(BlendMode blendMode, float alpha)
		{
			if (alpha < ALPHA_BLEND_THRESHOLD &&
				(blendMode == BlendMode::Opaque || blendMode == BlendMode::AlphaTest || blendMode == BlendMode::FastAlphaBlend))
			{
				return BlendMode::AlphaBlend;
			}

			return blendMode;
		}

		inline RendererObject& GetStaticRendererObject(short objectNumber)
		{
			return _staticObjects[Statics.GetIndex(objectNumber)].value();
		}

		inline void TexturesAreNotAnimated()
		{
			if (_stAnimated.Animated == 0)
				return;
			_stAnimated.Animated = 0;
			UpdateConstantBuffer(&_stAnimated, _cbAnimated.get());
		}

		static inline bool IsWaterfall(short objectNumber)
		{
			return (objectNumber >= ID_WATERFALL1 && objectNumber <= ID_WATERFALLSS2);
		}

		static inline unsigned int PackEffectsAndIndexInPoly(Vector3 effects, float sheen, int indexInPoly)
		{
			// Clamp values to 254 (UCHAR_MAX - 1) to avoid overflow during back conversion in shaders.

			int packed =
				((int)floor(effects.x * (UCHAR_MAX - 1)) << GLOW_VERTEX_SHIFT) |
				((int)floor(effects.y * (UCHAR_MAX - 1)) << MOVE_VERTEX_SHIFT) |
				((int)floor(sheen     * (UCHAR_MAX - 1)) << SHININESS_VERTEX_SHIFT) |
				((int)effects.z << LOCKED_VERTEX_SHIFT) |
				(indexInPoly << INDEX_IN_POLY_VERTEX_SHIFT);

			return packed;
		}

		static inline unsigned int PackVector3(Vector3 n)
		{
			if (n.Length() > EPSILON)
				n.Normalize();

			auto ToS8 = [](float v) -> unsigned int
			{
				float x = std::clamp(v, -1.0f, 1.0f) * CHAR_MAX;
				return (char)(std::lround(x));
			};

			const unsigned char X = (unsigned char)(ToS8(n.x));
			const unsigned char Y = (unsigned char)(ToS8(n.y));
			const unsigned char Z = (unsigned char)(ToS8(n.z));
			const unsigned char W = (unsigned char)(ToS8(0.0f));

			// Little-endian: memoria [R][G][B][A], come DXGI_FORMAT_R8G8B8A8_SNORM
			return (unsigned int)X | ((unsigned int)Y << 8) | ((unsigned int)Z << 16) | ((unsigned int)W << 24);
		}

		static inline unsigned int PackAnimationFrameOffsetIndexHash(int frameOffset, int meshIndex, int hash)
		{
			int packed =
				((hash & 0xFF) << 0) |
				((meshIndex & 0xFFFF) << 8) | 
				((frameOffset & 0xFF) << 24);
			return packed;
		}

		static inline int GetOriginalIndex(unsigned int v)
		{
			return ((v >> 8) & 0xFFFF);
		}

	public:
		Renderer();
		~Renderer();

		RendererMesh* GetRendererMeshFromTrMesh(RendererObject* obj, MESH* meshPtr, short boneIndex, int isJoints, int isHairs, int* lastVertex, int* lastIndex);
		void DrawBar(float percent, const RendererHudBar& bar, GAME_OBJECT_ID textureSlot, int frame, bool poison);
		void Create();
		void Initialize(const std::string& gameDir, int w, int h, bool windowed);
		void ReloadShaders(bool recompileAAShaders = false);
		void Render(float interpFactor);
		void RenderTitle(float interpFactor);
		void Lock();
		bool PrepareDataForTheRenderer();
		void UpdateCameraMatrices(CAMERA_INFO* cam, float farView);
		void RenderSimpleSceneToParaboloid(IRenderTarget2D* renderTarget, Vector3 position, int hemisphere);
		void DumpGameScene(SceneRenderMode renderMode = SceneRenderMode::Full);
		void RenderInventory();
		void RenderScene(IRenderSurface2D* renderTarget, RenderView& view, SceneRenderMode renderMode = SceneRenderMode::Full);
		void PrepareScene();
		void ClearScene();
		void SaveScreenshot();
		void DrawDisplayPickup(const DisplayPickup& pickup);
		int  Synchronize();
		void AddString(int x, int y, const std::string& string, unsigned int color, int flags);
		void AddString(const std::string& string, const Vector2& pos, const Color& color, float scale, int flags);
		void AddString(const std::string& string, const Vector2& pos, const Vector2& area, const Color& color, float scale, int flags);
		void AddString(const std::string& string, const Vector2& currentPos, const Vector2& prevPos, const Vector2& area, const Color& color, float scale, int flags);
		void AddDebugString(const std::string& string, const Vector2& pos, const Color& color, float scale, RendererDebugPage page = RendererDebugPage::None);
		void FreeRendererData();
		void AddDynamicPointLight(const Vector3& pos, float radius, const Color& color, bool castShadows, int hash = 0);
		void AddDynamicFogBulb(const Vector3& pos, float radius, float density, const Color& color, int hash = 0);
		void AddDynamicSpotLight(const Vector3& pos, const Vector3& dir, float radius, float falloff, float distance, const Color& color, bool castShadows, int hash = 0);
		void RenderLoadingScreen(float percentage);
		void RenderFreezeMode(float interpFactor, bool staticBackground);
		void RenderFullScreenTexture(ITextureBase* texture, float aspect);
		void UpdateVideoTexture(ITexture2D* texture);
		void UpdateProgress(float value);
		void ToggleFullScreen(bool force = false);
		void SetFullScreen();
		bool IsFullsScreen();
		void RenderTitleImage();

		void AddLine2D(const Vector2& origin, const Vector2& target, const Color& color, RendererDebugPage page = RendererDebugPage::None);

		void AddDebugLine(const Vector3& origin, const Vector3& target, const Color& color, RendererDebugPage page = RendererDebugPage::None);
		void AddDebugTriangle(const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2, const Color& color, RendererDebugPage page = RendererDebugPage::None);
		void AddDebugTarget(const Vector3& center, const Quaternion& orient, float radius, const Color& color, RendererDebugPage page = RendererDebugPage::None);
		void AddDebugBox(const std::array<Vector3, BOX_VERTEX_COUNT>& corners, const Color& color, RendererDebugPage page = RendererDebugPage::None, bool isWireframe = true);
		void AddDebugBox(const Vector3& min, const Vector3& max, const Color& color, RendererDebugPage page = RendererDebugPage::None, bool isWireframe = true);
		void AddDebugBox(const BoundingOrientedBox& box, const Color& color, RendererDebugPage page = RendererDebugPage::None, bool isWireframe = true);
		void AddDebugBox(const BoundingBox& box, const Color& color, RendererDebugPage page = RendererDebugPage::None, bool isWireframe = true);
		void AddDebugCone(const Vector3& center, const Quaternion& orient, float radius, float length, const Vector4& color, RendererDebugPage page, bool isWireframe = true);
		void AddDebugCylinder(const Vector3& center, const Quaternion& orient, float radius, float length, const Color& color, RendererDebugPage page = RendererDebugPage::None, bool isWireframe = true);
		void AddDebugSphere(const Vector3& center, float radius, const Color& color, RendererDebugPage page = RendererDebugPage::None, bool isWireframe = true);
		void AddDebugSphere(const BoundingSphere& sphere, const Color& color, RendererDebugPage page = RendererDebugPage::None, bool isWireframe = true);

		void PrintDebugMessage(const char* msg, va_list args);
		void PrintDebugMessage(const char* msg, ...);
		void DrawDebugInfo(RenderView& view);
		void DrawDebugRenderTargets(RenderView& view);
		void SwitchDebugPage(bool goBack);
		RendererDebugPage GetCurrentDebugPage();

		void ChangeScreenResolution(int width, int height, bool windowed);
		void FlipRooms(short roomNumber1, short roomNumber2);
		void UpdateLaraAnimations(bool force);
		void UpdateItemAnimations(int itemNumber, bool force);
		std::vector<BoundingSphere> GetSpheres(int itemNumber);
		void GetBoneMatrix(short itemNumber, int jointIndex, Matrix* outMatrix);
		SkinningMode GetSkinningMode(const RendererObject& obj, int skinIndex);
		void DrawObjectIn2DSpace(int objectNumber, Vector2 pos2D, EulerAngles orient, float scale1, float opacity = 1.0f, int meshBits = NO_JOINT_BITS);
		void DrawObjectIn3DSpace(const DisplayItem& item);
		void SetLoadingScreen(std::wstring& fileName);
		std::unique_ptr<ITexture2D> SetTextureOrDefault(std::wstring path);
		std::string GetDefaultAdapterName();
		const AdapterInfo& GetAdapterInfo() const;
		void SaveOldState();

		float						GetFramerateMultiplier() const;
		float						GetInterpolationFactor(bool forceRawValue = false) const;
		Vector2i					GetScreenResolution() const;
		int							GetScreenRefreshRate() const;
		std::optional<Vector2>		Get2DPosition(const Vector3& pos) const;
		std::pair<Vector3, Vector3> GetRay(const Vector2& pos) const;
		std::optional<std::pair<Vector2, Vector2>> GetDisplayItemBounds(const DisplayItem& item) const;
		Vector3	   GetMoveableBonePosition(int itemNumber, int boneID, const Vector3& relOffset = Vector3::Zero);
		Quaternion GetMoveableBoneOrientation(int itemNumber, int boneID);

		void AddDisplaySprite(const RendererSprite& sprite, const Vector2& pos2D, short orient, const Vector2& size, const Vector4& color,
			int priority, BlendMode blendMode, const Vector2& aspectCorrection, RenderView& renderView);
		void CollectDisplaySprites(RenderView& renderView);

		PostProcessMode	GetPostProcessMode();
		void			SetPostProcessMode(PostProcessMode mode);
		float			GetPostProcessStrength();
		void			SetPostProcessStrength(float strength);
		Vector3			GetPostProcessTint();
		void			SetPostProcessTint(Vector3 color);

		void SetGraphicsSettingsChanged();

		RendererDebugPage GetDebugPage() const;
	};

	extern Renderer g_Renderer;
}