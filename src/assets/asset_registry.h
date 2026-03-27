#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct AssetRegistry
{
    void ScanFbx(const std::filesystem::path& root);
    const std::filesystem::path* FindByRelativePath(std::string_view relativePath) const;

    std::filesystem::path root;
    std::vector<std::string> fbxRelativePaths;
    std::unordered_map<std::string, std::filesystem::path> fbxByRelativePath;
};
