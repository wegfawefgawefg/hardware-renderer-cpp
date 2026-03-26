# Rendering Scalability Notes

This renderer is currently a straightforward forward rasterizer with dynamic shadows and local lights.

That is good for learning, but it does not scale the same way a modern production renderer does.

## Why The Current Night Path Gets Expensive

- The sun uses cascaded shadow maps.
- Nearby shadowed spotlights add more shadow-map passes.
- The main fragment shader loops over all active local lights per fragment.
- Until basic culling is in place, the renderer still submits geometry that is off-screen.

That means the GPU cost grows from both sides:

- more shadow rendering work
- more per-pixel lighting work

## Forward Vs Deferred Vs Clustered

### Naive Forward

The current main path is naive forward shading:

- rasterize visible geometry
- in the fragment shader, loop over the active light arrays

This is simple, but it scales poorly when local light counts rise.

### Deferred

Deferred rendering usually means:

1. write a G-buffer
   - depth
   - normals
   - albedo
   - material properties
2. run lighting in a later pass using that screen-space data

Why it helps:

- many local lights can be evaluated without re-running material/geometry shading for every object

Tradeoffs:

- more bandwidth and memory
- more awkward with transparency

### Clustered / Tiled Forward+

Clustered lighting means:

- split the view into tiles or clusters
- assign only relevant lights to each tile/cluster
- each fragment loops only the small local list for its cluster

Why it helps:

- keeps a forward-style material path
- avoids looping over every active light for every fragment
- scales much better for many local lights

For this renderer, clustered/tiled forward+ is likely the better long-term answer than jumping straight to a full deferred pipeline.

## Culling Matters First

Before changing lighting architecture, basic visibility culling should be in place.

Important first steps:

1. main-camera frustum culling
2. chunk/cell-based city submission
3. distance culling / LOD
4. later, optional occlusion culling

At the moment, the biggest obvious missing optimization is frustum culling.

## World Structure

Production games usually do not treat the entire world as one permanently submitted blob.

Typical structure:

- world split into chunks / sectors / cells
- frustum culling chooses visible chunks
- only visible chunk draw lists are submitted
- shadows may use their own culling rules
- streaming loads and unloads distant content

This renderer does not need full world streaming yet, but chunked city submission is the next natural step after basic frustum culling.

## Shadow Cost

Directional shadows:

- usually use cascaded shadow maps
- higher quality near the player
- more expensive than one global shadow map, but much better allocation of texels

Local shadowed lights:

- should stay on a tight budget
- nearest / most relevant lights get shadows
- the rest stay unshadowed

That is the practical scalable model for streetlights and headlights.

## Recommended Order

1. basic frustum culling
2. better profiler breakdown
   - sun shadows
   - local spot shadows
   - main pass
3. chunk the city
4. only then consider clustered or deferred local lighting

This keeps the renderer understandable while removing the biggest obvious waste first.
