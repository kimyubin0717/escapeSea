"""Export one Streetscape object from its Blender source with a stable UE origin."""
import sys
from pathlib import Path
import bpy

name = sys.argv[sys.argv.index("--")+1]
obj = bpy.context.scene.objects.get(name)
if not obj or obj.type!="MESH" or not obj.data.polygons:
    raise RuntimeError(f"Missing or empty mesh: {name}")
if obj.parent or obj.location.length>1e-5 or obj.rotation_euler.to_quaternion().angle>1e-5 or any(abs(v-1)>1e-5 for v in obj.scale):
    raise RuntimeError("Keep the mesh at the world origin with rotation and scale applied.")
if not obj.data.uv_layers or not obj.data.materials:
    raise RuntimeError("Keep the surface UVs and existing named material slots.")
bpy.ops.object.select_all(action="DESELECT")
obj.select_set(True)
bpy.context.view_layer.objects.active=obj
dst=Path(bpy.data.filepath).with_suffix(".fbx")
bpy.ops.export_scene.fbx(filepath=str(dst),use_selection=True,apply_unit_scale=True,
                         apply_scale_options="FBX_SCALE_UNITS",axis_forward="-Y",axis_up="Z",
                         add_leaf_bones=False,mesh_smooth_type="FACE",bake_anim=False)
print("CITY_PART_EXPORTED",dst)

