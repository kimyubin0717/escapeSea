"""Import only BobbitWorm and add one rooted hunter to the configured play map."""
import json
import re
import shutil
from pathlib import Path

import unreal as u

project = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
folder = '/Game/IntegratedCity/Monsters/BobbitWorm'
source = project / 'Source/SourceArt/IntegratedCity/Monsters/BobbitWorm'
lib = u.EditorAssetLibrary
tools = u.AssetToolsHelpers.get_asset_tools()
u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.PNG 0')
u.SystemLibrary.execute_console_command(None, 'Editor.AsyncTextureCompilation 0')


def import_file(file, destination, options=None):
    task = u.AssetImportTask()
    task.filename = str(file)
    task.destination_path = destination
    task.automated = True
    task.save = True
    task.replace_existing = True
    if file.suffix.lower() == '.png':
        task.factory = u.TextureFactory()
    if options:
        task.options = options
    tools.import_asset_tasks([task])
    assert task.imported_object_paths, str(file)
    return [lib.load_asset(str(path)) for path in task.imported_object_paths]


mesh_path = folder + '/SK_BobbitWorm'
if not lib.does_asset_exist(folder + '/SK_BobbitWorm_Skeleton'):
    options = u.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.import_as_skeletal = True
    options.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    import_file(source / 'SK_BobbitWorm.fbx', folder, options)
    # FBX creates additional skeleton/animation packages beyond the mesh task result.
    for created in lib.list_assets(folder):
        assert lib.save_loaded_asset(lib.load_asset(str(created)))
mesh = lib.load_asset(mesh_path)
assert isinstance(mesh, u.SkeletalMesh)
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([folder], True)
animations = [lib.load_asset(str(p)) for p in lib.list_assets(folder)
              if lib.find_asset_data(str(p)).asset_class_path.asset_name == 'AnimSequence']
if not animations:
    options = u.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = u.FBXImportType.FBXIT_ANIMATION
    options.import_mesh = False
    options.import_animations = True
    options.skeleton = mesh.get_editor_property('skeleton')
    import_file(source / 'SK_BobbitWorm.fbx', folder + '/Animations', options)
    animations = [lib.load_asset(str(p)) for p in lib.list_assets(folder)
                  if isinstance(lib.load_asset(str(p)), u.AnimSequence)]
    for animation in animations:
        assert lib.save_loaded_asset(animation)
assert len(animations) == 10, [a.get_name() for a in animations]
assert all(a.get_skeleton() == mesh.get_editor_property('skeleton') for a in animations)
idle = next(a for a in animations if a.get_name().endswith('_Idle'))
assert any(a.get_name().endswith('_Attack') for a in animations)
assert all(a.get_play_length() > 0 for a in animations)

material_path = folder + '/M_BobbitWorm'
material = lib.load_asset(material_path) if lib.does_asset_exist(material_path) else tools.create_asset('M_BobbitWorm', folder, u.Material, u.MaterialFactoryNew())
edit = u.MaterialEditingLibrary
edit.delete_all_material_expressions(material)
for index, (suffix, prop) in enumerate((('BaseColor', 'BASE_COLOR'), ('Normal', 'NORMAL'), ('Roughness', 'ROUGHNESS'), ('Emissive', 'EMISSIVE_COLOR'))):
    path = folder + '/Textures/T_BobbitWorm_' + suffix
    texture = lib.load_asset(path) if lib.does_asset_exist(path) else import_file(source / 'Textures' / ('T_BobbitWorm_' + suffix + '.png'), folder + '/Textures')[0]
    texture.set_editor_property('max_texture_size', 2048)
    if suffix == 'Normal':
        texture.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP)
    texture.set_editor_property('srgb', suffix in ('BaseColor', 'Emissive'))
    lib.save_loaded_asset(texture)
    node = edit.create_material_expression(material, u.MaterialExpressionTextureSample, -400, index * 200)
    node.set_editor_property('texture', texture)
    node.set_editor_property('sampler_type', getattr(u.MaterialSamplerType, 'SAMPLERTYPE_' + ('NORMAL' if suffix == 'Normal' else 'LINEAR_COLOR' if suffix == 'Roughness' else 'COLOR')))
    assert edit.connect_material_property(node, 'R' if suffix == 'Roughness' else 'RGB', getattr(u.MaterialProperty, 'MP_' + prop))
edit.recompile_material(material)
slots = mesh.get_editor_property('materials')
for index in range(len(slots)):
    slot = slots[index]
    slot.set_editor_property('material_interface', material)
    slots[index] = slot
mesh.set_editor_property('materials', slots)
lib.save_loaded_asset(material)
lib.save_loaded_asset(mesh)

config = (project / 'Config/DefaultEngine.ini').read_text(encoding='utf-8')
map_path = re.search(r'^GameDefaultMap=(.+)$', config, re.M)[1].strip()
map_file = project / 'Content' / (map_path.removeprefix('/Game/') + '.umap')
backup = Path.home() / 'SeaIntegration20261123/Backup/Content' / map_file.relative_to(project / 'Content')
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(map_file, backup)
editor = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
assert editor.load_level(map_path)
existing = [a for a in actors.get_all_level_actors() if a.actor_has_tag('AbyssBobbitWorm')]
assert len(existing) <= 1
actor = existing[0] if existing else actors.spawn_actor_from_object(mesh, u.Vector(0, -8500, 5), u.Rotator())
actor.set_actor_label('BobbitWorm Ambush')
actor.set_folder_path('Integrated/Monsters/BobbitWorm')
actor.tags = ['AbyssPatrolCreature', 'AbyssBenthicHunter', 'AbyssBobbitWorm']
actor.set_actor_scale3d(u.Vector(0.8, 0.8, 0.8))
actor.set_actor_enable_collision(False)
component = actor.skeletal_mesh_component
component.set_animation_mode(u.AnimationMode.ANIMATION_SINGLE_NODE)
component.override_animation_data(idle, True, True, 0, 1)
component.set_editor_property('skin_cache_usage', [u.SkinCacheUsage.DISABLED])
assert editor.save_current_level()
assert editor.load_level(map_path)
verified = [a for a in actors.get_all_level_actors() if a.actor_has_tag('AbyssBobbitWorm')]
assert len(verified) == 1
assert verified[0].skeletal_mesh_component.get_skeletal_mesh_asset() == mesh
assert verified[0].skeletal_mesh_component.get_editor_property('animation_data').anim_to_play == idle
result = {'mesh': mesh.get_path_name(), 'skeleton': mesh.get_editor_property('skeleton').get_path_name(), 'animations': [a.get_name() for a in animations], 'map': map_path, 'actor': verified[0].get_actor_label()}
output = project / 'Saved/Automation/bobbit_applied.json'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(result, indent=2), encoding='utf-8')
u.log('BOBBIT_APPLIED ' + json.dumps(result))
