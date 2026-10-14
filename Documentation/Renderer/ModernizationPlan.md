# TombEngine Renderer - Piano di modernizzazione (prestazioni, memoria, asset)

Seguito di `RendererPlan.md`, che ha portato il renderer nuovo alla parita' e ha rimosso il legacy. Base: `develop_new_renderer` a `5ee1c4f93` (dopo l'annullamento di compressione al caricamento, qualita' delle texture, preset e filtro di upscale).

## Obiettivi

1. **30 FPS sui livelli giganti** con GPU integrate che usano la RAM di sistema e con GPU discrete dal 2015 in poi, ai settaggi bassi e a 1080p. I livelli normali vanno gia' abbastanza bene.
2. **Meno VRAM** anche con molti materiali e texture (oggi ci sono livelli da 5 GB).
3. **Vulkan al massimo delle prestazioni**, con le caratteristiche avanzate disponibili sulle GPU 2016+. Chi ha GPU piu' vecchie usa DX11 e abbassa i settaggi.
4. **Standard moderni** nella gestione degli asset (mesh, texture, stanze) e nell'implementazione del renderer.
5. **LOD e mip mapping** come parte del formato e del renderer, non come ripiego.
6. **Facce ordinate** con le caratteristiche avanzate di Vulkan accessibili alle GPU 2016+.

## Cosa sappiamo gia' (misure di ottobre 2026)

- **CPU del renderer:** circa 0,3 ms per frame con circa 370 draw (Release, livelli di benchmark). Sui livelli di benchmark le draw call non sono un problema; sui livelli giganti non e' ancora misurato.
- **GPU integrata (Radeon del Ryzen 7 7700, 2 CU, DDR5 condivisa, 2560x1600, DX11):**
  - con la configurazione attuale circa 92 ms per frame: Scene 45, Post-process 14, SSAO 10, Distortion 10, DOF 4;
  - con settaggi bassi e scala di render ridotta circa 51 ms, di cui 14,5 ms solo per FSR 1 (EASU + RCAS a piena risoluzione);
  - con upscale bilineare circa 36,5 ms (prototipo, annullato).
- **Texture:** ogni pagina di atlas ha fino a quattro mappe (colore, normali, ORSH, emissive). I DDS di Tomb Editor arrivano gia' in BC con i mip; i PNG si caricano in RGBA8 con mip generati in GPU. I dati codificati restano in `g_Level.*Textures` fino allo scaricamento del livello, quindi occupano RAM per tutta la partita.
- **Vulkan passa da SDL GPU, e ci resta.** SDL GPU da' Vulkan, D3D12 e Metal con lo stesso codice (Mac e Linux compresi), gestisce barriere, layout, descrittori, memoria, cycling dei buffer e swapchain, e oggi gira senza errori di validazione. Ha draw indiretti (`SDL_DrawGPUIndexedPrimitivesIndirect` con piu' draw per chiamata), compute e storage buffer; con `SDL_GPUVulkanOptions` (SDL 3.4) si chiedono un'API piu' alta, feature ed estensioni in piu'. Non espone: descriptor indexing (bindless), draw indiretti con conteggio dalla GPU, cache delle pipeline, budget di memoria, code di compute asincrone, push constant. L'RHI non ha ancora draw indiretti.
- **Facce ordinate:** raccolta e radix sort sulla CPU (0,05 + 0,15 ms sui benchmark), indici ordinati caricati ogni frame, un draw per run di stato, cluster con union-find per gli oggetti sovrapposti. I blend mode non commutano tra loro (provato), quindi restano tutti nel sort.

## Livelli hardware

| Livello | GPU | Backend | Note |
|---|---|---|---|
| Moderno | GCN 1+ / Polaris, Pascal, Intel Gen9+ (2016+), tutte le piu' recenti | Vulkan 1.2 tramite SDL GPU | Percorso GPU-driven (compute + draw indiretti), sort in compute, texture array |
| Compatibile | Maxwell, Kepler, GCN senza i requisiti, Intel piu' vecchie | DX11 (feature level 11_0, 11.1 dove c'e') | Stessi dati, culling e sort sulla CPU, texture array |

Requisiti del livello moderno, chiesti con `SDL_GPUVulkanOptions` (tutti presenti su GCN, Pascal e Gen9 con driver Windows recenti):
- Vulkan 1.2, `multiDrawIndirect`, `drawIndirectFirstInstance`, `shaderDrawParameters`, operazioni di subgroup basic/ballot/arithmetic nel compute;
- opzionali con fallback: `shaderFloat16` e `storageBuffer16BitAccess`;
- da verificare con un prototipo: che SDL GPU accetti le feature chieste e che gli shader SPIR-V che le usano passino la validazione.
- Esclusi perche' assenti sulle GPU 2016: mesh shader, ray tracing, `VK_EXT_fragment_shader_interlock` (manca su AMD), sampler feedback.
- Esclusi perche' SDL GPU non li espone: bindless, conteggio dei draw indiretti dalla GPU, compute asincrono (vedi Decisioni da rivalutare).

---

## Fase 0 - Banco di prova sui livelli giganti

Senza un livello gigante da misurare, ogni priorita' sotto e' una stima.

- [ ] Uno o due livelli giganti reali (anche da 5 GB) compilati da `.prj2` con l'harness TombLib e aggiunti al benchmark, con un percorso di camera fisso.
- [ ] CSV per frame: CPU del renderer, draw call, triangoli, tempo GPU per pass (timestamp anche su Vulkan, non solo DX11).
- [ ] Contabilita' della memoria per categoria (texture per mappa, geometria, render target, buffer per frame) per VRAM e RAM, con `MemoryTracker`, nel pannello F8 e nel log.
- [ ] Modalita' con budget simulato (per esempio `vram 1024`), che fa comportare il motore come su una scheda da 1 GB.
- [ ] Misure di riferimento su iGPU AMD, RTX 3050 e, se possibile, una Intel integrata e una GPU del 2015-2016.

## Fase 1 - Memoria (VRAM e RAM)

- [ ] **Tutte le texture in BC.** I DDS di Tomb Editor si caricano come sono; per i PNG la compressione al caricamento (`stb_dxt`) resta come ripiego per i livelli vecchi, con una cache su disco per non ricomprimere a ogni avvio.
- [ ] **Mappe di materiale solo dove servono.** Non allocare normal, ORSH ed emissive per le pagine dove sono assenti o costanti (una normale piatta, un ORSH uniforme): un valore nel materiale le sostituisce. Normali in BC5; valutare BC4 per le mappe a un canale e BC7 dove BC3 perde qualita'.
- [ ] **Budget di memoria.** La qualita' delle texture (mip alti scartati) si sceglie dal budget di `IDXGIAdapter3::QueryVideoMemoryInfo` (valido anche per Vulkan sullo stesso adapter), con un'opzione manuale nel menu.
- [ ] **Liberare la RAM.** I dati codificati delle texture si liberano dopo il caricamento in GPU, e cosi' le copie CPU della geometria che servono solo all'upload. Sulle integrate la RAM di sistema e' anche la VRAM.
- [ ] **Render target.** Inventario dei target a piena risoluzione (scena, distorsione, sorgente AA, copia della depth R32F, normali, emissive, glow, DOF, upscale): aliasing dei target transienti nel pool, formati piu' piccoli (`R11G11B10F`, `R16`), mezza risoluzione dove la qualita' regge.
- [ ] **Streaming delle texture per stanza** (dopo la Fase 4): mip bassi sempre residenti, mip alti caricati per le stanze vicine e visibili, scaricati con LRU entro il budget.

## Fase 2 - Vulkan avanzato attraverso SDL GPU

SDL GPU resta il backend Vulkan: da' portabilita' (Metal su Mac) e la gestione di sincronizzazione e memoria che oggi funziona senza errori. Si estende quanto permette.

- [ ] **Opzioni Vulkan.** `SDL_GPUVulkanOptions` con API 1.2 e le feature del livello moderno; prototipo che verifica creazione del device, feature attive e validazione degli shader che le usano.
- [ ] **Draw indiretti nell'RHI.** `DrawIndexedIndirect` con piu' draw per chiamata (SDL GPU usa `multiDrawIndirect` dove c'e'); su DX11 emulati con `DrawIndexedInstancedIndirect` in un ciclo. I draw scartati dalla GPU hanno zero istanze.
- [ ] **Compute nell'RHI** per culling e sort: storage buffer scritti dal compute e letti come argomenti indiretti e index buffer, con le dipendenze tra pass gestite da SDL GPU.
- [ ] **Shader.** Per Vulkan DXC con Shader Model 6.x dove servono subgroup e 16 bit (solo compute del culling e del sort, e FSR in FP16); per DX11 resta SM 5.0 con FXC. Un sorgente HLSL con macro per le due varianti.
- [ ] **Pipeline.** Tutte create al caricamento, su piu' thread se SDL GPU lo regge; misurare i tempi di creazione al primo avvio (la cache dei driver NVIDIA e AMD copre gli avvii successivi).
- [ ] **Timestamp GPU su Vulkan** per il CSV della Fase 0, se SDL GPU li espone; altrimenti misure con RenderDoc/Radeon GPU Profiler/Nsight.
- [ ] Rilevamento del livello all'avvio: se le feature non ci sono, il livello moderno si spegne (percorso CPU come su DX11) e lo si scrive nel log.
- [ ] Zero errori di validazione e nessun DEVICE_LOST sui benchmark e sui livelli giganti.

## Fase 3 - Rendering GPU-driven (livello moderno)

- [ ] **Texture array con layer nel materiale.** Il layer passa dalla tabella dei materiali (storage buffer) invece che da un uniform per draw, cosi' draw con layer diversi dello stesso gruppo si fondono in un draw indiretto. I gruppi restano uno per dimensione e formato; meno gruppi grazie alle texture singole della Fase 5.
- [ ] **Cluster di geometria.** Stanze e mesh divise in cluster di 64-128 triangoli con bound (sfera, AABB, cono delle normali), costruiti al caricamento con meshoptimizer (MIT) finche' Tomb Editor non li produce.
- [ ] **Culling in compute.** Frustum, rettangolo del portale della stanza, cono delle normali e occlusione Hi-Z dalla depth del frame precedente (due fasi). Il risultato sono draw indiretti, uno per cluster con zero istanze se scartato, inviati con una chiamata per pipeline e gruppo di texture invece che un draw per bucket.
- [ ] **Ombre.** Stessi cluster per le facce del cube; cache delle shadow map statiche (stanze e statiche), ridisegnando ogni frame solo gli oggetti in movimento.
- [ ] **DX11.** Stessi dati: culling dei cluster sulla CPU con `g_Parallel` e range di indici fusi; texture array come oggi.
- [ ] Criterio: CPU del renderer sotto 1 ms e draw call per pipeline costanti anche sui livelli giganti.

## Fase 4 - Costo GPU per pixel (GPU integrate)

- [ ] **Upscale.** Filtro bilineare come opzione (quasi gratis), RCAS fuso nel pass di composizione, FSR in FP16 su Vulkan.
- [ ] **Risoluzione dinamica** per tenere 30 FPS: la scala di render segue il tempo GPU misurato, su Vulkan e DX11.
- [ ] **Post-process in meno pass.** Distorsione, colore, dissolvenza e AA uniti dove possibile; un compute shader al posto di piu' pass a schermo intero sul livello moderno.
- [ ] **Pass opzionali sul profilo basso.** Distorsione a un quarto di risoluzione o spenta, SSAO spenta, DOF e glow ridotti.
- [ ] **Preset grafici** (basso, medio, alto, ultra) e scelta automatica al primo avvio in base alla VRAM e al tipo di GPU (integrata o discreta).
- [ ] Obiettivo verificato: 30 FPS sui livelli giganti sull'iGPU AMD a 1080p con il preset basso.

## Fase 5 - Asset moderni (con Tomb Editor)

Questa fase cambia il formato dei livelli: va coordinata con Tomb Editor. I livelli vecchi devono continuare a caricarsi con un percorso di conversione al caricamento.

- [ ] **Texture singole al posto degli atlas.** Ogni texture di Tomb Editor resta un'immagine propria in BC, con la sua catena di mip, raggruppata in texture array per dimensione e formato. Spariscono il bleeding tra tessere, il padding e il mip limitato dalle tessere piccole.
- [ ] **Mesh pronte per la GPU.** Vertici indicizzati e quantizzati, ottimizzati per la vertex cache, cluster e LOD calcolati in Tomb Editor (meshoptimizer anche li').
- [ ] **Stanze in blocchi.** Geometria della stanza divisa in cluster con bound e materiali raggruppati; statiche della stanza pronte per l'instancing.
- [ ] **Contenitore del livello.** Sezioni versionate e allineate, compresse con LZ4 (gia' in `Libs`), caricate in GPU senza elaborazione, con indice delle sezioni per lo streaming.
- [ ] **Materiali come dati.** Tabella di materiali con riferimenti alle texture (colore, normali, ORSH, emissive) invece di quattro mappe parallele per pagina.

## Fase 6 - LOD e mip mapping

- [ ] **Mip.** Catene complete da Tomb Editor, bias dei mip legato alla scala di render, filtro anisotropo come opzione, mip alti scartati in base al budget (Fase 1), poi streaming per distanza e dimensione sullo schermo (Fase 1, ultimo punto).
- [ ] **LOD delle mesh.** Statiche e moveable con 2-4 livelli generati offline (semplificazione di meshoptimizer), scelti per dimensione sullo schermo, con transizione in dither per evitare scatti.
- [ ] **LOD per le ombre:** le mesh nelle facce del cube usano un livello piu' basso.
- [ ] **Stanze.** Prima si misura: la geometria a griglia ha gia' pochi triangoli. Se serve, LOD dei cluster lontani visti attraverso i portali.
- [ ] Regola: nessun cambio visibile al look classico ai settaggi alti; il LOD aggressivo solo nei preset bassi.

## Fase 7 - Facce ordinate sul livello moderno

- [ ] **Un solo blend state per i modi piu' comuni.** Alpha blend, premoltiplicato e additivo si esprimono tutti con `ONE, ONE_MINUS_SRC_ALPHA` (alpha blend: colore moltiplicato per alpha nello shader; additivo: alpha di uscita 0). I run consecutivi di questi modi si fondono in un unico draw, senza cambiare il risultato. Subtractive, Screen, Lighten ed Exclude restano run separati.
- [ ] **Sort in compute.** Chiavi di profondita' dai centri dei poligoni gia' calcolati al caricamento e dalle trasformazioni delle istanze; radix sort con subgroup (reduce-then-scan, che non richiede le garanzie di avanzamento del onesweep); indici ordinati scritti direttamente nell'index buffer; argomenti dei draw indiretti per run scritti dalla GPU. Niente upload degli indici ordinati per frame.
- [ ] **Divisione del lavoro.** La CPU tiene l'ordine tra oggetti e i cluster (union-find); la GPU ordina i poligoni dentro i cluster grandi. Le pipeline per run sono note alla CPU dalla sequenza dei cluster; ogni cluster ha un numero massimo di run, e quelli non usati restano con zero indici.
- [ ] **Occlusione.** Il culling Hi-Z della Fase 3 vale anche per i poligoni trasparenti dietro geometria opaca.
- [ ] **OIT solo come opzione misurata.** Weighted blended OIT per le particelle; liste per pixel (`fragmentStoresAndAtomics`, disponibile sulle GPU 2016) scartate come default perche' costano VRAM, contro l'obiettivo 2.
- [ ] **DX11** mantiene il percorso CPU attuale, che costa gia' meno di 0,3 ms.
- [ ] Criterio: sui livelli giganti con molte facce ordinate, sort + submit sotto 0,3 ms di CPU e nessun upload per frame degli indici.

---

## Decisioni da prendere

- [x] **Backend Vulkan:** resta SDL GPU, esteso con `SDL_GPUVulkanOptions`, draw indiretti e compute. *(deciso il 2026-10-14: portabilita' su Mac con Metal, meno codice, sincronizzazione e memoria gia' senza errori)*
- [ ] **Dipendenza nuova:** meshoptimizer (MIT) per cluster, ottimizzazione dei vertici e LOD.
- [ ] **Shader Model 6 per i compute di Vulkan.** Supera la decisione "SM 5.0 ovunque" di `RendererPlan.md` solo per culling, sort e FSR FP16; il resto e DX11 restano SM 5.0.
- [ ] **Cambio di formato dei livelli** (Fase 5) e calendario con Tomb Editor; percorso di conversione per i livelli vecchi.
- [ ] **Soglia del livello moderno:** Vulkan 1.2, oppure 1.1 con estensioni per includere driver piu' vecchi.

## Decisioni da rivalutare dopo le misure

- [ ] **Backend Vulkan nativo** (volk, Vulkan Memory Allocator), solo se sui livelli giganti i texture array e i draw indiretti a zero istanze si rivelano un limite reale. Darebbe bindless, conteggio dei draw dalla GPU, cache delle pipeline, budget di memoria e compute asincrono, al costo di molto codice e della portabilita' su Mac (MoltenVK o un backend Metal).
- [ ] **Budget di memoria senza `VK_EXT_memory_budget`:** su Windows DXGI (`QueryVideoMemoryInfo`) vale anche per Vulkan sullo stesso adapter; su Linux e Mac serve un'altra via.

## Ordine consigliato

| # | Fase | Dipende da | Completata quando |
|---|---|---|---|
| 1 | Fase 0 - Banco di prova | - | Livelli giganti nel benchmark, CSV di VRAM, RAM e tempi per pass su iGPU e RTX |
| 2 | Fase 1 - Memoria | 1 | Livelli giganti entro 1 GB di VRAM al preset basso, RAM liberata dopo il caricamento |
| 3 | Fase 4 - Costo per pixel | 1 | 30 FPS sui livelli giganti sull'iGPU a 1080p (preset basso), anche su DX11 |
| 4 | Fase 2 - Vulkan avanzato con SDL GPU | 1 | Feature 1.2 attive, draw indiretti e compute nell'RHI, zero errori di validazione |
| 5 | Fase 3 - GPU-driven | 4 | CPU sotto 1 ms e draw costanti sui livelli giganti |
| 6 | Fase 7 - Facce ordinate | 4, 5 | Sort in GPU, nessun upload degli indici per frame |
| 7 | Fase 5 - Asset moderni | 2, decisione sul formato | Livelli nuovi caricati senza elaborazione, livelli vecchi convertiti |
| 8 | Fase 6 - LOD e mip | 7 | LOD e streaming dei mip attivi, nessun cambio visibile ai settaggi alti |

Le Fasi 1 e 4 danno i guadagni piu' grandi sulle GPU deboli e non dipendono dalle estensioni Vulkan: vanno per prime. La Fase 5 si puo' progettare con Tomb Editor in parallelo alle Fasi 2 e 3.
