# Ideas

This is the running list of gameplay / renderer experiments that fit this repo's direction.

## Character / Animation

- gun or held-object test scene for pinning a prop to the player's hand
- crouch animation with blend support
- broader animation blending
- feet reaching toward nearby ground via raycasts
- hands reaching toward nearby surfaces or props via raycasts

## Combat / Interaction

- simple shooting
- decals
- paint gun

## Cheap Quad Decals

Add a separate cheap decal path that is intentionally *not* the heavier box/projector decal system.

Target shape:

- a floating quad stamp
- centered from a hit point
- oriented from the hit normal
- width and height controls
- a small normal offset so it can sit just above the hit surface
- image/texture-driven rather than only procedural blobs

Why this version:

- prioritize very high decal counts over robustness
- acceptable artifacts are fine if the system is cheap enough to allow hundreds or thousands of marks
- good fit for bullet holes, scorch marks, blood marks, graffiti-like stamps, and other cheap repeated impacts

Tradeoff versus box/projected decals:

- cheaper and simpler
- easier to spam at high counts
- but more likely to clip, float, z-fight, or look wrong across corners / uneven geometry

This should stay a consciously separate design from box/projected decals:

- cheap quad decals = high count, low complexity, "good enough"
- box/projected decals = more robust, more expensive, better on complex geometry

## Paint Gun

The paint gun idea is stronger than a pure decal test because it can become real scene interaction.

Minimal version:

- bind fire to a key or mouse button
- spawn a bouncing paint ball
- on impact, add a paint splat

Preferred version:

- paint does not just place a flat decal
- paint writes into a per-object paint layer / mask
- the object's shader blends that paint layer over the base texture
- impacts accumulate into a blobby painted surface

That keeps the effect feeling like actual paint instead of sticker decals.

## Enemies

- simple enemies that move toward the player
- primitive enemy gibbing
- cheap AABB-style physics for gibs

The goal is scale and readability over simulation fidelity.

## Destruction

- break chunks off buildings
- break chunks off light poles

This is a heavier feature because it starts pulling on scene representation and spawned debris behavior.

## Suggested Order

1. crouch + better animation blending
2. paint gun or simple projectile interaction
3. shooting + decals / paint
4. gun / held-object attachment
5. basic enemies
6. gibbing
7. destructible props / chunks
8. more advanced foot/hand placement

That order gives fast visible wins without immediately dragging the repo into the heaviest systems.
