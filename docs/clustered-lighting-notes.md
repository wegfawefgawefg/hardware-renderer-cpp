# Clustered Lighting Notes

Clustered lighting is a way to reduce wasted local-light work during raster shading.

It does **not** mean triangles are split into clusters for rendering.

It means the **view space** is split into regions, and each region gets a list of relevant lights.

## The Key Idea

The rasterizer already tells us:

- which surface is visible at a pixel
- the depth of that visible surface

With screen position plus depth, the renderer can recover the fragment's position in view space or world space.

That means a fragment is not "just a 2D pixel." It is a visible 3D surface point.

## Normal Raster Lighting

Without clustering, a fragment shader often does this:

1. get visible surface data
2. loop over all active local lights
3. test distance / cone / shadow for each light

That scales badly when local light counts rise.

## Clustered Lighting

With clustering, the flow becomes:

1. divide the view into tiles or clusters
   - usually screen-space X/Y tiles
   - plus depth slices in view space
2. for each cluster, build a list of lights that overlap it
3. when shading a fragment:
   - determine which cluster it belongs to
   - read that cluster's light list
   - loop only those lights

So the big win is:

- the fragment no longer checks every active light
- it only checks the lights assigned to its cluster

## What A Cluster Owns

A cluster owns:

- a region of view space
- a list of local-light indices

It does **not** own:

- a subset of triangles

Triangles are still rasterized normally.

The cluster system is only helping decide:

- which local lights are relevant for this fragment

## How The Fragment Knows Its Cluster

For a visible fragment we already have:

- screen X/Y
- depth

Those are enough to determine:

- tile X
- tile Y
- depth slice Z

That gives a cluster index.

Then the shader looks up:

- the light list for that cluster

## How Lights Get Into A Cluster

Before the fragment shading step, the renderer tests lights against cluster bounds:

- point light sphere vs cluster box
- spotlight cone or sphere approximation vs cluster box

If they overlap:

- add that light to the cluster list

So the fragment is not "finding lights from scratch." It is using a precomputed light list.

## Why This Helps

Clustered lighting keeps the normal raster geometry path intact, but reduces local-light work a lot.

That makes it a good fit for:

- many streetlights
- many headlights / taillights
- scenes with lots of overlapping local lights

## Relation To This Renderer

The current renderer is still using:

- fixed-size local-light arrays
- per-draw light masks

That is a good intermediate step.

If local light counts grow much more, the natural next step is:

- SSBO light storage
- clustered or tiled light index lists

That would let the renderer support much larger local-light counts without every fragment paying for every light.
