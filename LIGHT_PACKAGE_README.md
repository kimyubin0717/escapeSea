# mama 경량 프로젝트

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
