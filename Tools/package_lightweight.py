"""Create a lossless, editable Unreal project ZIP without generated bulk files."""
from pathlib import Path
from datetime import datetime
import json
import os
import sys
import zipfile

project=Path(__file__).resolve().parent.parent
stamp=datetime.now().strftime('%Y%m%d')
output=Path(sys.argv[1]) if len(sys.argv)>1 else project.parent/f'mama_light_{stamp}.zip'
if output.exists():
    output=output.with_name(output.stem+'_'+datetime.now().strftime('%H%M%S')+'.zip')
temporary=output.with_suffix('.zip.part')
if temporary.exists():
    raise RuntimeError('Temporary archive already exists: '+str(temporary))

files=[]
for folder in ['Config','Content','Source','Tools','UI']:
    files.extend(p for p in (project/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
for name in ['mama.uproject','.vsconfig','Build Abyss.cmd','Play Abyss.cmd','Open Abyssal City.cmd']:
    path=project/name
    if path.exists():files.append(path)
files.extend(project.glob('*.md'))

# Preserve the currently selected small runtime module, plus the ordinary module
# referenced by the editor receipt. Debug symbols and obsolete hot-reload files
# are rebuildable and intentionally excluded.
binary_root=project/'Binaries'/'Win64'
manifest_path=binary_root/'UnrealEditor.modules'
module_manifest=json.loads(manifest_path.read_text(encoding='utf-8-sig'))
files.append(manifest_path)
for filename in set(module_manifest['Modules'].values())|{'UnrealEditor-mama.dll'}:
    path=binary_root/filename
    if not path.is_file():raise RuntimeError('Required module is missing: '+filename)
    files.append(path)
files.extend(binary_root.glob('*.target'))
files=sorted(set(files),key=lambda p:p.relative_to(project).as_posix())
entries=[{'path':p.relative_to(project).as_posix(),'bytes':p.stat().st_size} for p in files]
included_bytes=sum(e['bytes'] for e in entries)
original_bytes=0
for directory,subdirs,names in os.walk(project):
    for name in names:
        try:original_bytes+=(Path(directory)/name).stat().st_size
        except OSError:pass

readme='''# mama 경량 프로젝트

압축을 해제한 뒤 `mama/Play Abyss.cmd`로 실행하거나 `mama.uproject`를 여세요.
Unreal Engine 5.7이 필요합니다. 포함된 Windows 모듈과 엔진 빌드가 다르면
Visual Studio C++ 도구를 설치하고 `Build Abyss.cmd`로 다시 빌드하세요.

현재 기본 맵은 `LV_AbyssalCity_Dense_Playable`입니다.
0.7배 축소·통짜 건물 작업은 중단 당시 미완료 상태 그대로 보존했습니다.
이어할 위치는 `PAUSED_WORK.md`에 있습니다. ZIP 생성 중 게임 작업은 재개하지 않았습니다.

포함: 맵, 모델, 원본 텍스처, 머티리얼, UI, C++ 소스, 설정, 도구, 작업 기록,
현재 실행 모듈. 텍스처 해상도나 에셋 품질은 낮추지 않았습니다.

제외: `.vs`, `DerivedDataCache`, `Intermediate`, `Saved`의 캐시·백업·검증 사본,
PDB 디버그 심볼과 이전 핫리로드 부산물. 캐시와 중간 파일은 필요 시 재생성됩니다.
기록에 나오는 과거 Saved 경로의 로그·백업은 원본 프로젝트에만 남아 있습니다.
원본 프로젝트의 파일은 삭제하지 않았습니다.

전체 파일 목록은 `LIGHT_PACKAGE_MANIFEST.json`을 참고하세요.
'''

with zipfile.ZipFile(temporary,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as archive:
    for index,(path,entry) in enumerate(zip(files,entries),1):
        archive.write(path,'mama/'+entry['path'])
        if index%100==0:print(f'Packed {index}/{len(files)} files',flush=True)
    archive.writestr('mama/LIGHT_PACKAGE_README.md',readme)
    archive.writestr('mama/LIGHT_PACKAGE_MANIFEST.json',json.dumps({'included_files':len(files),'included_bytes':included_bytes,'files':entries},ensure_ascii=False,indent=2))

with zipfile.ZipFile(temporary,'r') as archive:
    bad=archive.testzip()
    if bad:raise RuntimeError('Archive CRC failed: '+bad)
    names=set(archive.namelist())
    for required in ['mama/mama.uproject','mama/Content/Maps/LV_AbyssalCity_Dense_Playable.umap','mama/Source/mama/mama.Build.cs','mama/Content/UI/index.html']:
        if required not in names:raise RuntimeError('Archive entry missing: '+required)
    for filename in module_manifest['Modules'].values():
        if 'mama/Binaries/Win64/'+filename not in names:raise RuntimeError('Active module omitted')
    for name in names:
        if not name.startswith('mama/') or '..' in Path(name).parts:raise RuntimeError('Unexpected archive path')
        if name.startswith(('mama/.vs/','mama/Intermediate/','mama/DerivedDataCache/','mama/Saved/')):raise RuntimeError('Generated folder included')

temporary.rename(output)
result={'zip':str(output),'files':len(files),'original_bytes':original_bytes,'included_bytes':included_bytes,'zip_bytes':output.stat().st_size,'reduction_percent':round(100*(1-output.stat().st_size/original_bytes),2),'crc_verified':True}
(project/'Saved'/'LightPackageReport.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False),flush=True)
