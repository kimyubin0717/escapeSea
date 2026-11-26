import unreal
import json
from pathlib import Path

MAP='/Game/Maps/LV_AbyssalCity_Dense'
lib=unreal.EditorAssetLibrary
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level.load_level(MAP):
    raise RuntimeError('Organized map failed to load')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()

# Project-owned primitives allow model edits without editing engine assets.
def model(source,name):
    path='/Game/Models/AbyssalCity/Primitives/'+name
    result=lib.load_asset(path) if lib.does_asset_exist(path) else lib.duplicate_asset(source,path)
    if not result:
        raise RuntimeError('Could not create editable model '+path)
    return result
primitives={
    '/Engine/BasicShapes/Cube.Cube':model('/Engine/BasicShapes/Cube.Cube','SM_CityBlock'),
    '/Engine/BasicShapes/Cylinder.Cylinder':model('/Engine/BasicShapes/Cylinder.Cylinder','SM_CityCylinder'),
    '/Engine/BasicShapes/Sphere.Sphere':model('/Engine/BasicShapes/Sphere.Sphere','SM_CitySphere'),
}
seabed=model('/Engine/BasicShapes/Cube.Cube','SM_SeabedBase')
changed=0
for actor in actors:
    if not isinstance(actor,unreal.StaticMeshActor):
        continue
    comp=actor.static_mesh_component
    if not comp.static_mesh:
        raise RuntimeError('Missing mesh: '+actor.get_actor_label())
    old=comp.static_mesh.get_path_name()
    if actor.get_actor_label()=='Seabed_Floor':
        comp.set_static_mesh(seabed)
        changed+=1
    elif old in primitives:
        comp.set_static_mesh(primitives[old])
        changed+=1
    # Ensure each explicitly assigned material still loads after the moves.
    for material in comp.get_materials():
        if material and not lib.does_asset_exist(material.get_path_name()):
            raise RuntimeError('Material reference missing: '+material.get_path_name())
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(0,0,55000),unreal.Rotator(-90,0,0))
if not level.save_current_level():
    raise RuntimeError('Final map save failed')
lib.save_directory('/Game/Models/AbyssalCity',only_if_is_dirty=False,recursive=True)
report={'map':MAP,'actor_count':len(actors),'interaction_sites':sum(any(str(t).startswith('AbyssSite_') for t in a.tags) for a in actors),'project_primitive_assignments':changed,'missing_meshes':[],'game_mode':str(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_world_settings().get_editor_property('default_game_mode'))}
Path(unreal.Paths.project_saved_dir(),'AbyssalCityIntegration.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('CITY_FINALIZED: '+json.dumps(report))
