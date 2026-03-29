# Module Cleanup Notes

This note is a direct cleanup reminder: recent prototype work has pushed too much feature-specific logic back into `App` and scene-specific glue. Before these systems grow further, they should be separated into clearer modules.

## Problem

`App` has started to absorb responsibility for systems that are no longer "small one-off experiment helpers."

The main culprits now are:

- mesh deformation / damage application
- cheap decal stamping
- ad hoc audio playback hooks

The issue is not that experimentation happened quickly. The issue is that these now look like reusable subsystems, but they are still wired as:

- `App` methods
- fracture-scene special cases
- direct scene mutation from gameplay glue

That will keep getting worse if more features get stacked on top.

## What Should Be Separated

### 1. Mesh deformation / damage

This should become its own damage module.

Suggested shape:

- `src/damage/mesh_damage.h`
- `src/damage/mesh_damage.cpp`

Responsibilities:

- dent
- punch
- future local mesh damage operators
- mesh-local mutation helpers
- normal recomputation / local cleanup helpers

This module should not know about:

- ImGui
- input handling
- fracture sandbox scene setup
- sound playback
- decal placement

### 2. Decal system

This should become a real dedicated decal module instead of pooled scene entities.

Suggested shape:

- `src/decals/decal_system.h`
- `src/decals/decal_system.cpp`

Responsibilities:

- decal template registration
- active decal instance storage
- placement by hit point / normal / size / roll
- pooling or lifetime management
- renderer-facing instance data

This should not be embedded inside fracture-scene logic long-term.

### 3. Sound / one-shot audio

This should become its own audio module with a tiny internal API.

Suggested shape:

- `src/audio/audio_system.h`
- `src/audio/audio_system.cpp`
- optional helpers for clip loading / sound events

Responsibilities:

- initialize and shut down audio cleanly
- load one-shot clips
- play named or id-based one-shots
- later support pitch / gain / variation

Current ricochet support proved the feature is useful, but it should stop being "just a few extra `App` methods."

## Suggested Internal API Direction

### Audio

A minimal internal API is enough:

- `InitializeAudioSystem()`
- `ShutdownAudioSystem()`
- `LoadOneShot(...)`
- `PlayOneShot(soundId, params)`

Where `params` can later include:

- gain
- pitch
- randomization
- maybe position if spatial sound happens later

This gives us a stable place to put:

- ricochets
- impacts
- footsteps
- UI sounds

without growing `App` every time.

### Decals

Likewise, decals should have a small internal API:

- `RegisterDecalTemplate(...)`
- `SpawnDecal(...)`
- `ClearDecals(...)`
- `UploadDecals(...)` or equivalent renderer bridge

### Damage

Damage should expose operators, not sandbox behavior:

- `ApplyDent(...)`
- `ApplyPunch(...)`
- later more operators

The sandbox should only decide which operator to call.

## What Should Stay In App

`App` should remain the orchestrator, not the owner of all feature logic.

Good `App` responsibilities:

- startup/shutdown order
- scene reload triggers
- top-level update sequencing
- passing events to systems

Bad `App` responsibilities:

- carrying detailed damage logic
- carrying detailed decal logic
- carrying ad hoc sound playback implementation

## Bottom Line

Yes, we polluted `App` again a bit.

Not catastrophically, but enough that it is worth correcting before:

- mesh damage grows
- decals turn into a real feature
- audio gets more than one-off ricochets

The cleanup target should be:

- damage module
- decal module
- audio module

with `App` reduced back to orchestration instead of feature ownership.
