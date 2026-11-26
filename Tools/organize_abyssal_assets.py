"""Move assets through Unreal so map/model/material references stay intact."""
import unreal
import json
from pathlib import Path

lib=unreal.EditorAssetLibrary
registry=unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
# An open world cannot be renamed. Switch to the engine entry world first.
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Engine/Maps/Entry')
paths=[]
for root in ['/Game/StarterContent','/Game/Maps','/Game/Materials']:
    paths.extend(lib.list_assets(root,recursive=True,include_folder=False))
plan=[]
for path in sorted(set(paths)):
    data=lib.find_asset_data(path)
    cls=str(data.asset_class_path.asset_name)
    if cls=='ObjectRedirector':
        continue
    name=str(data.asset_name)
    package=str(data.package_name)
    if package.startswith('/Game/Maps/AbyssalCity/') or package.startswith('/Game/Materials/AbyssalCity/'):
        continue
    if cls=='World':
        if package.startswith('/Game/Maps/'):
            continue
        target='/Game/Maps/'+('Examples' if '/StarterContent/' in package else 'AbyssalCity')
    elif cls=='StaticMesh':
        section=package.split('/')[3] if '/StarterContent/' in package else 'Other'
        target='/Game/Models/AbyssalCity/'+section
    elif cls.startswith('Material'):
        section='Variants' if package.startswith('/Game/Materials/') else ('Effects' if '/Particles/' in package else 'Props' if '/Props/' in package else 'Blueprints' if '/Blueprints/' in package else 'Surfaces')
        target='/Game/Materials/AbyssalCity/'+section
    elif cls.startswith('Texture'):
        target='/Game/Textures/AbyssalCity/'+('HDRI' if '/HDRI/' in package else 'Surfaces')
    elif cls.startswith('Sound'):
        target='/Game/Audio/AbyssalCity'
    elif cls=='ParticleSystem':
        target='/Game/Effects/AbyssalCity'
    elif cls=='MapBuildDataRegistry':
        target='/Game/Maps/Examples'
    elif cls=='Blueprint':
        target='/Game/Blueprints/AbyssalCity'
    else:
        target='/Game/Misc/AbyssalCity/'+cls
    destination=target+'/'+name
    if package==destination:
        continue
    if lib.does_asset_exist(destination):
        raise RuntimeError('Destination collision: '+destination)
    plan.append((path,destination,cls))

# Load every referencer before moving dependencies, including the source maps.
for old,new,cls in plan:
    if not lib.load_asset(old):
        raise RuntimeError('Failed to load '+old)
renames=[]
for old,new,cls in sorted(plan,key=lambda row: row[2]=='World'):
    unreal.log('CITY_MOVE: {} -> {}'.format(old,new))
    rename=unreal.AssetRenameData(lib.load_asset(old),new.rsplit('/',1)[0],new.rsplit('/',1)[1])
    renames.append(rename)
if renames and not unreal.AssetToolsHelpers.get_asset_tools().rename_assets(renames):
    raise RuntimeError('Batch asset move failed')
for root in ['/Game/Maps','/Game/Models','/Game/Materials','/Game/Textures','/Game/Effects','/Game/Audio','/Game/Blueprints','/Game/Misc']:
    if lib.does_directory_exist(root):
        lib.save_directory(root,only_if_is_dirty=False,recursive=True)
# Include moves completed by an earlier interrupted pass, recorded in its log.
report=[]
log_path=Path(unreal.Paths.project_saved_dir(),'Logs','AbyssalAssetOrganization.log')
for prior in Path(unreal.Paths.project_saved_dir(),'Logs').glob('AbyssalAssetOrganization*.log'):
    for line in prior.read_text(encoding='utf-8',errors='replace').splitlines():
        if 'CITY_MOVE: ' not in line:
            continue
        old,new=line.split('CITY_MOVE: ',1)[1].split(' -> ',1)
        if lib.does_asset_exist(new):
            report.append({'old':old,'new':new})
report.extend({'old':a,'new':('/Game/Maps/Examples/'+b.rsplit('/',1)[1]) if c=='MapBuildDataRegistry' else b,'class':c} for a,b,c in plan)
Path(unreal.Paths.project_saved_dir(),'AbyssalAssetMoves.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('CITY_ORGANIZE_COMPLETE: {} assets moved'.format(len(plan)))
