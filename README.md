# 디지털 트윈 프로젝트

Unreal Engine 5.7 기반 해저 탐험 게임 프로젝트입니다.

## 내려받기

Git과 Git LFS를 설치한 뒤 다음 명령으로 내려받습니다.

```powershell
git lfs install
git clone https://github.com/kimyubin0717/escapeSea.git
cd escapeSea
git lfs pull
```

맵·모델·텍스처는 Git LFS로 관리합니다. 위 명령으로 실제 에셋을 내려받으세요.

## 실행

Unreal Engine 5.7과 Visual Studio C++ 빌드 도구가 필요합니다.
`mama.uproject`를 열고 게임 모듈을 빌드하거나, 포함된 `Build Mama.cmd`의
로컬 엔진 경로를 확인한 뒤 실행하세요. `Play Abyss.cmd`로 게임을 시작합니다.
일부 실행·제작 도구에는 제작 PC의 절대 경로가 있으므로 다른 PC에서는 조정이 필요합니다.

소스, 설정, 콘텐츠, 제작 원본, UI와 도구를 포함합니다.
빌드 결과, IDE 파일, 캐시, 개인 저장 데이터는 제외하며 필요 시 다시 생성합니다.

작업 구조와 진행 상황은 `AGENTS.md`, `PARTS.md`, `PROGRESS.md`를 참고하세요.
이번 업로드는 로컬 `mama` 폴더의 스냅샷이며 게임 빌드나 플레이 검수를 새로 수행하지 않았습니다.
