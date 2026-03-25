# Hardware Renderer Features

This doc describes what `hardware-renderer-cpp` should grow into if it continues.

The goal is not "full engine." The goal is a clean GPU raster renderer that can carry a real scene
from the software renderer experiments without dragging over old CPU-era design mistakes.

## Purpose

This repo should become:

- GPU rasterization
- GPU lighting
- GPU animation where that is the normal place to do it
- CPU gameplay and world simulation
- CPU collision for normal game objects
- optional GPU-only collision-style queries for particles or special effects

That means:

- do not turn this repo into a physics engine
- do not turn this repo into a software renderer with Vulkan paint on top
- do keep the scene, asset, and render data model clean enough that the Peach's Castle sandbox can
  live here later

## Core Target

The practical target is:

- load a larger world scene like Peach's Castle
- place one or more controllable characters in it
- render it at high framerate with textured materials and realtime lighting
- animate characters on the GPU
- keep the CPU focused on input, camera, game state, and collision/query logic

That is a better fit for this repo than:

- path tracing
- baking systems
- giant editor tooling
- exotic full-GPU simulation

## Feature Buckets

### 1. Scene And Asset Model

We want the same clean ideas that the software renderer was aiming at:

- assets loaded once
- instances placed many times
- scene as world state, not asset storage
- models that can contain multiple primitives and materials

Target data model:

- `TextureAsset`
- `MaterialAsset`
- `MeshAsset`
- `ModelAsset`
- `SkeletonAsset`
- `AnimationClipAsset`
- `ModelInstance`
- `SkinnedInstance`
- `Scene`

Important point:

- the renderer should consume flat GPU-friendly draw data
- the gameplay/world layer should not know or care about Vulkan objects

### 2. Raster Rendering

The renderer should stay fundamentally raster-first:

- forward or forward+ style base renderer first
- depth testing
- multiple meshes per frame
- multiple materials/textures
- transparent and masked materials later
- debug draw overlays

This repo does not need to become deferred immediately. Forward rendering is a better first fit for
the current codebase and teaching goal.

### 3. Lighting

The natural lighting progression is:

1. multiple realtime lights
2. material-controlled diffuse/specular response
3. shadow maps
4. optional normal maps
5. optional simple post-processing

This should stay "good realtime lighting" before it tries to become physically perfect.

That means a sensible short-term target is:

- textured materials
- per-material roughness/specular strength
- a few dynamic lights
- shadowing

That alone is enough to make the castle or character scene look much better than the current sample.

### 4. Animation

GPU animation is normal and desirable here.

What belongs on GPU:

- skeletal skinning in the vertex shader
- morph target evaluation later if we ever need it

What belongs on CPU:

- animation state machine
- clip selection
- blend weights
- joint matrices per frame

So the usual practical split is:

- CPU evaluates the animation pose
- CPU uploads a palette of joint matrices
- GPU skins vertices during the draw

That is the normal path. We do not need GPU-side animation graph logic.

### 5. Collision And World Queries

For normal gameplay, collisions should stay CPU-side.

That includes:

- player vs static scene
- dynamic object broadphase
- simple character controllers
- capsule / sphere / AABB style object interactions

Static scene collision is a very good fit for:

- CPU triangle BVH
- CPU raycasts
- CPU sweep tests

That is boring, correct, and easy to debug.

The repo is already moving that way:

- a static triangle collider is built from the loaded scene
- the current camera rides on a grounded player-style controller
- jump and wall-slide behavior now happen against scene geometry instead of in free-fly space

GPU collision is only attractive here for specialized effects:

- particle collision against depth
- particle collision against signed fields or simple scene proxies
- crowd-ish or effect-heavy systems

So the rule should be:

- gameplay collision on CPU
- effect collision can be GPU if it earns its complexity

### 6. UI And Debugging

This repo should support normal renderer debugging:

- FPS and frame time
- current resolution
- light gizmos
- toggles for debug views
- simple on-screen controls

Dear ImGui is a reasonable next step for this repo. It is much more appropriate than trying to grow
the hand-rolled text overlay into a full UI system.

## Realistic Milestones

### Milestone A: Clean Forward Renderer

- split Vulkan backend into smaller files
- multiple draw items instead of one sample mesh
- material records instead of one texture path
- per-instance transforms

### Milestone B: Castle Scene

- load Peach's Castle assets
- support multi-material OBJ/GLTF scene assets
- static scene instance path
- camera/navigation over a real space instead of a floating sample prop

### Milestone C: Character

- load skinned character asset
- load idle/run/jump clips
- CPU animation state
- GPU skinning

### Milestone D: Collision

- CPU static-scene collider from the castle mesh
- player capsule/sphere controller
- raycasts and ground queries

### Milestone E: Lighting Upgrade

- shadow maps
- per-material lighting controls
- better light rig

At that point the repo has reached the actual intended overlap with the software renderer work.

## What Should Not Be Copied From The Software Renderer

We should not blindly port:

- software-renderer-specific stage structure
- CPU-oriented asset/layout assumptions
- rendering code mixed tightly with gameplay state
- legacy loader choices if better modern libraries exist
- giant monolithic files

The software renderer is useful as a feature reference and scene reference. It should not be copied
as an architecture template.

## Recommended CPU / GPU Split

### CPU

- window/input
- scene graph-ish world state
- asset loading
- animation state selection
- collision and gameplay
- render list building
- culling
- upload/update of per-frame and per-draw data

### GPU

- vertex transform
- skinning
- rasterization
- texture/material shading
- shadow map rendering and sampling
- post-processing
- optional effect simulation later

That split is the center of gravity for this repo.
