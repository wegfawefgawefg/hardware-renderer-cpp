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
