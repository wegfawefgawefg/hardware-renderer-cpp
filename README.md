# hardware-renderer-cpp

Clean educational Vulkan rasterizer in C++.

This project plays the same role for the software rasterizers that `gpu-raytracer` plays for `raytrace-rs`: take the core ideas, drop the CPU-only constraints, and rebuild them around the GPU in a cleaner form instead of copying architecture blindly.

## Current State

- CMake + Ninja build
- Clang-format and VS Code `F5` flow
- Vendored SDL3 through CMake `FetchContent`
- SDL3 floating Vulkan window on X11/i3
- Vulkan graphics pipeline with swapchain, depth buffer, and textured mesh rendering
- First-person free camera with right-mouse look
- OBJ mesh loading
- PNG texture loading
- Realtime hardware lighting in fragment shader
- Animated point light orbit
- Smoothed `ms / fps` in the window title

## Build

```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/hardware-renderer-cpp
```

## VS Code

Open the `hardware-renderer-cpp` folder and press `F5`.

That configures, builds, and launches `build/debug/hardware-renderer-cpp` under `gdb`.

## Controls

- `Right Mouse`: hold to capture mouse and look around
- `W A S D`: move
- `Space` / `Shift`: move up / down
- `Escape`: quit

## Sample Scene

The current baseline uses:

- `assets/viking_room.obj`
- `assets/viking_room.png`

This is just a good first textured mesh target. The point is the renderer structure, not the asset itself.

## What It Teaches

- how Vulkan graphics setup differs from a software rasterizer
- how mesh and texture assets get uploaded into GPU resources
- how a hardware raster pipeline is split across CPU setup, vertex shader, fragment shader, and presentation
- how to keep a renderer flat and explicit without dragging simulation/gameplay concerns into it

## Docs

- `docs/current-renderer-walkthrough.md`: file map and frame flow
- `docs/roadmap.md`: where this renderer would naturally go next

## Status

This is already a good stopping point for an educational hardware rasterizer:

- one clean Vulkan path
- one real asset path
- one movable camera
- one lit textured scene

If this repo continues later, the natural next topics are richer materials, multiple meshes, animation/skinning, shadowing, and post-processing.
