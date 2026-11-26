"""Read-only capability check, executed by Unreal's PythonScript commandlet."""
import json
from pathlib import Path

import unreal


def available(class_name, method=None):
    cls = getattr(unreal, class_name, None)
    return cls is not None and (method is None or callable(getattr(cls, method, None)))


report = {
    "engine_version": unreal.SystemLibrary.get_engine_version(),
    "anim_blueprint_factory": available("AnimBlueprintFactory"),
    "create_asset": available("AssetTools", "create_asset"),
    "save_asset": available("EditorAssetLibrary", "save_asset"),
    "editor_actor_subsystem": available("EditorActorSubsystem"),
    "nav_mesh_bounds_volume": available("NavMeshBoundsVolume"),
    "set_anim_instance_class": available("SkeletalMeshComponent", "set_anim_instance_class"),
    "blueprint_editor_library": available("BlueprintEditorLibrary"),
    "compile_blueprint": available("BlueprintEditorLibrary", "compile_blueprint"),
}
output = Path(unreal.Paths.project_saved_dir()) / "Automation" / "editor_capabilities.json"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("EDITOR_AUTOMATION_CHECK " + json.dumps(report))
assert report["engine_version"].startswith("5.7."), "Review APIs for this engine version"
assert report["save_asset"], "Editor Scripting Utilities is unavailable"
