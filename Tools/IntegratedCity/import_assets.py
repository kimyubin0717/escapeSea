"""Import source FBXs and bind the baked maps to the two animated hunters."""
from pathlib import Path
import os
import unreal as u

project = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
source = project/"Source/SourceArt/IntegratedCity"
root = "/Game/IntegratedCity"
lib=u.EditorAssetLibrary
tools=u.AssetToolsHelpers.get_asset_tools()
u.SystemLibrary.execute_console_command(None,"Interchange.FeatureFlags.Import.FBX 0")

def import_one(path, folder, options=None):
    task=u.AssetImportTask()
    for k,v in dict(filename=str(path),destination_path=folder,automated=True,replace_existing=True,replace_existing_settings=True,save=True).items():
        task.set_editor_property(k,v)
    if options: task.options=options
    tools.import_asset_tasks([task])
    u.EditorLoadingAndSavingUtils.save_dirty_packages(False,True)
    return [lib.load_asset(str(p)) for p in task.imported_object_paths]

for category in ("Terrain","Submarines","Monsters","Player","Equipment","Salvage","NautilusMk2"):
    if os.environ.get("MAMA_PLAYER_KIT") and category not in ("Player","Equipment","Salvage","NautilusMk2"):
        continue
    if os.environ.get("MAMA_TERRAIN_ONLY") and category!="Terrain":
        continue
    for file in sorted((source/category).rglob("*.fbx")):
        species=file.stem.removeprefix("SK_")
        folder=root+"/"+("Geometry" if category=="Terrain" else category)+("/"+species if category=="Monsters" else "")
        if category in ("Equipment","Salvage","NautilusMk2","Player"):
            folder+="/Meshes"
        path=folder+"/"+file.stem
        skeletal=category in ("Monsters","Player")
        metric_static=category in ("Terrain","Equipment","Salvage","NautilusMk2")
        unit_update=metric_static and lib.does_asset_exist(path) and not lib.load_asset(path).get_editor_property("asset_import_data").get_editor_property("convert_scene_unit")
        if unit_update or not lib.does_asset_exist(path) or (skeletal and not lib.does_asset_exist(path+"_Skeleton")):
            options=u.FbxImportUI()
            options.automated_import_should_detect_type=False
            options.import_as_skeletal=skeletal
            options.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH if skeletal else u.FBXImportType.FBXIT_STATIC_MESH
            options.import_animations=skeletal
            options.import_materials=not skeletal
            options.import_textures=False
            if category=="Player":
                options.skeletal_mesh_import_data.convert_scene_unit=True
                options.anim_sequence_import_data.convert_scene_unit=True
            options.static_mesh_import_data.combine_meshes=True
            options.static_mesh_import_data.auto_generate_collision=file.stem.startswith("SM_Block")
            options.static_mesh_import_data.generate_lightmap_u_vs=False
            if metric_static:
                options.static_mesh_import_data.convert_scene_unit=True
            import_one(file,folder,options)
        mesh=lib.load_asset(path)
        assert mesh,path
        if file.stem=="SM_CityBlockTerrain":
            extent=mesh.get_bounds().box_extent
            assert abs(extent.x-25000)<1 and abs(extent.y-25000)<1,(path,extent)
        if not skeletal: continue
        matpath=folder+"/M_"+species
        material=lib.load_asset(matpath) if lib.does_asset_exist(matpath) else tools.create_asset("M_"+species,folder,u.Material,u.MaterialFactoryNew())
        edit=u.MaterialEditingLibrary
        edit.delete_all_material_expressions(material)
        for index,(suffix,prop) in enumerate([("BaseColor","BASE_COLOR"),("Normal","NORMAL"),("Roughness","ROUGHNESS"),("Emissive","EMISSIVE_COLOR")]):
            name="T_"+species+"_"+suffix
            texpath=folder+"/Textures/"+name
            tex=lib.load_asset(texpath) if lib.does_asset_exist(texpath) else import_one(file.parent/"Textures"/(name+".png"),folder+"/Textures")[0]
            tex.max_texture_size=2048
            if suffix=="Roughness": tex.srgb=False
            if suffix=="Normal":
                tex.compression_settings=u.TextureCompressionSettings.TC_NORMALMAP
                tex.srgb=False
            lib.save_loaded_asset(tex)
            sample=edit.create_material_expression(material,u.MaterialExpressionTextureSample,-400,index*200)
            sample.texture=tex
            sample.sampler_type=getattr(u.MaterialSamplerType,"SAMPLERTYPE_"+("NORMAL" if suffix=="Normal" else "LINEAR_COLOR" if suffix=="Roughness" else "COLOR"))
            assert edit.connect_material_property(sample,"R" if suffix=="Roughness" else "RGB",getattr(u.MaterialProperty,"MP_"+prop))
        edit.recompile_material(material)
        slots=mesh.materials
        for slot in slots: slot.material_interface=material
        mesh.materials=slots
        lib.save_loaded_asset(material)
        lib.save_loaded_asset(mesh)
        animations=[p for p in lib.list_assets(folder) if "swim" in str(p).lower() and isinstance(lib.load_asset(p),u.AnimSequence)]
        if not animations:
            anim=u.FbxImportUI()
            anim.automated_import_should_detect_type=False
            anim.mesh_type_to_import=u.FBXImportType.FBXIT_ANIMATION
            anim.import_mesh=False
            anim.import_animations=True
            anim.skeleton=mesh.skeleton
            import_one(file,folder+"/Animations",anim)
            animations=[p for p in lib.list_assets(folder) if "swim" in str(p).lower() and isinstance(lib.load_asset(p),u.AnimSequence)]
        assert animations,(species,lib.list_assets(folder))
        u.log("HUNTER_SWIM "+str(animations))
u.log("INTEGRATED_ASSETS_READY")

