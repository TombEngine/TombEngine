# TombEngine Renderer - Piano per il nuovo renderer

## Priorita' e vincoli

1. **Prestazioni ottimali.** L'architettura e' progettata intorno al costo minimo per frame, non adattata dopo.
2. **Modernita' senza complessita' inutile.** Pipeline pre-compilate, render pass, command buffer, shader compilati offline, dati della scena persistenti in GPU, draw tramite indice d'istanza, instancing ovunque. Le tecniche piu' complesse (culling su GPU, occlusione Hi-Z, illuminazione clustered) restano fuori finche' le misure non le giustificano.
3. **Backend.** SDL3 GPU con **Vulkan come default** (D3D12 e Metal come effetto collaterale). DX11 resta e implementa la **stessa** astrazione e lo **stesso** renderer.
4. **Shader Model 5.0 ovunque.** Un solo sorgente HLSL, limitato alle feature SM 5.0: FXC per DX11, DXC/shadercross per SPIR-V e DXIL. Niente wave intrinsics, niente tipi a 16 bit.
5. **Formato dei livelli invariato.** I dati ottimizzati per la GPU si costruiscono al caricamento a partire dal formato attuale. Nessuna modifica a TombLib o Tomb Editor in questa fase.
6. **Debug.** Ogni pass, risorsa e pipeline ha un nome leggibile in RenderDoc fin dal primo giorno.

Il renderer attuale riceve solo bug fix: il resto del lavoro va nel renderer nuovo. Il look classico deve restare identico di default.

---

## 1. Obiettivi misurabili (budget)

| Metrica | Oggi (stima) | Obiettivo |
|---|---|---|
| Draw call CPU per frame (livello tipico) | 2000-2500 | < 300 (instancing + texture array + sort per stato) |
| Attraversamenti della scena per frame | 5 | 1 (piu' 1 per vista d'ombra) |
| Upload di constant buffer per draw | 1-3, fino a 64 KB | 0 (solo indice d'istanza o push di pochi byte) |
| Byte caricati in GPU per frame | 10-20 MB (develop_performance: molto meno) | < 1 MB in gameplay (solo dati dinamici) |
| Allocazioni heap per frame nel renderer | decine/centinaia | 0 |
| Compilazioni di shader/pipeline a runtime | si' (prima esecuzione) | 0 in release |
| CPU render thread, livello tipico (release) | da misurare | < 2 ms |
| Trasparenti: CPU per sort + submit, 13.000 poligoni | 0.07 ms solo sort (develop_performance) | < 0.3 ms totale |
| Caricamento livello | da misurare | non peggiore di oggi nonostante la preparazione dei dati |

I valori "oggi" vanno confermati con la baseline (Fase 0).

---

## 1b. Risultati della baseline (Release, 1728x1083)

Dettagli in `Baseline.md`.
- **Correzione:** le prime misure erano sulla GPU integrata AMD, non sulla RTX 3050. Il renderer legacy creava il device sull'adapter 0, che sui portatili ibridi e' l'integrata, e l'export `NvOptimusEnablement` non veniva rispettato. Ora (`3cd8e79f4`) chiede esplicitamente l'adapter ad alte prestazioni: il frame GPU passa da 30-36 ms a 3,3-3,5 ms.
- **Sulla GPU integrata** il renderer e' limitato dalla GPU e la SSAO prende oltre meta' del frame (10,8-13,3 ms dopo il commit di `develop_ssao_performance`).
- **Sulla RTX 3050** il renderer legacy e' limitato dalla CPU: circa 3 ms per frame, di cui 0,3 ms per le facce ordinate. GPU: SSAO 0,9-1,1 ms, opachi 0,7-0,85 ms, post-process 0,7 ms, trasparenti 0,3-0,4 ms.
- Conseguenza per l'ordine di lavoro: costo per pixel basso (SSAO a mezza risoluzione, post-process, overdraw) per le GPU integrate, e CPU per frame bassa per le dedicate.

## 2. Stato attuale (sintesi)

### 2.1 Frame
- Ordine: CollectRooms -> shadow cube per item -> ~24 Prepare* di sprite/effetti -> cielo, nuvole, aurora, god ray -> GBuffer (4 MRT) -> SSAO -> Opaque -> Additive -> Distortion -> CollectTransparentFaces -> sort -> Transparent -> GunFlashes -> DustStorm -> post-process -> HUD.
- La geometria viene attraversata 5 volte; con gli specchi, gli oggetti vengono ridisegnati per ogni specchio, in ogni pass.

### 2.2 Costi principali
1. `CObjectsBuffer` da 64.6 KB (32 ossa + 100 oggetti con 8 luci ciascuno), caricato per ogni item, effetto, run ordinato e faccia d'ombra. develop_performance ora copia solo la parte usata, ma la struttura resta.
2. Churn di stato per bucket: 4 SRV, materiale, PerDraw. Shader e buffer ri-bindati senza cache.
3. Shadow cube: 6 facce x tutte le mesh, senza culling per faccia e senza `MeshBits`.
4. Allocazioni per frame: liste di luci per oggetto, `GetSpheres()`, `std::map` delle statiche, scansione di tutte le luci per i fog bulb.
5. Sprite: matrici ricalcolate a ogni pass, CB da 64 KB per bucket.

### 2.3 Astrazione e shader
- `IGraphicsDevice` e' un wrapper immediate-mode con la forma di D3D11: nessuna pipeline, nessun render pass con load/store, nessun command buffer, nessun debug marker. Tutti i buffer sono `DYNAMIC`.
- SDL **3.4.8** con `SDL_gpu.h` e' gia' in `Libs/sdl3`. Mancano DXC e SDL_shadercross.
- Shader compilati a runtime con FXC; cache `.cso` che ignora gli `.hlsli`. Sintassi effect in SMAA. Registri CB b9, b10, b12 e b13 condivisi tra buffer diversi. 6 sampler fissi, mentre SDL GPU usa coppie texture+sampler.

### 2.4 Asset (formato attuale, che resta)
- Il compilatore (TombLib) fornisce gia': bucket per {texture, blend, materiale, animato}, poligoni con indici, UV, normali e tangenti per angolo, normale di faccia, effetti per vertice, ossa e pesi, texture come blob PNG o DDS, sequenze di texture animate.
- Oggi il motore al caricamento de-indicizza tutto (un vertice per angolo di poligono), impacchetta i vertici a 52 B, ricalcola normali di faccia e hash, salda pelle e capelli di Lara in O(n^2), decodifica i PNG e genera i mip senza compressione. Tre coppie VB/IB (stanze, moveable, statiche), tutte `DYNAMIC`.

### 2.5 Gia' fatto in develop_2.1 (da develop_performance)
- Upload parziale dei CB, PerDraw pigro, niente upload per item senza bucket nel pass, contatori di byte, timer GPU DX11 (solo SSAO), SSAO ottimizzato, GBuffer front-to-back, niente campionamento d'ombra per chi non riceve ombre.
- **Trasparenza riscritta** (`RendererSorted.cpp`, `TombEngine/Renderer/TransparentSorting.md`): un oggetto per bucket, entry da 16 B per poligono, radix sort, cluster con union-find solo per gli oggetti sovrapposti.

Nel renderer nuovo questi interventi vengono superati dall'architettura; **l'algoritmo di ordinamento dei trasparenti invece si riusa** (vedi Fase 5).

### 2.6 Bug noti da correggere subito (anche nel renderer attuale)
- [x] `RendererFrame.cpp:475`: l'operatore virgola ignora `!inFrustum` nel ciclo delle sfere.
- [x] `RendererFrame.cpp:529-530`: `InterpolatedRotation` e `InterpolatedScale` interpolano da se stessi invece che da `Prev*` (anche per gli effetti, riga 583).
- [x] Le shadow map ignorano `MeshBits`.
- [x] Marker di conflitto rotti (`<<<<<< <`) dentro `#ifdef PARABOLOID` in `RendererDraw.cpp`.
- [x] `DX11GraphicsDevice::BindShader` con `forceNull` azzera sempre il VS.
- [x] Leak del blob del PS in `DX11GraphicsDevice::CreateShader`.
- [x] `DX11VertexBuffer::Update` applica `startVertex` alla sorgente invece che alla destinazione. *(non si manifesta: tutte le chiamate usano `startVertex = 0`; il nuovo RHI lo sostituisce)*
- [x] Centro del bucket delle stanze diviso per `NumIndices` invece che per il numero di vertici.
- [x] Distanza dei trasparenti degli item da `Camera.pos` globale invece che da `view.Camera`.
- [x] Bucket mesh `FastAlphaBlend` che sembrano non arrivare mai al color buffer. *(non e' un bug: il compilatore di Tomb Editor non genera mai il valore 12, e' solo interno al motore)*

---

## 3. Architettura target

```
Gioco (logica a 30 FPS + interpolazione)
  |
  v
[Extract]   Snapshot della scena per il frame: trasformazioni interpolate, ossa, luci, sprite, effetti.
  |
  v
[Prepare]   Parallelo (g_Parallel), tutto in memoria per frame:
            portal culling + frustum culling (CPU, come oggi) -> draw packet con sort key ->
            istanze, ossa, luci, sprite, indici ordinati scritti nel ring di upload.
  |
  v
[Pass list] Pass con nome, eseguiti in ordine fisso:
            Upload (un solo copy pass) -> Shadow -> Depth/Normals -> SSAO -> Opaque -> Sky ->
            Additive/Distortion -> Trasparenti -> Post -> UI
  |
  v
[RHI]       Pipeline, RenderPass, CommandBuffer, CopyPass (+ ComputePass per il post-process)
  |                       |
  v                       v
SDL3 GPU (Vulkan / D3D12)      DX11 (stessa API, stesso renderer)
```

Principi:
- **Dati della scena persistenti in GPU.** Geometria, materiali, istanze delle statiche e dati delle stanze vengono caricati una volta al caricamento del livello. Per frame si caricano solo i dati che cambiano: trasformazioni e ossa degli oggetti animati, luci dinamiche, sprite, particelle.
- **Draw tramite indice d'istanza.** Ogni draw legge i propri dati (`world`, materiale, offset delle ossa, lista di luci) da storage buffer tramite un indice. Niente constant buffer per draw.
- **Culling sulla CPU.** Il portal culling attuale e' gia' efficace; si aggiunge solo un frustum culling per istanza fatto bene e senza allocazioni.
- **Pochi draw grazie allo stato condiviso.** Texture array per categoria e instancing permettono di raggruppare molti bucket nello stesso draw.
- **Pipeline pre-compilate**, create tutte al caricamento.
- **Zero allocazioni per frame**: arene lineari per frame, container riutilizzati.

---

## 4. Fase 0 - Baseline e bug (breve, propedeutica)

- [x] Correggere i bug del punto 2.6 (un commit ciascuno).
- [x] Benchmark ripetibile: 3-4 livelli (interno denso, esterno con cielo e nuvole, molti trasparenti, molti item) con un percorso di camera scriptato tramite l'harness headless (`.prj2` compilato con TombLib + script Lua). Output CSV: CPU ms per fase, GPU ms per pass (timestamp DX11), draw call, byte caricati, tempo di caricamento del livello. *(modalita' `benchmark <secondi>`, livelli tut1_water, Karnak e Coastal; vedi `Baseline.md`)*
- [x] Salvare la baseline in `Documentation/Renderer/Baseline.md`. Ogni fase successiva si confronta con questa.
- [x] Congelare il renderer attuale: solo bug fix fino alla sostituzione. *(in vigore: il legacy riceve solo gli agganci per il renderer nuovo e le correzioni condivise del frontend)*

---

## 5. Fase 1 - RHI moderna e backend (fondamenta)

### 5.1 Astrazione (`Renderer/RHI/`)
- [x] **Device**: creazione di buffer, texture, sampler, shader, pipeline; query delle capacita' (formati, MSAA, limiti).
- [x] **Buffer** con uso esplicito: `Vertex`, `Index`, `StorageRead`, e `ComputeWrite` solo dove serve al post-process. Memoria GPU-only di default; upload solo tramite copy pass.
- [x] **Texture**: 2D, 2D array, cube, 3D; render target, depth (`D32_FLOAT`); mip e layer espliciti.
- [x] **Sampler** come oggetti (filtro, address mode, anisotropia, compare). Binding sempre a coppie texture+sampler.
- [x] **GraphicsPipelineDesc**: VS, PS, vertex layout (stream per vertice e per istanza), topologia, rasterizer (cull, fill, depth bias), depth-stencil, blend per render target, formati dei target, sample count. Hash -> `PipelineCache`.
- [x] **ComputePipelineDesc** (SM 5.0, CS 5.0): solo per il post-process (blur separabili, riduzioni).
- [x] **CommandBuffer**: `BeginRenderPass` (attachment con load op Clear/Load/DontCare e store op Store/DontCare), `BeginCopyPass`, `BeginComputePass`, `Submit`. Dentro i pass: `BindPipeline`, `BindVertexBuffers`, `BindIndexBuffer`, `BindSamplers`, `BindStorageBuffers`, `PushUniforms(stage, slot, data, size)`, `SetViewport`, `SetScissor`, `Draw`, `DrawIndexed`, `Dispatch`.
- [x] **Debug fin dal primo giorno**: `PushDebugGroup` / `PopDebugGroup` / `InsertDebugLabel` sul command buffer, `SetName` su tutte le risorse e pipeline. Helper `TEN_GPU_SCOPE("Nome")` gratuito nelle build release.
- [x] **Ring di upload per frame** (N frame in volo): allocatore lineare che restituisce `{puntatore CPU, buffer, offset}`. Un solo copy pass per frame trasferisce tutto prima dei render pass. *(`RHI::UploadRing`: un transfer buffer ciclato per copy pass, copie registrate a fine pass; anche il frame video)*
- [x] **Modello di threading:** SDL GPU vuole che lo swapchain sia acquisito dal thread che ha creato la finestra, mentre TEN renderizza sul thread di gioco. Il thread di gioco disegna in una texture offscreen; il thread principale la copia sullo swapchain e presenta (un blit per frame). *(`FinishFrame` sul thread di gioco, `PresentOnWindowThread` sul thread principale)*
- [x] **Test di validazione dei backend** (`rhitest <vulkan|dx11|d3d12>`): scena deterministica, screenshot in `Logs/RHITest_<backend>.png`. Vulkan e DX11 producono immagini identiche al pixel.
- [x] Vincoli di SDL GPU applicati anche a DX11, cosi' il renderer e' portabile per costruzione: *(il device DX11 segnala copy e compute pass aperti dentro un render pass)*
  - niente upload dentro un render pass;
  - massimo 4 uniform buffer per stage (usati come push);
  - niente geometry shader;
  - niente bindless (si usano texture array);
  - **`SV_InstanceID` non include `first_instance` in modo portabile**: l'indice d'istanza arriva da uno stream di vertici per istanza, che lo rispetta su tutti i backend.

### 5.2 Backend SDL3 GPU (`Renderer/RHI/SDLGPU/`)
- [x] Device: `SDL_CreateGPUDevice(SPIRV | DXIL, debug, "vulkan")`, con fallback a `"direct3d12"` e poi al backend DX11 se la creazione fallisce. `SDL_ClaimWindowForGPUDevice`.
- [x] Swapchain: `SDL_WaitAndAcquireGPUSwapchainTexture`, `SDL_SetGPUSwapchainParameters` (VSYNC / MAILBOX / IMMEDIATE), `SDL_SetGPUAllowedFramesInFlight`, resize. *(manca il resize esplicito: SDL lo gestisce all'acquisizione)*
- [x] Mappatura 1:1 del RHI: render pass, copy pass (`SDL_UploadToGPUBuffer`, `SDL_UploadToGPUTexture`, transfer buffer con `cycle`), compute pass, `SDL_GenerateMipmapsForGPUTexture`, `SDL_BlitGPUTexture`.
- [x] Readback asincrono (occlusione delle nuvole, screenshot) con `SDL_DownloadFromGPUTexture` + fence, consumato N frame dopo. *(`IFence` nel RHI, `TextureReadback` per gli screenshot; le nuvole non sono ancora portate)*
- [x] Debug: `SDL_PushGPUDebugGroup`, `SDL_PopGPUDebugGroup`, `SDL_InsertGPUDebugLabel`, `SDL_SetGPUBufferName`, `SDL_SetGPUTextureName`. Validation layer Vulkan attivo nelle build debug.

### 5.3 Backend DX11 (stessa RHI)
- [x] Riscrivere `Native/DirectX11` come implementazione del nuovo RHI: *(`Renderer/RHI/DirectX11/DX11Device.cpp`, swapchain flip-model; uniform ancora su un CB per slot con DISCARD, il ring con offset resta da fare)*
  - pipeline come bundle di oggetti stato pre-creati;
  - render pass che fanno clear e bind dei target e scollegano gli SRV in conflitto;
  - push uniform su un ring di constant buffer (`MAP_WRITE_NO_OVERWRITE` + `VSSetConstantBuffers1` con offset);
  - buffer statici `IMMUTABLE`/`DEFAULT`;
  - debug group con `ID3DUserDefinedAnnotation`, nomi con `SetPrivateData`.
- [x] Timestamp GPU per pass (solo DX11; SDL GPU non espone timestamp query: per Vulkan si usano RenderDoc, Nsight, RGP).

### 5.4 Lista dei pass (render graph minimale)
Un render graph completo (dipendenze automatiche, aliasing, pass culling) non serve: l'ordine dei pass di TEN e' fisso. Serve solo una struttura che dia ordine, nomi e risorse condivise.

- [x] `RenderPassList`: elenco ordinato di pass con nome, attachment, load/store op e una funzione di esecuzione. Ogni pass apre e chiude il suo debug group automaticamente. *(`FRAME_PASSES`: nome, flag e funzione; le attachment restano nelle funzioni dei pass)*
- [x] Pool di render target transienti per descrizione (formato, dimensione relativa alla risoluzione): i pass di post-process prendono e restituiscono i target invece di possederli. *(`RHI::RenderTargetPool` per i target intermedi: blur di SSAO e glow, dilatazione del DOF, edge e weight di SMAA)*
- [x] Pass disattivati (effetti spenti) saltati con un flag, senza logica di grafo.

### 5.5 Dipendenze
- [ ] Rimuovere DirectXTK salvo la matematica (SimpleMath e' portabile):
  - loader di texture proprio: parser DDS (BC con mip) + `SDL_LoadPNG` per i PNG del formato attuale;
  - `SpriteBatch`, `SpriteFont` e `PrimitiveBatch` riscritti sul RHI (quad instanziati, font atlas);
  - screenshot con download + `SDL_SavePNG`.
- [x] ImGui con `imgui_impl_sdlgpu3` (aggiornare imgui in `Libs/imgui` se manca) e un backend RHI per DX11. *(backend ImGui sul RHI, senza file nuovi da imgui: riusa la pipeline del layer 2D su Vulkan e DX11)*

---

## 6. Fase 2 - Toolchain degli shader (Shader Model 5.0)

- [x] **Un solo sorgente HLSL limitato a SM 5.0.** Compilazione offline in build:
  - **FXC** (`vs_5_0`, `ps_5_0`, `cs_5_0`) -> DXBC per DX11;
  - **SDL_shadercross** (DXC) -> SPIR-V e DXIL per SDL GPU, con i metadati di reflection richiesti da `SDL_CreateGPUShader`. DXC compila come SM 6.0, ma il sorgente usa solo feature SM 5.0.
- [x] Step MSBuild che produce blob e manifest in `Shaders/Bin/<formato>/`; build fallita se uno shader non compila con uno dei due compilatori.
- [x] Header `ShaderInterop.hlsli` con macro di binding per backend:
  - SDL GPU: vertex shader in space0 (risorse) e space1 (uniform), pixel shader in space2 e space3;
  - DX11: registri classici.
- [x] Header condivisi C++/HLSL per le struct GPU (`GpuInstance`, `GpuMaterial`, `GpuLight`, ...) con `static_assert` su dimensioni e offset; packing esplicito (niente `float3` seguito da campi impliciti), `-fvk-use-dx-layout`, `-Zpr` per row-major. *(`Shaders/RHI/GpuShared.hlsli`, incluso anche da `GpuTypes.h`: struct dei buffer e liste di campi dei blocchi uniform; `static_assert` su dimensioni e offset dei registri)*
- [x] **Permutazioni a compile-time** (alpha test, skinned, animato, blend mode) invece dei branch a runtime sul CB. L'elenco delle permutazioni genera anche l'elenco delle pipeline da pre-creare. Tenere il numero di permutazioni basso: solo dove il branch costa davvero. *(`permutations=` nelle righe `TEN_SHADER`; per ora solo `NO_ALPHA_TEST` di stanze e oggetti, che senza `clip` tengono l'early-z: i bucket senza alpha test sono disegnati prima con quella variante)*
- [x] SMAA riscritto senza sintassi effect e senza `technique10`; metriche di risoluzione in uniform, non in define. *(`SMAACommon.hlsli` con metriche in uniform)*
- [x] Hot-reload degli shader solo nelle build dev, con dipendenze dagli include tracciate. *(F9 nelle build debug ricompila con lo script, che ora segue gli include di ogni file, e ricrea shader e pipeline)*
- [x] Informazioni di debug negli shader delle build dev (FXC `/Zi /Od`, DXC `-Zi -Qembed_debug`, `-fspv-debug=vulkan-with-source`): RenderDoc mostra il sorgente HLSL. *(DXC: `-fspv-debug=vulkan-with-source` fallisce sulle macro, si usa `-Zi`)*

---

## 7. Fase 3 - Dati per la GPU costruiti dal formato attuale

Il formato del livello non cambia. Tutto quello che serve alla GPU si ricava al caricamento, in parallelo dove possibile, e il costo aggiunto deve essere compensato dal lavoro inutile che si toglie.

### 7.1 Geometria
- [x] **Vertici indicizzati davvero**: i poligoni del formato attuale hanno gia' gli indici; si deduplica per (posizione, UV, colore, normale, tangente, effetti) invece di creare un vertice per angolo. Meno vertici e meno banda. *(misurato: anche ignorando i dati per angolo che servono solo ai bucket animati, i vertici identici sono meno dell' 1%: gli angoli differiscono per UV, normale di faccia, colore e tangente; la ricerca raddoppiava il caricamento ed e' stata tolta)*
- [x] **Vertice compresso** (da 52 B a circa 28-32 B) costruito al caricamento: *(fatto: stanze 40 B, statiche e moveable 48 B con UV unorm16; per scendere a 28-32 B serve togliere la normale di faccia dal vertice, decisione ancora aperta in 13; i detriti restano a 52 B perche' ricostruiti ogni frame)*
  - posizione float3;
  - UV half2 o unorm16x2;
  - normale e tangente ottaedriche (snorm16x2);
  - colore RGBA8;
  - effetti + indice nel poligono + frame animato + hash impacchettati.
  La normale di faccia, oggi nel vertice, va in un buffer per poligono letto con `SV_PrimitiveID`, oppure resta nel vertice se serve a pochi shader (da misurare).
- [x] **Un unico VB e un unico IB** per tutta la geometria del livello (stanze, statiche, moveable), in memoria GPU-only. Solo i vertici di pelle e capelli di Lara restano in un buffer aggiornabile. *(sezioni stanze, statiche e moveable con base vertex e base index; i vertici moveable si ricaricano quando cambia la pelle del giocatore, prima il renderer nuovo li ignorava)*
- [x] Per bucket: range di indici, AABB e bounding sphere calcolati una volta. *(AABB in `RendererBucket` dal caricamento legacy: spazio mondo per le stanze, spazio oggetto per le mesh; la sfera si ricava dal box)*
- [x] Centri dei poligoni trasparenti calcolati una volta (in world space per le stanze, in object space per le mesh), riusati dal sort a ogni frame. *(nel caricamento legacy, riusati dal sort)*
- [x] Saldature di pelle e capelli di Lara: algoritmo con griglia spaziale o hash delle posizioni invece di O(n^2), cosi' il costo di caricamento scende senza cambiare il formato. *(misurato: 1,3 ms la pelle e 0,1 ms i capelli in debug, non vale la riscrittura)*
- [x] Lettura del file in blocco (`ReadBytes`) invece che per elemento in `LoadStaticRoomData`; scartare le binormali senza salvarle. *(anche mesh, poligoni costruiti sul posto e bucket spostati invece che copiati: in debug oggetti da 270-400 a 83-125 ms, stanze da 240-290 a 190-220 ms)*

### 7.2 Texture
- [x] DDS (gia' supportato dal formato attuale) caricato cosi' com'e', con i suoi mip e la sua compressione BC. *(`DecodeDDS` tiene formato BC e mip)*
- [x] PNG: decode con `SDL_LoadPNG` di SDL 3.4 (in parallelo per atlas) e mip generati in GPU (`SDL_GenerateMipmapsForGPUTexture` / `GenerateMips`). Nessuna compressione a runtime: costa troppo al caricamento. *(blocchi di 8 pagine decodificati con `g_Parallel` e caricati con un solo submit e una sola attesa per blocco; i livelli di benchmark hanno poche pagine e non mostrano differenze)*
- [x] **Texture array per categoria** (stanze, moveable, statiche, animate, sprite) costruiti al caricamento: le pagine dello stesso formato e della stessa dimensione diventano layer di un unico `Texture2DArray`. Le pagine di dimensione diversa vanno in array separati (uno per dimensione), oppure con padding se l'array risultante non spreca troppa memoria. Il materiale contiene array e layer. *(fatto per stanze, statiche, moveable, animate e cielo, raggruppando le pagine per dimensione e formato di tutte le mappe anche tra categorie diverse: coastal 18 pagine in 5 gruppi; il layer arriva con un uniform per draw e un cambio di pagina nello stesso gruppo non ricollega texture. Gli sprite restano texture singole perche' li campiona anche il layer 2D)*

### 7.3 Dati persistenti in GPU
- [x] Tabelle in storage buffer create al caricamento: `Meshes[]` (range di indici per bucket), `Materials[]` (array e layer di texture, blend mode, parametri del materiale, flag di animazione), `StaticInstances[]`, `Rooms[]` (ambient, flag, AABB). *(fatto per i materiali, caricati al caricamento e poi solo quando gli script li cambiano; i range di indici per mesh sono le sezioni della geometria; le istanze delle statiche restano per frame perche' contengono la fetta di luci del frame, e misurate valgono 1,5-1,8 KB per frame)*
- [x] Per frame solo: `DynamicInstances[]` (oggetti animati), `Bones[]` (pool unico con offset per oggetto), `Lights[]` e liste di indici di luci per stanza e per oggetto, sprite e particelle. *(misurato sui benchmark: 52-130 KB per frame in tutto, luci 1,3-2 KB e istanze 1,5-1,8 KB; il resto sono ossa, sprite e indici ordinati)*
- [x] Texture animate: frame in storage buffer (gia' cosi' oggi tramite `AnimatedFrames`), selezione nel VS.

---

## 8. Fase 4 - Pipeline di rendering

### 8.1 Frame
- [x] **Un solo attraversamento** della scena produce i draw packet per tutti i pass e per tutte le viste (camera, specchi, ombre). *(stanze e oggetti sono raccolti una volta per frame in liste usate da tutti i pass, ombre e specchi compresi; il culling e la vista restano del legacy finche' il renderer nuovo non e' indipendente)*
- [x] Draw packet con **sort key a 64 bit** (vista | pass | pipeline | materiale | profondita'), radix sort, poi submit con eliminazione dei cambi ridondanti. *(chiave: variante alpha test, gruppo di texture array, layer, materiale, sorgente; radix sort stabile a cifre di 8 bit. Opaco, additivo e distorsione disegnano in ordine di stato, prepass e bordi alpha test nell'ordine dei portali)*
- [x] ~~Specchi come viste separate (camera riflessa + clip), non come ridisegno dentro ogni pass.~~ *(non adottato: gli specchi legacy riflettono solo oggetti, giocatore e statiche della stanza dello specchio con la matrice di riflessione, mentre le stanze riflesse sono geometria del livello; una camera riflessa cambierebbe cosa appare. Le istanze riflesse sono raccolte una volta per frame)*

### 8.2 Culling (CPU)
- [x] Portal culling come oggi, con clip rect per stanza. *(il legacy applicava gia' il rettangolo del portale come scissor alle stanze e alle facce ordinate delle stanze; ora anche il renderer nuovo, convertendo l'asse orizzontale quando l'anteprima ha un altro rapporto d'aspetto)*
- [x] Frustum culling per istanza (sfera o AABB) senza allocazioni, vettorizzabile; correggere il bug della virgola (2.6). *(le sfere degli oggetti si scrivono in un array sullo stack invece di un vector per oggetto e per frame; le statiche usavano gia' una sfera)*
- [x] Culling per bucket dentro le stanze grandi (AABB del bucket contro il frustum), gratis con i dati calcolati al caricamento. *(con il frustum della proiezione dell'anteprima e un margine per gli effetti di vertice; coastal da 482 a 470 draw call)*

### 8.3 Illuminazione (semplice)
- [x] Un `Lights[]` unico per frame e **liste di indici di luci** per stanza e per oggetto, calcolate sulla CPU in Prepare (come oggi in sostanza, ma una volta per frame e senza copie). Lo shader legge le luci tramite indice. *(un solo buffer di luci per frame con fette per oggetto e per stanza; le liste di indici non sono state adottate: la stessa luce cambia valore tra oggetti per la dissolvenza tra stanze, e le luci copiate valgono 1,3-2 KB per frame)*
- [x] Stesso modello di luce legacy (attenuazioni, spot, shadow mode per oggetto): stessa immagine, costo diverso. *(`ToGpuLight` come `BindLight`, `Lighting.hlsli`)*
- [x] Fog bulb raccolti una volta per frame, non per stanza.
- [ ] Illuminazione clustered (Forward+) **non** prevista: si valuta solo se le misure mostrano un collo di bottiglia nelle liste di luci.

### 8.4 Pass principali
- [x] **Depth + normali** (stanze, statiche, moveable; alpha test con una permutazione dedicata) per SSAO ed early-z: sostituisce il GBuffer a 4 MRT con il minimo indispensabile. *(una sola uscita: normali in view space; la depth del prepass e' riusata dal pass di scena con LessOrEqual)*
- [x] **Opaque** con `DepthCompare=Equal` e senza depth write: nessun overdraw di pixel shading. *(stanze e oggetti opachi, che sono gli stessi draw del prepass; specchi, detriti e sprite opachi scrivono ancora la depth perche' non sono nel prepass)*
- [x] Additive e Distortion con draw packet propri, senza riattraversare la scena.
- [x] **Instancing ovunque**: statiche (tutte le mesh, senza limite di 100), moveable non skinned con lo stesso oggetto, sciami, bossoli, debris. *(le mesh rigide degli oggetti semplici vanno nella stessa coda di effetti e sciami, raggruppate per mesh tra oggetti identici; restano a parte pelle completa, cascate, dissolvenze e oggetti che proiettano ombra. Draw call: karnak e tut1 da circa 340 a 230, coastal da 470 a 355)*
- [x] ~~Sprite e particelle: istanze compatte da 16-32 B (posizione, dimensione, rotazione, colore, frame) in storage buffer, billboarding nel VS, una sola preparazione per frame.~~ *(non adottato: gli sprite legacy hanno quattro colori e quattro UV per angolo e il tipo 3D con angoli espliciti, che 16-32 B non contengono; con 100-200 sprite per frame l'istanza da 96 B vale 10-20 KB)*

### 8.5 Ombre
- [x] Culling per faccia del cube; facce fuori dal frustum della camera saltate. *(fatto per oggetto: sfera dalle mesh alla posa interpolata contro il frustum di ogni faccia, che scarta anche gli oggetti oltre il raggio della luce. Nota: i 5,6 ms delle ombre misurati su DX11 in debug erano il GPU in attesa della CPU: senza draw passano al pass successivo)*
- [x] Rispetto di `MeshBits` e dei flag d'ombra per oggetto.
- [x] Draw delle ombre tramite gli stessi draw packet (indice d'istanza), senza upload per faccia. *(le ombre riusano i draw degli oggetti; per faccia solo la view-projection)*
- [x] ~~Caching delle shadow map statiche: solo se le misure lo richiedono.~~ *(non richiesto dalle misure)*

### 8.6 Post-process
- [x] Post-process come pass della lista con target transienti condivisi: distortion, DOF, glow, color grading, lens flare, SMAA/FXAA. *(`FRAME_PASSES` e `RenderTargetPool`)*
- [x] Effetti a bassa frequenza a mezza o un quarto di risoluzione con upsample bilaterale (SSAO, glow, nuvole, aurora, god ray) dove la qualita' regge. *(SSAO fatta: mezza risoluzione, blur bilaterale H/V, upsample bilineare; 0,15-0,33 ms contro 0,9-1,1 ms del legacy sulla RTX 3050)* *(nuvole, aurora e god ray non sono ancora portati: Tomo)*
- [x] ~~Compute shader (CS 5.0) solo per i blur separabili e le riduzioni, dove battono il pixel shader.~~ *(non adottato: i blur sono gia' a risoluzione ridotta e costano 0,1-0,2 ms; nessun guadagno misurabile)*
- [ ] HDR lineare (`RGBA16F` + tonemapping) **non** previsto ora: cambia il look e aggiunge rischio. Da valutare dopo la parita'.

---

## 9. Fase 5 - Trasparenza

L'algoritmo di develop_performance (bucket come unita', entry compatte, radix sort, cluster per gli oggetti sovrapposti) e' gia' buono e **si porta nel renderer nuovo**. Cambia come si disegna.

- [x] ~~Raccolta dentro Prepare, in parallelo per oggetto con buffer per thread; centri dei poligoni calcolati al caricamento (7.1).~~ *(misurato: la raccolta costa 0,05 ms per frame e l'ordinamento 0,15 ms sui benchmark, il parallelismo non darebbe guadagni; i centri sono gia' calcolati al caricamento)*
- [x] Profondita' sempre dalla camera della vista.
- [x] ~~**Blend mode commutativi fuori dal sort**: Additive, Lighten (MAX), Subtractive e Screen danno lo stesso risultato in qualunque ordine e vanno disegnati in batch non ordinati. Restano ordinati solo AlphaBlend e Premultiplied (ed Exclude, da verificare).~~ *(provato e tolto: ogni modo e' commutativo solo con se stesso (Additive con Subtractive), ma Screen e Lighten non commutano tra loro ne' con Additive, quindi mescolandoli l'ordine cambia il risultato; restano nel sort, come nel legacy. Additive e Distortion erano gia' fuori)*
- [x] **Cambio di oggetto gratuito**: ogni run ordinato e' un draw con indice d'istanza; dati dell'oggetto, ossa e luci dai buffer persistenti e per frame. Nessun upload per run. *(renderer nuovo: un'istanza per oggetto per frame, anche se i cluster lo spezzano in piu' run)*
- [x] **Un solo upload per frame** degli indici ordinati (ring), ogni run usa `firstIndex`. *(indici copiati dagli array CPU del livello in un index buffer per frame con transfer buffer ciclato)*
- [x] Run di stanze fusi per {pipeline, array e layer di texture}: con le liste di luci indicizzate la stanza non e' piu' parte della chiave. *(il renderer nuovo fonde i run consecutivi di stanze con stesso stato e stesso rettangolo di portale: gli indici sono contigui, quindi l'ordine resta; lo scissor per stanza del legacy impedisce di fondere stanze con rettangoli diversi)*
- [x] Sprite dentro i cluster (particelle viste attraverso il vetro), disegnati come istanze in ordine di profondita'.
- [x] Rettangoli dei cluster dai bounding box invece che dai centri dei poligoni. *(otto angoli del box del bucket invece di una proiezione per poligono: piu' preciso e piu' economico; vale per entrambi i renderer)*
- [ ] Debris, bossoli e ragni inclusi nella raccolta. *(bossoli solo opachi, ragni codice morto nel legacy; restano fuori dal sort solo i detriti con blend mode trasparenti, disegnati dopo le facce ordinate come nel legacy)*
- [x] ~~Weighted Blended OIT: solo come eventuale opzione futura per le particelle, non in questo piano.~~ *(fuori dal piano)*

---

## 10. Fase 6 - Porting delle feature e sostituzione

- [x] Ordine di porting: UI/HUD/2D -> post-process -> cielo, nuvole, aurora, god ray, dust storm (coordinarsi con Tomo) -> stanze -> statiche -> moveable e Lara (pelle, capelli) -> sprite ed effetti -> ombre -> trasparenti -> specchi -> inventario e menu. *(seguito; resta il cielo atmosferico di Tomo)*
- [x] Il renderer vecchio resta compilato dietro un flag finche' il nuovo non raggiunge la parita'. *(`next <vulkan|dx11>`: il renderer nuovo disegna la finestra principale; il legacy prepara ancora viste, ordinamento e layer 2D e disegna in una finestra nascosta senza presentare. Senza flag resta il legacy, con `preview` i due affiancati)*
- [x] **Test di parita' visiva** automatici con l'harness headless: screenshot dello stesso frame (vecchio renderer, nuovo DX11, nuovo Vulkan), diff con soglia, su tutti i livelli di benchmark. *(`Documentation/Renderer/RendererParity.ps1`: livelli, backend e modalita' `preview` o `next`, soglia sulla quota di pixel diversi, errori di validazione e device lost, codice di uscita)*
- [x] Zero errori di validation Vulkan e del debug layer D3D11. *(renderer nuovo: zero sui livelli di benchmark; il debug layer D3D11 e la validazione Vulkan finiscono nel log o in stderr)* *(verificato da `RendererParity.ps1` a ogni passo)*
- [ ] Rimozione del renderer vecchio e di `IGraphicsDevice`.

---

## 11. Fase 7 - Vulkan di default

- [x] Impostazione `GraphicsBackend` nella configurazione flatbuffers (`Vulkan` default, `D3D12`, `DX11`) e nel menu grafico; il cambio richiede il riavvio. *(valori Legacy, Vulkan e DirectX 11, voce "Renderer" nel menu grafico, attiva al riavvio; il default resta Legacy finche' il renderer nuovo non ha la parita' completa (cielo atmosferico, JPG) e la condizione del punto sotto; D3D12 escluso per scelta)*
- [x] Fallback automatico in ordine Vulkan -> D3D12 -> DX11, con log del motivo. *(anche quando il device parte ma gli shader no: il backend richiesto, poi Vulkan, poi DX11, infine il legacy sulla finestra principale; provato con d3d12 senza DXIL)*
- [ ] Test su NVIDIA, AMD e Intel (anche integrate), Windows 10 e 11; driver vecchi.
- [ ] Vulkan diventa default solo se e' >= DX11 su tutti i benchmark.

---

## 11b. Stato del renderer nuovo (anteprima `preview <backend>`)

Il renderer nuovo gira in una seconda finestra accanto a quello vecchio, sugli stessi dati di `RenderView`; il benchmark salva `Logs/Compare_Legacy.png` e `Logs/Compare_Next.png` allo stesso frame di gioco.

- [x] Stanze opache e additive, texture animate (frame e UV rotate), nebbia di distanza.
- [x] Statiche raggruppate per mesh con un draw istanziato per bucket; moveable rigidi per mesh.
- [x] Luci dinamiche di stanze e oggetti (un buffer di luci per frame, slice per oggetto).
- [x] Lara: palette di ossa per frame (classica e pelle completa), fondine, capelli.
- [x] Facce ordinate (stanze, statiche, moveable, effetti) con pipeline per blend mode.
- [x] Cielo classico (due strati) e orizzonte.
- [x] Sprite istanziati e ordinati (un'istanza da 96 B per sprite, un draw per run).
- [x] Effetti di vertice (Move/Glow/Wibble) e rifrazione delle stanze d'acqua.
- [x] Soft particle, sprite laser, detriti, vampate delle armi, oggetti skinned diversi dal giocatore.
- [x] Specchi: giocatore, moveable, statiche, effetti mesh, sprite e facce ordinate riflessi con culling invertito.
- [x] Stelle, meteore e sprite del sole del cielo classico (helper condivisi col legacy).
- [x] Cupola del cielo atmosferico (stesso blocco uniform del legacy). Il legacy non la disegna nel test di benchmark: da verificare.
- [ ] Nuvole volumetriche, aurora, disco di sole e luna, god rays, tempesta di sabbia, cielo subacqueo (Tomo).
- [x] Materiali: normal map, ORSH, emissive, parallax, specular e sheen.
- [x] SSAO a mezza risoluzione con prepass depth + normali.
- [x] Ombre (cubemap della luce ombreggiante in 6 layer R32F, PCF 5x5) e ombre blob.
- [x] Fog bulb per vertice, caustiche nelle stanze d'acqua, decal delle stanze.
- [x] Post-process in un solo pass: schemi colore, dissolvenza, bande cinematiche, tinta, gamma.
- [x] Zero errori di validazione Vulkan e nessun device lost sui livelli di benchmark, con DOF attivo e spento.
- [x] Alpha test a due passate del legacy (bordi sfumati), lens flare, distorsione (mezza risoluzione), glow, DOF, FXAA, SMAA medium/ultra.
- [x] Riflessi: materiali riflettenti (frame precedente a mezza risoluzione) e riflessi del cielo (doppio paraboloide 512x512).
- [x] Video in background su set animati di tipo video e sugli sprite video.
- [x] Sprite alpha test nel prepass: non serve. Il legacy li disegna nel GBuffer senza scrivere la depth lineare e con un solo output, quindi il loro colore finisce nel target delle normali: e' un artefatto, non un contributo voluto all'AO.
- [x] Sprite opachi e alpha test nel pass opaco con scrittura della depth, come nel legacy (`SetBlendMode` riattiva la depth per Opaque e AlphaTest).
- [x] Oggetti in dissolvenza (alpha < 1): bucket opachi solo tra le facce ordinate, come `GetBlendModeFromAlpha`; facce ordinate degli sciami con alpha test 0.5 come `DrawMoveableAsStaticSorted`.
- [x] Pass 2D: testo (font DirectXTK letto dal file, layout delle stringhe del legacy), barre di stato, display sprite, lampo e ottiche (binocolo, mirino laser).
- [x] Oggetti 3D dell'HUD (riepilogo raccolti, DrawItems), dell'inventario e dell'esamina con lo shader Inventory, in un pass con depth dentro il layer 2D.
- [x] Inventario, title screen, menu, schermate di caricamento e video esclusivi: ogni present del legacy registra il layer 2D (`UiFrame`) che il renderer nuovo compone sull'ultima scena.
- [x] Facce ordinate dei capelli (HairPrimary e HairSecondary): riusano l'istanza dell'unita' di capelli creata con il giocatore.
- [x] Glow e antialiasing dei menu: il layer 2D dei menu riceve il glow dell'emissive degli oggetti e l'antialiasing su tutta l'immagine, come il target del menu legacy.

## 12. Decisioni prese

- **Shader Model 5.0 ovunque**, un solo sorgente HLSL: FXC per DX11, DXC/shadercross per SPIR-V e DXIL. DX11 fa girare tutto il renderer nuovo.
- **Formato dei livelli invariato**: i dati per la GPU si costruiscono al caricamento. Un formato v2 si valutera' solo se il tempo di caricamento diventa un problema.
- **Renderer semplice**: niente culling su GPU, occlusione Hi-Z, illuminazione clustered, render graph completo, HDR o OIT in questo piano. Si aggiungono solo se le misure le giustificano.

## 13. Decisioni ancora aperte

- [x] Texture array con pagine di dimensioni diverse: array separati per dimensione o padding (dipende dai livelli reali, da misurare). *(scelto: array separati per dimensione e formato)*
- [x] Normale di faccia nel vertice o in un buffer per poligono. *(scelto: resta nel vertice, il vertice compresso e' 40 B per le stanze e 48 B per gli oggetti; un buffer per poligono richiede `SV_PrimitiveID` e un indice per draw, per 4 B per vertice)*
- [ ] Coordinamento con Tomo sul porting di cielo e nuvole.
- [x] Dipendenze nuove (DXC dal Vulkan SDK, solo per la build): licenze e dimensione della distribuzione. *(DXC serve solo alla build per lo SPIR-V, licenza NCSA/MIT; la distribuzione contiene solo i binari degli shader. Da decidere invece un decoder JPEG per le schermate di caricamento: oggi il renderer nuovo legge solo PNG e DDS)*

---

## 14. Ordine consigliato

| # | Fase | Dipende da | Completata quando |
|---|---|---|---|
| 1 | Fase 0 - Baseline e bug | - | CSV di baseline; bug di 2.6 chiusi |
| 2 | Fase 1 - RHI + SDL GPU + DX11 + lista dei pass | 1 | Un frame di prova (triangolo, texture, instancing) identico su Vulkan e DX11, con debug group in RenderDoc |
| 3 | Fase 2 - Shader toolchain SM 5.0 | 2 | Tutti gli shader compilati offline in DXBC, SPIR-V e DXIL |
| 4 | Fase 3 - Dati per la GPU dal formato attuale | 2 | Livello caricato in buffer GPU-only, vertici indicizzati e compressi, texture array; caricamento non piu' lento di oggi |
| 5 | Fase 4 - Pipeline di rendering | 2, 3, 4 | Stanze, statiche e moveable con draw per indice d'istanza; draw call < 300 |
| 6 | Fase 5 - Trasparenza | 5 | Zero upload per run; sort + submit < 0.3 ms |
| 7 | Fase 6 - Porting e parita' | 5, 6 | Parita' visiva su tutti i benchmark; renderer vecchio rimosso |
| 8 | Fase 7 - Vulkan default | 7 | Vulkan >= DX11 ovunque; fallback testato |

Le Fasi 1, 2 e 3 possono procedere in parallelo dopo la Fase 0, se ci sono piu' persone.
