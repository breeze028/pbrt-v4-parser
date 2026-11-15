# PBRT Scene Lab UI redesign

Date: 2026-11-15 (Asia/Shanghai).

This is a frontend redesign of the existing application, not a disconnected demo. It retains Vite, TypeScript, the C++/WASM loader, Scene IR, and the existing Renderer. All application copy, loading messages, accessibility labels and frontend runtime errors are English. Scene values remain exactly as supplied.

## Reference review and design decisions

- [PlayCanvas Editor](https://developer.playcanvas.com/user-manual/editor/interface/): separate Explorer, Viewport and Inspector responsibilities. The official annotated interface image was inspected.
- [Spline V2](https://spline.design/blog/spline-v2): explicit workspace modes and restrained editor chrome. The release article and its editor preview were inspected.
- [SuperSplat](https://superspl.at/editor) and its [interface guide](https://developer.playcanvas.com/user-manual/supersplat/editor/interface/): the actual editor was opened; compact floating controls and optional diagnostics informed the viewport controls.
- [Babylon Inspector](https://doc.babylonjs.com/legacy/inspector/): the official scene explorer / inspector documentation informed filtering and contextual property groups. The documentation browser later timed out; this was not treated as a runtime test of Babylon's editor.

The implementation uses original stroke icons, local system font fallbacks, neutral charcoal tokens and one sage accent. No framework migration, icon font, external font request, new renderer, picking pipeline, or third-party branding was introduced.

## Implemented behavior

Showcase is the default. Spaceship offers a landscape first view; Bathroom and Cornell Box remain selectable. The full Film image is fit inside the available viewport without cropping or distortion. A floating toolbar opens exposure, render settings and help, and provides camera reset, pause/resume, accumulation restart and browser fullscreen. The compact status bar shows actual samples, triangles, buffer size and adapter identity. It does not invent FPS or GPU timings.

Inspect reveals grouped, searchable scene nodes and a read-only Properties panel. The virtual list uses 34 px rows and a five-row overscan: Bathroom's 908 entries produced about 24 DOM rows at the tested desktop height. Search covers names, types, source files and parameter values. Object / instance records remain available alongside Camera, Geometry, Materials, Textures, Lights, Media and Render settings. Keyboard navigation scrolls the virtual window to offscreen rows.

Properties displays identity, source location, parameters, inherited parameters, transforms, references, resources and associations. Number values are not rounded for display. Large arrays expand on demand; the Raw Data tab serializes the complete selected object only when requested. Texture selection retains the actual decoded preview, image dimensions, mip count and color/data role.

Panels can collapse and resize by pointer or keyboard. At 1440 px and above, Inspect supports three columns; at 1100–1439 px, Properties is an overlay drawer; below 1100 px both panels become optional drawers. On small screens only one side drawer opens at a time. Opening a settings popover closes overlapping side drawers.

Window, mode and panel resizing refits CSS display bounds only. The camera continues to use the original Film ratio and the existing sampling backbuffer. The Preview resolution control explicitly reallocates the existing renderer's pixel buffers, as before. This preserves accumulation while changing UI layout. Depth override, random seed, sample clamp, alpha, bump and LOD controls preserve their existing sampling behavior. Manual depth stays across scene switches; disabling the override restores the current PBRT default.

Diagnostics is a closed-by-default bottom drawer with Scene, GPU checks and Activity tabs. It invokes the existing actual GPU self-test. Activity load durations measure the complete frontend loading operation, not GPU time. Fullscreen requests use the browser API and report a readable English fallback when the host refuses permission.

## Validation performed

- TypeScript and production Vite build passed. All three original scene bundles, worker scripts, embedded WGSL, WASM compile/MIME and licenses passed the production HTTP checker under `/pbrt-v4-parser/`.
- Native/WASM regression: **7/7 passed**, including Cornell, Spaceship, Bathroom, exception recovery and ownership tests.
- Real browser, NVIDIA Ampere: all three scenes loaded and rendered. Bathroom's GPU self-test passed **6 rays, 28,681 BSDF queries, 220 texture samples and 3 bump normals**, with actual rendering diagnostics `[0,0,0,0]`. No page Console warnings/errors were observed.
- Real viewport checks: **1920×1080**, **1440×900**, **1050×900**, **390×844**. No document horizontal overflow. Film ratio was checked against actual DOM bounds; the final mobile Bathroom image was 366×366.
- Paused accumulation stayed at **41 spp** with a **512×512** backbuffer when changing desktop to mobile dimensions. Mode toggles, panel resizing, searches and list navigation also preserved samples.
- Searched and selected Bathroom `Texture02`; its `rug_mask.tga`, 1024×1024 size, 11 mip levels, float/linear role and full Raw Data were observed. Virtual-list End navigation reached the final geometry rows with approximately 24 mounted rows.
- Manual depth **35**, scene defaults, preview resolution, seed and target **16 spp** were exercised. Automatic pause and actual `spaceship-16spp.pfm` download succeeded. Camera arrow-key input restarted accumulation. Exposure remains display-only.
- A final screenshot review caught a square-Film overlap with the scene caption. Desktop / tablet layouts now reserve a separate caption band; narrow landscape screens use a compact caption.
- A mobile header issue found during observation (scene selector reduced to an arrow) was corrected to an explicit two-row layout. Final selector width was approximately 212 px at a 390 px viewport.
- Fullscreen was rejected by the embedded test browser; the English fallback was verified. Actual fullscreen display in a standalone browser is not certified here.

A source comparison confirmed that the only changes inside `renderer.ts`, `texture-check.ts` and the loader Worker are string literals. **C++ sources, WASM structures, `model.ts`, WGSL and rendering algorithms are unchanged.** Browser viewport overrides were reset after testing.

## Before / after evidence

The previous published [Spaceship UI](assets/webgpu-online-spaceship.jpg) and [Bathroom UI](assets/webgpu-online-bathroom.jpg) show the fixed three-column layout and permanent JSON pane. The new captures below show the connected renderer. Different sampling resolutions and viewport sizes are stated; these are UI comparisons, not image-quality or performance benchmarks.

| View | Capture |
|---|---|
| Showcase, 1920×1080, Spaceship at 512×288 / 256 spp | [Screenshot](assets/webgpu-ui-showcase.jpg) |
| Inspect, 1440×900, Bathroom texture properties | [Screenshot](assets/webgpu-ui-inspect.jpg) |
| Optional Properties drawer, 1050×900 | [Screenshot](assets/webgpu-ui-drawer.jpg) |
| Showcase, 390×844, full square Film | [Screenshot](assets/webgpu-ui-mobile.jpg) |

These are agent observations, not user acceptance or cross-device performance guarantees. Publication follows the existing GitHub Pages workflow; workflow success and live UI checks are reported separately in the delivery.
