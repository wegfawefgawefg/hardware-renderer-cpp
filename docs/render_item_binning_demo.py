from dataclasses import dataclass, field
from enum import Enum, auto


class Pipeline(Enum):
    MAIN = auto()
    PROC_CITY = auto()
    SHADOW = auto()


@dataclass(frozen=True)
class MaterialKey:
    albedo_id: int
    normal_id: int
    material_flags: int


@dataclass(frozen=True)
class MeshKey:
    mesh_id: int
    primitive_index: int


@dataclass(frozen=True)
class RenderKey:
    pipeline: Pipeline
    mesh: MeshKey
    material: MaterialKey
    casts_shadows: bool
    skinned: bool


@dataclass
class InstanceData:
    transform: str
    light_mask: int
    spot_mask: int
    shadowed_spot_mask: int


@dataclass
class RenderItem:
    key: RenderKey
    bounds: str
    instance: InstanceData


@dataclass
class RenderBin:
    key: RenderKey
    items: list[RenderItem] = field(default_factory=list)

    def add(self, item: RenderItem) -> None:
        self.items.append(item)

    @property
    def can_instance(self) -> bool:
        return not self.key.skinned and len(self.items) > 1


def build_render_items(scs: list[dict]) -> list[RenderItem]:
    items: list[RenderItem] = []
    for obj in scene_objects:
        key = RenderKey(
            pipeline=obj["pipeline"],
            mesh=MeshKey(obj["mesh_id"], obj["primitive_index"]),
            material=MaterialKey(
                obj["albedo_id"],
                obj["normal_id"],
                obj["material_flags"],
            ),
            casts_shadows=obj["casts_shadows"],
            skinned=obj["skinned"],
        )
        items.append(
            RenderItem(
                key=key,
                bounds=obj["bounds"],
                instance=InstanceData(
                    transform=obj["transform"],
                    light_mask=obj["light_mask"],
                    spot_mask=obj["spot_mask"],
                    shadowed_spot_mask=obj["shadowed_spot_mask"],
                ),
            )
        )
    return items


def cull_visible(items: list[RenderItem], camera: str) -> list[RenderItem]:
    # Real engines do frustum tests, distance tests, occlusion, portals, chunks, etc.
    # This sketch just pretends some items survive.
    _ = camera
    return [item for index, item in enumerate(items) if index % 2 == 0]


def bin_visible_items(items: list[RenderItem]) -> list[RenderBin]:
    bins_by_key: dict[RenderKey, RenderBin] = {}
    for item in items:
        render_bin = bins_by_key.get(item.key)
        if render_bin is None:
            render_bin = RenderBin(key=item.key)
            bins_by_key[item.key] = render_bin
        render_bin.add(item)
    return list(bins_by_key.values())


def build_draw_lists(bins: list[RenderBin]) -> tuple[list[RenderBin], list[RenderItem]]:
    instanced_bins: list[RenderBin] = []
    single_draw_items: list[RenderItem] = []
    for render_bin in bins:
        if render_bin.can_instance:
            instanced_bins.append(render_bin)
        else:
            single_draw_items.extend(render_bin.items)
    return instanced_bins, single_draw_items


def main() -> None:
    scene_objects = [
        {
            "pipeline": Pipeline.PROC_CITY,
            "mesh_id": 1,
            "primitive_index": 0,
            "albedo_id": 10,
            "normal_id": 11,
            "material_flags": 8,
            "casts_shadows": True,
            "skinned": False,
            "bounds": "box_aabb_0",
            "transform": "building_transform_0",
            "light_mask": 0b0011,
            "spot_mask": 0b0101,
            "shadowed_spot_mask": 0b0001,
        },
        {
            "pipeline": Pipeline.PROC_CITY,
            "mesh_id": 1,
            "primitive_index": 0,
            "albedo_id": 10,
            "normal_id": 11,
            "material_flags": 8,
            "casts_shadows": True,
            "skinned": False,
            "bounds": "box_aabb_1",
            "transform": "building_transform_1",
            "light_mask": 0b0001,
            "spot_mask": 0b0101,
            "shadowed_spot_mask": 0b0001,
        },
        {
            "pipeline": Pipeline.MAIN,
            "mesh_id": 7,
            "primitive_index": 0,
            "albedo_id": 22,
            "normal_id": 23,
            "material_flags": 0,
            "casts_shadows": True,
            "skinned": False,
            "bounds": "prop_aabb_0",
            "transform": "cone_transform",
            "light_mask": 0b1111,
            "spot_mask": 0b0000,
            "shadowed_spot_mask": 0b0000,
        },
    ]

    all_items = build_render_items(scene_objects)
    visible_items = cull_visible(all_items, camera="main_camera")
    visible_bins = bin_visible_items(visible_items)
    instanced_bins, single_draw_items = build_draw_lists(visible_bins)

    print("visible items:", len(visible_items))
    print("instanced bins:", len(instanced_bins))
    print("single draws:", len(single_draw_items))


if __name__ == "__main__":
    main()
