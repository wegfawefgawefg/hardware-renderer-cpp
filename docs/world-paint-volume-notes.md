# World Paint Volume Notes

This note captures the idea of a persistent paint system that is *not* tied to mesh UVs.

The motivating problem is the current city kit:

- many assets use tightly packed, repeated, or trim-style UVs
- per-object UV texture painting works on some meshes, but fails badly on roads, poles, and tall buildings
- a flat world paint mask only works for ground and would smear vertically onto walls

So the interesting alternative is a sparse 3D world paint volume.

## Core Idea

Store persistent paint in chunked world-space volumes rather than in object UVs.

Each chunk would own a low-resolution 3D paint texture or paint density/color field.

A paint hit would:

- find the affected world chunk(s)
- write paint into the local 3D cells around the impact
- allow the paint to persist independently of whatever mesh happens to occupy that world region

At render time, visible fragments would:

- map their world position into the relevant chunk volume
- sample the paint volume
- blend that paint onto the fragment

This makes paint depend on *where it is in the world*, not on the asset's UV layout.

## Why This Is Interesting

It avoids the main problem with per-object UV painting:

- no dependence on whether a mesh has nice unique UVs
- roads, poles, buildings, and arbitrary props can all participate

It also avoids the limitation of a flat ground-only chunk mask:

- paint can exist at different heights
- paint on walls does not automatically smear across all geometry at the same XY location

Conceptually this is closer to:

- sparse voxel paint

than to:

- decal stamps
- per-object texture painting

## Expected Shape

The useful version is not a giant dense world voxel grid.

It would need to be:

- chunked
- sparse / allocated only where paint exists
- lowish resolution by default
- streamed or resident only near the player/camera

Likely design:

- divide world space into cubic or box-shaped paint chunks
- only allocate chunks when paint is written there
- keep nearby chunks higher fidelity / more available
- allow farther chunks to be lower resolution or unloaded entirely

The rough intuition is similar to shadow cascades or world streaming:

- nearby paint data matters most
- distant paint can be coarser or absent

## Why This Could Work Here

This repo does not need hyper-realistic paint fidelity.

Low-resolution, stylized, persistent painting would still be useful for:

- paint gun gameplay
- grime / soot buildup
- blood
- damage staining
- territory marking

So the system can optimize for:

- scale
- persistence
- flexibility across many mesh types

rather than for detailed per-texel realism.

## Main Tradeoffs

Benefits:

- works across arbitrary mesh UV layouts
- supports persistence on ground, walls, poles, and props
- shading cost can stay bounded if sampling is simple
- memory can be controlled with sparse chunk allocation

Costs:

- more infrastructure than transient splats or quad decals
- more complex write path for paint impacts
- more complex chunk residency / streaming logic
- more expensive than a 2D ground mask
- harder filtering / aliasing problems than object-UV painting

## Comparison To Other Paint Systems

### Transient splats

Good for:

- temporary soft marks
- cheap impacts
- reusable gameplay effects

Weak for:

- true persistence at high counts

### Cheap quad decals

Good for:

- high-count texture-driven impact marks
- bullet holes, blood, scorch marks, graffiti-like stamps

Weak for:

- true painted accumulation over time
- soft volumetric paint behavior

### Per-object UV accumulation

Good for:

- assets with sane unique UVs

Weak for:

- trim-sheet / tiled / repeated UV assets
- general city-world painting

### 3D world paint volumes

Good for:

- arbitrary surfaces
- persistence
- UV-independent paint behavior

Weak for:

- implementation complexity
- storage / streaming complexity

## Likely Practical Order

If this idea is pursued later, the likely order is:

1. keep transient splats
2. add cheap quad decals for general textured impact marks
3. prototype sparse 3D world paint chunks in a limited test scene
4. only then consider replacing broad persistent paint behavior with world-volume storage

This should be treated as a larger rendering/gameplay infrastructure project, not a quick shader tweak.
