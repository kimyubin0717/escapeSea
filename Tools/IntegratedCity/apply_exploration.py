"""Update only loot meshes, three new caches and the cave/wreck residents."""
import json
import shutil
from pathlib import Path

import unreal as u

project = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
record = Path.home() / 'SeaExploration20261123'
source = project / 'Source/SourceArt/IntegratedCity/ExplorationProps'
manifest = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
lib = u.EditorAssetLibrary
tools = u.AssetToolsHelpers.get_asset_tools()
editor = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
u.SystemLibrary.execute_console_command(None, 'Editor.AsyncTextureCompilation 0')


def backup(package, extension='.uasset'):
    relative = Path(package.removeprefix('/Game/') + extension)
    original = project / 'Content' / relative
    target = record / 'Backup/Content' / relative
    if original.exists() and not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)


material_folder = '/Game/Models/AbyssalCity/Interiors/Exploration'
materials = {}
edit = u.MaterialEditingLibrary
for name, values in manifest['materials'].items():
    path = material_folder + '/' + name
    material = lib.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset(name, material_folder, u.Material, u.MaterialFactoryNew())
    edit.delete_all_material_expressions(material)
    color = edit.create_material_expression(material, u.MaterialExpressionConstant3Vector, -350, 0)
    color.set_editor_property('constant', u.LinearColor(*values['color']))
    assert edit.connect_material_property(color, '', u.MaterialProperty.MP_BASE_COLOR)
    for index, (key, prop) in enumerate((('metallic', u.MaterialProperty.MP_METALLIC), ('roughness', u.MaterialProperty.MP_ROUGHNESS))):
        node = edit.create_material_expression(material, u.MaterialExpressionConstant, -350, 140*(index+1))
        node.set_editor_property('r', values[key])
        assert edit.connect_material_property(node, '', prop)
    edit.recompile_material(material)
    assert lib.save_loaded_asset(material, False)
    materials[name] = material

meshes = {}
mesh_folder = '/Game/Models/AbyssalCity/Interiors'
for name in manifest['objects']:
    path = mesh_folder + '/' + name
    backup(path)
    task = u.AssetImportTask()
    task.filename = str(source / (name + '.fbx'))
    task.destination_path = mesh_folder
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    task.factory = u.FbxFactory()
    current = lib.load_asset(path)
    if current:
        data = current.get_editor_property('asset_import_data')
        if not isinstance(data, u.FbxStaticMeshImportData):
            data = u.FbxStaticMeshImportData(outer=current)
            current.set_editor_property('asset_import_data', data)
        data.scripted_add_filename(task.filename, 0, '')
        assert Path(data.get_first_filename()).resolve() == Path(task.filename).resolve()
        data.set_editor_property('convert_scene_unit', True)
        data.set_editor_property('convert_scene', True)
        data.set_editor_property('import_uniform_scale', 1.0)
    options = u.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
    options.import_materials = False
    options.import_textures = False
    data = options.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('convert_scene_unit', True)
    data.set_editor_property('convert_scene', True)
    data.set_editor_property('import_uniform_scale', 1.0)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('generate_lightmap_u_vs', False)
    task.options = options
    tools.import_asset_tasks([task])
    mesh = lib.load_asset(path)
    assert isinstance(mesh, u.StaticMesh), path
    slots = mesh.get_editor_property('static_materials')
    for index in range(len(slots)):
        slot = slots[index]
        imported = str(slot.get_editor_property('imported_material_slot_name'))
        primary = 'M_ExplorerSteel' if 'Safe' in name else 'M_ExplorerWood'
        material_name = 'M_ExplorerBrass' if 'Brass' in imported else primary
        slot.set_editor_property('material_interface', materials[material_name])
        slots[index] = slot
    mesh.set_editor_property('static_materials', slots)
    assert lib.save_loaded_asset(mesh, False)
    expected_cm = {
        'SM_CityBankSafeClosed': (90, 94.75, 110),
        'SM_CityBankSafeOpen': (114.04, 146.49, 110),
        'SM_CityMarketCrateClosed': (116, 81.85, 74.4),
        'SM_CityMarketCrateOpen': (116, 118.44, 116.11),
    }
    extent = mesh.get_bounds().box_extent
    actual = sorted((extent.x*2, extent.y*2, extent.z*2))
    assert all(abs(a-b) < 1.5 for a,b in zip(actual, sorted(expected_cm[name]))), (name, actual)
    assert Path(mesh.get_editor_property('asset_import_data').get_first_filename()).resolve() == Path(task.filename).resolve()
    meshes[name] = mesh

map_path = '/Game/IntegratedCity/Maps/Access/LV_AbyssalCity_Dense_Streetscape_IntegratedCity_Harbor_Mk2'
backup(map_path, '.umap')
assert editor.load_level(map_path)
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
existing = actors.get_all_level_actors()
by_label = {actor.get_actor_label(): actor for actor in existing}
cave = by_label['Seafloor - SeaCave']
wreck = by_label['AbyssShipwreck']
for actor in (cave, wreck):
    component = actor.static_mesh_component
    mesh = component.static_mesh
    backup(mesh.get_path_name().split('.')[0])
    body = mesh.get_editor_property('body_setup')
    body.set_editor_property('collision_trace_flag', u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    component.set_collision_profile_name('BlockAll')
    actor.set_actor_enable_collision(True)
    component.set_static_mesh(None)
    component.set_static_mesh(mesh)
    assert lib.save_loaded_asset(mesh, False)
cave.tags = list(dict.fromkeys([str(t) for t in cave.tags] + ['AbyssInteriorRoof', 'AbyssInterior_SeaCave']))

# Preserve actor identities and container tags while discarding stale material overrides.
for actor in existing:
    if actor.actor_has_tag('AbyssCacheSafe') or actor.actor_has_tag('AbyssCacheCrate') or actor.actor_has_tag('AbyssCacheOpened'):
        component = actor.get_component_by_class(u.StaticMeshComponent)
        if component:
            component.set_editor_property('override_materials', [])


def floor_at(x, y, ceiling=250):
    hit = u.SystemLibrary.line_trace_single(world, u.Vector(x,y,ceiling), u.Vector(x,y,-300),
        u.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], u.DrawDebugTrace.NONE)
    assert hit is not None, ('No floor',x,y)
    values = hit.to_tuple()
    assert values[0], ('No blocking floor', x,y)
    point, normal = values[5], values[7]
    assert normal.z > .65, ('Unwalkable floor', x,y,normal)
    return point


def static_actor(name, mesh, location, tags, hidden=False):
    actor = by_label.get(name)
    if not actor:
        actor = actors.spawn_actor_from_object(mesh, location, u.Rotator())
        assert actor.rename(name), name
        by_label[name] = actor
    actor.set_actor_label(name)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.static_mesh_component.set_editor_property('override_materials', [])
    actor.set_actor_location(location, False, True)
    actor.tags = tags
    actor.set_folder_path('Integrated/Exploration/Loot')
    actor.set_actor_enable_collision(False)
    actor.set_actor_hidden_in_game(hidden)
    return actor


placements = [
    ('ExplorationChest_000_Cave', -18050, -15500, 250, 'AbyssInterior_SeaCave'),
    ('ExplorationChest_001_WreckBow', -16800, 17850, 650, ''),
    ('ExplorationChest_002_WreckStern', -14300, 17850, 650, ''),
]
chests = []
for name, x, y, ceiling, interior in placements:
    point = floor_at(x,y,ceiling)
    point.z += 4
    identity = 'AbyssCacheId_' + name
    tags = ['AbyssCacheCrate', 'AbyssExplorationCache', 'AbyssNewCache', 'AbyssTreasureChest', identity]
    if interior:
        tags.append(interior)
    closed = static_actor(name, meshes['SM_CityMarketCrateClosed'], point, tags)
    static_actor(name+'_Opened', meshes['SM_CityMarketCrateOpen'], u.Vector(point.x,point.y,point.z-5000), ['AbyssCacheOpened', identity], True)
    chests.append({'name':name,'location':[point.x,point.y,point.z]})


def resident(name, species, clip_path, position, span, crawler):
    folder = '/Game/IntegratedCity/Monsters/' + species
    model = lib.load_asset(folder + '/SK_' + species)
    clip = lib.load_asset(clip_path)
    assert model and clip, (name, clip_path)
    bounds = model.get_bounds()
    scale = span / (2*max(bounds.box_extent.x,bounds.box_extent.y))
    point = u.Vector(*position)
    if crawler:
        ground = floor_at(point.x,point.y)
        point.z = ground.z-(bounds.origin.z-bounds.box_extent.z)*scale+5
    actor = by_label.get(name)
    if not actor:
        actor = actors.spawn_actor_from_object(model, point, u.Rotator())
        assert actor.rename(name), name
        by_label[name] = actor
    actor.set_actor_label(name)
    actor.set_actor_location(point, False, True)
    actor.set_actor_scale3d(u.Vector(scale,scale,scale))
    actor.tags = ['AbyssPatrolCreature','AbyssExplorationHunter', 'AbyssCaveResident' if crawler else 'AbyssWreckResident']
    if crawler:
        actor.tags = list(actor.tags)+['AbyssBenthicHunter']
    actor.set_folder_path('Integrated/Exploration/Monsters')
    actor.set_actor_enable_collision(False)
    component = actor.skeletal_mesh_component
    component.set_skeletal_mesh_asset(model)
    component.set_animation_mode(u.AnimationMode.ANIMATION_SINGLE_NODE)
    component.override_animation_data(clip, True, True, 0, 1)
    component.set_editor_property('enable_update_rate_optimizations', True)
    component.set_editor_property('visibility_based_anim_tick_option', u.VisibilityBasedAnimTickOption.ONLY_TICK_POSE_WHEN_RENDERED)
    component.set_editor_property('skin_cache_usage', [u.SkinCacheUsage.DISABLED])
    return actor


walk = '/Game/IntegratedCity/Monsters/GiantIsopod/SK_GiantIsopod_Anim_Armature_IS_Walk'
resident('ExplorationCrawler_Cave_01','GiantIsopod',walk,(-18450,-15150,5),240,True)
resident('ExplorationCrawler_Cave_02','GiantIsopod',walk,(-17650,-15350,5),240,True)
resident('ExplorationSwimmer_Wreck','DeepSeaEel','/Game/IntegratedCity/Monsters/DeepSeaEel/Animations/SK_DeepSeaEel_Armature_EEL_Swim',(-15550,19400,200),360,False)

for actor in actors.get_all_level_actors():
    fog = actor.get_component_by_class(u.ExponentialHeightFogComponent)
    if fog:
        fog.set_editor_property('fog_density', .012)
assert editor.save_current_level()
assert editor.load_level(map_path)
all_actors = actors.get_all_level_actors()
assert sum(actor.actor_has_tag('AbyssExplorationCache') for actor in all_actors) == 3
assert sum(actor.actor_has_tag('AbyssCaveResident') for actor in all_actors) == 2
assert sum(actor.actor_has_tag('AbyssWreckResident') for actor in all_actors) == 1
report = {'chests':chests,'cave_crawlers':2,'wreck_swimmers':1,'meshes':manifest['objects']}
(record/'applied_level.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
u.log('EXPLORATION_APPLIED ' + json.dumps(report))
u.SystemLibrary.quit_editor()
