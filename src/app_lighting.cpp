#include "app.h"

#include "lighting/runtime.h"

void App::ApplyLighting(SceneUniforms& uniforms, float dtSeconds)
{
    ApplyRuntimeLighting(m_state, uniforms, dtSeconds);
}
