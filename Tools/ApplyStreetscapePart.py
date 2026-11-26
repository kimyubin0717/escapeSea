"""Update selected Static Mesh assets in place and retain their UE material assignments."""
import json, os
from pathlib import Path
import unreal

lib=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
folder="/Game/Models/AbyssalCity/Streetscape"
request=json.loads(Path(os.environ["MAMA_STREETSCAPE_REQUEST"]).read_text(encoding="utf-8-sig"))
for name, filename in request.items():
    path=folder+"/"+name
    mesh=lib.load_asset(path)
    assert isinstance(mesh,unreal.StaticMesh),path
    old={str(slot.material_slot_name):slot.material_interface for slot in mesh.get_editor_property("static_materials")}
    assert old and all(old.values()),(name,"Missing assigned UE material")
    options=unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type",False)
    options.set_editor_property("mesh_type_to_import",unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("import_materials",False)
    options.set_editor_property("import_textures",False)
    options.static_mesh_import_data.set_editor_property("auto_generate_collision",False)
    options.static_mesh_import_data.set_editor_property("generate_lightmap_u_vs",False)
    options.static_mesh_import_data.set_editor_property("build_nanite",False)
    task=unreal.AssetImportTask()
    for key,value in dict(filename=filename,destination_path=folder,automated=True,
                          replace_existing=True,replace_existing_settings=False,save=False,options=options).items():
        task.set_editor_property(key,value)
    tools.import_asset_tasks([task])
    assert any(str(p).split(".")[0]==path for p in task.get_editor_property("imported_object_paths")),(path,"No update")
    mesh=lib.load_asset(path)
    slots=mesh.get_editor_property("static_materials")
    assert set(old)=={str(slot.material_slot_name) for slot in slots},(path,"Keep the existing Blender material names")
    for i,slot in enumerate(slots): mesh.set_material(i,old[str(slot.material_slot_name)])
    assert lib.save_loaded_asset(mesh),path
    unreal.log("CITY_PART_SYNCED: "+path)
unreal.log("CITY_SYNC_COMPLETE")

