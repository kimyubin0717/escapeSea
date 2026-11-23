# Game System ver1 — 메뉴와 진행 저장

새로하기 / 이어하기 / 자동 저장 / 메뉴 복귀 / 게임 종료 / 재시작과
진행 상태·인벤토리 복원에 직접 관련된 파일만 선별했습니다.
맵, 캐릭터, AI, 생존/수리/부품 회수 구현, 에셋, 실행 파일은 포함하지 않습니다.

## 용도와 적용 대상

이 묶음은 완성된 독립 Unreal 프로젝트나 플러그인이 아닙니다.
Unreal Engine 5.7의 기존 mama 프로젝트에 적용할 소스 파일 묶음입니다.
기준은 mama_Integrated이며, 이 프로젝트에는 이미 같은 파일이 적용되어 있습니다.
게임 공용 상태와 기존 클래스가 없는 빈 프로젝트에 이것만 복사하면 빌드되지 않습니다.
구형 mama에 적용할 때는 Integration/CONNECTIONS.md의 연결 변경이 추가로 필요합니다.

## 파일 구분

- Source/mama/.../Save: 실제 게임 진행을 디스크에 저장/복원
- Source/mama/.../UI: 메뉴 명령 라우팅, 게임 상태 전달, HTML UI 컴포넌트
- Source/mama/.../Inventory: 아이템 배치 저장과 복원에 필요한 지원 기능
- Content/UI/scripts/menu.js: UI 메뉴 + 브라우저 미리보기용 진행 관리
- Content/UI/scripts/runtime.js: 실제 게임에서 C++에 명령을 보내는 어댑터
- Tools/test_runtime.cjs: UI 통신 계약 검증
- FILES.json: 선별 파일의 역할과 SHA-256

실제 게임의 저장은 C++ SaveService가 담당합니다. HTML 미리보기에서는 menu.js가
브라우저 localStorage를 사용하며, runtime=1로 게임에 연결하면 runtime.js가 그 동작을 대체합니다.
menu.js만 복사해서 실제 게임 저장이 구현되는 것은 아닙니다.

## UI ver1과 함께 올리기

GitHub에서는 기존 `ui ver1`과 나란히 `game system ver1` 폴더를 올리면 됩니다.
실제 mama 프로젝트에 적용할 때는 이 폴더 안의 Source와 Content를 프로젝트 루트에 매핑합니다.
UI ver1의 나머지 HTML/CSS/JS도 Content/UI에 있어야 합니다. JS 두 파일은 UI 패키지와 중복되며 같은 버전입니다.

검증: 11개 원본 파일과 복사본의 SHA-256 일치, ZIP CRC 확인.
기준 통합 프로젝트의 빌드와 게임 테스트는 이미 통과했습니다.
이 부분 묶음만 별도로 빌드하거나 빈 프로젝트에서 실행한 것은 아닙니다.
