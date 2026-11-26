"""Export and reimport only the requested Streetscape part."""
import json, os, subprocess, sys, tempfile
from pathlib import Path

tools=Path(__file__).resolve().parent
project=tools.parent
parts=json.loads((tools/"StreetscapeParts.json").read_text())
if len(sys.argv)!=2 or sys.argv[1] not in (*parts,"all"):
    raise SystemExit("Choose roads, terrain, markings, rim, plaza, or all.")
part=sys.argv[1]
blender=Path("C:/Program Files/Blender Foundation/Blender 5.2/blender.exe")
unreal=Path("C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe")
if not blender.is_file() or not unreal.is_file():
    raise SystemExit("Blender 5.2 or Unreal Engine 5.7 was not found.")
active=subprocess.check_output(["tasklist","/FI","IMAGENAME eq UnrealEditor.exe","/NH"],creationflags=subprocess.CREATE_NO_WINDOW)
if b"UnrealEditor.exe" in active:
    raise SystemExit("Close Unreal Editor before syncing the project.")
request={}
for role in parts if part=="all" else [part]:
    name=parts[role]
    source=project/"SourceArt/AbyssalCity"/role/(name+".blend")
    subprocess.run([blender,"-b",source,"-t","2","--python-exit-code","2","-P",
                    tools/"ExportStreetscapePart.py","--",name],check=True)
    request[name]=str(source.with_suffix(".fbx"))
with tempfile.TemporaryDirectory(prefix="mama-streetscape-") as td:
    manifest=Path(td)/"parts.json"
    manifest.write_text(json.dumps(request),encoding="utf-8")
    log=project/"Saved/Logs"/("Streetscape-"+part+".log")
    log.parent.mkdir(parents=True,exist_ok=True)
    proc=subprocess.run([unreal,project/"mama.uproject","-ExecutePythonScript="+str(tools/"ApplyStreetscapePart.py"),
        "-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities","-nullrhi","-unattended","-NoSound","-NoSplash",
        "-abslog="+str(log)],env=dict(os.environ,MAMA_STREETSCAPE_REQUEST=str(manifest)),
        creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS|subprocess.CREATE_NO_WINDOW, timeout=120)
    raw=log.read_bytes() if log.exists() else b""
    if proc.returncode or b"CITY_SYNC_COMPLETE" not in raw:
        raise SystemExit(f"Unreal import did not complete. See {log}")
    for line in raw.decode("utf-8",errors="replace").splitlines():
        if "CITY_PART_SYNCED" in line:
            print(line.partition("CITY_PART_SYNCED: ")[2])

