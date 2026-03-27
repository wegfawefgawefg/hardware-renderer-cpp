# Paint Systems Notes

This repo should keep two paint / mark systems long-term:

- transient world-space splats
- persistent texture-backed accumulation

The current per-object local-space persistent stamp system is a temporary bridge and should be removed once the texture-backed path is in place.

## 1. Transient Splats

Transient splats are the cheap, flexible mark system.

Shape:

- world-space point
- radius
- normal
- color

Render model:

- evaluated in the fragment shader against visible world-space fragments
- not vertex painting
- not texture editing

Good uses:

- soft paint bursts
- goo / slime
- blood hits
- scorch marks
- temporary debug marks

Why keep them:

- simple
- fun
- broad reuse outside paint
- cheap at low / moderate counts

## 2. Remove the Old Persistent Stamp Path

The current persistent system stores per-object local-space stamps and evaluates them in the shader.

Why it should not stay:

- runtime cost still scales with stamp count
- object seams are awkward by design
- it is not true surface accumulation
- it is not generally useful outside the paint mechanic

This system is a stepping stone, not a final architecture.

## 3. Persistent Texture-Backed Accumulation

Persistent paint should become UV texture accumulation.

Target model:

- impacts resolve to entity / primitive / UV
- paint is written into a paint texture for that draw / surface
- mesh shading samples the base texture plus the paint texture
- shading cost stays roughly constant regardless of past hit count

Why this is the right permanent path:

- effectively unbounded paint history
- better long-term performance than looping stamp records forever
- easier to reuse for paint, damage, soot, grime, blood buildup, etc.

Tradeoffs:

- depends on UV quality
- shared / mirrored UVs can produce duplicated paint
- requires texture upload / update infrastructure

## Intended Long-Term Split

- transient splats:
  cheap temporary marks
- texture-backed accumulation:
  real persistent surface modification

That is the paint architecture this repo should aim for.
