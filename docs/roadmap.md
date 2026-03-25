# Hardware Renderer Roadmap

This is the natural next-step list for `hardware-renderer-cpp`.

The important point is that the current repo is already a valid educational stopping point. These are extension ideas, not missing essentials.

## Current Baseline

We already have:

- SDL3 Vulkan window
- Vulkan graphics pipeline
- depth testing
- OBJ and glTF loading
- PNG texture loading
- multi-model scene + entity split
- first-person camera
- realtime fragment lighting
- animated point light
- CPU-side static triangle collider groundwork
- one skinned character path with CPU pose evaluation and GPU skinning

That is enough to explain the basic GPU raster architecture cleanly.

## Sensible Next Steps

### 0. Current Structural Baseline

The first rearch pass is already underway:

- `src/render/` exists as a shallow Vulkan module
- `SceneData` now separates models from entities
- `src/assets/` is the current loader boundary

The next cleanup here is:

- keep `src/assets/` narrow and reusable
- avoid leaking sample-scene assumptions into the renderer API
- prepare a clean animation/skinning module instead of mixing it into assets or render files

### 1. Bigger Static World

The next useful scene step is:

- castle-scale static scene asset
- stable scene framing/spawn points
- renderer handling a larger world without special-casing one demo model

### 2. Shadowing

The biggest visual jump from here would be shadows.

The obvious path is:

- directional or point light shadow map
- shadow sampling in the fragment shader

That would be a good lesson because it is very much a raster-era lighting technique, unlike the GPU raytracer repo.

### 3. Animation / Skinning

If this repo wants to overlap more with the software renderer experiments, the next major feature would be:

- broaden the current one-character path into:
  - generic skinned mesh assets
  - multiple skinned instances
  - cleaner animation asset separation

That would be a good line in the sand between “basic rasterizer” and “real animated renderer.”

### 4. Better Material Model

Right now the shader is deliberately simple.

Later upgrades could include:

- roughness
- metalness
- normal maps
- emissive maps

But that should happen after the scene and shadow path are settled.

### 5. Post Processing

After the base forward renderer feels good, a natural next move is:

- offscreen color target
- tone mapping pass
- bloom or simple post effects

That is a nice bridge toward more engine-like rendering without changing the core scene path too much.

## What This Repo Should Probably Avoid

To stay coherent, this repo should probably not immediately grow into:

- an ECS
- gameplay systems
- giant abstraction layers
- multiple rendering backends at once

The current value is that it is explicit and easy to trace.

## Longer-Term Direction

If the repo keeps going toward the software-renderer sandbox goals, the intended path is:

1. castle-scale static scene
2. skinned player character
3. CPU-side collision/controller
4. better realtime lighting

That is the line where this becomes "software-renderer ideas, but done as a proper GPU rasterizer."
