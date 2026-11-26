# mama 개발 진행 기록 / 다음 작업 인수인계

## 2026-11-16 해저 도시 맵 통합

### 카메라·건물 재질 수정

- 실행용 레벨: `/Game/Maps/LV_AbyssalCity_Dense_Playable`. 잠긴 원본 레벨은 보존하고 수정본으로 기본 게임·에디터 실행을 연결했다.
- 지형의 월드 좌표·회전은 고정. 플레이어에 연결한 Spring Arm이 기존 -68도 3인칭 탑뷰 시야를 유지하며, 조준 시 카메라가 회전하지 않는다.
- 외벽 165곳의 구역별 재질·창문·층 패턴과 고정 노출을 적용했다. `Materials/AbyssalCity/Buildings`에서 별도 편집한다.
- 실행 중인 기존 모듈을 보존하고 `UnrealEditor.modules`를 새 검증 모듈 `UnrealEditor-mama-61116.dll`에 연결했다. 기존 에디터를 재시작하거나 새 게임을 실행하면 새 코드가 적용된다. 다음 정상 빌드는 일반 모듈 이름으로 돌아가도 된다.
- 코드 검증 사본은 `Saved/CameraFacadePreview`, 최종 프로젝트 테스트는 `Saved/Logs/CameraGameplayTest.log`의 `ABYSS_TEST ALL PASSED`를 확인한다.

최신 지형·시작 맵은 `LV_AbyssalCity_Dense`입니다. 아래의 Entry 맵/40m 테스트 지형 설명은 이전 기록입니다.
새 파일 구조와 편집 방법은 `ABYSSAL_CITY_GUIDE.md`를 먼저 확인하세요.
맵·모델·머티리얼·텍스처·이펙트는 종류별 폴더로 분리하고 언리얼에서 참조를 갱신했습니다.
기존 테스트 지형 생성은 도시 맵에서 실행하지 않으며, 그 외 기존 맵의 동작은 유지합니다.
도시 전용 세이브는 `AbyssCityRun.json`으로 구분합니다.

최종 기록: 2026-11-02 (한국 시간). 다음 세션에서는 이 문서를 먼저 읽고 실제 소스를 확인한 뒤 이어서 작업한다.
이 문서는 구현 상태와 작업 맥락의 기록이며, 플레이 중 세이브 데이터는 아니다.

## 프로젝트와 사용자 방향

- 위치: `C:\Users\USER\Documents\Unreal Projects\mama`
- 엔진: Unreal Engine 5.7, Windows, C++ 프로젝트. 임시 게임명 `ABYSS / LAST LIGHT`.
- 전 세계가 물에 잠겼고, 고장 난 잠수함의 수리 재료를 모아 탈출하는 탑뷰 3D 수중 숨바꼭질 게임. Dead by Daylight의 추적·위험 요소 참고.
- 사용자는 맵, 캐릭터, 몬스터, 아이템 등 외형 에셋을 나중에 통합할 예정이라고 했다. 작은 테스트 맵과 기본 도형으로 시스템 구현을 우선한다.
- 수중 분위기는 어두운 푸른 조명, 짙어지는 안개, 주기적인 물방울로 표현한다.
- UI는 최근 요청으로 청록·블루 네온의 SF HUD로 개선했다. 참고 방향: <https://stock.adobe.com/kr/search?k=game+ui>.
- 외부 스톡 에셋을 구매하거나 다운로드해 사용한 것이 아니라 C++ Canvas로 직접 그린 UI다.

## 현재 실행·빌드

- `Play Abyss.cmd`: 독립 게임 창 실행. 설치된 UE 5.7의 `UnrealEditor.exe -game`을 이용하며 패키징된 게임 실행 파일은 아니다.
- `Build Abyss.cmd`: `mamaEditor Win64 Development` 빌드. 실행 중인 mama 게임/에디터 모듈을 닫고 빌드한다.
- `mama.uproject`를 에디터에서 열고 Play도 가능하다. 기존 맵의 GameMode Override가 설정되어 있다면 `AbyssGameMode`인지 확인한다.
- 시작 맵은 `/Engine/Maps/Entry`, 기본 게임 모드는 `/Script/mama.AbyssGameMode`.
- 도시는 `BuildSeafloor()`에서 플레이 시작 시 생성한다. 에디터에서 시작 맵이 비어 보이는 것은 정상이다.
- 기존 `Content/Untitled.umap`, `BPMyActor.uasset` 등은 보존했다.
- 처음에는 셰이더 컴파일로 실행이 오래 걸릴 수 있다.

## 최신 조작법

| 입력 | 동작 |
|---|---|
| WASD | 화면 기준 이동 |
| 마우스 | 캐릭터 방향과 손전등 조준 |
| Shift + 이동 | 빠르게 이동, 산소 소모 3배, 적이 듣는 범위 증가 |
| E | 부품 회수 이벤트 시작, 산소통 수집, 해초 은신/해제, 수리 시작, 탈출 |
| Space | 부품 회수 타이밍 이벤트 판정 |
| Tab | 장비/인벤토리 표시 전환. 게임은 정지하지 않음 |
| Q | 예비 산소통 1개 사용, 산소 55 보충 |
| F | 손전등 켜기/끄기 |
| R | 승리/사망 후 재시작 |
| Alt+F4 | 독립 게임 창 닫기 |

**숫자키 1~4 선택 및 하단 퀵슬롯은 제거했다. 되살리지 않는다.**

## 구현된 게임 시스템

### 이동·생존·수중 표현

- `AAbyssDiver`: 탑뷰 카메라, 충돌 및 벽을 따라 미끄러지는 이동, 마우스 평면 조준, 손전등, 산소통 모형.
- 일반 이동 315, 빠른 이동 510 Unreal units/초. 높이는 고정되어 있으며 상하 자유 수영은 미구현이다.
- 체력 100. 몬스터 접촉 시 35 피해, 공통 피격 보호 시간 1.5초. 체력 0이면 사망한다.
- 산소 100. 일반 소모 0.65/초, Shift 이동 소모 1.95/초. 산소 0이면 체력도 0으로 만들고 즉시 사망한다.
- 시작 예비 산소통 2개. 초록 원통을 주우면 자동 저장. Q로 산소 55 회복, 최대 100.
- 손전등은 실제 켜진 시간으로 120초 사용. 끄거나 해초 은신 중에는 보존한다. 고갈되면 다시 켤 수 없고 주변 보조광도 120에서 12로 약해진다.
- 약 3분에 걸쳐 방향광이 1.6에서 0.12로 약해지고 안개 밀도가 증가한다. 노출은 수동이며 현재 AutoExposureBias=1.0.
- 약 1.2초마다 플레이어 근처에서 물방울 4개 발생, 약 2.8초간 위로 떠오른다. 메시 컴포넌트 24개를 재사용하는 고정 풀이다. 사망 후 새 물방울은 발생하지 않는다.
- `FlashlightDuration`, `BubbleInterval`, `SalvageDuration`, `SalvageWindow`는 GameMode의 EditDefaultsOnly 속성이다. 나머지 다수 수치는 아직 C++ 상수/일반 멤버다.

### 작은 테스트 맵·건물

- 약 40m 맵, 코드의 `Abyss::MapScale=0.55`로 생성 지형의 XY를 축소한다. 생성 함수에 전달하는 원래 좌표와 최종 월드 좌표를 혼동하지 말 것.
- 중앙 잠수함, 북서 연구소(전력 셀), 남서 선박 묘지(선체 합금), 북동 펌프장(추진 부품), 해초 은신 지점, 산소통.
- 잠수함 남동쪽에 큰 직사각형 침수 대피소. 실제 중심 `(-935,495,0)`, XY 반폭 `(385,330)`.
- 북쪽 벽 중앙의 약 220cm 출입구로 입장한다. 내부에 들어가면 벽과 지붕 재질이 반투명으로 바뀌고 그림자를 끈다. 나가면 원래 재질/그림자로 복구한다. 충돌은 유지한다.
- 반투명 재질은 엔진 `Widget3DPassThrough_Translucent`의 MID, `TintColorAndOpacity`와 흰 `SlateUI` 텍스처를 사용한다. 재질 로딩 실패 시에는 충돌을 남긴 채 벽 가시성을 끄는 대체 동작이 있다.
- 별도 바닥색과 파란 실내 조명, 추가 선체 부품 회수 지점과 산소통이 있다.
- 실내 판정은 축 정렬 사각형 범위 검사이며 별도 스트리밍 레벨/인스턴스 공간은 아니다.

### 인벤토리·부품 회수 타이밍 이벤트

- `Inventory[0..3]`: 전력 셀, 선체 합금, 추진기 부품, 예비 산소통 개수.
- 수리 부품 회수 장소에서 E → 타이밍 이벤트 시작. 녹색 성공 구간을 흰 커서가 지날 때 Space.
- 제한 2.4초, 전체 진행도 중 성공 구간 폭 0.17, 시작 위치는 0.35~0.65에서 무작위.
- 성공해야 부품 1개가 자동으로 인벤토리에 저장되고 회수 대상이 사라진다.
- 이른/늦은 입력, 제한 시간 초과, 대상에서 150 units 이상 이탈하면 실패한다.
- 실패해도 부품은 남아 재시도 가능하다. 대신 적 3마리 모두 소리가 난 위치를 18초 추적하도록 알린다.
- 산소통은 타이밍 이벤트 없이 즉시 획득한다. 게임과 자원 소모는 이벤트 중에도 진행된다.

### 순차 수리·탈출 (최신 동작)

- `NextRepairStage()`: 전력 → 선체 → 추진기 순서로 첫 미완료 단계를 반환한다. 모든 단계 완료 시 3.
- 잠수함의 어느 수리 단말기에서든 E를 누르면 **다음 단계**를 시작한다. 필요한 부품 종류를 자동 판단하며 선택 UI는 없다.
- 각 단계는 대응 부품 2개 필요. 부족하면 시작하지 않고 인벤토리를 열어 해당 항목을 강조한다.
- E 한 번으로 4초 수리를 시작하고 단말기에서 150 units 안에 머물러야 한다. E를 계속 누를 필요 없다.
- 완료 시에만 부품 2개 소모, 해당 단계 해금. 다음 단계는 다시 E를 눌러야 시작한다.
- 이탈하면 진행도 초기화. 수리 소음은 몬스터를 유인한다.
- 단말기 월드 라벨은 `SERVICE 01`, `SERVICE 02`, `SERVICE 03`으로 짧게 변경했다.
- 세 단계 완료 후 ESCAPE HATCH에서 E → 승리. R로 시작 맵을 다시 연다.

### 적·은신

- 붉은 몬스터 총 3마리. 기존 `Hunter`와 `ExtraHunters` 2개로 구성한다.
- 각자 기억 시간, 마지막 확인 위치, 순찰 경로 인덱스를 가진다. AI 갱신은 공통 `TickHunter()`.
- 기본 순찰, 시야 추적, 달리기/수리 소리 감지, 부품 이벤트 실패 유인, 접촉 피해가 있다.
- 손전등을 켜면 시각 발견 범위 증가. 건물이 시야를 막는다.
- 장애물은 sphere sweep와 우회 방향 선택으로 처리한다. NavMesh 기반 완성형 길찾기는 아니다.
- 해초에서 E로 은신. 이동하면 해제되고 은신 중 손전등은 보이지 않는다. 적이 이미 위치를 기억하면 즉시 안전해지지 않는다.

## 최신 UI 디자인

- `AAbyssHUD::DrawHUD()`에서 Canvas로 직접 그린다. UMG 위젯/외부 UI 텍스처 팩은 아직 없다.
- 청록·블루 발광 테두리, 반투명 어두운 패널, 사선 모서리, 텍스트 그림자, 분절 게이지, 원형 장비 게이지.
- 좌상단 산소/상태, 우상단 잠수함 단계, 우하단 격자/회전 소나 미니맵, 하단 조작 안내.
- 캐릭터 좌표를 화면으로 투영한 뒤 아래쪽에 따라다니는 체력바 표시.
- Tab: 배경을 어둡게 처리하고 산소/손전등 장비 카드, 예비 산소통 수, 부품 목록과 수리 해금 상태 표시.
- 미완료 수리 부품이 2개 미만이면 금색 점멸. 수리 시 부족한 부품은 추가 강조.
- 산소/배터리 **잔량 30% 이하**이면 숫자·게이지가 빨간색 점멸. 사용자 표현의 30%는 현재 잔량 기준으로 해석해 구현했다.
- 위험 경고, 상호작용 안내, 수리 진행도, 타이밍 이벤트, 종료 화면도 같은 프레임 계열.
- 화면 가로·세로 비율을 함께 고려해 스케일 조정. 720p 가독성 확인 후 글씨를 15% 키웠다.
- 게임 문구는 기본 엔진 폰트 호환성을 위해 영어다. 한국어 UI 전환은 아직 미구현.

## 핵심 파일과 구조

| 파일 | 역할 |
|---|---|
| `Source/mama/AbyssGame.h` | Pawn/Controller/HUD/GameMode, 사이트·적 데이터와 상태 선언 |
| `Source/mama/AbyssGame.cpp` | 현재 주요 게임 시스템과 맵 생성, Canvas UI 전체 |
| `Config/DefaultEngine.ini` | 시작 맵과 기본 GameMode 설정 |
| `Play Abyss.cmd` | 로컬 실행 |
| `Build Abyss.cmd` | 로컬 빌드 |
| `PLAY_GUIDE.md` | 사용자 조작/플레이 설명 |
| `Saved/Logs/AbyssBuild.log` | 최신 빌드 로그 |
| `Saved/Logs/AbyssSmokeTest.log` | 시스템 통합 검사 로그 |
| `Saved/Logs/AbyssHUDPreview.log` | 최신 렌더링 캡처 실행 로그 |
| `Saved/Screenshots/AbyssPreview.png` | 최신 실제 플레이 HUD 캡처, 1600×900 |
| `Saved/Screenshots/AbyssInventory.png` | 최신 실제 인벤토리 캡처, 1600×900 |

기존 연습 코드의 컴파일 문제도 수정했다: `CodingTest.cpp`의 정의되지 않은 `testActor` 사용 제거, `MyActor.h`의 private BlueprintReadWrite 속성에 AllowPrivateAccess 메타 추가.

## 검증 결과와 범위

- 최신 UI 수정본도 `mamaEditor Win64 Development` 빌드 성공 (`Result: Succeeded`).
- 게임 시스템 최신 통합 검사: 2026-11-02 10:12 KST, `ABYSS_TEST ALL PASSED` 확인.
- 검사 항목: 이동/Shift 산소 소모 3배, 적 3마리 생성, 건물 진입 재질 전환과 충돌 유지/퇴장 복원, 물방울 풀, 배터리 소모·보존·고갈, QTE 실패/시간 초과/이탈/성공과 아이템 저장, 산소통, 은신, 부족한 부품 차단·강조, E 순차 수리와 부품 소모, 탈출, 피해 쿨다운, 산소 고갈 사망.
- 그 이후 변경은 UI 디자인과 단말기 표시 이름, 캡처 도우미다. UI만 바꾼 뒤 시스템 검사를 불필요하게 다시 실행하지 않고 빌드와 실제 렌더링을 확인했다.
- 최신 HUD/인벤토리는 2026-11-02 10:27 KST, **DX11/SM5에서 실제 실행한 1600×900 캡처를 직접 열어 확인**했다. 글씨 크기와 단말기 이름 겹침을 한 차례 수정했다.
- 기본 실행 스크립트는 기존 프로젝트 렌더러 설정(DX12)을 유지한다. 최신 UI의 시각 확인은 DX11에서 했으므로 DX12의 최종 조명·성능까지 확인했다는 뜻은 아니다.
- 건물 투명화는 상태/재질/충돌을 자동 검사했지만, 플레이어가 직접 걸어서 진입하는 전체 경로 및 투명도 외관은 별도 육안 확인이 남아 있다.
- 빨간 점멸과 QTE 화면은 코드 적용/빌드 완료. 최신 수동 캡처는 평상시 HUD와 인벤토리이며 모든 경고 상태의 스크린샷을 확보한 것은 아니다.

## 재현용 명령과 환경 주의

이 작업 환경에서 기본 AppData 경로의 UBT 설정/캐시 접근과 기본 셰이더 임시 경로 쓰기가 실패한 적이 있다. 권한을 광범위하게 바꾸지 않고 프로젝트 내부 캐시와 기존 XML 캐시를 사용했다.

PowerShell 빌드 예시 (프로젝트 루트에서):

```powershell
& 'C:/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/Build.bat' mamaEditor Win64 Development '-Project=C:/Users/USER/Documents/Unreal Projects/mama/mama.uproject' '-XmlConfigCache=C:/Users/USER/Documents/Unreal Projects/mama/Intermediate/Build/XmlConfigCache.bin' -NoUBTMakefiles -WaitMutex -NoHotReloadFromIDE -NoUBA '-Log=C:/Users/USER/Documents/Unreal Projects/mama/Saved/Logs/AbyssBuild.log'
```

- `-XmlConfigCache`만으로는 기존 Makefile 로딩 중 AppData 접근이 남아 `-NoUBTMakefiles`도 함께 필요했다.
- `Intermediate/Build/XmlConfigCache.bin`이 없는 새 환경에서는 이 캐시에 의존하는 우회가 불가능할 수 있다. 실행 환경에 맞게 UBT 설정 경로를 다시 확인한다.
- 실행 환경 변수 `UE-LocalDataCachePath`는 프로젝트의 `DerivedDataCache`로 지정한다. `-ddc=InstalledNoZenLocalFallback`, `-ShaderWorkingDir=프로젝트/Intermediate/...`를 사용한다. 실행 스크립트에 반영되어 있다.
- PowerShell에서 한글 문서를 읽을 때 `Get-Content -Encoding UTF8`을 사용한다. 기본 인코딩으로 출력하면 한글이 깨져 보일 수 있다.

통합 검사 실행은 위 환경 변수/캐시 옵션을 유지한 채 UnrealEditor.exe에 다음 인수를 사용한다:

```text
"프로젝트절대경로/mama.uproject" /Engine/Maps/Entry?game=/Script/mama.AbyssGameMode -game -nullrhi -nosound -unattended -AbyssSmokeTest -abslog="프로젝트절대경로/Saved/Logs/AbyssSmokeTest.log"
```

실제 UI 캡처 옵션:

```text
-game -d3d11 -sm5 -RenderOffscreen -windowed -ForceRes -ResX=1600 -ResY=900 -nosound -unattended -AbyssCapture
```

`-AbyssCapture` 전용 동작: 디버그 화면 메시지를 끄고 12초에 HUD 캡처, 16초에 인벤토리 열기, 18초에 인벤토리 캡처. 이 자동 인벤토리 전환은 일반 플레이에는 적용되지 않는다. 캡처 뒤 테스트로 실행한 프로세스만 종료해 자원을 정리했다.

## 남은 작업 / 다음 세션 참고

확정된 추가 기능 요청은 아직 없다. 아래는 미구현·검증 필요 영역이며 임의로 큰 작업을 시작하지 말고 다음 사용자 요청에 맞춰 진행한다.

- 사용자 실제 플레이 피드백 수렴, 수리/QTE 난이도·적 배치·산소·배터리 밸런스 조정.
- 실제 건물 출입, 벽 투명도, 각 적의 장애물 우회 및 내부 추적을 육안 확인.
- 저산소/저배터리/피해/추적/QTE/승리·실패 UI와 다른 해상도에서 겹침 점검.
- DX12 기본 실행의 최종 화면 및 성능 확인. DX11 캡처와 차이가 있을 수 있다.
- 정식 모델, 애니메이션, 맵, 물/기포 VFX, 음향, UMG UI 등 외형 통합.
- 멀티플레이, 패키징 배포, 저장/불러오기, 일시정지 메뉴, 한국어 폰트/UI, 완성형 적 NavMesh 경로 탐색은 아직 없다.
- 현재 한 C++ 파일에 시스템과 렌더링이 모여 있다. 규모가 커지면 Actor/Component/Widget으로 분리하고 필요한 상태를 Blueprint에 노출하는 리팩터링을 고려한다.

다음 세션 시작용 요청 예시:

> `C:\Users\USER\Documents\Unreal Projects\mama\PROGRESS.md`를 읽고 현재 구현을 확인한 뒤, 이어서 [원하는 수정 사항]을 작업해줘.


//내가 추가해야하는거 -ai로 자연재해 랜덤 인카운트 .

소용돌이- 빨려들어가서 랜덤 장소로 이동.
, 졸라 깜깜해지기, 심해 공포증(방향키 반대), 과호흡, 
수온이 떨어져서 이동속도감소, 천둥으로 피까이고 몬스터 다 몰려오기( 건물에 숨어야함) 

## 사용자 요청으로 작업 중단
2026-11-16: 변경 코드와 중단 위치를 PAUSED_WORK.md에 저장했다. Compact 맵 생성은 건물 배치 단계에서 미완료이며 기본 실행 맵은 기존 Playable을 유지한다. 사용자 재개 요청 전에는 작업하지 않는다.

