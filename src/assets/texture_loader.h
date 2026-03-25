#pragma once

#include <string_view>

#include "scene.h"

TextureData LoadTexture(std::string_view path);
TextureData MakeSolidTexture(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);
