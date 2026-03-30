# Performance Diagnosis

This document explains why `hardware-renderer-cpp` currently runs much slower
than a modern production engine in the dense city scene, even when the raw
triangle count is modest.

It is not a "the GPU is bad" problem.

It is mostly a renderer architecture and workload-shape problem.

## Short Version

The renderer is currently expensive because it combines several brute-force
choices:

- a fat forward fragment shader in `shaders/mesh.frag`
- dynamic shadow passes in `src/render/shadow.cpp`
- no instancing for repeated meshes in `src/render/resources_scene.cpp`
- no LOD / HLOD system
- no occlusion system
- repeated city geometry drawn again for cascades and shadowed spotlights

Modern engines usually do not keep all of those costs at once.

They aggressively reduce one or more of:

- shaded pixels
- shadow caster count
- draw submission cost
- number of lights considered per pixel
- amount of full-detail geometry rendered at distance

## What We Observed In The City Scene

Recent profiling and feature toggles showed:

- `Material effects` are expensive.
- `Sun shadows` are expensive.
- `Local lights` are very expensive.
- `Local shadows` cost less than local lights, but still matter.
- `Paint splats` are basically irrelevant in the tested city case.
- `Albedo` mode runs far faster than `Lit`.

That means the renderer is not mainly triangle-bound.

It is mostly paying for:

- fragment shading cost
- shadow rendering cost
- light evaluation cost

## Why "160k Tris" Can Still Be Slow

Raw triangle count is not a reliable predictor once the scene becomes
fragment-heavy and shadow-heavy.

In this project:

- the main pass shades a lot of building wall pixels
- the shader does more than plain albedo + one light
- shadow maps redraw scene geometry
- nearby lights add more per-pixel work

So "only 160k triangles" is not the real workload.

The real workload is closer to:

- draw visible geometry
- draw it again for sun cascade 0
- draw it again for sun cascade 1
- draw it again for each shadowed local light
- shade the visible pixels with a fairly expensive forward shader

## What Is Expensive In The Current Shader

The main material path in `shaders/mesh.frag` is not a tiny baseline shader.

It can include:

- albedo sampling
- normal map sampling and tangent reconstruction
- persistent paint / mask sampling
- surface effect logic
- procedural noise / FBM work
- sun lighting
- sun shadow sampling
- point light evaluation
- spotlight evaluation
- shadowed spotlight evaluation
- specular lighting

Even after the debug toggles were added, the toggles showed that:

- turning off material effects gives a large gain
- turning off sun shadows gives a large gain
- turning off local lights gives a very large gain

That is exactly what a fragment-bound forward renderer looks like.

## Shadow Cost Is Real, And It Was Partly Hidden

The shadow cost is not just "sample the shadow texture in the shader."

It also includes generating those shadow maps.

Shadow rendering is handled in `src/render/shadow.cpp`, and until recently the
shadow toggles only disabled shader sampling, not the shadow-map passes.

That meant:

- sun cascades could still be rendered
- shadowed spotlight maps could still be rendered
- `shadowDrawDistance` could still strongly affect performance

That mismatch made the renderer feel confusing until the pass-generation path
was hooked up to the shadow toggles.

There was also a slot-indexing bug in `src/render/frame.cpp` where local shadow
passes were assumed to come after sun cascades even when sun shadows were off.
That caused stale or wrong local shadow behavior until fixed.

## The Renderer Is Still Brute-Force In A Few Important Ways

### 1. No Instancing For Repeated Meshes

In `src/render/resources_scene.cpp`, repeated entity geometry is merged into the
scene buffers per entity.

That means repeated buildings are not currently treated as true GPU instances.

The result:

- more vertex/index data in memory
- heavier uploads / rebuilds
- less efficient repeated-mesh rendering than an instanced path

Modern engines almost always instance repeated world pieces aggressively.

### 2. No LOD / HLOD

City buildings currently stay full detail across distance.

That hurts:

- main pass cost
- shadow pass cost
- bandwidth
- submission count

Modern engines usually swap distant assets to cheaper meshes or cluster them
into hierarchical LOD representations.

### 3. No Occlusion System

Current visibility is mostly frustum + distance based in
`src/render/renderer_visibility.cpp`.

That means:

- buildings behind other buildings can still remain active
- shadow passes can still draw a lot of hidden casters
- dense city views stay more expensive than they should

Production engines usually add one or more of:

- occlusion culling
- depth pyramids / HZB
- portal / cell systems
- chunk-level coarse culling

### 4. Naive Local Light Evaluation

The current renderer is still a direct forward path.

It does not yet use:

- clustered lighting
- tiled lighting
- deferred lighting

So local lights are still expensive relative to what a modern engine would do,
especially in a dense night scene.

That is why headlights and streetlights can hurt more than expected.

## What Big Engines Usually Do Instead

Big engines are faster not because they "just have better code everywhere," but
because they avoid paying full price in the obvious expensive places.

Typical production strategies:

### Instancing

- one mesh
- many transforms
- one repeated submission path

This is the normal answer for repeated buildings, props, fences, lamps, and
other city pieces.

### LOD / HLOD

- nearby: full detail
- mid-distance: reduced detail
- far distance: cluster or billboard style replacement

This dramatically reduces both main-pass and shadow-pass work.

### Better Light Culling

Instead of looping over many active lights for every fragment, engines often
use:

- tiled forward+
- clustered forward+
- deferred lighting

That keeps per-pixel local light work bounded by the actual nearby light set.

### Tighter Shadow Budgets

Production engines usually limit shadows aggressively:

- only a few local lights get shadows
- many lights are unshadowed
- distant objects may not cast shadows
- distant shadow receivers may use cheaper options
- shadow casters may use simplified geometry

The scalable model is not "everything dynamic, everything shadowed."

It is "a small important set is fully dynamic; the rest is approximated."

### Occlusion And World Partitioning

Large worlds are usually organized into:

- chunks
- cells
- sectors
- streaming regions

Only the relevant subsets are submitted.

This is especially important in cities, where a lot of geometry is hidden by
other geometry.

## Why A Huge GPU Does Not Automatically Save This

A fast GPU helps, but it does not erase brute-force renderer structure.

If the renderer still does:

- multiple full shadow passes
- expensive per-pixel forward lighting
- no instancing
- no LOD
- no occlusion

then a powerful GPU just burns through a bigger version of the same waste.

The hardware is not the limiting idea here.

The limiting idea is that the renderer still asks the GPU to do work that
modern engines try hard not to do.

## Practical Priority Order

The most useful improvements are not all equally urgent.

Recommended order:

1. Instance repeated city building meshes.
2. Add city LOD / HLOD.
3. Reduce shadow caster participation for distant city buildings.
4. Tighten the budget for local dynamic lights, especially shadowed ones.
5. Add chunk-level city submission and later occlusion.
6. Consider clustered forward+ if night scenes remain a goal.

## Immediate Mental Model

When the city scene runs poorly, the right mental model is:

"We are still paying modern-game visuals costs with a mostly brute-force
renderer."

Not:

"The GPU should brute-force this because the meshes are simple."

The geometry is simple.

The rendering path is not.
