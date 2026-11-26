#include "Player/AbyssController.h"
#include "UI/AbyssUIComponent.h"
#include "Characters/AbyssDiver.h"
#include "Game/AbyssGameLookup.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/PlayerInput.h"
#include "Framework/Application/SlateApplication.h"
#include "UnrealClient.h"
#include "InputKeyEventArgs.h"
#include "Engine/Texture.h"

AAbyssController::AAbyssController()
{
    bShowMouseCursor = true;
    DefaultMouseCursor = EMouseCursor::Crosshairs;
    GameUI = CreateDefaultSubobject<UAbyssUIComponent>(TEXT("GameUI"));
}
void AAbyssController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (InPawn) SetViewTarget(InPawn);
}
void AAbyssController::BeginPlay()
{
    Super::BeginPlay();
    if (APawn* P=GetPawn()) SetViewTarget(P);
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
    SetInputMode(Mode);
}
void AAbyssController::PlayerTick(float Dt)
{
    Super::PlayerTick(Dt);
    AAbyssGameMode* G = Abyss::Game(this);
    if (!G) return;
    const bool bShowUI = G->bEnded || G->bMenu || G->bInventory;
    if (bShowUI != bShowMouseCursor)
    {
        bShowMouseCursor = bShowUI;
        if (bShowUI)
        {
            FInputModeGameAndUI Mode;
            Mode.SetHideCursorDuringCapture(false);
            SetInputMode(Mode);
        }
        else
        {
            FInputModeGameOnly Mode;
            Mode.SetConsumeCaptureMouseDown(false);
            SetInputMode(Mode);
            if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
        }
    }
    if (WasInputKeyJustPressed(EKeys::Escape) && !G->bMenu)
    {
        if (GameUI) GameUI->HandleAction(TEXT("menu"),TEXT(""));
        else { G->bMenu=true; G->bInventory=false; }
        return;
    }
    if (bShowUI && GameUI && GameUI->IsConnected()) return;
    if (WasInputKeyJustPressed(EKeys::R) && G->bEnded) { UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)),true,TEXT("game=/Script/mama.AbyssGameMode?AbyssNew=1")); return; }
    if (G->bEnded || G->bMenu) return;
    if (!G->bMenu && !G->bInventory && WasInputKeyJustPressed(EKeys::V))
        if (auto* Diver=Cast<AAbyssDiver>(GetPawn())) Diver->ToggleView();
    if (WasInputKeyJustPressed(EKeys::E)) G->Interact();
    if (WasInputKeyJustPressed(EKeys::SpaceBar)) G->SubmitSalvage();
    if (WasInputKeyJustPressed(EKeys::Q)) G->UseOxygen();
    if (WasInputKeyJustPressed(EKeys::F)) G->ToggleFlashlight();
    if (WasInputKeyJustPressed(EKeys::Tab)) G->bInventory = !G->bInventory;
}

