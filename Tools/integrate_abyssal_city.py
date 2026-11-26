"""Prepare the migrated city without rebuilding or replacing its authored geometry."""
import unreal
import json
from pathlib import Path

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not level.load_level('/Game/Maps/LV_AbyssalCity_Dense'):
    raise RuntimeError('City map failed to load')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
mode = unreal.load_class(None, '/Script/mama.AbyssGameMode')
if not mode:
    raise RuntimeError('mama game mode missing')
world.get_world_settings().set_editor_property('default_game_mode', mode)

# Repeatable: modify only integration-owned markers and pier access steps.
for actor in actors.get_all_level_actors():
    if 'MamaCityIntegration' in [str(tag) for tag in actor.tags]:
        actors.destroy_actor(actor)

def marker(kind, x, y, z=4):
    actor = actors.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(x,y,z))
    actor.set_actor_label('Mama_Interaction_{}_{}_{}'.format(kind,x,y))
    actor.set_folder_path('13_Mama_Gameplay')
    actor.tags = ['MamaCityIntegration', 'AbyssSite_{}'.format(kind)]
    return actor

# Objectives sit on the wide street intersections, outside building walls.
for kind, (x,y) in enumerate([(-12000,-11000),(12000,-11000),(12000,11000)]):
    for offset in [-250,0,250]:
        marker(kind,x+offset,y)
    marker(3,x-500,y)
    marker(3,x+500,y)
for y in [-14000,-10000,-7000,-4000,4000,7000,10000,14000]:
    marker(3,500,y)
for x in [-18000,-15000,-9000,-6000,-3000,3000,6000,9000,15000,18000]:
    marker(3,x,500)
for x,y in [(-12000,-7000),(12000,-7000),(-12000,7000),(12000,7000),(700,-7000),(700,7000)]:
    marker(4,x,y)
for kind,y in [(5,18400),(6,18800),(7,19200),(8,20000)]:
    marker(kind,-850,y,200)
marker(3,-850,16600,200)

# The original pier has a 2m vertical ledge. Add a broad flight of 40cm steps.
unreal.log('CITY_STAGE: loading pier assets')
cube = unreal.load_asset('/Engine/BasicShapes/Cube.Cube')
material = unreal.load_asset('/Game/Materials/AbyssalCity/Surfaces/M_Concrete_Panels.M_Concrete_Panels')
unreal.log('CITY_STAGE: spawning pier access')
for i in range(5):
    h = (i+1)*40
    actor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,15050+i*100,h/2))
    actor.set_actor_label('Mama_Pier_Access_Step_{}'.format(i+1))
    actor.set_folder_path('13_Mama_Gameplay/PierAccess')
    actor.tags=['MamaCityIntegration']
    actor.static_mesh_component.set_static_mesh(cube)
    actor.static_mesh_component.set_material(0,material)
    actor.set_actor_scale3d(unreal.Vector(12,1,h/100))

unreal.log('CITY_STAGE: verifying and saving')
all_actors=actors.get_all_level_actors()
missing=[a.get_actor_label() for a in all_actors if isinstance(a,unreal.StaticMeshActor) and not a.static_mesh_component.static_mesh]
if missing:
    raise RuntimeError('Missing meshes: '+str(missing))
if not level.save_current_level():
    raise RuntimeError('City map save failed')
unreal.EditorAssetLibrary.save_directory('/Game/Maps',only_if_is_dirty=True,recursive=True)
report={'map':'/Game/Maps/LV_AbyssalCity_Dense','actor_count':len(all_actors),'interaction_sites':sum(any(str(t).startswith('AbyssSite_') for t in a.tags) for a in all_actors),'missing_meshes':missing,'game_mode':str(world.get_world_settings().get_editor_property('default_game_mode'))}
Path(unreal.Paths.project_saved_dir(),'AbyssalCityIntegration.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('ABYSS_CITY_INTEGRATION: '+json.dumps(report))
