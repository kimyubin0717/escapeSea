"""Frame Streetscape after its actual level viewport is ready."""
import unreal

def start_top_view(verify_only=False):
    state = {"seconds": 0.0, "handle": None}


    def tick(dt):
        state["seconds"] += dt
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = editor.get_editor_world() if editor else None
        if world and world.get_name() in ("LV_AbyssalCity_Dense_Streetscape",
                                          "LV_AbyssalCity_Dense_Streetscape_Integrated",
                                          "LV_AbyssalCity_Dense_Streetscape_IntegratedCity",
                                          "LV_AbyssalCity_Dense_Streetscape_IntegratedCity_Mk2",
                                          "LV_AbyssalCity_Dense_Streetscape_IntegratedCity_Harbor_Mk2"):
            target = unreal.Vector(0, 0, 52000)
            editor.set_level_viewport_camera_info(target, unreal.Rotator(pitch=-90, yaw=0, roll=0))
            view = editor.get_level_viewport_camera_info() or ()
            rotation = next((v for v in view if isinstance(v, unreal.Rotator)), None)
            position = next((v for v in view if isinstance(v, unreal.Vector)), None)
            if rotation and position and abs(rotation.pitch+90)<.1 and abs(position.z-target.z)<1:
                unreal.unregister_slate_post_tick_callback(state["handle"])
                unreal.log(f"MAMA_TOP_VIEW_READY: pitch={rotation.pitch:.1f}, height_cm={position.z:.0f}")
                if verify_only:
                    unreal.SystemLibrary.execute_console_command(world, "QUIT_EDITOR")
                return
        if state["seconds"]>20:
            unreal.unregister_slate_post_tick_callback(state["handle"])
            unreal.log_error("Streetscape editor top view unavailable after startup")

    state["handle"] = unreal.register_slate_post_tick_callback(tick)

