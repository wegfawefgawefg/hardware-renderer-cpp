#include "render/renderer.h"

#include <cstring>

void VulkanRenderer::UpdatePersistentPaintData(const std::vector<EntityPaintLayer>& entityLayers)
{
    for (DrawItem& drawItem : m_drawItems)
    {
        drawItem.persistentPaintOffset = 0;
        drawItem.persistentPaintCount = 0;
    }

    if (m_persistentPaintBuffer.mapped == nullptr)
    {
        return;
    }

    std::array<PersistentPaintGpuStamp, kMaxPersistentPaintStamps> gpuStamps{};
    std::uint32_t nextStamp = 0;
    for (std::uint32_t entityIndex = 0; entityIndex < entityLayers.size(); ++entityIndex)
    {
        const EntityPaintLayer& layer = entityLayers[entityIndex];
        if (layer.stampCount == 0 || nextStamp >= kMaxPersistentPaintStamps)
        {
            continue;
        }

        std::uint32_t count = std::min(layer.stampCount, kMaxPersistentPaintStamps - nextStamp);
        std::uint32_t ringStart = layer.stampCount == kMaxAccumulatedPaintPerEntity ? layer.nextStampIndex : 0;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const PersistentPaintStamp& stamp = layer.stamps[(ringStart + i) % kMaxAccumulatedPaintPerEntity];
            gpuStamps[nextStamp + i] = PersistentPaintGpuStamp{
                .positionRadius = Vec4Make(stamp.localPosition.x, stamp.localPosition.y, stamp.localPosition.z, stamp.radius),
                .normalSeed = Vec4Make(stamp.localNormal.x, stamp.localNormal.y, stamp.localNormal.z, stamp.seed),
                .colorOpacity = Vec4Make(stamp.color.x, stamp.color.y, stamp.color.z, stamp.opacity),
            };
        }

        for (DrawItem& drawItem : m_drawItems)
        {
            if (drawItem.entityIndex != entityIndex)
            {
                continue;
            }
            drawItem.persistentPaintOffset = nextStamp;
            drawItem.persistentPaintCount = count;
        }
        nextStamp += count;
    }

    std::memcpy(
        m_persistentPaintBuffer.mapped,
        gpuStamps.data(),
        sizeof(PersistentPaintGpuStamp) * kMaxPersistentPaintStamps
    );
}
