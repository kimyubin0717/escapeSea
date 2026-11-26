"""Create a playable city copy from the authored terrain layout."""
import json
import re
from pathlib import Path
import unreal as u

lib=u.EditorAssetLibrary
tools=u.AssetToolsHelpers.get_asset_tools()
editor=u.get_editor_subsystem(u.LevelEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
root="/Game/IntegratedCity"
source=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))/"Source/SourceArt/IntegratedCity"
name="LV_AbyssalCity_Dense_Streetscape_IntegratedCity"
dest=root+"/Maps/"+name
base="/Game/Maps/LV_AbyssalCity_Dense_Streetscape"
def get(path):
    obj=lib.load_asset(path)
    assert obj,path
    return obj

def static(mesh,label,folder,loc,yaw=0,scale=(1,1,1)):
    actor=actors.spawn_actor_from_object(mesh,u.Vector(*loc),u.Rotator(yaw=yaw))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.set_actor_scale3d(u.Vector(*scale))
    return actor

def norm(s):
    return re.sub(r"[._]\d{3}$","",str(s))

for group in ("Terrain","Submarines"):
    palette={norm(k):v for k,v in json.loads((source/group/"Palette.json").read_text(encoding="utf8")).items()}
    for file in (source/group).glob("*.fbx"):
        mesh=get(root+"/"+("Geometry" if group=="Terrain" else group)+"/"+file.stem)
        slots=mesh.static_materials
        changed=False
        for slot in slots:
            key=norm(slot.material_slot_name)
            existing="/Game/Materials/AbyssalCity/Streetscape/"+key
            path=root+"/"+group+"/Materials/"+key
            if lib.does_asset_exist(existing): mat=get(existing)
            elif lib.does_asset_exist(path): mat=get(path)
            elif key in palette:
                color,metal,rough=palette[key]
                mat=tools.create_asset(key,root+"/"+group+"/Materials",u.Material,u.MaterialFactoryNew())
                edit=u.MaterialEditingLibrary
                rgb=edit.create_material_expression(mat,u.MaterialExpressionConstant3Vector,-280,0)
                rgb.constant=u.LinearColor(*color)
                edit.connect_material_property(rgb,"",u.MaterialProperty.MP_BASE_COLOR)
                if any(s in key for s in ("Tube","Window","Display","Furnace","Lamp","LED")):
                    edit.connect_material_property(rgb,"",u.MaterialProperty.MP_EMISSIVE_COLOR)
                for i,(value,property_) in enumerate([(metal,u.MaterialProperty.MP_METALLIC),(rough,u.MaterialProperty.MP_ROUGHNESS)]):
                    scalar=edit.create_material_expression(mat,u.MaterialExpressionConstant,-280,150+i*90)
                    scalar.r=value
                    edit.connect_material_property(scalar,"",property_)
                edit.recompile_material(mat)
                lib.save_loaded_asset(mat)
            else:
                raise ValueError((file.stem,key,list(palette)))
            if slot.material_interface!=mat:
                slot.material_interface=mat
                changed=True
        if changed:
            mesh.static_materials=slots
            assert lib.save_loaded_asset(mesh)

if not lib.does_asset_exist(dest):
    assert editor.load_level(base)
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    assert u.EditorLoadingAndSavingUtils.save_map(world,dest)
    del world
assert editor.load_level(dest)
assert u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_path_name().startswith(dest+".")
reserved=[]
for a in actors.get_all_level_actors():
    folder=str(a.get_folder_path())
    label=a.get_actor_label()
    if (folder.startswith(("Integrated/","02_Financial","03_Commercial","04_Collapsed","05_Residential","06_FarEast","07_FarWest","17_Urban_Polish/Infill","18_Neon_District"))
        or label in ("CityStreetGrid","CityStreetMarkings","CityBlockTerrain")
        or label.startswith("Submarine_") and label not in ("Submarine_Repair_Console","Submarine_Gangway")):
        assert actors.destroy_actor(a)
        continue
    if folder.startswith(("13_Mama_Gameplay","15_CityInteriors","16_Exploration","08_Harbor")) or "AbyssPlayerSpawn" in [str(t) for t in a.tags]:
        pos,ext=a.get_actor_bounds(False)
        if max(ext.x,ext.y,ext.z)<.01: pos=a.get_actor_location()
        reserved.append((pos,ext))
reserved.append((u.Vector(6000,3500,0),u.Vector(260,260,0)))

layout=json.loads((source/"Terrain/Layout.json").read_text())
count=0
for i,row in enumerate(layout):
    key=row["mesh"]
    building=key.startswith("SM_Block")
    mesh=get(root+"/Geometry/"+key)
    actor=static(mesh,key+"_"+str(i),"Integrated/Buildings" if building else "Integrated/Terrain",row["xyz"],row["yaw"],row["scale"])
    if building:
        p,e=actor.get_actor_bounds(False)
        if any(abs(p.x-c.x)<e.x+b.x+180 and abs(p.y-c.y)<e.y+b.y+180 for c,b in reserved):
            actors.destroy_actor(actor)
            continue
        count+=1
    else: actor.set_actor_enable_collision(False)
assert count>100,count
sub=get(root+"/Submarines/SM_Submarine_Wrecked")
ship=static(sub,"Nautilus - service berth","Integrated/Submarines",(0,0,0))
p,e=ship.get_actor_bounds(False)
scale=3300/(2*max(e.x,e.y))
ship.set_actor_scale3d(u.Vector(scale,scale,scale))
ship.set_actor_rotation(u.Rotator(yaw=90 if e.x>e.y else 0),False)
p,e=ship.get_actor_bounds(False)
ship.set_actor_location(u.Vector(-950-e.x-p.x,19300-p.y,205+e.z-p.z),False,False)
ship.set_actor_enable_collision(False)

# Start on the service deck with the wreck and the repair terminals in view.
starts=[a for a in actors.get_all_level_actors() if "AbyssPlayerSpawn" in [str(t) for t in a.tags]]
assert len(starts)<=1
start=starts[0] if starts else actors.spawn_actor_from_class(u.TargetPoint,u.Vector(),u.Rotator())
start.tags=list(dict.fromkeys([str(tag) for tag in start.tags]+["AbyssPlayerSpawn"]))
start.set_actor_label("Nautilus - dive entry")
start.set_folder_path("Integrated/Harbor")
start.set_actor_location(u.Vector(-560,19100,274),False,False)

for i,(x,y) in enumerate([(-12000,0),(12000,-11000),(0,11000),(-18000,-8500),(18000,8500),(0,-17000)]):
    species=("DeepSeaEel","Gigantura")[i%2]
    folder=root+"/Monsters/"+species
    mesh=get(folder+"/SK_"+species)
    swim=next(get(p) for p in lib.list_assets(folder) if "swim" in str(p).lower() and isinstance(get(p),u.AnimSequence))
    a=actors.spawn_actor_from_object(mesh,u.Vector(x,y,95),u.Rotator())
    a.set_actor_label(species+" Patrol "+str(i+1))
    a.set_folder_path("Integrated/Monsters/"+species)
    a.tags=["AbyssPatrolCreature"]
    a.set_actor_enable_collision(False)
    c=a.skeletal_mesh_component
    c.set_mobility(u.ComponentMobility.MOVABLE)
    c.set_animation_mode(u.AnimationMode.ANIMATION_SINGLE_NODE)
    data=c.animation_data
    data.anim_to_play=swim
    data.saved_looping=True
    data.saved_playing=True
    c.animation_data=data
    c.enable_update_rate_optimizations=True

assert editor.save_current_level()
live=actors.get_all_level_actors()
hunters=[a for a in live if "AbyssPatrolCreature" in [str(t) for t in a.tags]]
assert len(hunters)==6 and all(a.skeletal_mesh_component.animation_data.anim_to_play for a in hunters)
assert sum(str(a.get_folder_path())=="Integrated/Buildings" for a in live)==count
u.log("INTEGRATED_LEVEL_READY buildings=%d hunters=%d"% (count,len(hunters)))

