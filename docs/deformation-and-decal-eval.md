# Mesh Deformation And Decal Evaluation

This note is an evaluation of the current fracture-sandbox deformation and damage-decal work. It is not an implementation plan yet. The goal is to decide whether the current code is clean enough to keep extending, and what should be split before the systems grow further.

## Short Answer

Yes, the current deformation/decal work is already intermixed enough that it should be isolated more cleanly before it expands much further.

The current state is acceptable for an experiment, but not a good long-term home for:

- mesh deformation
- topology-changing damage
- cheap high-count decals
- audio/event hooks attached to damage placement

The right direction is:

1. keep the current sandbox behavior working
2. split mesh damage into its own module layer
3. build a real decal system that is not based on normal scene entities

## What Exists Now

### Mesh damage

Current mesh damage mostly lives in:

- [destruction_mesh.h](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/gameplay/destruction_mesh.h)
- [destruction_mesh.cpp](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/gameplay/destruction_mesh.cpp)

This is the right file pair conceptually. The actual algorithms are already somewhat self-contained:

- `Dent`
- `Punch`

and they operate on:

- `SceneData`
- `ModelData`
- collider ray hits

That part is not terrible.

### Fracture sandbox runtime

Current sandbox orchestration lives in:

- [fracture_runtime.cpp](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/gameplay/fracture_runtime.cpp)
- [windows_fracture.cpp](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/ui/windows_fracture.cpp)
- [scene_fracture.cpp](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/scene_fracture.cpp)

This file set currently mixes together:

- sandbox-specific hit logic
- generated-prism scene setup
- decal pool ownership
- fracture-mode switching
- fire cadence
- debug hit state
- decal placement
- ricochet trigger hookup

This is workable for a toy scene, but it is already blending:

- gameplay decisions
- test-scene authoring
- prototype rendering strategy

in a way that will get messy if reused outside `Fracture Test`.

### Damage decals

Current damage decals are not a real decal system yet.

They are:

- preallocated quad entities in [scene_fracture.cpp](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/scene_fracture.cpp)
- repositioned from [fracture_runtime.cpp](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/gameplay/fracture_runtime.cpp)

This means:

- each decal is a scene entity
- each decal is a normal draw item
- each decal goes through the regular scene/model/material path

That is fine for the current experiment. It is not the right architecture for a cheap large-count decal system.

## Is The Current Deformation Code Too Polluted?

### Mostly yes, but in a specific way

The actual deformation math is not the main problem.

The mess is more about ownership and boundaries:

- `destruction_mesh.cpp` owns deformation logic, which is good
- `fracture_runtime.cpp` owns decal placement and sandbox firing logic, which is okay for now
- but the overall feature is still coupled to:
  - a generated scene
  - ad hoc scene rebuild paths
  - a sandbox-specific decal pool
  - generic `App` methods

So the code is not "hopelessly tangled," but it is already leaning too hard on:

- `App`
- `State`
- fracture-scene special cases

for systems that should eventually be reusable.

### The main smell

The biggest architectural smell right now is that we do not have clean separation between:

- damage authoring/input
- damage simulation/mutation
- damage rendering representation

For example:

- clicking in the fracture sandbox directly chooses a damage mode
- then directly mutates scene/model data
- then directly refreshes scene resources
- decals are handled as scene entities instead of a separate render structure

That is normal for a prototype, but it will become painful if:

- other scenes want to use dents
- weapons want to stamp decals
- explosions want different damage policies
- destruction wants audio/VFX hooks

## Recommended Module Split

If this gets cleaned up, the split should be something like this.

### 1. Damage operators

Own only the geometry mutation logic.

Suggested home:

- `src/damage/mesh_damage.h`
- `src/damage/mesh_damage.cpp`

Responsibilities:

- `ApplyDent(...)`
- `ApplyPunch(...)`
- later other operators

Should know about:

- mesh/model data
- local transforms
- hit shapes

Should not know about:

- ImGui
- scene selection
- decal spawning
- audio

### 2. Damage sandbox / gameplay glue

Own only "what happens when the player clicks."

Suggested home:

- current [fracture_runtime.cpp](/home/vega/Coding/Graphics/hardware-renderer-cpp/src/gameplay/fracture_runtime.cpp), but thinner

Responsibilities:

- raycast to get hit
- choose damage mode
- call damage operator
- request renderer/collider refresh
- trigger sound/VFX/decal stamps

This layer should not own the actual mutation algorithms.

### 3. Decal system

Own decal templates, placement, storage, and rendering.

Suggested home:

- `src/decals/decal_system.h`
- `src/decals/decal_system.cpp`

Responsibilities:

- register decal templates
- store active decal instances
- place decals by hit point/normal/size/roll
- manage decal lifetime or pool reuse
- feed a render path

This should not be done through generic scene entities long-term.

## Should We Build A Real Decal System?

Yes.

The current entity-based quad pool was the right prototype, but it is not the right permanent solution if the goal is:

- lots of bullet dents
- repeated machine-gun impacts
- shotgun sprays
- many templates later

### What the real system should look like

A cheap dedicated decal system should have:

- a small set of registered decal templates
  - albedo
  - normal
  - optional extra parameters later
- a flat array of active decal instances
  - position
  - normal
  - size
  - roll
  - template id
  - maybe tint / lifetime
- one shared quad mesh
- one or a few draws, ideally instanced

That gives:

- far cheaper scaling than scene entities
- less scene pollution
- much simpler template management

### Template idea

The user-facing mental model should be:

- register `n` decal templates
- stamp them by id or choose randomly from a group

For example:

- `metal_dent_small`
- `metal_dent_large`
- `ricochet_mark_01`
- `burn_scorch_01`

Then gameplay can do:

- bullet impact -> choose from metal dent family
- explosion -> choose from scorch family

That is the correct system shape.

## How Cheap Could It Be?

Much cheaper than the current entity-based prototype.

The current prototype cost model is:

- one entity per decal
- one normal scene draw path per decal

A dedicated decal system can move to:

- one quad mesh
- one material set per template group
- one instance buffer
- one instanced draw per template group, or a compact atlas approach

That is the version that can plausibly support:

- hundreds comfortably
- likely low-thousands with sane batching

Not "10k for free," but dramatically better than now.

## What Should Happen Next

Recommended order:

1. keep the current fracture sandbox working as a behavior testbed
2. do not add more long-term features to the entity-based decal path
3. split mesh damage logic into a dedicated module namespace/folder
4. build a real decal system with:
   - template registry
   - instance storage
   - dedicated render path
5. then reconnect the fracture sandbox to use that real decal system

## Bottom Line

Answers to the original questions:

1. Is the mesh deformation code messy/intermixed?
   - somewhat yes
   - the math itself is okay
   - the ownership/boundary story is the messy part

2. Should it be modularized?
   - yes
   - especially to separate:
     - mesh damage operators
     - sandbox/gameplay glue
     - decal rendering/storage

3. Should there be a real isolated decal system with registered templates?
   - definitely yes
   - that is the right next architecture if decals are going to matter

The current sandbox got us the important answers:

- `Dent` is promising
- cheap quad damage marks are visually useful
- the entity-based decal approach is good for proving the idea
- but not the final system shape
