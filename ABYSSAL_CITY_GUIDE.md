# LV_AbyssalCity_Dense 작업 안내

## 열기

`Open Abyssal City.cmd`로 편집하거나 `Play Abyss.cmd`로 플레이합니다.
기본 에디터 시작 맵과 게임 시작 맵은 `/Game/Maps/LV_AbyssalCity_Dense_Playable`입니다.
`LV_AbyssalCity_Dense` 원본 배치 맵은 보존합니다. 플레이용 복사본에는 개선한 건물 재질과 정렬한 편집기 시점이 저장되어 있습니다.
이미 열려 있던 에디터는 다시 열면 새 카메라 코드와 기본 플레이용 레벨을 읽습니다. `Play Abyss.cmd`는 바로 새 게임 버전을 실행합니다.

## 에셋 구조

| 폴더 | 수정 대상 |
| --- | --- |
| `Content/Maps` | 도시 맵, 지형·도로·건물 배치, 조명 |
| `Content/Models/AbyssalCity/Architecture` | 건축 모듈 메시 |
| `Content/Models/AbyssalCity/Props` | 바위·가구·소품 메시 |
| `Content/Models/AbyssalCity/Shapes` | 파이프 등 기본 모델 |
| `Content/Models/AbyssalCity/Primitives` | 프로젝트 전용 기본 도형과 독립 해저 바닥 모델 `SM_SeabedBase` |
| `Content/Materials/AbyssalCity` | 표면·소품·색상 변형·이펙트 머티리얼 |
| `Content/Materials/AbyssalCity/Buildings` | 건물 외벽과 금융·주거·병원·공업 구역별 재질 인스턴스 |
| `Content/Textures/AbyssalCity` | 텍스처와 HDRI |
| `Content/Effects/AbyssalCity` | 먼지·증기·불꽃 파티클 |
| `Content/Audio/AbyssalCity` | 사운드 |
| `Content/Blueprints/AbyssalCity` | 배경 관련 블루프린트 |
| `Content/Maps/Examples` | 원본 StarterContent의 예제 맵 |
| `Source/mama/Private/World` | 지형과 게임 상호작용 연결 코드 |

맵에서 액터의 위치·회전·크기를 바꾸면 해당 배치만 달라집니다. Static Mesh 에셋 자체를 수정하면 그 모델을 쓰는 모든 배치에 반영됩니다. 특정 배치만 별도 모델로 바꾸려면 모델 에셋을 복제해 지정하세요.

이 맵의 해저 바닥은 원본과 같은 **Static Mesh** 방식입니다. Landscape 높이맵 방식은 아니므로 Landscape 조각 도구 대신 메시 편집·배치로 수정합니다. 맵의 액터들은 개별 편집 가능한 상태를 유지합니다.

콘텐츠 브라우저에서 에셋을 이동·이름 변경하면 언리얼이 참조를 갱신합니다. 탐색기에서 `.uasset`/`.umap`을 직접 옮기지 마세요.

## 게임 연결

- 지형은 원래 월드 위치에 고정되고 플레이어만 이동합니다.
- 카메라는 플레이어에 연결된 Spring Arm을 따라갑니다. 기존 3인칭 탑뷰 원근 시야와 -68도 각도를 유지하고, 캐릭터 조준 방향이 카메라의 좌우 회전이나 기울기를 바꾸지 않습니다.
- 시작 위치는 금융 구역 건물 입구 앞입니다.
- 건물 외벽 165곳에 창문·층 패턴을 적용했습니다. `Buildings` 폴더의 `MI_Facade_*` 에셋에서 구역별 벽 색상·표면 텍스처·유리 색을 수정할 수 있습니다.

- 원본 500m 도시와 해저 지형을 유지합니다.
- 이 맵에서는 기존 코드 생성 테스트 지형을 만들지 않습니다.
- `13_Mama_Gameplay` 폴더에 부품·산소·은신·잠수함 상호작용 마커를 분리했습니다.
- 마커의 `AbyssSite_0`~`AbyssSite_8` 태그가 상호작용 종류를 지정합니다. 위치를 옮기면 플레이 시 그 위치에 생성됩니다.
- 부두 접근 계단은 `13_Mama_Gameplay/PierAccess`에서 수정합니다.
- 도시 전용 진행 저장은 `Saved/SaveGames/AbyssCityRun.json`입니다.
- 원본 다운로드 프로젝트와 기존 `Untitled` 맵은 유지합니다.

## 작업 도구와 기록

- `Tools/integrate_abyssal_city.py`: 상호작용 마커와 부두 계단 구성. 기존 통합 마커를 갱신합니다.
- `Tools/organize_abyssal_assets.py`: 언리얼 API를 사용한 에셋 분류·이동.
- `Tools/finalize_abyssal_city.py`: 프로젝트 전용 기본 모델 지정·최종 참조 검사.
- `Tools/improve_city_facades.py`: 원본 배치를 바탕으로 플레이용 복사본과 건물 재질을 생성합니다.
- `Saved/AbyssalAssetMoves.json`: 에셋 이동 전후 경로.
- `Saved/AbyssalCityIntegration.json`: 맵 액터·메시 검사 결과.
- `Saved/Backups/AbyssalCityIntegration`: 통합 전 설정·코드 백업.
- `Saved/Backups/AbyssalAssetRecovery_*`: 참조 복구 전 에셋 보존본.
- `Saved/Logs/AbyssalCityBuild.log`: C++ 빌드 결과.
- `Saved/Logs/AbyssalCitySmokeTest.log`: 실제 게임 상호작용 검증 결과.
- `Saved/Logs/CameraGameplayTest.log`: 최종 플레이용 맵의 지형 고정·카메라 추적·조준 및 게임 흐름 검증.
- `Saved/CityFacadeReport.json`: 건물 외벽 변경 목록과 지면 회전 검사.

## 검증 결과 (2026-11-16)

- C++ 에디터 빌드 성공.
- 종류별 에셋 274개 이동, 원본 에셋 271개 참조 비교에서 누락 0건.
- 도시 맵 액터 797개, 상호작용 마커 44개, 누락 메시 0개.
- 이전 경로 리다이렉터 정리 완료.
- 실제 게임 입력으로 이동·부두 계단 진입을 검증했습니다.
- 부품 수집·산소·은신·수리 3단계·탈출·피해 처리 통합 테스트 `ABYSS_TEST ALL PASSED`.
