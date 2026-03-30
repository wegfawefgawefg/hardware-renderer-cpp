# Proc City Tiled Lighting Bug

## Summary

`Proc City` has a correctness bug in the new tiled dynamic-light path.

When `Dynamic proc lights` and `Tiled proc lights` are both enabled, lighting shows a hard moving cutoff across the screen. The seam tracks camera pitch and appears as though whole rows or regions of tiles are missing light influence.

This is a rendering correctness bug, not just a quality issue.

## Repro

1. Open `Proc City`.
2. Enable:
   - `Dynamic proc lights`
   - `Tiled proc lights`
3. Use a high light count like `512`.
4. Stand in a street corridor with visible side-wall lighting.
5. Look slightly up/down.

Observed:
- a hard horizontal-ish seam appears
- lit surfaces abruptly stop across that seam
- the seam moves with the view

Expected:
- tiled lights should match the naive proc-light path visually, aside from minor tile granularity
- no large hard cutoff should be visible

## Current Status

The tiled path is definitely faster than the naive per-instance path, but it is still wrong visually.

The seam remains after several obvious fixes, so there is still at least one unresolved bug in the tile assignment or tile lookup path.

## Relevant Files

- [`src/render/render_batches.cpp`](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/render/render_batches.cpp)
- [`shaders/proc_city.frag`](/home/vega/Coding/Graphics/hardware-renderer-cpp/shaders/proc_city.frag)
- [`src/runtime/update.cpp`](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/runtime/update.cpp)
- [`src/render/renderer.h`](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/render/renderer.h)

## Current Tiled Path

### CPU side

`BuildProcCityTiledLightLists()` in [`src/render/render_batches.cpp`](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/render/render_batches.cpp):

- builds a 2D tile grid over the swapchain
- projects each proc-city point light into screen space
- estimates a screen-space radius
- assigns the light to overlapping tiles
- flattens per-tile light buckets into:
  - `m_procCityLightTiles`
  - `m_procCityTileLightIndices`

### GPU side

`proc_city.frag` in [`shaders/proc_city.frag`](/home/vega/Coding/Graphics/hardware-renderer-cpp/shaders/proc_city.frag):

- computes tile coord from `gl_FragCoord.xy / 32`
- fetches a tile header
- loops that tile’s light indices
- evaluates each assigned light normally

## Fixes Already Tried

These changes have already been made and did not fully solve the bug:

1. Added both `tileCountX` and `tileCountY` to uniforms.
2. Clamped shader tile coordinates before tile-header lookup.
3. Corrected CPU-side Y mapping to match top-left screen space.
4. Made the projected screen radius more conservative by using nearest sphere depth instead of center depth.
5. Increased tile-light index capacity drastically to avoid silent truncation.

So the bug is not explained by:
- missing `tileCountY`
- obvious out-of-bounds tile fetches
- the earlier simple center-depth underestimation
- the previous smaller tile-index buffer cap

## Strong Clues

### 1. The seam moves with camera pitch

This strongly suggests the failure is still in screen-space tile mapping or tile coverage, not in world-space light simulation.

### 2. The seam is hard, not noisy

This suggests:
- entire tiles or tile rows are wrong or missing
- not just individual lights being slightly under-assigned

### 3. Tiled mode is much faster than naive mode

That is expected, but the seam may also indicate the tiled path is still under-lighting large parts of the screen, which would artificially improve perf.

## Most Likely Remaining Causes

## A. Screen-space sphere bounds are still too naive

The current CPU tile assignment uses:
- projected light center
- estimated pixel radius from `range / depth`

That can still be wrong for perspective projection, especially for:
- near lights
- lights near the screen edge
- large ranges
- elongated visible influence on walls

This is currently the top suspected correctness issue.

### Better version

Instead of the current approximate radius:
- project a more conservative screen-space bound for the sphere
- or project multiple extremal points
- or derive min/max tile extents from view-space sphere bounds more carefully

## B. Tile assignment is too screen-space-only for large depth variation

The implementation is plain 2D tiled Forward+, not clustered/froxel.

That alone should not produce a hard seam, but it can produce poor culling quality in streets with strong depth variation.

Still, the current artifact looks more severe than “normal 2D tile over/undercull,” so this is probably not the only issue.

## C. A tile-grid convention mismatch still exists somewhere

Even after the Y fix, there may still be disagreement between:
- CPU tile-space generation
- fragment-space lookup
- viewport/projection conventions

Worth re-checking against actual Vulkan viewport behavior and `gl_FragCoord` assumptions.

## D. Tile contents need direct visualization

At this point, debugging by eyeballing lit walls is inefficient.

The next useful tool is a tile debug mode.

## Recommended Next Steps

### 1. Add a tile debug overlay

Visualize at least one of:
- tile grid lines
- light count per tile as a heatmap
- selected tile index under cursor

This will immediately show whether the seam lines up with missing tile populations.

### 2. Compare naive vs tiled tile-by-tile

Add a temporary debug mode in `proc_city.frag`:
- left half of screen uses naive proc lights
- right half uses tiled proc lights

This makes the mismatch obvious and helps localize whether the issue is:
- missing lights
- wrong tile fetch
- bad tile assignment

### 3. Replace the current radius approximation with a more conservative sphere projection

Current estimate:
- project center
- scale radius by `projScale / nearestDepth`

Better options:
- project several sphere-extreme points in view space
- compute min/max NDC extents conservatively
- bias outward by one tile if needed

### 4. Add instrumentation for total tile refs

Log or display:
- total populated tiles
- max lights in any tile
- total flattened tile refs used

This will help confirm whether some rows are under-populated.

## Suggested Agent Task

If handing this to another agent, the task should be:

"Debug the `Proc City` tiled dynamic-lighting seam. Focus on tile assignment correctness in `src/render/render_batches.cpp` and tile lookup in `shaders/proc_city.frag`. Add a tile visualization/debug mode first, then fix the incorrect tile coverage so tiled lighting matches naive proc-lighting without the moving hard cutoff."

## Non-Goals For This Bug

Do not change these yet:
- move to froxels/clustered depth slicing
- bindless resource work
- shadowed-light redesign
- proc-city content changes

Those may come later, but this bug should first be fixed within the current 2D tiled-light prototype.
