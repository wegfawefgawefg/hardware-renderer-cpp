# Rearchitecture Notes

This doc is the "before we grow this further" cleanup plan.

The current renderer works, but it is already violating the repo's size/style rules in the place
that matters most:

- [`src/render/renderer.cpp`](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/render/renderer.cpp)
  is carrying far too many responsibilities

The renderer should be split before we pile castle assets, skinning, shadow maps, and collision
plumbing on top of it.

## Current Problems

### 1. Vulkan Backend Is Too Monolithic

Right now one file owns:

- instance/device/swapchain setup
- descriptor creation
- render pass creation
- mesh pipeline creation
- overlay pipeline creation
- light marker pipeline creation
- texture upload
- overlay upload
- command recording
- draw submission
- depth buffer creation

That is workable for the first demo. It is the wrong shape for the next phase.

### 2. Scene And Renderer Boundaries Are Still Thin

The current repo mostly has:

- one mesh
- one texture
- one renderer-owned sample scene path

That is fine for the first target, but not for:

- multi-model scenes
- material systems
- skinning
- castle + character + props

We need a clearer seam between:

- asset data
- world instances
- renderer-owned GPU resources

### 3. Overlay/UI Path Is Still "Good Enough," Not Settled

The small SDL_ttf-to-texture overlay is fine as a stopgap.

For the next phase:

- keep this path for simple HUD text
- add Dear ImGui if we want real toggles/panels
- do not grow the current custom overlay into a fake UI framework

## Recommended File Splits

The repo prefers small files with one clear job. The next split should stay flat and boring.

## Vulkan Backend

Suggested split from `vulkan_renderer.cpp`:

- `vulkan_device.cpp`
  - instance
  - surface
  - physical device
  - logical device
  - queues

- `vulkan_swapchain.cpp`
  - swapchain
  - image views
  - framebuffers
  - depth image
  - resize path

- `vulkan_pipeline.cpp`
  - render pass
  - mesh pipeline
  - light marker pipeline
  - overlay pipeline

- `vulkan_resources.cpp`
  - vertex/index/uniform buffers
  - texture upload
  - overlay upload
  - samplers
  - descriptor set updates

- `vulkan_frame.cpp`
  - render()
  - record command buffer
  - submit/present
  - per-frame upload work

Keep the public state bundle in:

- `vulkan_renderer.h`

That keeps the layout flat while making ownership readable.

## Scene And Asset Layer

The next clean split should be:

- `asset_types.h`
- `material_asset.h/.cpp`
- `model_asset.h/.cpp`
- `scene_instance.h`
- `scene_builder.cpp`

That sounds like more files, but it prevents the repo from turning into "random structs in
whatever file happened to exist first."

## Animation Layer

When skinning lands, do not cram it into `scene.*` or `mesh_loader.*`.

Give it explicit files:

- `animation_types.h`
- `animation_player.cpp`
- `skinned_mesh_loader.cpp`

The GPU part should stay inside the renderer/shader path. The CPU pose/blending logic should stay
outside the Vulkan backend.

## Collision Layer

If castle collision gets added, keep it CPU-side and separate from rendering:

- `collision_mesh.cpp`
- `collision_query.cpp`
- `character_controller.cpp`

Do not let collision helpers start depending on Vulkan or renderer-only types.

## Recommended Library Choices

This repo should prefer explicit libraries with narrow jobs, not giant kitchen-sink stacks.

### Mesh / Scene Formats

Recommended direction:

- use glTF as the preferred "modern runtime asset" format
- keep OBJ support for static/simple scenes

Practical library choices:

- `cgltf` or `fastgltf` for glTF
- keep `tinyobjloader` only as the simple OBJ path

Rationale:

- glTF is a much better target for multi-material scenes, transforms, and modern asset pipelines
- OBJ is still fine for the castle/static-world side if that is what the source assets are today

### Animation / Skinning

Recommended direction:

- `ufbx` if FBX support is still needed

Rationale:

- the software renderer already relied on FBX-based character/anims
- if we want Kenney-style animated assets without offline conversion first, `ufbx` is a cleaner
  focused dependency than dragging in a giant importer stack

Longer term, converting animated assets to glTF would be even cleaner.

### Mesh Processing

Recommended:

- `meshoptimizer`

Use cases:

- vertex/index remap
- cache optimization
- LOD generation later
- general "make mesh data better for GPU" cleanup

### UI

Recommended:

- Dear ImGui

Use it for:

- resolution toggles
- debug toggles
- light controls
- animation debug

Do not reinvent that layer.

### What To Avoid

Avoid defaulting to:

- `assimp` as the first choice

Reason:

- it is broad and convenient
- it also hides a lot and brings in more than this repo needs
- for a teaching-oriented renderer, narrower libraries are a better fit

## Asset Strategy

This repo should support two asset lanes.

### Lane 1: Static World

- castle
- props
- environment meshes
- multi-material static models

Best format:

- OBJ short term
- glTF medium term

### Lane 2: Animated Characters

- skinned mesh
- skeleton
- clips

Best format:

- FBX short term if we need to reuse the existing Kenney assets as-is
- glTF if we can convert and simplify the pipeline

The important thing is not forcing one loader to do everything.

## Collision Strategy

Collision should not be "GPU collision engine."

Recommended split:

- static world triangle collider on CPU
- dynamic object broadphase on CPU
- simple capsule / sphere / AABB gameplay shapes

Use the renderer assets as source data for collision builds, but keep collision data in its own
optimized CPU representation.

That means:

- world mesh render data and world collision data may come from the same source asset
- they should not be the same in-memory structure forever

## Lighting Strategy

Short term:

- forward shading
- multiple dynamic lights
- shadow maps
- material-controlled diffuse/specular response

Medium term:

- normal maps
- emissive
- tone map / post

Do not jump to deferred or clustered lighting before the scene/asset path is settled.

## What "GPU Accelerated" Should Mean Here

This repo should mean:

- draw and shade on GPU
- animate vertices on GPU
- keep lighting on GPU
- keep optional post-processing on GPU

It should not mean:

- every system forced onto the GPU whether it fits or not

The useful rule is:

- if the result is directly tied to drawing or per-vertex/per-pixel work, GPU is natural
- if the result is gameplay logic or sparse world queries, CPU is usually the correct first home

## Immediate Cleanup Before Major Features

Before adding castle, character, or shadows, do this:

1. split `vulkan_renderer.cpp`
2. introduce explicit asset and instance structs
3. make the renderer consume a small render-list style input instead of one hardcoded sample scene
4. keep overlay text simple and add ImGui separately if wanted

That is the right point to branch from.
