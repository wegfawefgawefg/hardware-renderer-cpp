#include "scene.h"

#include "mesh_loader.h"
#include "texture_loader.h"

SceneData LoadSampleScene()
{
    SceneData scene{};
    scene.mesh = LoadObjMesh(HARDWARE_RENDERER_SAMPLE_OBJ_PATH);
    scene.texture = LoadTexture(HARDWARE_RENDERER_SAMPLE_TEXTURE_PATH);
    return scene;
}
