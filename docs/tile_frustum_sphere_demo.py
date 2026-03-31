from __future__ import annotations

from dataclasses import dataclass
from math import sqrt


@dataclass
class Vec3:
    x: float
    y: float
    z: float

    def dot(self, other: "Vec3") -> float:
        return self.x * other.x + self.y * other.y + self.z * other.z

    def length(self) -> float:
        return sqrt(self.dot(self))

    def normalized(self) -> "Vec3":
        n = self.length()
        if n <= 1e-8:
            return Vec3(0.0, 0.0, 0.0)
        return Vec3(self.x / n, self.y / n, self.z / n)

    def __add__(self, other: "Vec3") -> "Vec3":
        return Vec3(self.x + other.x, self.y + other.y, self.z + other.z)

    def __sub__(self, other: "Vec3") -> "Vec3":
        return Vec3(self.x - other.x, self.y - other.y, self.z - other.z)

    def __mul__(self, scalar: float) -> "Vec3":
        return Vec3(self.x * scalar, self.y * scalar, self.z * scalar)


@dataclass
class Plane:
    normal: Vec3
    d: float

    def signed_distance(self, p: Vec3) -> float:
        return self.normal.dot(p) + self.d


@dataclass
class Sphere:
    center_vs: Vec3
    radius: float


@dataclass
class TileRect:
    min_x_px: int
    min_y_px: int
    max_x_px: int
    max_y_px: int


def build_tile_side_planes(
    tile: TileRect,
    width: int,
    height: int,
    fov_y_radians: float,
    near_z: float,
) -> list[Plane]:
    """
    Build the 4 side planes of a camera-space tile frustum.

    Conventions:
    - view/camera space
    - camera at origin
    - looking down -Z
    - positive X right, positive Y up
    """
    aspect = width / max(height, 1)
    tan_half_y = __import__("math").tan(fov_y_radians * 0.5)
    tan_half_x = tan_half_y * aspect

    def pixel_to_near_point(px: float, py: float) -> Vec3:
        ndc_x = (px / width) * 2.0 - 1.0
        ndc_y = 1.0 - (py / height) * 2.0
        return Vec3(
            ndc_x * tan_half_x * near_z,
            ndc_y * tan_half_y * near_z,
            -near_z,
        )

    def cross(a: Vec3, b: Vec3) -> Vec3:
        return Vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x,
        )

    p00 = pixel_to_near_point(tile.min_x_px, tile.min_y_px)
    p10 = pixel_to_near_point(tile.max_x_px, tile.min_y_px)
    p01 = pixel_to_near_point(tile.min_x_px, tile.max_y_px)
    p11 = pixel_to_near_point(tile.max_x_px, tile.max_y_px)

    # Normals face inward.
    left_n = cross(p01, p00).normalized()
    right_n = cross(p10, p11).normalized()
    top_n = cross(p00, p10).normalized()
    bottom_n = cross(p11, p01).normalized()

    return [
        Plane(left_n, 0.0),
        Plane(right_n, 0.0),
        Plane(top_n, 0.0),
        Plane(bottom_n, 0.0),
    ]


def sphere_intersects_tile_frustum(
    sphere: Sphere,
    tile: TileRect,
    width: int,
    height: int,
    fov_y_radians: float,
    near_z: float = 0.1,
) -> bool:
    """
    Conservative tile-frustum vs sphere test.

    This only uses the 4 side planes plus a simple near-plane reject.
    For Forward+, this is often already enough for a first pass.
    """
    if sphere.center_vs.z + sphere.radius > -near_z:
        # Sphere crosses or is behind near plane.
        # In a production renderer you might mark the tile conservatively
        # or switch to a special fallback path here.
        return True

    planes = build_tile_side_planes(tile, width, height, fov_y_radians, near_z)
    for plane in planes:
        if plane.signed_distance(sphere.center_vs) < -sphere.radius:
            return False
    return True


def cheap_early_light_reject(sphere: Sphere, near_z: float, far_z: float) -> bool:
    """
    Example of a very cheap first cull before tile testing.

    Returns True if the light is definitely irrelevant to the whole view.
    """
    depth = -sphere.center_vs.z
    if depth + sphere.radius < near_z:
        return True
    if depth - sphere.radius > far_z:
        return True
    return False


def example():
    width = 1440
    height = 900
    tile_size = 32

    tile_x = 20
    tile_y = 10
    tile = TileRect(
        min_x_px=tile_x * tile_size,
        min_y_px=tile_y * tile_size,
        max_x_px=(tile_x + 1) * tile_size,
        max_y_px=(tile_y + 1) * tile_size,
    )

    light = Sphere(center_vs=Vec3(0.5, 0.0, -8.0), radius=2.4)

    if cheap_early_light_reject(light, near_z=0.1, far_z=200.0):
        print("Light skipped before tile testing")
        return

    hit = sphere_intersects_tile_frustum(
        light,
        tile,
        width=width,
        height=height,
        fov_y_radians=1.0471975512,  # 60 deg
    )
    print("tile hit =", hit)


if __name__ == "__main__":
    example()
