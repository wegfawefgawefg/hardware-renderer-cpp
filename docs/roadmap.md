# Hardware Renderer Roadmap

This is the natural next-step list for `hardware-renderer-cpp`.

The important point is that the current repo is already a valid educational stopping point. These are extension ideas, not missing essentials.

## Current Baseline

We already have:

- SDL3 Vulkan window
- Vulkan graphics pipeline
- depth testing
- OBJ mesh loading
- PNG texture loading
- first-person camera
- realtime fragment lighting
- animated point light

That is enough to explain the basic GPU raster architecture cleanly.

## Sensible Next Steps

### 1. Multiple Meshes And Materials

The next useful structural step would be:

- scene with more than one mesh
- separate material records
- separate textures per material

This keeps the current renderer model but makes it feel more like a real scene instead of a single sample asset.

### 2. Shadowing

The biggest visual jump from here would be shadows.

The obvious path is:

- directional or point light shadow map
- shadow sampling in the fragment shader

That would be a good lesson because it is very much a raster-era lighting technique, unlike the GPU raytracer repo.

### 3. Animation / Skinning

If this repo wants to overlap more with the software renderer experiments, the next major feature would be:

- skeletal animation data
- GPU skinning in the vertex shader

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
