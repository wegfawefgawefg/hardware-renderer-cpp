#include "assets/texture_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdexcept>

TextureData LoadTexture(std::string_view path)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(
        std::string(path).c_str(),
        &width,
        &height,
        &channels,
        STBI_rgb_alpha
    );

    if (pixels == nullptr)
    {
        throw std::runtime_error("Failed to load texture: " + std::string(path));
    }

    TextureData texture{};
    texture.width = static_cast<std::uint32_t>(width);
    texture.height = static_cast<std::uint32_t>(height);
    texture.pixels.assign(
        pixels,
        pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4
    );

    stbi_image_free(pixels);
    return texture;
}

TextureData MakeSolidTexture(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    TextureData texture{};
    texture.width = 1;
    texture.height = 1;
    texture.pixels = {r, g, b, a};
    return texture;
}
