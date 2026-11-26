"""Editable world-scaled building surfaces and an aligned editor overview."""
import unreal
import json
from pathlib import Path

lib=unreal.EditorAssetLibrary
edit=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
folder='/Game/Materials/AbyssalCity/Buildings'
master_path=folder+'/M_BuildingFacade'
needs_graph=not lib.does_asset_exist(master_path)
if not needs_graph:
    master=lib.load_asset(master_path)
else:
    master=tools.create_asset('M_BuildingFacade',folder,unreal.Material,unreal.MaterialFactoryNew())
def build_graph():
    edit.delete_all_material_expressions(master)
    def node(cls,**props):
        obj=edit.create_material_expression(master,getattr(unreal,'MaterialExpression'+cls),0,0)
        for key,value in props.items():
            obj.set_editor_property(key,value)
        return obj
    def link(a,b,name):
        if not edit.connect_material_expressions(a,'',b,name):
            raise RuntimeError('Material connection failed: '+name)
    def scalar(value):
        return node('Constant',r=float(value))
    def op(cls,a,b):
        obj=node(cls);link(a,obj,'A');link(b,obj,'B');return obj
    def unary(cls,a):
        obj=node(cls);link(a,obj,'');return obj
    def mask(a,r=False,g=False,b=False):
        obj=node('ComponentMask',r=r,g=g,b=b,a=False);link(a,obj,'');return obj
    position=node('WorldPosition')
    x=mask(position,r=True);y=mask(position,g=True);z=mask(position,b=True)
    horizontal=op('Add',x,y)
    # Use physical centimetres, so scaled block meshes retain proper floor/window sizes.
    u=op('Multiply',horizontal,scalar(1/240))
    v=op('Multiply',z,scalar(1/320))
    u_band=unary('Ceil',op('Subtract',scalar(0.33),unary('Abs',op('Subtract',unary('Frac',u),scalar(0.5)))))
    v_band=unary('Ceil',op('Subtract',scalar(0.28),unary('Abs',op('Subtract',unary('Frac',v),scalar(0.5)))))
    normal=node('VertexNormalWS')
    roof_weight=unary('Abs',mask(normal,b=True))
    wall_weight=unary('OneMinus',roof_weight)
    windows=op('Multiply',op('Multiply',u_band,v_band),wall_weight)
    wall_uv=op('AppendVector',horizontal,z)
    roof_uv=mask(position,r=True,g=True)
    uv=node('LinearInterpolate');link(wall_uv,uv,'A');link(roof_uv,uv,'B');link(roof_weight,uv,'Alpha')
    uv_scale=op('Multiply',uv,scalar(1/400))
    texture=node('TextureSampleParameter2D',parameter_name='SurfaceTexture',texture=lib.load_asset('/Game/Textures/AbyssalCity/Surfaces/T_Concrete_Panels_D'))
    link(uv_scale,texture,'')
    tint=node('VectorParameter',parameter_name='FacadeTint',default_value=unreal.LinearColor(0.45,0.52,0.56,1))
    glass=node('VectorParameter',parameter_name='GlassTint',default_value=unreal.LinearColor(0.025,0.12,0.16,1))
    concrete=op('Multiply',texture,tint)
    base=node('LinearInterpolate');link(concrete,base,'A');link(glass,base,'B');link(windows,base,'Alpha')
    edit.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR)
    roughness=node('LinearInterpolate');link(scalar(0.88),roughness,'A');link(scalar(0.22),roughness,'B');link(windows,roughness,'Alpha')
    edit.connect_material_property(roughness,'',unreal.MaterialProperty.MP_ROUGHNESS)
    edit.connect_material_property(op('Multiply',windows,scalar(0.55)),'',unreal.MaterialProperty.MP_METALLIC)
    glow=node('ScalarParameter',parameter_name='WindowGlow',default_value=0.18)
    edit.connect_material_property(op('Multiply',glass,op('Multiply',windows,glow)),'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.layout_material_expressions(master)
    edit.recompile_material(master)
if needs_graph:
    build_graph()

def variant(name,tint,texture):
    path=folder+'/'+name
    mat=lib.load_asset(path) if lib.does_asset_exist(path) else tools.create_asset(name,folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    mat.set_editor_property('parent',master)
    edit.set_material_instance_vector_parameter_value(mat,'FacadeTint',unreal.LinearColor(*tint,1))
    edit.set_material_instance_texture_parameter_value(mat,'SurfaceTexture',lib.load_asset('/Game/Textures/AbyssalCity/Surfaces/'+texture))
    edit.update_material_instance(mat)
    return mat
finance=variant('MI_Facade_Financial',(0.46,0.55,0.62),'T_Concrete_Panels_D')
residential=variant('MI_Facade_Residential',(0.65,0.57,0.48),'T_Brick_Clay_Old_D')
hospital=variant('MI_Facade_Hospital',(0.8,0.85,0.84),'T_Concrete_Tiles_D')
industrial=variant('MI_Facade_Industrial',(0.48,0.5,0.47),'T_Concrete_Poured_D')
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level.load_level('/Game/Maps/LV_AbyssalCity_Dense'):
    raise RuntimeError('City map load failed')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
changed=[];ground=[]
for actor in actors:
    label=actor.get_actor_label()
    if isinstance(actor,unreal.PostProcessVolume):
        settings=actor.get_editor_property('settings')
        settings.set_editor_property('override_auto_exposure_method',True)
        settings.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
        settings.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True)
        settings.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
        settings.set_editor_property('override_auto_exposure_bias',True)
        settings.set_editor_property('auto_exposure_bias',1.0)
        settings.set_editor_property('override_bloom_intensity',True)
        settings.set_editor_property('bloom_intensity',0.25)
        actor.set_editor_property('settings',settings)
    if label=='Seabed_Floor' or label.startswith(('Avenue_','Boulevard_')):
        rot=actor.get_actor_rotation()
        ground.append({'actor':label,'previous_rotation':[rot.pitch,rot.yaw,rot.roll]})
        actor.set_actor_rotation(unreal.Rotator(0,0,0),False)
    if not isinstance(actor,unreal.StaticMeshActor):
        continue
    if '_Wall_' in label or label.endswith('_TowerCrown') or label=='Leaning_Tower_Body':
        if label.startswith(('Apt_','Condo_','Hotel','Tavern','Cathedral','Museum')):
            mat=residential
        elif label.startswith(('Hospital','Clinic','BioLab')):
            mat=hospital
        elif label.startswith(('Factory','Warehouse','PowerStation','PumpHouse','WaterPlant','AutoRep','Bunker','Train','Subway')):
            mat=industrial
        else:
            mat=finance
        actor.static_mesh_component.set_material(0,mat)
        changed.append(label)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(0,0,55000),unreal.Rotator(-90,0,0))
lib.save_directory(folder,only_if_is_dirty=False,recursive=True)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
output_map='/Game/Maps/LV_AbyssalCity_Dense_Playable'
if not unreal.EditorLoadingAndSavingUtils.save_map(world,output_map):
    raise RuntimeError('Could not save the playable copy of the city')
report={'map':output_map,'facades_updated':len(changed),'actors':changed,'ground_rotations_before':ground,'editor_camera_rotation':[-90,0,0],'master_material':master_path}
Path(unreal.Paths.project_saved_dir(),'CityFacadeReport.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('CITY_FACADE_COMPLETE: {} surfaces; ground and overview aligned'.format(len(changed)))
