"""350m city, solid editable building blocks, and Starter Content road surfaces.

Always derive from the preserved full-size playable level, never compound scale.
"""
import unreal
import json
import math
from pathlib import Path

SCALE=0.7
GAP=180.0
SIDEWALK=160.0
LIMIT=17150.0
SOURCE='/Game/Maps/LV_AbyssalCity_Dense_Playable'
OUTPUT='/Game/Maps/LV_AbyssalCity_Dense_Compact'
lib=unreal.EditorAssetLibrary
edit=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not level.load_level(SOURCE):
    raise RuntimeError('Full-size source map could not load')
actors=list(sub.get_all_level_actors())
cube=lib.load_asset('/Game/Models/AbyssalCity/Primitives/SM_CityBlock')
if not cube:
    raise RuntimeError('Project block mesh missing')

def bounds(actor):
    p,e=actor.get_actor_bounds(False)
    return [p.x-e.x,p.y-e.y,p.z-e.z,p.x+e.x,p.y+e.y,p.z+e.z]

def union(items):
    boxes=[bounds(a) for a in items]
    return [min(b[i] for b in boxes) for i in range(3)]+[max(b[i] for b in boxes) for i in range(3,6)]

# Discover authored buildings from their structural pieces before simplifying.
prefixes=set()
for actor in actors:
    label=actor.get_actor_label()
    if '_Wall_' in label:
        prefixes.add(label.split('_Wall_')[0])
prefixes.discard('Leaning_Lobby')
prefixes.update(['Leaning','FinPavilion'])
records=[]
for prefix in sorted(prefixes):
    members=[a for a in actors if a.get_actor_label().startswith(prefix+'_')]
    structural=[a for a in members if isinstance(a,unreal.StaticMeshActor) and any(k in a.get_actor_label() for k in ['_Wall_','_Floor','_Ceiling','_TowerCrown','_Tower_Body','_Roof'])]
    if not structural:
        raise RuntimeError('No building envelope: '+prefix)
    b=union(structural)
    records.append({'name':prefix,'members':members,'width':(b[3]-b[0])*SCALE,'depth':(b[4]-b[1])*SCALE,'height':max((b[5]-b[2])*SCALE,400.0),'base':max(b[2]*SCALE,0.0),'original_center':[(b[0]+b[3])*SCALE/2,(b[1]+b[4])*SCALE/2]})
removed=set()
for record in records:
    for actor in record['members']:
        if actor.get_path_name() not in removed:
            removed.add(actor.get_path_name());sub.destroy_actor(actor)
for actor in actors:
    if actor.get_path_name() in removed:
        continue
    label=actor.get_actor_label()
    if label.startswith(('Skybridge_','Highway_','Barricade_','Comp_','Pod_','Light_Pod_','Cafe_')):
        removed.add(actor.get_path_name());sub.destroy_actor(actor)

for actor in list(sub.get_all_level_actors()):
    p=actor.get_actor_location();s=actor.get_actor_scale3d()
    actor.set_actor_location(unreal.Vector(p.x*SCALE,p.y*SCALE,p.z*SCALE),False,False)
    actor.set_actor_scale3d(unreal.Vector(s.x*SCALE,s.y*SCALE,s.z*SCALE))
    if actor.get_actor_label().startswith('Marine_Snow_'):
        s=actor.get_actor_scale3d();actor.set_actor_scale3d(unreal.Vector(s.x*0.3,s.y*0.3,s.z*0.3))
    if isinstance(actor,unreal.PointLight):
        c=actor.point_light_component;c.set_attenuation_radius(c.attenuation_radius*SCALE)
    elif isinstance(actor,unreal.SpotLight):
        c=actor.spot_light_component;c.set_attenuation_radius(c.attenuation_radius*SCALE)

def road_material(name,texture,tint,tile):
    path='/Game/Materials/AbyssalCity/Roads/'+name
    if lib.does_asset_exist(path):
        return lib.load_asset(path)
    material=tools.create_asset(name,path.rsplit('/',1)[0],unreal.Material,unreal.MaterialFactoryNew())
    def node(cls,**props):
        obj=edit.create_material_expression(material,getattr(unreal,'MaterialExpression'+cls),0,0)
        for key,value in props.items():obj.set_editor_property(key,value)
        return obj
    def link(a,b,pin=''):
        if not edit.connect_material_expressions(a,'',b,pin):raise RuntimeError('Road material pin: '+pin)
    world=node('WorldPosition')
    xy=node('ComponentMask',r=True,g=True,b=False,a=False);link(world,xy)
    uv=node('Multiply',const_b=1/tile);link(xy,uv,'A')
    sample=node('TextureSample',texture=lib.load_asset('/Game/Textures/AbyssalCity/Surfaces/'+texture));link(uv,sample)
    color=node('Constant3Vector',constant=unreal.LinearColor(*tint,1))
    base=node('Multiply');link(sample,base,'A');link(color,base,'B')
    edit.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=node('Constant',r=0.92);edit.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    spec=node('Constant',r=0.15);edit.connect_material_property(spec,'',unreal.MaterialProperty.MP_SPECULAR)
    edit.layout_material_expressions(material);edit.recompile_material(material)
    return material

asphalt=road_material('M_Road_Paved','T_Concrete_Poured_D',(0.16,0.17,0.18),400.0)
paving=road_material('M_Sidewalk_Tiles','T_Concrete_Tiles_D',(0.72,0.72,0.68),200.0)
yellow=lib.load_asset('/Game/Materials/AbyssalCity/Variants/M_HazardYellow')
white=lib.load_asset('/Game/Materials/AbyssalCity/Surfaces/M_Basic_Wall')
roads=[]
for actor in sub.get_all_level_actors():
    if actor.get_actor_label().startswith(('Avenue_','Boulevard_')):
        b=bounds(actor)
        actor.static_mesh_component.set_material(0,asphalt)
        roads.append({'name':actor.get_actor_label(),'vertical':(b[4]-b[1])>(b[3]-b[0]),'x':(b[0]+b[3])/2,'y':(b[1]+b[4])/2,'width':min(b[3]-b[0],b[4]-b[1]),'top':b[5]})
if len(roads)!=10:raise RuntimeError('Unexpected street network: '+str(len(roads)))
vertical=sorted([r for r in roads if r['vertical']],key=lambda r:r['x'])
horizontal=sorted([r for r in roads if not r['vertical']],key=lambda r:r['y'])

def gaps(road_list,axis,extra):
    result=[];start=-LIMIT
    for road in road_list:
        lo=road[axis]-road['width']/2-extra
        hi=road[axis]+road['width']/2+extra
        if lo-start>100:result.append((start,lo))
        start=hi
    if LIMIT-start>100:result.append((start,LIMIT))
    return result

# Dense packing happens inside road blocks: no building can cover a street.
cells=[]
for x1,x2 in gaps(vertical,'x',SIDEWALK+30):
    for y1,y2 in gaps(horizontal,'y',SIDEWALK+30):
        cells.append((x1+60,y1+60,x2-60,y2-60))
free=[(i,*cell) for i,cell in enumerate(cells)]
placed=[]
def distance_to_rect(x,y,b):
    return max(b[0]-x,0,x-b[2])**2+max(b[1]-y,0,y-b[3])**2
for record in sorted(records,key=lambda r:r['width']*r['depth'],reverse=True):
    candidates=[]
    for index,(cell,x1,y1,x2,y2) in enumerate(free):
        for rotated in [False,True]:
            w,d=(record['depth'],record['width']) if rotated else (record['width'],record['depth'])
            if w<=x2-x1 and d<=y2-y1:
                distance=distance_to_rect(*record['original_center'],cells[cell])
                waste=(x2-x1)*(y2-y1)-w*d
                candidates.append((distance*10000+waste,index,rotated,w,d))
    if not candidates:raise RuntimeError('Building does not fit clear road blocks: '+record['name'])
    _,index,rotated,w,d=min(candidates)
    cell,x1,y1,x2,y2=free.pop(index)
    record.update({'x':x1+w/2,'y':y1+d/2,'w':w,'d':d,'rotated':rotated,'rect':[x1,y1,x1+w,y1+d]})
    placed.append(record)
    if x2-(x1+w+GAP)>100:free.append((cell,x1+w+GAP,y1,x2,y1+d))
    if y2-(y1+d+GAP)>100:free.append((cell,x1,y1+d+GAP,x2,y2))

def solid(label,position,size,material,folder,mesh=cube,collision=True):
    actor=sub.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*position))
    actor.set_actor_label(label);actor.set_folder_path(folder)
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.static_mesh_component.set_material(0,material)
    actor.set_actor_scale3d(unreal.Vector(size[0]/100,size[1]/100,size[2]/100))
    if not collision:actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.static_mesh_component.set_cast_shadow(collision)
    return actor

for record in placed:
    name=record['name']
    surface='M_Brick_Clay_New' if name.startswith(('Apt','Condo','Tavern','Hotel')) else 'M_Basic_Wall' if name.startswith(('Clinic','Hospital','BioLab')) else 'M_Concrete_Poured' if name.startswith(('Factory','Warehouse','Power','Water','Bunker')) else 'M_Concrete_Panels'
    material=lib.load_asset('/Game/Materials/AbyssalCity/Surfaces/'+surface)
    path='/Game/Models/AbyssalCity/Buildings/SM_Building_'+name
    mesh=lib.load_asset(path) if lib.does_asset_exist(path) else lib.duplicate_asset(cube.get_path_name(),path)
    if not mesh:raise RuntimeError('Building model creation failed: '+name)
    mesh.set_editor_property('static_materials',[unreal.StaticMaterial(material_interface=material,material_slot_name='BuildingSurface',imported_material_slot_name='BuildingSurface')])
    actor=solid('Building_'+name,(record['x'],record['y'],record['base']+record['height']/2),(record['w'],record['d'],record['height']),material,'14_CompactBuildings',mesh)
    actor.tags=['CompactSolidBuilding',name]
    record['model']=path;record['material']=surface

# Continuous pavement strips are split at crossings, so junctions stay flat.
mark_count=0
for road in roads:
    axis='y' if road['vertical'] else 'x'
    perpendicular=horizontal if road['vertical'] else vertical
    spans=gaps(perpendicular,axis,50)
    for a,b in spans:
        for side in [-1,1]:
            edge=(road['x'] if road['vertical'] else road['y'])+side*(road['width']/2+SIDEWALK/2)
            p=(edge,(a+b)/2,5) if road['vertical'] else ((a+b)/2,edge,5)
            size=(SIDEWALK,b-a,10) if road['vertical'] else (b-a,SIDEWALK,10)
            solid('Walk_'+road['name']+'_'+str(mark_count),p,size,paving,'15_Roads/Sidewalks');mark_count+=1
            edge=(road['x'] if road['vertical'] else road['y'])+side*(road['width']/2-35)
            p=(edge,(a+b)/2,road['top']+0.5) if road['vertical'] else ((a+b)/2,edge,road['top']+0.5)
            size=(10,b-a,0.6) if road['vertical'] else (b-a,10,0.6)
            solid('Edge_'+road['name']+'_'+str(mark_count),p,size,white,'15_Roads/Markings',collision=False);mark_count+=1
        dash=a+100
        while dash+450<b:
            p=(road['x'],dash+225,road['top']+0.6) if road['vertical'] else (dash+225,road['y'],road['top']+0.6)
            size=(12,450,0.6) if road['vertical'] else (450,12,0.6)
            solid('Dash_'+road['name']+'_'+str(mark_count),p,size,yellow,'15_Roads/Markings',collision=False);mark_count+=1
            dash+=1000

spawn=sub.spawn_actor_from_class(unreal.TargetPoint,unreal.Vector(1200,-3200,80))
spawn.set_actor_label('Compact_Player_Start');spawn.set_folder_path('13_Mama_Gameplay')
spawn.tags=['AbyssPlayerSpawn']

overlaps=[]
for i,a in enumerate(placed):
    for b in placed[i+1:]:
        ar=a['rect'];br=b['rect']
        if min(ar[2],br[2])>max(ar[0],br[0]) and min(ar[3],br[3])>max(ar[1],br[1]):overlaps.append([a['name'],b['name']])
if overlaps:raise RuntimeError('Building overlaps: '+str(overlaps))
if len(placed)<25:raise RuntimeError('Unexpected building count')
lib.save_directory('/Game/Models/AbyssalCity/Buildings',only_if_is_dirty=False,recursive=True)
lib.save_directory('/Game/Materials/AbyssalCity/Roads',only_if_is_dirty=False,recursive=True)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-14000,-14000,27000),unreal.Rotator(-55,45,0))
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
if not unreal.EditorLoadingAndSavingUtils.save_map(world,OUTPUT):raise RuntimeError('Compact map save failed')
report={'map':OUTPUT,'scale':SCALE,'width_metres':350,'building_count':len(placed),'minimum_packing_gap_cm':GAP,'building_overlaps':overlaps,'roads':len(roads),'road_elements':mark_count,'actor_count':len(sub.get_all_level_actors()),'building_models':[{'name':r['name'],'model':r['model'],'position':[r['x'],r['y']],'size':[r['w'],r['d'],r['height']],'material':r['material']} for r in placed]}
Path(unreal.Paths.project_saved_dir(),'CompactCityReport.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('COMPACT_CITY_COMPLETE: '+json.dumps({k:v for k,v in report.items() if k!='building_models'}))
