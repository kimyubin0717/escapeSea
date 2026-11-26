# 기존 mama에 연결할 때 필요한 항목

원본을 덮어쓰기 전에 백업하고 파일별 변경 내용을 비교합니다.
이 문서는 필요한 연결만 설명하며 자동으로 프로젝트를 수정하지 않습니다.

## 파일 의존성

선별 C++ 파일은 기존 `AbyssGame.h`와 `Common/AbyssShared.h`를 포함합니다.
AAbyssGameMode의 게임 상태/함수, AAbyssController, AAbyssDiver와 기존 월드/사이트가 필요합니다.
이 의존 기능들의 구현은 이번 메뉴/저장 패키지에 포함하지 않았습니다.

SaveService는 게임 모드의 산소, 체력, 배터리, 경과 시간, 소지품 수량, 수리 완료 상태,
사용된 상호작용 지점, 플레이어 위치와 Cargo 배치를 저장합니다.
LoadRun은 현재 월드의 Sites 수와 저장 데이터가 일치하는지 검증합니다.
다른 맵에 기존 세이브를 그대로 적용하거나 필드를 임의로 바꾸지 마세요.

## 게임 모드 초기화

`Private/Game/AbyssGameMode.cpp`에서 Inventory 헤더를 include하고 생성자에서:

```cpp
Cargo = CreateDefaultSubobject<UAbyssInventoryComponent>(TEXT("Cargo"));
```

공개 헤더에는 Cargo 속성과 UAbyssInventoryComponent 전방 선언이 있어야 합니다.
BeginPlay에서 새 게임 레벨 옵션을 읽습니다:

```cpp
if (UGameplayStatics::HasOption(OptionsString,TEXT("AbyssNew"))) bMenu=false;
```

새로하기/재시작은 현재 레벨을 `game=/Script/mama.AbyssGameMode?AbyssNew=1` 옵션으로 엽니다.
메뉴 상태에서는 게임 로직과 캐릭터 이동이 진행되지 않도록 해당 Tick의 bMenu 경계를 유지합니다.
이 코드 자체는 아래 파일에 포함되어 있지 않으므로 기존 구현과 비교해 적용해야 합니다.

## 플레이어 컨트롤러

`Private/Player/AbyssController.cpp`에서 UI/AbyssUIComponent.h를 include하고 생성자에서:

```cpp
GameUI = CreateDefaultSubobject<UAbyssUIComponent>(TEXT("GameUI"));
```

공개 헤더에는 GameUI UPROPERTY와 UAbyssUIComponent 전방 선언이 필요합니다.
메뉴에서는 플레이 입력을 차단합니다. 브라우저가 인벤토리/종료 화면 입력을 소유할 때
컨트롤러에서 같은 키를 중복 처리하지 않도록 기존 입력 경계를 유지합니다.
HTML UI 연결 시 Canvas HUD와 중복 표시되지 않도록 HUD의 IsConnected() 확인도 필요합니다.

## 빌드/HTML 연결

- 모듈 이름은 mama이며 MAMA_API를 사용합니다. 다른 모듈로 옮기면 API와 스크립트 경로를 검토해야 합니다.
- Build.cs: Core, CoreUObject, Engine, InputCore 및 Slate, SlateCore, WebBrowser, Json, JsonUtilities, ApplicationCore 의존성을 유지합니다.
- include 경로: 모듈 루트와 Private 경로가 포함되어 있어야 합니다.
- mama.uproject: WebBrowserWidget 플러그인 활성화가 필요합니다.
- 배포 시 `RuntimeDependencies.Add("$(ProjectDir)/Content/UI/...", StagedFileType.NonUFS)` 설정으로 HTML UI를 함께 포함합니다.
- UI ver1의 index.html은 ko.js, inventory.js, menu.js, app.js, runtime.js 순으로 로드합니다.
- runtime.css와 나머지 UI CSS도 필요합니다. UI 전용 패키지에 들어 있습니다.
- 게임에서는 UI/index.html?runtime=1을 열고 ABYSS_ACTION 메시지를 C++ HandleAction에 전달합니다.
- C++ SendState는 window.abyssRuntime.receive(state, cargo)를 호출합니다.

## 저장 경로

실제 게임은 프로젝트 Saved/SaveGames 아래 JSON을 사용합니다.
기본 월드: AbyssRun.json / 도시: AbyssCityRun.json / 축소 도시: AbyssCompactRun.json.
사용자 실제 세이브 파일은 이 패키지에 포함하지 않았습니다.

## 검증

전체 UI를 적용한 기존 mama 프로젝트에서 `node Tools/test_runtime.cjs`를 실행할 수 있습니다.
이 테스트는 JavaScript 통신 계약을 확인하며 실제 디스크 저장/불러오기 전체를 검증하는 것은 아닙니다.
기존 게임 모드/컨트롤러 연결을 바꾸면 관련 빌드와 새 게임→저장→메뉴→이어하기 흐름을 확인합니다.
