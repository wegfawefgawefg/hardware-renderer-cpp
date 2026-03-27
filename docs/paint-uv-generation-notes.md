# Paint UV Generation Notes

This note captures the idea of generating a second UV set specifically for persistent paint and material-layer masks.

The motivation is the current split between:

- base material UVs that are good enough for normal asset texturing
- paint UV requirements that are much stricter if we want local editable persistence

Many assets can have UVs that are perfectly valid for albedo/material texturing while still being poor for UV-backed painting.

Examples:

- trim-sheet style buildings
- repeated road UVs
- tiny packed islands on props
- shared or mirrored texture regions

Those assets can look correct in-game and in Blender while still being a bad fit for texture-backed paint accumulation.

## Goal

Add an alternative UV channel whose purpose is *not* base texturing.

Instead, it would exist specifically for:

- paint masks
- grime masks
- wetness masks
- glow masks
- burn / scorch masks
- vanish / dissolve masks
- other gameplay-authored material layers

So the intended material model would become:

- original UV set:
  authored albedo / base material UVs
- generated paint UV set:
  stable persistent mask space

This keeps the original asset look intact while giving the game a separate editable surface layer.

## Important Clarification

The goal is **not**:

- a magical seamless single-sheet unwrap with no discontinuities

For arbitrary 3D meshes, that is not realistic.

The real goal is:

- cover the whole object in `0..1`
- minimize overlap
- keep distortion reasonable
- place seams in controlled / acceptable places
- give enough unique texel area to make local paint edits meaningful

So the correct standard is:

- "good enough with controlled seams"

not:

- "perfectly continuous everywhere"

## What This Would Look Like

The practical version is an **offline asset-processing step**.

For each supported asset:

1. load mesh geometry
2. compute or generate a second UV unwrap
3. pack the resulting charts/islands into `0..1`
4. export or cache the mesh with the extra UV channel

At runtime:

- base shading continues to use the original UVs
- paint/material-layer sampling uses the generated paint UVs

This avoids trying to solve UV generation during gameplay or scene load.

## Why Not Runtime

Doing this at runtime is possible in theory but not a good fit here.

Problems:

- too much complexity in the load path
- hard to debug
- expensive for large scenes
- poor place to run a mesh-unwrap algorithm
- worse reproducibility than an offline cache/build step

So if this idea is pursued, it should be:

- offline
- deterministic
- asset-pipeline driven

## Why This Is Attractive

It preserves the more general layered-mask material idea:

- several scalar or packed channels
- gameplay can write to them
- shaders can mix grime / glow / wetness / damage / vanish at fragment time

That system is useful even if it is not applied to every random city-kit asset.

This approach would let selected assets become:

- intentionally paintable
- intentionally damageable
- intentionally mask-driven

without depending on the asset's original texture UV layout.

## Relationship To Other Approaches

### Original asset UV painting

Pros:

- simple concept
- no extra UV channel

Cons:

- fails on trim-sheet / tiled / mirrored assets

### Generated secondary paint UVs

Pros:

- keeps base materials untouched
- makes texture-backed persistent masks viable
- general enough for multiple mask layers

Cons:

- needs offline asset processing
- still has seams
- still needs good packing and texel density choices

### World/projected paint

Pros:

- not tied to mesh UVs

Cons:

- different storage and projection problems
- broader renderer/gameplay infrastructure cost

### Cheap decals / transient splats

Pros:

- simple and broadly useful

Cons:

- not the same as persistent material-layer editing

## Best Use

This approach is probably best for:

- hero props
- characters
- enemies
- weapons
- selected destructible or interactable objects

It is probably *not* the first thing to do for:

- every generic road tile
- every generic building in a trim-sheet kit
- every kitbashed prop in a large city

## Summary

This is a strong candidate for keeping the layered-mask material idea alive without forcing it onto unsuitable original UV layouts.

The right shape is:

- do not replace original material UVs
- generate a second UV set offline
- use that second UV set for persistent paint / mask layers

That gives the project a path toward:

- grime
- glow
- wetness
- damage
- dissolve/vanish
- paint accumulation

without depending on whether the base asset UVs were authored for editable persistence.
