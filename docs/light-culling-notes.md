# Light Culling Notes

This renderer currently treats the sun and moon as global lights and local lights as bounded influences.

## Current Structure

- sun / moon:
  apply globally to visible geometry
- local lights:
  point lights, spotlights, and shadowed spotlights

That is a good split.

## Why Per-Draw Light Culling Helps

Without culling, every visible fragment loops over every active local light.

Even if:

- the object is nowhere near a light
- the light cannot possibly affect the object

the fragment shader still pays the loop cost.

Per-draw light culling fixes that by assigning only relevant local lights to each draw.

## Conservative First Pass

The simplest useful version is:

1. compute a world-space bounding sphere for each draw
2. test that sphere against local light influence bounds
3. build bitmasks per draw
4. fragment shader skips local lights whose bits are not set

This is conservative:

- point lights use source position + range
- spotlights also use source position + range as a coarse outer bound

That means some unnecessary lights may still be assigned, but it is still much better than assigning every light to every draw.

## Why Not Start With Exact Cone Tests

Exact cone-vs-bounds tests are possible, but they are not necessary for the first optimization pass.

The first win is:

- object nowhere near light source range
- skip it entirely

That is cheap, robust, and easy to reason about.

## Relationship To Clustered / Deferred

Per-draw light masking is not the final scalable solution for very large light counts.

Longer-term options are:

- clustered / tiled forward+
- deferred lighting

But per-draw masking is a good intermediate step because it:

- keeps the renderer simple
- reduces obvious wasted fragment work
- works well with the current draw-item architecture

## World Structure Reminder

The large static triangle collider is for CPU collision/query work, not rendering.

Rendering still uses:

- scene models
- scene entities
- renderer draw items

So buildings and props are still rendered as normal scene objects, one draw item at a time.

That means renderer-side culling and light assignment still make sense even though the collision system flattens the static scene for gameplay queries.
