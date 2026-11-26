"""Export each evaluated building mesh once and preserve its instances in the layout."""
import bpy
import json
import math
from pathlib import Path

folder = Path(bpy.data.filepath).parents[2] / "IntegratedCity" / "Terrain"
folder.mkdir(parents=True, exist_ok=True)
meshes = {}
rows = []
for instance in bpy.context.evaluated_depsgraph_get().object_instances:
    obj = instance.object
    if obj.type != "MESH":
        continue
    name = obj.data.name
    key = obj.name if obj.name.startswith("SM_City") else "SM_Block_" + name[:2]
    if key not in meshes:
        meshes[key] = bpy.data.meshes.new_from_object(obj, preserve_all_data_layers=True,
                           depsgraph=bpy.context.evaluated_depsgraph_get())
    position, rotation, scale = instance.matrix_world.decompose()
    rx, ry, rz = rotation.to_euler()
    assert abs(rx) + abs(ry) < .001
    rows.append(dict(mesh=key, xyz=[100*position.x,-100*position.y,100*position.z],
                     yaw=-math.degrees(rz), scale=list(scale)))
assert len(meshes)==15 and len(rows)>250
for key,data in meshes.items():
    obj=bpy.data.objects.new(key+"_Export",data)
    bpy.context.scene.collection.objects.link(obj)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active=obj
    assert data.polygons and all(math.isfinite(n) for v in data.vertices for n in v.co)
    assert bpy.ops.export_scene.fbx(filepath=str(folder/(key+".fbx")),use_selection=True,
        axis_forward="-Y",axis_up="Z",apply_scale_options="FBX_SCALE_UNITS",
        mesh_smooth_type="FACE",bake_anim=False)=={'FINISHED'}
    bpy.data.objects.remove(obj,do_unlink=True)
(folder/"Layout.json").write_text(json.dumps(rows,indent=1),encoding="utf8")
print("INTEGRATED_LAYOUT",len(meshes),len(rows))

