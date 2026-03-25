# Current Renderer Walkthrough

This document explains how `hardware-renderer-cpp` works in its current form.

It is intentionally a small Vulkan rasterizer, not a full engine.

## High-Level Shape

The renderer is split into a few obvious layers:

- `app`: SDL window, input, camera, frame timing
- `scene`: sample asset loading
- `mesh_loader`: OBJ ingestion into flat vertex/index buffers
- `texture_loader`: PNG ingestion into RGBA pixels
- `vulkan_helpers`: boring Vulkan allocation and one-shot command helpers
- `render`: actual graphics backend
- `shaders/mesh.vert` and `shaders/mesh.frag`: GPU shading

That is the core seam:

- CPU side owns window, input, camera, timing, and resource setup
- GPU side owns vertex processing, rasterization, texturing, and lighting

## File Map

### `src/app.cpp`

Owns the main loop.

Responsibilities:

- initialize SDL
- create the floating Vulkan window
- load the sample scene
- create the renderer
- process input
- move the camera
- update the animated light
- build the per-frame uniform block
- call `m_renderer.Render(...)`
- keep the title bar updated with `ms` and `fps`

### `src/camera.cpp`

Very small free-fly camera module.

Responsibilities:

- compute forward/right vectors
- build the view matrix
- update position from keyboard input
- apply mouse-look deltas

### `src/mesh_loader.cpp`

Loads an OBJ into a GPU-friendly mesh format.

Responsibilities:

- parse the OBJ with `tinyobjloader`
- deduplicate vertex tuples
- build a flat `vertices + indices` mesh
- generate normals if the asset does not provide them

### `src/texture_loader.cpp`

Loads a PNG into raw RGBA8 pixels with `stb_image`.

### `src/scene.cpp`

Builds the current sample scene by loading the sample OBJ and PNG.

### `src/vulkan_helpers.cpp`

Contains the repetitive Vulkan utility pieces:

- result checking
- file loading for SPIR-V
- buffer creation
- image creation
- one-shot command buffers

### `src/render/renderer.cpp`

This is the actual hardware renderer.

Responsibilities:

- create Vulkan instance/device/surface/swapchain
- create vertex/index/uniform buffers
- upload the texture into a sampled Vulkan image
- create descriptor set, render pass, depth buffer, pipeline, and framebuffers
- record command buffers
- submit draw work and present the swapchain image

## Frame Flow

Each frame looks like this:

1. SDL polls events
2. camera updates from input
3. app builds `SceneUniforms`
4. CPU copies the uniform block into the mapped uniform buffer
5. Vulkan acquires a swapchain image
6. command buffer records:
   - begin render pass
   - bind graphics pipeline
   - bind vertex buffer
   - bind index buffer
   - bind descriptor set
   - draw indexed mesh
7. queue submit
8. queue present

That is the stable hardware-raster shape.

## Scene Data Layout

The mesh lives in:

- one vertex buffer
- one index buffer

The texture lives in:

- one sampled image
- one image view
- one sampler

The camera/light state lives in:

- one uniform buffer

The current renderer is deliberately simple:

- one mesh
- one texture
- one descriptor set
- one graphics pipeline

That keeps the Vulkan side readable.

## Shading Model

The current fragment shader is basic realtime lighting, not physically based rendering.

It does:

- texture lookup
- ambient term
- point-light diffuse term
- Blinn-Phong specular term
- simple distance attenuation
- tonemap-ish compression
- gamma correction

That is enough to prove:

- raster hardware is active
- lighting is happening on the GPU
- the project has already moved well past a “just draw textured triangles” baseline

## Animation

The current animation is intentionally simple:

- the mesh stays static
- the point light orbits over time

That gives visible hardware lighting change every frame without yet committing to a full skeletal-animation stack.

## What This Repo Is Not Doing Yet

It is not yet doing:

- multiple materials
- multiple textures
- shadow maps
- normal mapping
- skeletal animation
- post-processing
- deferred rendering

That is fine. This repo is supposed to teach the baseline clearly first.

## Why This Is Cleaner Than Copying A Software Rasterizer Directly

A software rasterizer often carries around CPU-era constraints:

- triangle setup tightly coupled to presentation
- CPU-side pixel loops
- simulation and rendering logic mixed together
- data structures optimized for CPU stepping rather than GPU upload

This repo does not copy that over.

Instead it keeps:

- explicit asset loading
- explicit camera state
- explicit GPU buffers
- explicit shaders
- explicit frame submission

That is the right architecture seam for a hardware renderer.
