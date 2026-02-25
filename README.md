# Rebel Engine

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![Windows](https://img.shields.io/badge/platform-Windows-0078D6)

**Status:** Active development  
**Current milestone:** Character Controller & Movement System  
**Next milestone:** Camera System (Player Camera Manager & Follow Rig)

Rebel Engine is a custom C++ engine built as a practical sandbox for responsive **3C** gameplay, animation-driven locomotion, and tooling. It is designed for fast iteration on core third-person foundations, especially **3C (Character, Camera, Controls)**. The runtime stays explicit and modular, with clear paths to scale as gameplay and locomotion systems evolve.

## Table of Contents

- [Quick Start](#quick-start)
- [Engine Direction](#engine-direction)
- [Design Principles](#design-principles)
- [Animation \& Locomotion Direction](#animation--locomotion-direction)
- [Screenshots \& Editor Views](#screenshots--editor-views)
- [Editor Interaction (GIF)](#editor-interaction-gif)
- [Video Demo](#video-demo)
- [Key Features](#key-features)
- [Renderer Boundary \& Future Scalability](#renderer-boundary--future-scalability)
- [Debug \& Visualization Tools](#debug--visualization-tools)
- [How to Explore the Engine](#how-to-explore-the-engine)
- [Architecture Overview](#architecture-overview)
  - [Runtime vs Editor Separation](#runtime-vs-editor-separation)
  - [Actor/ECS Model (EnTT)](#actorecs-model-entt)
- [Core Library](#core-library)
- [Engine Runtime Systems](#engine-runtime-systems)
- [Animation Runtime](#animation-runtime)
- [Editor](#editor)
- [3C Systems \& Gameplay Responsiveness](#3c-systems--gameplay-responsiveness)
- [Build \& Run](#build--run)
- [Repository Layout](#repository-layout)
- [Roadmap (3C / Animation Priorities)](#roadmap-3c--animation-priorities)
- [Contributing / License](#contributing--license)
- [Future Documentation](#future-documentation)

## Quick Start

```bat
.\scripts\generate_build.bat
```

1. Open `Build/RebelEngine.sln` in Visual Studio 2022.
2. Build the `Editor` target (dependencies are included in the solution).
3. Run `Editor` (Debug/Release path depends on configuration).
4. In the editor toolbar, use **Load scene** to load `Editor/scene.Ryml`.
5. Use **Play** for Play-In-Editor mode.

## Engine Direction

Rebel Engine is evolving toward a gameplay-first architecture with strong **3C foundations**, responsive control loops, and clear runtime behavior.

Current direction:

- **Responsive character control and camera workflows**
- **Animation-driven locomotion runtime growth**
- **Fast gameplay iteration in-editor**
- **Deterministic runtime update flow**

The engine is an evolving foundation focused on responsive gameplay and rapid iteration, with clear extension points.

## Design Principles

- **Explicit over implicit:** update phases and module boundaries are named and intentional.
- **Deterministic update flow:** runtime uses a fixed phase order that reduces hidden dependencies.
- **Modular subsystems:** rendering, physics, input, assets, and editor tooling stay separated by design.
- **Data-driven tooling:** reflection metadata powers inspector UI and serialization paths.
- **Runtime/editor parity:** editor workflows operate on the same runtime scene/component systems.

## Animation & Locomotion Direction

The animation runtime is implemented and functional. Pose evaluation runs per-frame, bone palettes are generated at runtime, and skeletal skinning consumes evaluated animation data in render submission. Animation playback is integrated into runtime update phases and is structured to extend cleanly into locomotion/controller work.

Current runtime pipeline:

```text
Animation Clip -> Pose Evaluation -> Bone Palette -> Skinning -> Render
```

Upcoming 3C integration flow:

```text
Intent -> Movement -> Animation -> Camera
```

This keeps the controller and locomotion milestone aligned with the existing animation runtime.

## Screenshots & Editor Views

These screenshots highlight the editor workflow, debugging tools, and rendering capabilities. Images are stored in `docs/images/`.

### Viewport Overview

![Viewport Overview](docs/images/viewport.png)

Viewport rendering of a loaded scene with real-time camera navigation and editor integration.

---

### Transform Gizmo & Object Editing

![Gizmo Editing](docs/images/gizmo.png)

Transform gizmos and ID-buffer picking support precise actor edits directly in the viewport.

---

### Skeleton Debug Rendering

![Skeleton Debug](docs/images/skeleton_debug.png)

Skeleton debug overlay validates hierarchy import, bone transforms, and animation-pipeline readiness.

---

### Physics Collider Debug Visualization

![Collider Debug](docs/images/collider_debug.png)

Collider debug rendering verifies generated physics shapes across editor and runtime inspection workflows.

---

### Animation Runtime (GIF)

![Animation Runtime](docs/images/animation_playback.gif)

Animation playback demonstrating runtime pose evaluation and skeletal deformation.

Suggested captures:

- animation playback in viewport
- skeleton debug overlay while animating
- pose update across frames
- root motion preview (if supported)

Suggested additional visuals:

- bone hierarchy debug
- animation pose progression
- runtime playback comparison

## Editor Interaction (GIF)

![Editor Interaction](docs/images/editor_demo.gif)

Animated editor interaction covering viewport selection, gizmo transforms, camera navigation, and debug overlays.

## Video Demo

Coming soon.

**Suggested content**

- Editor layout overview
- Scene loading & Play-In-Editor
- Gizmo editing & selection
- Debug visualization tools
- Camera movement & interaction

## Key Features

- **Reflection-driven tooling:** runtime type metadata powers modules, inspector UI, and serialization.
- **Deterministic tick phases:** explicit update order (`PreTick`, `Physics`, `Render`, etc.).
- **Actor + EnTT architecture:** actor ownership with ECS query flexibility.
- **OpenGL MDI submission path:** packed buffers + SSBO data + `glMultiDrawElementsIndirect`.
- **Robust editor picking:** ID-buffer pass for precise viewport selection.
- **Asset handle pipeline:** binary `.rasset` metadata + lazy load flow.
- **Scene persistence:** reflection-driven YAML save/load (`.Ryml`).
- **Jolt physics foundation:** fixed-step simulation + debug visualization.
- **Editor-first workflow:** docked tools, gizmos, and Play-In-Editor scene handoff.
- **Skeletal animation runtime:** runtime pose evaluation drives final bone palettes and skeletal deformation.

## Renderer Boundary & Future Scalability

Render submission is isolated behind a modular boundary (`RenderModule` + `RenderAPI` abstraction).  
The OpenGL backend prioritizes clarity and iteration speed. The abstraction leaves room for future renderer evolution without forcing gameplay or tooling rewrites.

## Debug & Visualization Tools

- **Collider debug draw:** primitive physics shapes are rendered for edit/runtime inspection.
- **Skeleton debug rendering:** skeletal hierarchy lines help validate rig import and transforms.
- **Bone pose validation:** runtime-vs-asset pose comparisons and bind-pose diagnostics support matrix/transform validation.
- **Animation playback verification:** animation debugger controls and sampled track inspection validate playback timing and pose output.
- **Root driver inspection/preview:** root-driver channels and sampled root transform behavior can be verified per frame in the animation debugger.
- **ID buffer picking:** viewport click -> object ID readback -> stable actor selection.
- **Gizmo editing:** ImGuizmo transform manipulation writes directly to reflected scene components.
- **Integrated console/logging:** in-editor log sink makes system behavior visible during iteration.

These tools keep debugging inside normal editor workflows and speed up iteration on gameplay and systems.

## How to Explore the Engine

1. Start with `Editor/src/main.cpp` and run the `Editor` target.
2. Load `Editor/scene.Ryml` from the level toolbar.
3. Inspect `RebelEngine/include/Scene.h` and `RebelEngine/src/Scene.cpp` for actor/ECS flow.
4. Follow render submission from `RebelEngine/src/RenderModule.cpp` into `RebelEngine/include/OpenGLRenderAPI.h`.
5. Inspect reflected inspector plumbing in `Editor/src/ImGuiModule.cpp` and `Core/include/Core/Reflection.h`.
6. Trace tick orchestration in `RebelEngine/src/BaseEngine.cpp`.

## Architecture Overview

```text
+---------------------+        +----------------------+
|      Editor App     |        |       Game App       |
|  (Editor/main.cpp)  |        |   (Game/main.cpp)    |
+----------+----------+        +-----------+----------+
           |                               |
           v                               v
+---------------------+        +----------------------+
|    EditorEngine     |        |      BaseEngine      |
| (inherits BaseEngine)|       |  (runtime loop/core) |
+----------+----------+        +-----------+----------+
           |                               |
           +---------------+---------------+
                           v
                  +-------------------+
                  |   ModuleManager   |
                  | (reflection scan) |
                  +---+---+---+---+---+
                      |   |   |   |
                      |   |   |   +--> AssetManagerModule
                      |   |   +------> PhysicsModule (Jolt)
                      |   +----------> InputModule
                      +--------------> RenderModule (OpenGL)
                                      + AnimationModule (runtime pose evaluation)
                                      + ImGuiModule (Editor)

                  +-------------------+
                  |       Scene       |
                  |  Actor + EnTT ECS |
                  +-------------------+

Core library underpins all layers:
TArray/TMap, memory, reflection, logging, serializer, math, delegates.
```

### Runtime vs Editor Separation

Runtime and editor responsibilities are split between `BaseEngine` and `EditorEngine`. This keeps edit-time behavior such as gizmos, editor camera controls, and tooling isolated from runtime simulation, and supports safe Play-In-Editor scene duplication via `TempPIE.Ryml`.

### Actor/ECS Model (EnTT)

Actor/ECS combines actor-style ownership with EnTT registry queries. This keeps gameplay-facing actor ergonomics while preserving ECS flexibility for system queries and editor serialization.

<details>
<summary>Actor/ECS data flow details</summary>

- `Scene` owns:
  - `entt::registry`,
  - actor objects (`TArray<RUniquePtr<Actor>>`),
  - entity-to-actor lookup map.
- `Actor` stores:
  - primary entity handle (`entt::entity m_Entity`),
  - root object component pointer (`SceneComponent*`),
  - object components as owned C++ objects (`EntityComponent` hierarchy).
- Data components and object components coexist:
  - data components on actor entity (`AddDataComponent`),
  - object components backed by separate ECS entities storing `T*` pointers (`AddObjectComponent`).
- Component registration is reflection-backed through `ComponentRegistry` + `REFLECT_ECS_COMPONENT`.

</details>

## Core Library

`Core` is the foundation used by both runtime and editor targets.

### Containers

Containers provide engine-native dynamic array and hash map types. This keeps allocation behavior and API style consistent across scene, module registry, asset registry, and reflection data.

### Memory & Ownership

Memory and ownership utilities implement allocator abstractions and smart pointer primitives. This keeps ownership explicit (`RUniquePtr` in actors/modules/assets) and avoids hidden lifetime coupling.

### Utilities, Math, Handles

Utilities, math, and handle primitives provide core type aliases, string helpers, GUID handles, and GLM aliases. This gives stable data types for reflection, serialization, and asset IDs across projects.

### Logging & Profiling

Logging and profiling provide category-based logs with an editor-visible sink plus frame/scoped timing helpers. This makes runtime behavior easier to inspect during gameplay and tooling iteration.

### Reflection & Serialization

Reflection and serialization provide runtime type/property metadata with reflection-driven YAML and binary serialization. This drives module discovery, editor inspectors, scene save/load, and asset type reconstruction.

### Delegates & Threading Helpers

Delegates and threading helpers provide event primitives and a bucket scheduler utility. Delegates unify event flow, and the scheduler supports future parallel task pipelines.

## Engine Runtime Systems

### 1) Scene / World / Actor Layer

This layer manages world state, actor lifecycle, tick participation, transform updates, and spawn/destroy flow. It keeps gameplay-side behavior explicit and predictable, which is critical for reliable movement and camera behavior.

<details>
<summary>Data flow details</summary>

- Spawn actor by reflected type -> allocate actor + ECS entity -> add base components (`IDComponent`, `NameComponent`, `SceneComponent`, etc.) -> optional tick registration.
- Per-frame actor tick list executes first, then transform update and pending destroy flush.
- Actor creation uses `TypeInfo::CreateInstance` for data-driven instantiation.

</details>

### 2) ECS Registry Integration (EnTT)

EnTT is the query and storage backend for runtime systems and editor tooling. Component reflection metadata keeps editor and serialization workflows generic while systems run direct data queries.

<details>
<summary>Data flow details</summary>

- Components are registered in `ComponentRegistry`.
- Editor and serializer iterate the registry for add/edit/save/load behavior.
- Runtime systems query views such as `entt::registry.view<...>()`.

</details>

### 3) Rendering (OpenGL + MDI + SSBO + Picking)

Rendering draws static and skeletal meshes to an offscreen viewport texture and runs ID-buffer picking for editor selection. The submission path stays explicit while supporting high draw throughput and stable editor interaction.

<details>
<summary>Rendering Pipeline Details</summary>

- Scene views gather `StaticMeshComponent*` and `SkeletalMeshComponent*`.
- Asset handles resolve through `AssetManagerModule`.
- Geometry uploads once (`AddStaticMesh`) into packed GPU buffers.
- Draw calls are queued with per-draw model/material/object ID/bone base.
- Batches are submitted via `glMultiDrawElementsIndirect`.
- Picking pass writes IDs into `GL_R32UI`; editor reads via `ReadPickID`.

Skeletal rendering status:

- Skeletal vertex format + bone buffers are wired.
- Skeleton debug rendering is implemented.
- Runtime animation evaluation is implemented and drives skeletal deformation.

</details>

### 4) Asset System (Handles, Metadata, Import, Binary Format)

The asset system maps stable `AssetHandle` IDs to typed metadata and lazy-loaded runtime objects. This decouples scene references from in-memory object lifetimes and keeps asset identity stable across reloads.

<details>
<summary>Data flow details</summary>

- Startup scan reads `.rasset` headers from `Editor/assets` / `Assets`.
- Registry stores `ID + type + path`.
- Load by handle creates reflected asset instance and deserializes payload from binary stream.

Import pipeline status:

- Mesh/skeletal import exists in `RebelEngine/src/MeshLoader.cpp` via Assimp.
- Editor menu actions can generate binary mesh/skeleton/skeletal assets.

</details>

### 5) Serialization (YAML Scenes + Reflection Save/Load)

Serialization saves and restores actor/component scene state through reflected properties. This keeps editor iteration and PIE scene duplication practical without hardcoded per-class serializers.

<details>
<summary>Data flow details</summary>

- Save: iterate actors -> serialize actor type + reflected component fields.
- Load: spawn actor by reflected class name -> add/deserialize components by `ComponentRegistry`.
- Example scene files: `Editor/scene.Ryml`, `Editor/TempPIE.Ryml`.

</details>

### 6) Physics (Jolt Integration)

Physics simulates primitive collider bodies and writes dynamic results back to scene transforms. The fixed-step design provides a stable motion layer that can anchor character locomotion and movement constraints.

<details>
<summary>Data flow details</summary>

- `PrimitiveComponent` (`BoxComponent`, `SphereComponent`, `CapsuleComponent`) defines authoring shape.
- Physics bodies are lazily created once.
- Simulation runs fixed-step (`1/60`) with accumulator.
- Dynamic bodies write back position/rotation to `SceneComponent`.
- Debug draw lines are consumed by `RenderModule`.

Editor/runtime behavior:

- Editor mode: debug collider rendering without stepping simulation.
- Runtime mode: full simulation step.

</details>

### 7) Input System

Input tracks per-frame key/button/mouse/scroll state from window events. It provides a single event-to-polling bridge used by camera and future gameplay control systems.

<details>
<summary>Data flow details</summary>

- GLFW callback -> `Window::GEngineEvent` -> `BaseEngine::OnEngineEvent` -> module `OnEvent` -> `InputModule` state buffers.
- Per-frame transients are reset in `BaseEngine::MainLoop`.
- Current controls include free-fly editor camera movement, rotation, and zoom.

</details>

### 8) Event / Messaging

Event/messaging broadcasts engine and window events to interested systems. This keeps event producers and consumers decoupled while modules remain independently testable.

### 9) Time, Tick, and Update Phases

Time/tick phases define explicit per-frame ordering for modules and engine logic. This makes it straightforward to add movement, camera, and animation systems without hidden update-order bugs.

<details>
<summary>Tick order in <code>BaseEngine::Tick</code></summary>

1. `PreTick`
2. `PrePhysics`
3. `Physics`
4. `PostPhysics`
5. Engine tick (`Scene::Tick` in runtime, transform maintenance in editor)
6. `Tick`
7. `PostTick`
8. `PreRender`
9. `Render`
10. `PostRender`

</details>

### 10) Animation System

The animation system evaluates skeletal animation poses at runtime and produces bone palettes consumed by rendering and gameplay systems.

Responsibilities:

- Animation clip playback and time progression.
- Pose sampling and bone hierarchy evaluation.
- Final skinning palette generation.
- Root motion extraction for locomotion integration.

Runtime role:

- Executes during the runtime `Tick` phase before render submission.
- Updates animation state on `SkeletalMeshComponent` instances.
- Outputs evaluated pose data for rendering and gameplay systems.
- Supplies root motion deltas for upcoming character movement systems.

`Animation System` is the runtime subsystem, `Animation Runtime` is the internal evaluation pipeline, and `AnimationModule` is the module implementation.

## Animation Runtime

### Animation Playback

Animation clips are sampled by runtime time progression on `SkeletalMeshComponent` (`PlaybackTime`, speed, loop/play controls). Playback time is normalized per clip duration and supports looping/clamped playback paths for deterministic sampling.

### Pose Evaluation

Per-frame evaluation builds bind/local pose data, samples animation tracks, evaluates the bone hierarchy from local to model space, and produces the final skinning palette (`globalPose * inverseBind`). The resulting palette is stored on component runtime data for rendering and debugger inspection.

### Root Motion Support

Root-driver motion extraction is supported through `AnimationAsset::m_RootDriver` data and runtime root-delta sampling. Root driver deltas are applied during pose evaluation and are structured so animation-driven locomotion can optionally feed actor movement systems in the character-controller milestone.

### Runtime Integration

`AnimationModule` runs in the runtime tick phases (`Tick`) and updates animation state before render submission. `RenderModule` consumes evaluated bone palette buffers (`FinalPalette`) for skinning. Animation logic and rendering remain cleanly separated through component runtime outputs and module boundaries.

## Editor

The editor executable (`Editor/src/main.cpp`) runs `EditorEngine`.

### ImGui Panels and Tools

ImGui tooling provides viewport, hierarchy, inspector, browser, and console workflows in a docked layout. Keeping these workflows in one executable supports fast level iteration and runtime inspection.

Panels:

- `Viewport###Viewport`
- `Outliner###Outliner`
- `Details###Details`
- `Content Browser###Content Browser`
- `Level Toolbar###LevelToolbar`
- `Console`

### Picking and Selection Workflow

Picking selects actors by clicking viewport pixels through object ID render targets. This keeps scene interaction direct and stable even with batched rendering.

### Gizmo and Transform Editing

Gizmo editing uses ImGuizmo to manipulate selected actor transforms. This supports quick transform iteration for camera framing, blockout, and collision setup.

### Reflection-Driven Inspector

The reflection-driven inspector draws actor and component properties from runtime reflection metadata. This reduces manual editor UI maintenance when component fields change.

### Editor Camera vs Runtime Camera

The engine switches between the free editor camera and the scene primary camera based on mode. This preserves editor navigation while keeping Play-In-Editor behavior clean.

## 3C Systems & Gameplay Responsiveness

### Current strengths for 3C work

- **Explicit tick phases** provide clear insertion points for movement, camera rigs, and animation updates.
- **Runtime/editor mode separation** avoids cross-contamination of state during iteration.
- **Reflection + component registry** keeps control/camera parameters easy to expose and tune in editor.
- **Fixed-step physics** provides a stable base for movement handoff.

### Current state

- **Controls**:
  - Free-fly editor camera input (`WASD`, `Q/E`, RMB look, scroll zoom) in `RebelEngine/src/Camera.cpp`.
- **Camera**:
  - Editor camera + scene `CameraComponent` primary camera workflow.
- **Character/locomotion**:
  - Dedicated character movement component/controller is the next milestone.
- **Skeletal/animation**:
  - Skeletal meshes and skeleton assets load and render.
  - Skeleton debug drawing is active.
  - Runtime animation evaluation, pose updates, and skinning palette generation are active in `AnimationModule`.

### Planned (next phase)

Next-phase integration focus:

```text
Intent -> Movement -> Animation -> Camera
```

This milestone centers on Character Controller & Movement System integration on top of the active animation runtime.

## Build & Run

### Prerequisites

- Windows (Premake + Visual Studio workflow is configured).
- Visual Studio 2022 (workspace generation target).

### Generate Projects

Use the included script:

```bat
.\scripts\generate_build.bat
```

Or run Premake directly:

```bat
.\vendor\bin\premake\premake5.exe vs2022 --file=Premake5.lua
```

### Build

Open `Build/RebelEngine.sln` and build `Editor` (dependencies are part of the workspace).

Optional command-line example:

```bat
msbuild .\Build\RebelEngine.sln /p:Configuration=Debug /m
```

### Run

- Editor executable: `Build/Bin/Debug/Editor/Editor.exe` (configuration-dependent).
- Game executable: `Build/Bin/Debug/Game/Game.exe`; `Game/src/main.cpp` is a starter entry point.

### Third-Party Dependencies Included in Repo

From `Premake5.lua` + `vendor/`:

- `yaml-cpp`
- `glfw`
- `imgui`
- `GLAD`
- `assimp`
- `ImGuizmo`
- `JoltPhysics`
- `glm`
- `spdlog`

Header-only integrations in engine include:

- `RebelEngine/include/ThirdParty/entt.h`
- `RebelEngine/include/ThirdParty/stb_image.h`

## Repository Layout

```text
RebelEngine/
|-- Core/
|   |-- include/Core/
|   |   |-- Containers/           # TArray, TMap
|   |   |-- Serialization/        # YAML + binary stream interfaces
|   |   |-- MultiThreading/       # BucketScheduler
|   |   |-- Reflection.h          # Type/property metadata system
|   |   |-- Log.h, Timer.h, String.h, CoreMemory.h
|   `-- src/
|-- RebelEngine/
|   |-- include/
|   |   |-- AssetManager/
|   |   |-- Animation/
|   |   |-- Components.h
|   |   |-- Scene.h, Actor.h, BaseEngine.h
|   |   `-- RenderModule.h, PhysicsModule.h, InputModule.h
|   `-- src/
|       |-- AssetManager/
|       |-- Animation/
|       |-- RenderModule.cpp
|       |-- PhysicsSystem.cpp
|       `-- Scene.cpp, Actor.cpp, BaseEngine.cpp
|-- Editor/
|   |-- include/
|   |-- src/
|   |   |-- EditorEngine.cpp
|   |   |-- ImGuiModule.cpp
|   |   `-- main.cpp
|   `-- assets/                  # sample .rasset + source model files
|-- Game/
|   `-- src/main.cpp
|-- scripts/generate_build.bat
`-- Premake5.lua
```

## Roadmap (3C / Animation Priorities)

1. Completed: runtime animation evaluation in `AnimationModule` (evaluated pose data drives skeletal deformation).
2. Character Controller & Movement System.
3. Locomotion state handling & motion blending.
4. Camera rig integration with movement states.
5. Motion warping & advanced locomotion features.


## Future Documentation

Planned documentation expansions:

- Animation system design and pose evaluation flow.
- Character controller architecture and movement state flow.
