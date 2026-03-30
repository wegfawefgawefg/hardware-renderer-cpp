"""
This file is deliberately simple pseudo-code.

The important idea is:

1. Scene objects become render items.
2. Render items get culled.
3. Surviving items are grouped by a render key.
4. Each group becomes either:
   - one instanced draw, or
   - a small list of individual draws.

The render key is usually some combination of:
    pipeline / shader
    mesh / primitive range
    material / texture identity
    alpha mode
    skinned vs static
    shadow caster mode

Once you have those bins, the draw loop becomes much simpler:

    for each render_bin:
        bind pipeline once
        bind descriptor state once
        if bin has many static items:
            upload instance data
            draw instanced
        else:
            draw one by one

That is the part mature engines automate. Content authors should not have to
manually say "this category of mesh gets batched". The renderer should infer
compatibility from the render key.
"""


class FakeRenderer:
    def record_main_pass(self, instanced_bins, single_draws):
        for render_bin in instanced_bins:
            self.bind_pipeline(render_bin.key.pipeline)
            self.bind_material(render_bin.key.material)
            self.bind_mesh(render_bin.key.mesh)
            self.upload_instances(render_bin.items)
            self.draw_instanced(render_bin)

        for item in single_draws:
            self.bind_pipeline(item.key.pipeline)
            self.bind_material(item.key.material)
            self.bind_mesh(item.key.mesh)
            self.push_transform(item.instance.transform)
            self.draw_one(item)

    def bind_pipeline(self, pipeline):
        print("bind pipeline", pipeline)

    def bind_material(self, material):
        print("bind material", material)

    def bind_mesh(self, mesh):
        print("bind mesh", mesh)

    def upload_instances(self, items):
        print("upload", len(items), "instances")

    def draw_instanced(self, render_bin):
        print("draw instanced", len(render_bin.items), "items")

    def push_transform(self, transform):
        print("push transform", transform)

    def draw_one(self, item):
        print("draw single", item.key)
