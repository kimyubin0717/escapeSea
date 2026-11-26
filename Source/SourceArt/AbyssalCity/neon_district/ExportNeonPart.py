"""Export an edited module and its matching authored collision as one FBX."""
import sys
from pathlib import Path
import bpy
from mathutils import Matrix


def main():
    if "--" not in sys.argv:
        raise SystemExit("Pass one mesh name after --, for example SM_Neon_RoadLoop")
    args = sys.argv[sys.argv.index("--") + 1:]
    if len(args) != 1 or not args[0].startswith("SM_Neon_"):
        raise SystemExit("Pass exactly one SM_Neon_ mesh name")
    name = args[0]
    obj = bpy.data.objects.get(name)
    if not obj or obj.type != "MESH":
        raise SystemExit(f"{name} is missing from the Blender source")
    module_scene = next(scene for scene in bpy.data.scenes if scene.name == "01 Blender source modules")
    bpy.context.window.scene = module_scene
    hull = bpy.data.objects.get(f"UCX_{name}_00")
    module_objects = [obj] + ([hull] if hull else [])
    undo = {o.name: (o.matrix_world.copy(), o.hide_get()) for o in module_objects}
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = obj
    try:
        # The source library lays the objects out for inspection. Export each local origin.
        for mesh in module_objects:
            mesh.hide_set(False)
            mesh.matrix_world = Matrix.Identity(4)
            mesh.select_set(True)
        bpy.context.view_layer.update()
        out = Path(bpy.data.filepath).with_name(name + ".fbx")
        bpy.ops.export_scene.fbx(filepath=str(out), use_selection=True, apply_unit_scale=True,
                                 apply_scale_options="FBX_SCALE_UNITS", axis_forward="-Y", axis_up="Z",
                                 mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False)
    finally:
        for mesh in module_objects:
            transform, was_hidden = undo[mesh.name]
            mesh.matrix_world = transform
            mesh.hide_set(was_hidden)


if __name__ == "__main__":
    main()
