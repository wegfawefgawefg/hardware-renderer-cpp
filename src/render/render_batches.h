#pragma once

#include <cstdint>

inline std::uint64_t MakeStaticBatchKey(std::uint32_t modelIndex, std::uint32_t primitiveIndex)
{
    return (static_cast<std::uint64_t>(modelIndex) << 32) |
           static_cast<std::uint64_t>(primitiveIndex);
}
