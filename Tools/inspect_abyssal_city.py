import unreal
import json
from pathlib import Path

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level.load_level('/Game/Maps/LV_AbyssalCity_Dense'):
    raise RuntimeError('Could not load LV_AbyssalCity_Dense')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
report = {'actor_count': len(actors), 'actors': [], 'missing_meshes': []}
for actor in actors:
    label = actor.get_actor_label()
    p = actor.get_actor_location()
    report['actors'].append({'label': label, 'class': actor.get_class().get_name(), 'position': [p.x,p.y,p.z]})
    if isinstance(actor, unreal.StaticMeshActor) and not actor.static_mesh_component.static_mesh:
        report['missing_meshes'].append(label)
Path(unreal.Paths.project_saved_dir(), 'AbyssalCityInspection.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('ABYSS_CITY_INSPECT: actors={} missing_meshes={}'.format(len(actors),len(report['missing_meshes'])))
