"""Place loot in the converted ground-floor lobbies of the Harbor Mk2 map."""
import unreal as u

level = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
assert level.load_level("/Game/IntegratedCity/Maps/LV_AbyssalCity_Dense_Streetscape_IntegratedCity_Harbor_Mk2")

authored = actors.get_all_level_actors()
lobbies = [actor for actor in authored if actor.actor_has_tag("AbyssInteriorRoof")
           and any(str(tag).startswith("AbyssInterior_Lobby_") for tag in actor.tags)]
assert len(lobbies) == 16, len(lobbies)
mesh_root = "/Game/Models/AbyssalCity/Interiors/SM_CityMarketCrate"
meshes = {state: u.load_asset(mesh_root + state) for state in ("Closed", "Open")}
assert all(meshes.values())

for roof in lobbies:
    tag = next(str(tag) for tag in roof.tags if str(tag).startswith("AbyssInterior_Lobby_"))
    center, extent = roof.get_actor_bounds(False)
    assert min(extent.x, extent.y) >= 400, tag
    forward = roof.get_actor_forward_vector()
    right = roof.get_actor_right_vector()
    # Keep the entry-to-center route free and put the crate in a reachable corner of the lobby.
    # This edge lobby has a blocked sightline in the far corner.
    depth = -260 if tag.endswith("SM_Block_03_217") else 260
    location = u.Vector(center.x, center.y, 4) + forward * depth + right * 230
    for state, mesh in meshes.items():
        label = tag.removeprefix("AbyssInterior_Lobby_") + " - supplies " + state.lower()
        actor = next((a for a in authored if a.get_actor_label() == label), None)
        if actor is None:
            actor = actors.spawn_actor_from_class(u.StaticMeshActor, location, roof.get_actor_rotation())
        assert actor, label
        actor.static_mesh_component.set_static_mesh(mesh)
        actor.set_actor_label(label)
        actor.set_folder_path("Integrated/Lobby Supplies")
        actor.set_actor_location(location, False, False)
        actor.set_actor_scale3d(u.Vector(1.38, 1.38, 1.38))
        actor.set_actor_enable_collision(False)
        actor.set_actor_hidden_in_game(state == "Open")
        actor.tags = [tag, "AbyssLobbySupply", "AbyssCacheOpened" if state == "Open" else "AbyssCacheCrate"]
        if state == "Closed":
            actor.tags = [*actor.tags, "AbyssNewCache"]
    u.log("LOBBY_SUPPLY " + tag)
assert level.save_current_level()
