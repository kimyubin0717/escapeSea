"""Replace the harbor vessel in a playable copy, keeping the authored service deck and sites."""
import unreal as u
base="/Game/IntegratedCity/Maps/LV_AbyssalCity_Dense_Streetscape_IntegratedCity"
dest=base+"_Mk2"
lib=u.EditorAssetLibrary
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
def load(name):
    asset=lib.load_asset("/Game/IntegratedCity/"+name)
    assert asset,name
    return asset
if not lib.does_asset_exist(dest):
    assert levels.load_level(base)
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    assert u.EditorLoadingAndSavingUtils.save_map(world,dest)
    del world
assert levels.load_level(dest)
ships=[a for a in actors.get_all_level_actors() if a.get_actor_label()=="Nautilus - service berth"]
assert len(ships)==1
ship=ships[0]
mesh=load("NautilusMk2/Meshes/SM_Sub2")
hull=mesh.get_bounds().box_extent
print("MK2_MODEL",hull,[(str(s.material_slot_name),str(s.material_interface.get_name()) if s.material_interface else None) for s in mesh.static_materials])
assert 350<hull.x<365 and 165<hull.y<185,hull
ship.static_mesh_component.set_mobility(u.ComponentMobility.MOVABLE)
ship.static_mesh_component.set_static_mesh(mesh)
ship.set_actor_scale3d(u.Vector(1,1,1))
ship.set_actor_rotation(u.Rotator(yaw=90),False)
# Berth along the three service points and hatch, with clearance on the deck.
ship.set_actor_location(u.Vector(-895-hull.y,19520,205),False,False)
ship.tags=["AbyssMk2Hull"]
ship.set_actor_enable_collision(False)
for name in ("SM_Sub2_Interior","SM_Sub2_LockerDoor","SM_Sub2_PowerLever","SM_Sub_BayDoor","SM_Sub_HatchLid"):
    # Only the cabin shares the hull origin. Hinged pieces use the matching mount socket.
    parent=ship if name!="SM_Sub2_LockerDoor" and name!="SM_Sub2_PowerLever" else cabin
    parent_mesh=parent.static_mesh_component
    sockets=[str(s) for s in parent_mesh.get_all_socket_names()]
    print("MK2_SOCKETS",name,sockets)
    socket={"SM_Sub2_LockerDoor":"LockerDoorHinge","SM_Sub2_PowerLever":"PowerLeverPivot","SM_Sub_BayDoor":"BayDoorHinge","SM_Sub_HatchLid":"HatchHinge"}.get(name)
    assert not socket or socket in sockets,(name,socket,sockets)
    label="Nautilus - "+name
    found=[a for a in actors.get_all_level_actors() if a.get_actor_label()==label]
    a=found[0] if found else actors.spawn_actor_from_object(load("NautilusMk2/Meshes/"+name),ship.get_actor_location(),ship.get_actor_rotation())
    a.static_mesh_component.set_mobility(u.ComponentMobility.MOVABLE)
    a.set_actor_label(label)
    a.set_folder_path("Integrated/Submarines/Mk2")
    a.set_actor_enable_collision(False)
    a.attach_to_component(parent_mesh,socket or "None",u.AttachmentRule.SNAP_TO_TARGET,u.AttachmentRule.SNAP_TO_TARGET,u.AttachmentRule.SNAP_TO_TARGET,False)
    a.set_actor_scale3d(u.Vector(1,1,1))
    if name=="SM_Sub2_Interior":
        cabin=a
        a.tags=["AbyssMk2Cabin"]
equipment=("SM_BCD","SM_Regulator","SM_DiveMask","SM_Fin","SM_Welder")
for i,name in enumerate(equipment):
    mesh=load("Equipment/Meshes/"+name)
    label="Dive station - "+name
    found=[a for a in actors.get_all_level_actors() if a.get_actor_label()==label]
    a=found[0] if found else actors.spawn_actor_from_object(mesh,u.Vector(),u.Rotator())
    a.set_actor_label(label)
    a.set_folder_path("Integrated/Harbor/Dive equipment")
    a.set_actor_location(u.Vector(-740,19000+i*65,204-mesh.get_bounding_box().min.z),False,False)
    a.set_actor_enable_collision(False)
lamp=load("Equipment/Meshes/SM_DiveLight")
diver=load("Player/Meshes/SK_Diver")
print("KIT_SIZES",lamp.get_bounds().box_extent,diver.get_bounds().box_extent)
assert 10<lamp.get_bounds().box_extent.x<13 and 85<diver.get_bounds().box_extent.z<100
assert levels.save_current_level()
print("WHITE_MK2_READY")
