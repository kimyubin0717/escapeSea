"""Repair the four reported monster materials and verify their saved bindings."""
import json
import shutil
from pathlib import Path

import unreal as u

project = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
record = Path.home() / 'SeaMaterialRepair20261123'
record.mkdir(exist_ok=True)
verify = '-seamaterialsverify' in u.SystemLibrary.get_command_line().lower()
lib = u.EditorAssetLibrary
registry = u.AssetRegistryHelpers.get_asset_registry()
u.SystemLibrary.execute_console_command(None, 'Editor.AsyncTextureCompilation 0')
targets = {
    'DeepSeaEel': 'M_DeepSeaEel',
    'GiantIsopod': 'M_GiantIsopod_Baked',
    'Gigantura': 'M_Gigantura',
    'Kraken': 'M_Kraken_Baked',
}
changed = []


def backup(asset):
    package = asset.get_path_name().split('.')[0]
    relative = Path(package.removeprefix('/Game/') + '.uasset')
    destination = record / 'Backup' / relative
    if not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(project / 'Content' / relative, destination)
    changed.append(package)


results = []
for species, name in targets.items():
    folder = '/Game/IntegratedCity/Monsters/' + species
    registry.scan_paths_synchronous([folder], True)
    material = lib.load_asset(folder + '/' + name)
    assert isinstance(material, u.Material), name
    textures = {}
    for suffix in ('BaseColor', 'Normal', 'Roughness', 'Emissive'):
        path = folder + '/Textures/T_' + species + '_' + suffix
        texture = lib.load_asset(path)
        assert isinstance(texture, u.Texture2D), path
        expected_srgb = suffix in ('BaseColor', 'Emissive')
        actual_srgb = texture.get_editor_property('srgb')
        if verify:
            assert actual_srgb == expected_srgb, path
        elif actual_srgb != expected_srgb:
            backup(texture)
            texture.set_editor_property('srgb', expected_srgb)
            assert lib.save_loaded_asset(texture, False), path
        textures[suffix] = texture

    meshes = [lib.load_asset(str(path)) for path in lib.list_assets(folder)
              if lib.find_asset_data(str(path)).asset_class_path.asset_name == 'SkeletalMesh']
    assert meshes, species
    for mesh in meshes:
        slots = mesh.get_editor_property('materials')
        assert len(slots) == 1, mesh.get_path_name()
        if verify:
            assert slots[0].get_editor_property('material_interface') == material, mesh.get_path_name()
        elif slots[0].get_editor_property('material_interface') != material:
            backup(mesh)
            slot = slots[0]
            slot.set_editor_property('material_interface', material)
            slots[0] = slot
            mesh.set_editor_property('materials', slots)
            assert lib.save_loaded_asset(mesh, False)

    if not verify:
        backup(material)
        u.MaterialEditingLibrary.recompile_material(material)
        assert lib.save_loaded_asset(material, False)
    used = {texture.get_path_name() for texture in u.MaterialEditingLibrary.get_used_textures(material)}
    if verify:
        assert textures['BaseColor'].get_path_name() in used, (name, 'BaseColor binding', sorted(used))
        assert textures['Roughness'].get_path_name() in used, (name, 'Roughness binding', sorted(used))
    results.append({'species': species, 'material': material.get_path_name(),
                    'meshes': [mesh.get_path_name() for mesh in meshes],
                    'textures': {suffix: texture.get_editor_property('srgb') for suffix, texture in textures.items()}})

output = {'verified': verify, 'changed_packages': changed, 'materials': results}
(record / ('verified.json' if verify else 'applied.json')).write_text(json.dumps(output, indent=2), encoding='utf-8')
u.log('SEA_MATERIALS_' + ('VERIFIED' if verify else 'APPLIED') + ' count=' + str(len(results)))
if '-run=' not in u.SystemLibrary.get_command_line().lower():
    u.SystemLibrary.quit_editor()
