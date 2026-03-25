cmake_minimum_required(VERSION 3.28)

project(hardware_renderer_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(FetchContent)
find_package(Vulkan REQUIRED)
find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_ALSA OFF CACHE BOOL "" FORCE)
set(SDL_JACK OFF CACHE BOOL "" FORCE)
set(SDL_PIPEWIRE OFF CACHE BOOL "" FORCE)
set(SDL_PULSEAUDIO OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO OFF CACHE BOOL "" FORCE)
set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-3.4.2
  GIT_SHALLOW TRUE
)

FetchContent_Declare(
  SDL3_ttf
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG release-3.2.2
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(SDL3)
FetchContent_MakeAvailable(SDL3_ttf)

set(VERT_SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.vert)
set(FRAG_SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/mesh.frag)
set(VERT_SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/mesh.vert.spv)
set(FRAG_SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/mesh.frag.spv)
set(OVERLAY_VERT_SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/overlay.vert)
set(OVERLAY_FRAG_SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/overlay.frag)
set(OVERLAY_VERT_SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/overlay.vert.spv)
set(OVERLAY_FRAG_SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/overlay.frag.spv)
set(LIGHT_VERT_SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/light.vert)
set(LIGHT_FRAG_SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/light.frag)
set(LIGHT_VERT_SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/light.vert.spv)
set(LIGHT_FRAG_SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/light.frag.spv)

add_custom_command(
  OUTPUT ${VERT_SHADER_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
  COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.3 -S vert -o ${VERT_SHADER_OUTPUT} ${VERT_SHADER_SOURCE}
  DEPENDS ${VERT_SHADER_SOURCE}
  VERBATIM
)

add_custom_command(
  OUTPUT ${FRAG_SHADER_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
  COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.3 -S frag -o ${FRAG_SHADER_OUTPUT} ${FRAG_SHADER_SOURCE}
  DEPENDS ${FRAG_SHADER_SOURCE}
  VERBATIM
)

add_custom_command(
  OUTPUT ${OVERLAY_VERT_SHADER_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
  COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.3 -S vert -o ${OVERLAY_VERT_SHADER_OUTPUT} ${OVERLAY_VERT_SHADER_SOURCE}
  DEPENDS ${OVERLAY_VERT_SHADER_SOURCE}
  VERBATIM
)

add_custom_command(
  OUTPUT ${OVERLAY_FRAG_SHADER_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
  COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.3 -S frag -o ${OVERLAY_FRAG_SHADER_OUTPUT} ${OVERLAY_FRAG_SHADER_SOURCE}
  DEPENDS ${OVERLAY_FRAG_SHADER_SOURCE}
  VERBATIM
)

add_custom_command(
  OUTPUT ${LIGHT_VERT_SHADER_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
  COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.3 -S vert -o ${LIGHT_VERT_SHADER_OUTPUT} ${LIGHT_VERT_SHADER_SOURCE}
  DEPENDS ${LIGHT_VERT_SHADER_SOURCE}
  VERBATIM
)

add_custom_command(
  OUTPUT ${LIGHT_FRAG_SHADER_OUTPUT}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
  COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.3 -S frag -o ${LIGHT_FRAG_SHADER_OUTPUT} ${LIGHT_FRAG_SHADER_SOURCE}
  DEPENDS ${LIGHT_FRAG_SHADER_SOURCE}
  VERBATIM
)

add_custom_target(
  hardware-renderer-cpp-shaders
  DEPENDS
    ${VERT_SHADER_OUTPUT}
    ${FRAG_SHADER_OUTPUT}
    ${OVERLAY_VERT_SHADER_OUTPUT}
    ${OVERLAY_FRAG_SHADER_OUTPUT}
    ${LIGHT_VERT_SHADER_OUTPUT}
    ${LIGHT_FRAG_SHADER_OUTPUT}
)

add_executable(hardware-renderer-cpp
  src/app.cpp
  src/animation/character.cpp
  src/animation/ufbx_impl.cpp
  src/assets/gltf_loader.cpp
  src/assets/obj_loader.cpp
  src/assets/texture_loader.cpp
  src/camera.cpp
  src/collision/triangle_collider.cpp
  src/gameplay/player_controller.cpp
  src/main.cpp
  src/math_types.cpp
  src/scene.cpp
  src/vulkan_helpers.cpp
  src/render/renderer.cpp
  src/render/device.cpp
  src/render/frame.cpp
  src/render/pipeline.cpp
  src/render/resources.cpp
  src/render/swapchain.cpp
)

target_compile_options(hardware-renderer-cpp
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-missing-field-initializers
)

target_include_directories(hardware-renderer-cpp
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(hardware-renderer-cpp
  PRIVATE
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    Vulkan::Vulkan
)

target_compile_definitions(hardware-renderer-cpp
  PRIVATE
    HARDWARE_RENDERER_VERT_SHADER_PATH="${VERT_SHADER_OUTPUT}"
    HARDWARE_RENDERER_FRAG_SHADER_PATH="${FRAG_SHADER_OUTPUT}"
    HARDWARE_RENDERER_OVERLAY_VERT_SHADER_PATH="${OVERLAY_VERT_SHADER_OUTPUT}"
    HARDWARE_RENDERER_OVERLAY_FRAG_SHADER_PATH="${OVERLAY_FRAG_SHADER_OUTPUT}"
    HARDWARE_RENDERER_LIGHT_VERT_SHADER_PATH="${LIGHT_VERT_SHADER_OUTPUT}"
    HARDWARE_RENDERER_LIGHT_FRAG_SHADER_PATH="${LIGHT_FRAG_SHADER_OUTPUT}"
    HARDWARE_RENDERER_KENNEY_CHARACTER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/kenney/characterMedium.fbx"
    HARDWARE_RENDERER_KENNEY_TEXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/kenney/survivorMaleB.png"
    HARDWARE_RENDERER_KENNEY_IDLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/kenney/idle.fbx"
    HARDWARE_RENDERER_KENNEY_RUN_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/kenney/run.fbx"
    HARDWARE_RENDERER_KENNEY_JUMP_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/kenney/jump.fbx"
    HARDWARE_RENDERER_MARIO_SCENE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/mario-64-mario/source/prototype_mario_super_mario_64/scene.gltf"
    HARDWARE_RENDERER_SAMPLE_OBJ_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/viking_room.obj"
    HARDWARE_RENDERER_SAMPLE_TEXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/viking_room.png"
)

add_dependencies(hardware-renderer-cpp hardware-renderer-cpp-shaders)
