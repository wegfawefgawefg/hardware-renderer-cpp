#pragma once

#include "render/renderer.h"
#include "state.h"

void ApplyRuntimeLighting(State& state, SceneUniforms& uniforms, float dtSeconds);
