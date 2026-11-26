"""Build from an ASCII staging path, then install just the editor build artifacts."""
import hashlib, json, os, shutil, subprocess
from pathlib import Path

project=Path(__file__).resolve().parent.parent
engine=Path("C:/Program Files/Epic Games/UE_5.7/Engine")
build=engine/"Build/BatchFiles/Build.bat"
if not build.is_file(): raise SystemExit("Unreal Engine 5.7 build tools were not found.")
stage=Path.home()/"MamaCityBlender/Build"
if not str(stage).isascii(): raise SystemExit("Use an ASCII path for the build cache.")
stage.mkdir(parents=True,exist_ok=True)
assert not stage.is_junction() and not stage.is_symlink()
def digest(path):
    return hashlib.sha256(path.read_bytes()).digest()
def copy(src,dst):
    if not dst.is_file() or digest(src)!=digest(dst):
        dst.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(src,dst)
copy(project/"mama.uproject",stage/"mama.uproject")
for part in ("Source","Config","Content/UI"):
    src,dst=project/part,stage/part
    for directory, children, names in os.walk(src):
        if Path(directory)==src and part=="Source" and "SourceArt" in children:
            children.remove("SourceArt")
        for name in names:
            p=Path(directory)/name
            copy(p,dst/p.relative_to(src))
    for p in dst.rglob("*"):
        if p.is_file() and not (src/p.relative_to(dst)).is_file():
            assert p.resolve().is_relative_to(stage.resolve())
            p.unlink()
command=[str(build),"mamaEditor","Win64","Development","-Project="+str(stage/"mama.uproject"),
         "-NoHotReloadFromIDE","-NoUBA","-MaxParallelActions=2","-WaitMutex"]
subprocess.run(command,check=True,creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
src=stage/"Binaries/Win64"
target=project/"Binaries/Win64"
built=json.loads((src/"UnrealEditor.modules").read_text(encoding="utf-8-sig"))
installed=json.loads((engine/"Binaries/Win64/UnrealEditor.modules").read_text(encoding="utf-8-sig"))
assert built["BuildId"]==installed["BuildId"] and built["Modules"]["mama"]=="UnrealEditor-mama.dll"
copy(src/"mamaEditor.target",target/"mamaEditor.target")
try:
    copy(src/"UnrealEditor-mama.dll",target/"UnrealEditor-mama.dll")
except PermissionError:
    # A running editor keeps its module mapped. New sessions can load the staged binary independently.
    name="UnrealEditor-mama-"+digest(src/"UnrealEditor-mama.dll").hex()[:12]+".dll"
    copy(src/"UnrealEditor-mama.dll",target/name)
    built["Modules"]["mama"]=name
manifest=target/"UnrealEditor.modules"
temp=manifest.with_suffix(".modules.tmp")
temp.write_text(json.dumps(built,indent=2),encoding="utf8")
os.replace(temp,manifest)
print("Installed the rebuilt mama editor module.")

