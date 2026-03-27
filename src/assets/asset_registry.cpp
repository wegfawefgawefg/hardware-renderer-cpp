#include "assets/asset_registry.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace
{
std::string NormalizeKey(std::string_view path)
{
    std::string key(path);
    std::replace(key.begin(), key.end(), '\\', '/');
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return key;
}
}

void AssetRegistry::ScanFbx(const std::filesystem::path& assetRoot)
{
    root = assetRoot;
    fbxRelativePaths.clear();
    fbxByRelativePath.clear();

    if (!std::filesystem::exists(root))
    {
        return;
    }

    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        std::string extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        std::filesystem::path relativePath = std::filesystem::relative(entry.path(), root);
        std::string relativeString = relativePath.generic_string();
        fbxByRelativePath.emplace(NormalizeKey(relativeString), entry.path());

        if (extension != ".fbx")
        {
            continue;
        }

        fbxRelativePaths.push_back(relativeString);
    }

    std::sort(fbxRelativePaths.begin(), fbxRelativePaths.end());
}

const std::filesystem::path* AssetRegistry::FindByRelativePath(std::string_view relativePath) const
{
    const auto it = fbxByRelativePath.find(NormalizeKey(relativePath));
    if (it == fbxByRelativePath.end())
    {
        return nullptr;
    }
    return &it->second;
}
