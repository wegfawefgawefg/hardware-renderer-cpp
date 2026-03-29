# Flat Decal System Spec

This is the intended first real decal system for the renderer.

It is explicitly **not**:
- scene-entity decals
- projected volume decals
- mesh decals
- lifetime/fade driven decals

It is:
- flat quad decals
- alpha blended
- lit by the main scene lighting
- not shadow casting
- capped and overwrite-on-full

## Goals

- Keep decal placement cheap and boring.
- Keep decal rendering separate from `SceneData` entities.
- Support a small reusable set of decal templates and many placed instances.
- Make the renderer draw decals in a dedicated pass instead of pretending they are normal scene objects.

## Constraints

- Templates are capped.
- Live instances are capped.
- No per-frame lifetime updates.
- No fadeout.
- No per-instance material branching.
- No shadow casting.
- Alpha behavior is one shared blended path for v1.

When the instance cap is reached, the oldest slot is overwritten.

## Naming

- `FlatDecalSystem`
- `FlatDecalTemplate`
- `FlatDecalTemplateId`
- `FlatDecalInstance`

This leaves room for future:
- mesh decals
- projected decals

## Template Data

Template data is renderer-facing reusable decal material data:

- `name`
- `albedoAssetPath`
- `normalAssetPath`
- `flipNormalY`

These are template-level properties because they affect batching and descriptor setup.

## Instance Data

Instance data is placement only:

- transform
- template id
- active bit

No lifetime or fade fields are needed for v1.

## API

The minimal API is:

- `ResetFlatDecalTemplates(FlatDecalSystem&)`
- `ClearFlatDecals(FlatDecalSystem&)`
- `RegisterFlatDecalTemplate(FlatDecalSystem&, FlatDecalTemplate)`
- `SpawnFlatDecal(FlatDecalSystem&, FlatDecalTemplateId, hitPosition, hitNormal, size, rollRadians)`

The system internally uses a ring buffer for instances.

## Rendering Model

The renderer owns the GPU-side draw path.

The intended path is:

1. load template textures once during renderer init
2. append them to the renderer texture tables
3. build world-space quad geometry for active decals into dynamic buffers
4. batch decal draws by template
5. render decals in a dedicated blended pass after opaque scene geometry

This gives:
- one shared quad representation
- one draw per template
- no scene entity overhead

## Current V1 Behavior

The implemented v1 path is:

- dedicated `FlatDecalSystem` module
- dedicated renderer flat-decal geometry build
- dedicated blended flat-decal pipeline
- one draw per template
- overwrite-on-full instance ring buffer
- lit decals with optional normal map
- no shadow casting

## Non-Goals For V1

Do not add these yet:

- fadeout/lifetimes
- remove handles
- per-instance blend modes
- projected decal volumes
- mesh decals
- shadow receiving/casting special cases beyond normal main-pass lighting

If we need more later, the next likely step is not more complexity inside this system. It is adding a second decal family with different constraints.
