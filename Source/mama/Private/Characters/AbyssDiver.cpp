#include "Characters/AbyssDiver.h"
#include "Status/AbyssConditionsComponent.h"
#include "Player/AbyssController.h"
#include "Game/AbyssGameLookup.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Engine/GameViewportClient.h"
#include "Misc/App.h"
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
#include "UnrealClient.h"
#include "InputKeyEventArgs.h"
#include "Engine/Texture.h"

#include "Common/AbyssShared.h"
AAbyssDiver::AAbyssDiver()
{
    PrimaryActorTick.bCanEverTick = true;
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("DiverCollision"));
    SetRootComponent(Collision);
    Collision->InitSphereRadius(36);
    Collision->SetCollisionProfileName(TEXT("Pawn"));
    Collision->SetSimulatePhysics(false);
    Movement=CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("DiverMovement"));
    Movement->SetUpdatedComponent(Collision);
    Movement->PrimaryComponentTick.bCanEverTick=false;
    DiverMesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Diver"));
    DiverMesh->SetupAttachment(RootComponent);
    DiverMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DiverMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    DiverMesh->SetRelativeLocation(FVector(0,0,-60));
    DiverMesh->SetRelativeScale3D(FVector(1.55f));
    DiverMesh->SetRelativeRotation(FRotator(0,-90,0));
    DiverMesh->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/IntegratedCity/Player/Meshes/SK_Diver.SK_Diver")));
    if (const USkeletalMesh* Model = DiverMesh->GetSkeletalMeshAsset())
        DiverMesh->SkinCacheUsage.Init(ESkinCacheUsage::Disabled, Model->GetLODNum());
    HandLamp=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandLamp"));
    HandLamp->SetupAttachment(DiverMesh,TEXT("hand_r"));
    // The animated hand carries the FBX unit conversion. Keep the lamp at diver scale.
    HandLamp->SetUsingAbsoluteScale(true);
    HandLamp->SetRelativeScale3D(DiverMesh->GetRelativeScale3D());
    HandLamp->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/IntegratedCity/Equipment/Meshes/SM_DiveLight.SM_DiveLight")));
    HandLamp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("TopDownCameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->SetUsingAbsoluteRotation(true);
    CameraBoom->SetWorldRotation(FRotator(ViewPitch,ViewYaw,0));
    CameraBoom->TargetArmLength = 1800;
    CameraBoom->TargetOffset=FVector(0,0,55);
    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->bInheritPitch = false;
    CameraBoom->bInheritYaw = false;
    CameraBoom->bInheritRoll = false;
    CameraBoom->bDoCollisionTest = false;
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
    Camera->SetupAttachment(CameraBoom,USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;
    Camera->FieldOfView = 72;
    Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("MouseFlashlight"));
    Flashlight->SetupAttachment(HandLamp,HandLamp->DoesSocketExist(TEXT("Beam")) ? TEXT("Beam") : TEXT("SOCKET_Beam"));
    if (!HandLamp->DoesSocketExist(Flashlight->GetAttachSocketName()))
        Flashlight->SetRelativeLocation(FVector(22,0,0));
    Flashlight->SetIntensity(6000);
    Flashlight->SetAttenuationRadius(2300);
    Flashlight->SetInnerConeAngle(15);
    Flashlight->SetOuterConeAngle(33);
    Flashlight->SetLightColor(FLinearColor(0.65, 0.88, 1));
    Flashlight->SetCastShadows(true);
    DiveGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("DiveLamp"));
    UPointLightComponent* Glow = DiveGlow;
    Glow->SetupAttachment(RootComponent);
    Glow->SetRelativeLocation(FVector(0, 0, 110));
    Glow->SetIntensity(260);
    Glow->SetAttenuationRadius(360);
    Glow->SetLightColor(Abyss::Teal);
}

void AAbyssDiver::BeginPlay()
{
    Super::BeginPlay();
    // A fixed pool keeps the breathing effect bounded, even during long sessions.
    for (int32 I=0; I<24; ++I)
    {
        UStaticMeshComponent* Bubble=NewObject<UStaticMeshComponent>(this);
        Bubble->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
        Bubble->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Bubble->SetCastShadow(false);
        Bubble->RegisterComponent();
        Bubble->SetVisibility(false);
        Bubbles.Add(Bubble); BubbleAges.Add(-1); BubbleOrigins.Add(FVector::ZeroVector);
    }
    const FString ClipPath=TEXT("/Game/IntegratedCity/Player/Meshes/SK_Diver_Anim_Armature_DV_");
    IdleClip=LoadObject<UAnimSequence>(nullptr,*(ClipPath+TEXT("SwimIdle")));
    SwimClip=LoadObject<UAnimSequence>(nullptr,*(ClipPath+TEXT("Swim")));
    SprintClip=LoadObject<UAnimSequence>(nullptr,*(ClipPath+TEXT("SwimFast")));
    CurrentClip=IdleClip;
    if (CurrentClip) DiverMesh->PlayAnimation(CurrentClip,true);
    UE_LOG(LogTemp,Display,TEXT("DIVE_KIT diver=%s hand=%d lamp=%s beam=%s idle=%s swim=%s fast=%s"),
        *GetNameSafe(DiverMesh->GetSkeletalMeshAsset()),DiverMesh->GetBoneIndex(TEXT("hand_r")),
        *GetNameSafe(HandLamp->GetStaticMesh()),*Flashlight->GetAttachSocketName().ToString(),
        *GetNameSafe(IdleClip),*GetNameSafe(SwimClip),*GetNameSafe(SprintClip));
}

void AAbyssDiver::UpdateBubbles(float Dt, bool bBreathing)
{
    BubbleTime+=Dt;
    BubbleClock+=Dt;
    AAbyssGameMode* G=Abyss::Game(this);
    const float Interval=G ? G->BubbleInterval : 1.2f;
    if (bBreathing && BubbleClock>=Interval && Bubbles.Num()>0)
    {
        BubbleClock=0; ++BubbleBursts;
        for (int32 I=0; I<4; ++I)
        {
            int32 K=NextBubble++ % Bubbles.Num();
            BubbleAges[K]=0;
            BubbleOrigins[K]=GetActorLocation()+GetActorForwardVector()*28+FVector(FMath::FRandRange(-16.f,16.f),FMath::FRandRange(-16.f,16.f),35+I*6);
            Bubbles[K]->SetWorldScale3D(FVector(FMath::FRandRange(0.035f,0.075f)));
            Bubbles[K]->SetVisibility(true);
        }
    }
    for (int32 I=0; I<Bubbles.Num(); ++I)
    {
        if (BubbleAges[I]<0) continue;
        BubbleAges[I]+=Dt;
        if (BubbleAges[I]>=2.8f) { BubbleAges[I]=-1; Bubbles[I]->SetVisibility(false); continue; }
        const float Age=BubbleAges[I];
        Bubbles[I]->SetWorldLocation(BubbleOrigins[I]+FVector(FMath::Sin(Age*3+I)*12,FMath::Cos(Age*2+I)*12,Age*85));
    }
}

void AAbyssDiver::Orbit(float DeltaX, float DeltaY)
{
    ViewYaw=FRotator::NormalizeAxis(ViewYaw+DeltaX*.65f);
    if (bAngledView) ViewPitch=FMath::Clamp(ViewPitch+DeltaY*.65f,-85.f,-10.f);
    CameraBoom->SetWorldRotation(FRotator(bAngledView ? ViewPitch : -90.f,ViewYaw,0));
}
void AAbyssDiver::ToggleView()
{
    bAngledView=!bAngledView;
    Orbit(0);
    CameraBoom->TargetArmLength=bAngledView ? 1800 : 2000;
}
void AAbyssDiver::Tick(float Dt)
{
    Super::Tick(Dt);
    AAbyssController* PC = Cast<AAbyssController>(GetController());
    AAbyssGameMode* G = Abyss::Game(this);
    if (!PC || !G) return;
    const bool bFocused=FApp::IsUnattended() || (FApp::HasFocus() && GEngine && GEngine->GameViewport &&
        GEngine->GameViewport->Viewport && GEngine->GameViewport->Viewport->HasFocus());
    const bool bPlaying = !G->bMenu && !G->bInventory && !G->bEnded && bFocused;
    if (bPlaying && bHadGameplayFocus && Dt<.1f && PC->PlayerInput)
    {
        const float MouseX = PC->PlayerInput->GetRawKeyValue(EKeys::MouseX);
        const float MouseY = PC->PlayerInput->GetRawKeyValue(EKeys::MouseY);
        if (FMath::Abs(MouseX)<300.f && FMath::Abs(MouseY)<300.f &&
            (!FMath::IsNearlyZero(MouseX) || !FMath::IsNearlyZero(MouseY)))
            Orbit(MouseX * .5f, MouseY * .5f);
    }
    bHadGameplayFocus=bPlaying;
    UpdateBubbles(Dt,!G->bEnded && G->Oxygen>0);
    if (!bPlaying) { bSprinting=false; return; }

    auto Held = [PC](FKey Primary, FKey Alternate)
    {
        return PC->IsInputKeyDown(Primary) || PC->IsInputKeyDown(Alternate) ? 1.f : 0.f;
    };
    FVector Move(Held(EKeys::W, EKeys::Up) - Held(EKeys::S, EKeys::Down),
                 Held(EKeys::D, EKeys::Right) - Held(EKeys::A, EKeys::Left), 0);
    Move = FRotator(0, ViewYaw, 0).RotateVector(Move);
    Move *= G->Conditions->InputDirection();
    if (!Move.IsNearlyZero()) SetActorRotation(Move.Rotation());
    bSprinting = PC->IsInputKeyDown(EKeys::LeftShift) && !Move.IsNearlyZero();
    UAnimSequence* Clip=bSprinting ? SprintClip.Get() : Move.IsNearlyZero() ? IdleClip.Get() : SwimClip.Get();
    UAnimSingleNodeInstance* Instance = DiverMesh->GetSingleNodeInstance();
    if (Clip && (Clip != CurrentClip || !Instance || Instance->GetCurrentAsset() != Clip || !DiverMesh->IsPlaying()))
    {
        CurrentClip=Clip;
        DiverMesh->PlayAnimation(Clip,true);
    }
    HandLamp->SetWorldRotation(GetActorRotation()+FRotator(-9,0,0));
    if (!Move.IsNearlyZero())
    {
        G->bHidden = false;
        MoveDiver(Move.GetSafeNormal() * G->SwimSpeed(bSprinting) * Dt);
    }
    Flashlight->SetVisibility(G->bLight && !G->bHidden);
    // Light the camera-facing side of the suit against the dark harbor hull.
    DiveGlow->SetWorldLocation(GetActorLocation()-CameraBoom->GetForwardVector()*115.f+FVector(0,0,35));
    DiveGlow->SetIntensity(G->bLight && !G->bHidden ? 180.f : 12.f);
}

void AAbyssDiver::MoveDiver(const FVector& Delta)
{
    AAbyssGameMode* G=Abyss::Game(this);
    if (!G || Delta.IsNearlyZero() || Delta.ContainsNaN()) return;
    FCollisionQueryParams Query;
    Query.AddIgnoredActor(this);
    const FQuat Rotation=GetActorQuat();
    FHitResult Hit;
    Movement->SafeMoveUpdatedComponent(Delta,Rotation,true,Hit);
    if (Hit.IsValidBlockingHit())
    {
        const FVector Contact=GetActorLocation();
        bool Stepped=false;
        if (G->bImportedCity && FMath::Abs(Hit.Normal.Z)<.65f)
        {
            FHitResult Up,Forward,Floor,Down;
            Movement->SafeMoveUpdatedComponent(FVector(0,0,45),Rotation,true,Up);
            if (!Up.bBlockingHit)
            {
                Movement->SafeMoveUpdatedComponent(Delta*(1-Hit.Time),Rotation,true,Forward);
                const FVector Raised=GetActorLocation();
                if (!Forward.bBlockingHit && GetWorld()->LineTraceSingleByChannel(Floor,Raised,Raised-FVector(0,0,120),ECC_WorldStatic,Query) &&
                    Floor.ImpactNormal.Z>.65f && Floor.ImpactPoint.Z+70>Contact.Z+.5f && Floor.ImpactPoint.Z+70<=Contact.Z+45.5f)
                {
                    Movement->SafeMoveUpdatedComponent(FVector(0,0,Floor.ImpactPoint.Z+70-Raised.Z),Rotation,true,Down);
                    Stepped=!Down.bStartPenetrating && FMath::Abs(GetActorLocation().Z-(Floor.ImpactPoint.Z+70))<1.f;
                }
            }
            // Roll back only to the known clear contact point when a step has no support.
            if (!Stepped) SetActorLocation(Contact);
        }
        if (!Stepped) Movement->SlideAlongSurface(Delta,1-Hit.Time,Hit.Normal,Hit,true);
    }
    FVector Position=GetActorLocation();
    FVector Target=Position;
    Target.X=FMath::Clamp(Target.X,-G->MapHalfExtent,G->MapHalfExtent);
    Target.Y=FMath::Clamp(Target.Y,-G->MapHalfExtent,G->MapHalfExtent);
    Movement->SafeMoveUpdatedComponent(Target-Position,Rotation,true,Hit);
    Position=GetActorLocation();
    FHitResult Floor;
    if (G->bImportedCity)
    {
        if (GetWorld()->LineTraceSingleByChannel(Floor,Position+FVector(0,0,20),Position-FVector(0,0,1000),ECC_WorldStatic,Query) && Floor.ImpactNormal.Z>.65f)
            Movement->SafeMoveUpdatedComponent(FVector(0,0,Floor.ImpactPoint.Z+70-Position.Z),Rotation,true,Hit);
    }
    else Movement->SafeMoveUpdatedComponent(FVector(0,0,70-Position.Z),Rotation,true,Hit);
}

