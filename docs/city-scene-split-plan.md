# City Scene Split Plan

This doc describes the intended split between the existing `City` scene and a
new `Proc City` scene.

The goal is to stop forcing one city demo to serve two conflicting purposes:

- feature experimentation
- performance-oriented large city rendering

Those should be separate scenes.

## Why Split The City Scene

The current city work has mixed together too many concerns:

- Kenney authored city assets
- procedural building generation
- paint and surface-mask experimentation
- deformation / damage experimentation
- lighting stress testing
- performance testing

That makes the scene hard to reason about.

Whenever one feature is added for experimentation, the city gets slower and
harder to optimize. Whenever the city is simplified for performance, it fights
the needs of paint, damage, or dense generated topology.

The right move is to split those goals into separate scenes.

## Scene Roles

### `City`

`City` should go back to being the authored-content baseline.

Use it for:

- Kenney buildings
- roads
- traffic
- street lights
- traversal
- baseline lighting tests
- general city mood and layout

Keep it relatively simple and stable.

Things that do not need to live in default `City` anymore:

- deformation-heavy building workflows
- dense generated building topology
- surface-mask brush workflows
- experimental effect-heavy building materials

Optional:

- paint-ball shooting can stay if it remains lightweight and is treated as a
  toy rather than a core renderer requirement

### `Proc City`

`Proc City` should be a dedicated high-performance procedural city scene.

Use it for:

- procedural buildings
- procedural road tiles
- very long draw distance goals
- culling experiments
- LOD / HLOD experiments
- scalability experiments
- performance-first visual design

`Proc City` should not inherit every expensive experimental rendering feature
from the other demos.

It should be intentionally constrained.

## Key Architectural Rule

`Proc City` should not use the same fat general-purpose material path as the
feature demos.

That shared shader approach is convenient, but it is part of why the renderer
became expensive in the city.

The current shared shader path accumulates unrelated costs:

- paint mask logic
- vanish / grime / wetness effects
- procedural effect noise
- deformation-oriented material behavior
- local-light stress test behavior

Those features are valid in their own demos.

They are not valid as the default shading contract for a performance-oriented
city renderer.

## Shader Strategy

The repo should move toward scene-appropriate material paths instead of one
"do everything" shader.

### Rich Shader Path

Keep the current richer shader path for:

- fracture / destruction
- paint and surface-mask demos
- effect experimentation

That path can stay flexible and more expensive.

### Lean `Proc City` Shader Path

`Proc City` should use a lean material path.

Target features:

- albedo
- optional normal map
- ambient + sun diffuse
- optional nearby shadows
- small local-light budget if needed
- tiled UVs derived procedurally

Things it should avoid:

- paint mask sampling
- vanish / grime / wetness effect paths
- procedural FBM/noise material work
- deformation-related material logic
- any effect that materially harms fragment cost without helping large-scale
  city readability

The principle is:

"If a feature does not help the distant city read better, it probably does not
belong in `Proc City`."

## Geometry Strategy For `Proc City`

`Proc City` should take the deliberately simple route.

### Buildings

Buildings should default to very low-poly forms:

- one quad per wall face
- one quad for roof faces where possible
- very low triangle count per building
- texture tiling used to create facade repetition

Geometry density should not be used to create facade detail.

Use:

- tiled materials
- silhouette variation
- footprint variation
- height variation

Do not use:

- dense wall subdivisions just to carry visual paneling

### Roads

Roads should also move toward simple procedural generation rather than relying
entirely on authored tile assets if that helps batching, consistency, and
scalability.

The point is not "more procedural because procedural is cool."

The point is:

- simpler content contract
- better control over batching and LOD
- easier tuning for large-distance rendering

## Performance Rules For `Proc City`

`Proc City` should be governed by explicit performance rules.

### Rule 1: Few Expensive Pixels

Avoid expensive fragment work.

That means:

- lean shader
- minimal dynamic material effects
- cautious local-light usage

### Rule 2: Few Shadow Casters

Nearby important geometry can cast shadows.

Distant geometry should often:

- stop casting
- use cheaper caster meshes
- use reduced participation rules

### Rule 3: Few Submitted Objects

Use:

- instancing
- chunking
- coarse culling
- later occlusion if needed

Do not submit the whole city blindly just because it exists.

### Rule 4: Low Detail At Distance

`Proc City` should be built around LOD and later HLOD from the start.

That means:

- cheap near mesh
- cheaper far mesh
- clustered representations later if needed

### Rule 5: Hard Light Budgets

Street lights and headlights should live under a strict policy:

- only a small nearby set active
- only a smaller subset shadowed
- no assumption that every visible light deserves full dynamic shadowing

## Proposed Scene Map

The project should roughly shake out like this:

- `Player Mask Test`: paint / mask / effect experiments
- `Fracture Test`: deformation, decals, generated mesh damage
- `City`: authored baseline city using Kenney buildings
- `Proc City`: performance-first procedural city
- other specialty scenes remain narrow and purposeful

That is much healthier than asking one city scene to prove every subsystem.

## Implementation Order

Recommended order:

1. Restore Kenney buildings to `City`.
2. Add `Proc City` as a new `SceneKind`.
3. Start `Proc City` from the current city layout skeleton.
4. Strip `Proc City` down to lean shading and simple building geometry.
5. Add instancing, chunking, and LOD there without worrying about paint or
   deformation constraints.

## Success Criteria

The split is successful when:

- `City` is stable, readable, and good for authored-content traversal
- `Proc City` is obviously faster and can sustain much larger draw distances
- feature-demo code stops leaking into the scalability path
- renderer profiling becomes easier to interpret scene by scene

## The Core Decision

The important decision is not merely "procedural vs authored."

The real decision is:

- `City` is for content and atmosphere
- `Proc City` is for scalability and speed

That distinction should drive both scene content and renderer design.
