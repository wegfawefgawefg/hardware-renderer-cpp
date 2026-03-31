# Static Batching Regression

## Summary

There is a live regression in the main static-batched mesh path.

The current build intentionally falls back to the single-draw path for main
scene meshes because restoring the normal static-batched path currently breaks
multiple scenes.

This is not only a `Virtual Geom Test` problem. It affects ordinary scene
rendering too.

## Symptoms

When the main static-batched path is active, scenes can render incorrectly or
disappear:

- `City` can collapse into tiny/far-off geometry or vanish
- `Vehicle Light Test` can lose most of its static scene meshes
- `Virtual Geom Test` can place visible geometry wildly wrong or make it vanish

At the same time, some other paths can still appear healthy:

- the character path can still render
- shadow previews can still show silhouettes
- some experiment scenes such as `Proc City` or `Many Lights` can look less affected

That points to a bug in the ordinary main-pass static-batched mesh path, not a
global camera/projection failure.

## Current Workaround

The temporary workaround is in
[`src/render/renderer_visibility.cpp`](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/render/renderer_visibility.cpp):

- main-pass visible draws are pushed into `m_visibleDrawItems`
- `m_visibleStaticBatchDrawItems` is left unused for the main scene pass

That restores scene correctness, but it disables one of the renderer’s main
performance features for ordinary static scene meshes.

## Why It Matters

This makes some benchmarks and comparisons unfair right now.

In particular:

- raw mesh vs virtualized mesh comparisons in `Virtual Geom Test` are not final
- any repeated static-mesh scene is currently missing its intended batching/binning win

## Files To Inspect

- [`src/render/renderer_visibility.cpp`](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/render/renderer_visibility.cpp)
- [`src/render/frame.cpp`](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/render/frame.cpp)
- [`src/render/render_batches.cpp`](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/render/render_batches.cpp)
- [`src/render/resources_scene.cpp`](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/render/resources_scene.cpp)
- [`shaders/mesh.vert`](/home/vega/Coding/Graphics/hardware-renderer-cpp/shaders/mesh.vert)

## Recommended Next Step

Treat this as a dedicated renderer bug hunt:

1. keep the single-draw fallback active until the root cause is fixed
2. compare the main batched path against the working shadow batching path
3. validate batch eligibility, descriptor usage, and instance submission
4. only then restore main-pass static batching
