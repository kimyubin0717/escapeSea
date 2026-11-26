#include "Game/AbyssGameMode.h"
#include "Characters/AbyssDiver.h"
#include "Player/AbyssController.h"
#include "UI/AbyssHUD.h"
#include "Inventory/AbyssInventoryComponent.h"
#include "Status/AbyssConditionsComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
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
AAbyssGameMode::AAbyssGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = AAbyssDiver::StaticClass();
    PlayerControllerClass = AAbyssController::StaticClass();
    HUDClass = AAbyssHUD::StaticClass();
    Cargo = CreateDefaultSubobject<UAbyssInventoryComponent>(TEXT("Cargo"));
    Conditions = CreateDefaultSubobject<UAbyssConditionsComponent>(TEXT("Conditions"));
}

void AAbyssGameMode::BeginPlay()
{
    Super::BeginPlay();
    BuildSeafloor();
    HarborHatchIndex=Sites.IndexOfByPredicate([](const FAbyssSite& Site){return Site.Kind==8;});
    BatteryRemaining=FlashlightDuration;
    bSmokeTest = FParse::Param(FCommandLine::Get(), TEXT("AbyssSmokeTest"));
    bMenu = !bSmokeTest && !UGameplayStatics::HasOption(OptionsString, TEXT("AbyssNew"));
    Cargo->Sync(Inventory);
    Conditions->Reset();
    if (bSmokeTest && bImportedCity) SmokeStage=-3;
    if (APawn* P = UGameplayStatics::GetPlayerPawn(this,0))
    {
        FVector Start=bImportedCity ? FVector(6000*CityScale,3500*CityScale,74) : Abyss::Map(FVector(430,0,70));
        TArray<AActor*> Starts;
        UGameplayStatics::GetAllActorsWithTag(this,TEXT("AbyssPlayerSpawn"),Starts);
        if (!Starts.IsEmpty()) Start=Starts[0]->GetActorLocation();
        P->SetActorLocation(Start);
    }
    Notify(TEXT("Search buildings for locked caches. Repair the white Nautilus Mk2 and escape."));
    UE_LOG(LogTemp, Display, TEXT("ABYSS_READY: %d interaction sites. Diver, hunter and seafloor initialized."), Sites.Num());
    if (FParse::Param(FCommandLine::Get(), TEXT("AbyssCapture")))
    {
        bMenu=false;
        if (APlayerController* PC=UGameplayStatics::GetPlayerController(this,0)) PC->ConsoleCommand(TEXT("DisableAllScreenMessages"));
        FTimerHandle CaptureTimer;
        GetWorldTimerManager().SetTimer(CaptureTimer, [this]()
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/AbyssPreview.png"),true,false);
        }, 12.f, false);
        FTimerHandle InventoryTimer;
        GetWorldTimerManager().SetTimer(InventoryTimer,[this]() { bInventory=true; },16.f,false);
        FTimerHandle InventoryCapture;
        GetWorldTimerManager().SetTimer(InventoryCapture,[]()
        { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/AbyssInventory.png"),true,false); },18.f,false);
        if (FParse::Param(FCommandLine::Get(),TEXT("AbyssCaptureExit")))
        {
            FTimerHandle ExitCapture;
            GetWorldTimerManager().SetTimer(ExitCapture,[](){ FPlatformMisc::RequestExit(false); },22.f,false);
        }
    }
}

void AAbyssGameMode::Notify(const FString& Message) { Notice = Message; NoticeTime = 6; }
const FAbyssSite* AAbyssGameMode::FirstDiveTarget() const
{
    const APawn* Player=UGameplayStatics::GetPlayerPawn(this,0);
    if (!bWelderRequired || !Player || NextRepairStage()!=0 ||
        Sites.ContainsByPredicate([](const FAbyssSite& Site){return Site.Kind==FAbyssSite::LootCache && Site.bUsed;})) return nullptr;
    const bool NeedsFins=Sites.ContainsByPredicate([](const FAbyssSite& Site){return Site.Kind==FAbyssSite::Fins && !Site.bUsed;});
    const int32 Goal=NeedsFins ? FAbyssSite::Fins : bHasWelder ? FAbyssSite::LootCache : FAbyssSite::Welder;
    const FAbyssSite* Target=nullptr;
    double Best=TNumericLimits<double>::Max();
    for (const FAbyssSite& Site:Sites)
    {
        if (Site.Kind!=Goal || Site.bUsed) continue;
        const double Distance=FVector::DistSquared2D(Player->GetActorLocation(),Site.Position);
        if (Distance<Best) { Best=Distance; Target=&Site; }
    }
    return Target;
}
float AAbyssGameMode::SwimSpeed(bool Sprint) const
{
    return (Sprint ? 612.f : 378.f)*(FinsRemaining>0 ? 1.5f : 1.f)*Conditions->MovementMultiplier();
}
float AAbyssGameMode::OxygenRate(bool bSprint) const
{
    return (bSprint ? 1.95f : 0.65f)*Conditions->OxygenMultiplier();
}
void AAbyssGameMode::UpdateSubmarineModel()
{
    const bool bReady=NextRepairStage()==3;
    for (const FName Tag : {FName(TEXT("AbyssMk2Hull")),FName(TEXT("AbyssMk2Cabin"))})
    {
        TArray<AActor*> Vessels;
        UGameplayStatics::GetAllActorsWithTag(this,Tag,Vessels);
        const FString Base=Tag==TEXT("AbyssMk2Hull") ? TEXT("SM_Sub2") : TEXT("SM_Sub2_Interior");
        const FString Path=TEXT("/Game/IntegratedCity/NautilusMk2/Meshes/")+Base+(bReady ? TEXT("_Repaired") : TEXT(""));
        for (AActor* Vessel : Vessels)
            if (auto* Part=Vessel->FindComponentByClass<UStaticMeshComponent>())
                if (auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path)) Part->SetStaticMesh(Mesh);
    }
}
int32 AAbyssGameMode::NearestSite() const
{
    APawn* P = UGameplayStatics::GetPlayerPawn(this,0);
    if (!P) return INDEX_NONE;
    float Best = 125.f*125.f;
    int32 Result = INDEX_NONE;
    for (int32 I=0; I<Sites.Num(); ++I)
    {
        const FAbyssSite& Site = Sites[I];
        const bool bCache = Site.Kind==FAbyssSite::LootCache;
        if (Site.bUsed && (Site.Kind<5 || Site.Kind>=FAbyssSite::LootCache)) continue;
        const FVector Pos = P->GetActorLocation();
        const float D = bCache ? FVector::DistSquared(Pos,Site.Position) : FVector::DistSquared2D(Pos,Site.Position);
        if (D>=Best) continue;
        if (bCache)
        {
            FCollisionQueryParams Query;
            Query.AddIgnoredActor(P);
            if (Site.Visual) Query.AddIgnoredActor(Site.Visual);
            if (GetWorld()->LineTraceTestByChannel(Pos,Site.Position,ECC_Visibility,Query)) continue;
        }
        Best=D; Result=I;
    }
    return Result;
}
FString AAbyssGameMode::SitePrompt(int32 I) const
{
    if (!Sites.IsValidIndex(I)) return TEXT("");
    const FAbyssSite& S = Sites[I];
    if (S.Kind==FAbyssSite::Fins) return TEXT("[E] Equip fins / +50% swim speed for 60s");
    if (S.Kind==FAbyssSite::Welder) return TEXT("[E] Take underwater welder / repair tool");
    if (S.Kind<3) return FString::Printf(TEXT("[E] SALVAGE %s / timing check"), *S.Name);
    if (S.Kind==3)
    {
        if (S.ResourceDrop == EAbyssResourceDrop::Battery) return TEXT("[E] RECHARGE FLASHLIGHT");
        if (S.ResourceDrop == EAbyssResourceDrop::Oxygen) return TEXT("[E] REFILL OXYGEN");
        return TEXT("[E] COLLECT RESERVE TANK");
    }
    if (S.Kind==4) return bHidden ? TEXT("[E] LEAVE KELP") : TEXT("[E] HIDE IN KELP / light off");
    if (S.Kind==FAbyssSite::LootCache) return FString::Printf(TEXT("[E] UNLOCK %s / timing check"),*S.Name);
    if (S.Kind==8) return Repaired[0]&&Repaired[1]&&Repaired[2] ? TEXT("[E] BOARD SUBMARINE / ESCAPE") : TEXT("HATCH LOCKED / repair all 3 systems");
    int32 K = NextRepairStage();
    if (K==3) return TEXT("ALL STAGES UNLOCKED / ENTER HATCH");
    if (bWelderRequired && !bHasWelder) return TEXT("Welder needed / collect it on the harbor deck");
    if (S.Kind!=5+K) return FString::Printf(TEXT("Use service terminal %02d / %s"),K+1,Abyss::Items[K]);
    if (RepairSite!=INDEX_NONE) return TEXT("REPAIRING / stay near the terminal");
    return FString::Printf(TEXT("[E] UNLOCK REPAIR STAGE %d / %s (%d/2)"), K+1, Abyss::Items[K],Inventory[K]);
}
int32 AAbyssGameMode::NextRepairStage() const
{
    for (int32 K=0;K<3;++K) if (!Repaired[K]) return K;
    return 3;
}
void AAbyssGameMode::DamageDiver(float Amount)
{
    if (bEnded || DamageCooldown>0) return;
    Health=FMath::Clamp(Health-Amount,0.f,100.f); DamageCooldown=1.5f;
    if (Health<=0) EndRun(false,TEXT("DIVER LOST / health depleted."));
    else Notify(TEXT("STALKER HIT / swim away!"));
}
void AAbyssGameMode::Interact()
{
    if (bEnded || SalvageSite!=INDEX_NONE) return;
    int32 I = NearestSite();
    if (I==INDEX_NONE) return;
    FAbyssSite& S = Sites[I];
    if (S.Kind<3 || S.Kind==FAbyssSite::LootCache)
    {
        SalvageSite=I; SalvageTime=0;
        bInventory=false;
        SalvageTarget=FMath::FRandRange(0.35f,0.65f);
        bHidden=false;
        Notify(TEXT("SALVAGE CHECK / Press SPACE inside the green zone. Failure attracts the stalker."));
    }
    else if (S.Kind==FAbyssSite::Fins || S.Kind==FAbyssSite::Welder)
    {
        S.bUsed=true;
        if (S.Visual) S.Visual->SetActorHiddenInGame(true);
        if (S.LabelVisual) S.LabelVisual->SetActorHiddenInGame(true);
        if (S.Kind==FAbyssSite::Fins) { FinsRemaining=60; Notify(TEXT("Fins equipped / swim speed +50% for 60 seconds")); }
        else { bHasWelder=true; Notify(TEXT("Welder secured / ready for submarine repairs")); }
    }
    else if (S.Kind==3)
    {
        if (S.ResourceDrop == EAbyssResourceDrop::Battery)
        {
            if (BatteryRemaining >= FlashlightDuration) { Notify(TEXT("Flashlight battery is full.")); return; }
            const float Restored = FMath::Min(BatteryRefill, FlashlightDuration-BatteryRemaining);
            BatteryRemaining += Restored;
            Notify(FString::Printf(TEXT("Flashlight charged. +%d seconds. Press F to toggle."), FMath::CeilToInt(Restored)));
        }
        else if (S.ResourceDrop == EAbyssResourceDrop::Oxygen)
        {
            if (Oxygen >= OxygenCapacity) { Notify(TEXT("Oxygen is full.")); return; }
            const float Restored = FMath::Min(OxygenRefill, OxygenCapacity-Oxygen);
            Oxygen += Restored;
            Notify(FString::Printf(TEXT("Oxygen refilled. +%d oxygen."), FMath::CeilToInt(Restored)));
        }
        else
        {
            ++Inventory[3];
            Notify(TEXT("Reserve tank collected. Press Q to use it."));
        }
        S.bUsed = true;
        if (S.Visual) S.Visual->SetActorHiddenInGame(true);
        if (S.LabelVisual) S.LabelVisual->SetActorHiddenInGame(true);
    }
    else if (S.Kind==4) { bHidden = !bHidden; Notify(bHidden ? TEXT("Hidden. Stay still. A hunter that already saw you may still find you.") : TEXT("Left cover.")); }
    else if (S.Kind==8)
    {
        if (Repaired[0]&&Repaired[1]&&Repaired[2]) EndRun(true,TEXT("NAUTILUS RESTORED. You escaped the drowned city."));
        else Notify(TEXT("Repair POWER, HULL and DRIVE at the three blue terminals first."));
    }
    else
    {
        int32 K=NextRepairStage();
        if (K==3 || RepairSite!=INDEX_NONE) return;
        if (bWelderRequired && !bHasWelder) { Notify(TEXT("Find the underwater welder on the harbor deck.")); return; }
        if (S.Kind!=5+K) { Notify(FString::Printf(TEXT("Next repair: terminal %02d / %s"),K+1,Abyss::Items[K])); return; }
        if (Inventory[K]<2)
        {
            MissingPart=K; MissingPartsUntil=Elapsed+5;
            bInventory=true;
            Notify(FString::Printf(TEXT("STAGE %d LOCKED / Need 2 %s. Missing part highlighted in inventory."),K+1,Abyss::Items[K]));
        }
        else { RepairSite=I; RepairProgress=0; bHidden=false; Notify(TEXT("REPAIR STARTED / stay here for 4 seconds. Next stage unlocks automatically.")); }
    }
}
void AAbyssGameMode::AdvanceSalvage(float Dt)
{
    if (SalvageSite==INDEX_NONE) return;
    if (bEnded) { SalvageSite=INDEX_NONE; return; }
    APawn* P=UGameplayStatics::GetPlayerPawn(this,0);
    SalvageTime+=Dt;
    if (!P || !Sites.IsValidIndex(SalvageSite) ||
        FVector::Dist2D(P->GetActorLocation(),Sites[SalvageSite].Position)>150 || SalvageTime>=SalvageDuration)
        FinishSalvage(false);
}
void AAbyssGameMode::SubmitSalvage()
{
    if (SalvageSite==INDEX_NONE || bEnded) return;
    APawn* P=UGameplayStatics::GetPlayerPawn(this,0);
    float Phase=SalvageTime/SalvageDuration;
    bool bInRange=P && Sites.IsValidIndex(SalvageSite) && FVector::Dist2D(P->GetActorLocation(),Sites[SalvageSite].Position)<=150;
    FinishSalvage(bInRange && Phase>=SalvageTarget && Phase<=SalvageTarget+SalvageWindow && Phase<1);
}
void AAbyssGameMode::FinishSalvage(bool bSuccess)
{
    if (!Sites.IsValidIndex(SalvageSite)) { SalvageSite=INDEX_NONE; return; }
    const int32 SiteIndex = SalvageSite;
    FAbyssSite& S=Sites[SiteIndex];
    SalvageSite=INDEX_NONE;
    if (bSuccess && !S.bUsed && S.Kind==FAbyssSite::LootCache)
    {
        const int32 Reward = S.CacheReward;
        ++Inventory[Reward];
        S.bUsed=true;
        if (S.OpenedVisual)
        {
            if (S.Visual) S.Visual->SetActorHiddenInGame(true);
            S.OpenedVisual->SetActorHiddenInGame(false);
        }
        Notify(FString::Printf(TEXT("%s opened / %s added to equipment."),*S.Name,Abyss::Items[Reward]));
        if (SpawnCacheAmbush(SiteIndex))
        {
            Notify(TEXT("상자에서 괴물이 기어나왔습니다! 거리를 벌리세요."));
            NoticeKind=TEXT("warning");
            ++NoticeSerial;
        }
    }
    else if (bSuccess && !S.bUsed && S.Kind<3)
    {
        ++Inventory[S.Kind]; S.bUsed=true;
        if (S.Visual) S.Visual->SetActorHiddenInGame(true);
        if (S.LabelVisual) S.LabelVisual->SetActorHiddenInGame(true);
        Notify(FString::Printf(TEXT("CHECK SUCCESS / %s stored in inventory."),*S.Name));
    }
    else if (!bSuccess)
    {
        const FVector Noise=S.Position;
        auto Hear=[&](AActor* Actor, float& Recall, FVector& Target)
        {
            if (Actor && FVector::DistSquared2D(Actor->GetActorLocation(),Noise)<=FMath::Square(2800.f))
            {
                Recall=18;
                Target=Noise;
                bChasing=true;
                Alert=1;
            }
        };
        Hear(Hunter,Memory,LastKnown);
        for (FAbyssHunter& Enemy:ExtraHunters) Hear(Enemy.Actor,Enemy.Memory,Enemy.LastKnown);
        Notify(TEXT("CHECK FAILED / lock noise carries 28 m. Move away!"));
    }
}
void AAbyssGameMode::UseOxygen()
{
    if (Inventory[3]<=0) { Notify(TEXT("No oxygen tanks. Find green canisters in the city.")); return; }
    if (Oxygen>=OxygenCapacity) { Notify(TEXT("Oxygen is full. Save your tank.")); return; }
    --Inventory[3]; Oxygen=FMath::Min(OxygenCapacity,Oxygen+OxygenRefill);
    Notify(TEXT("Reserve oxygen tank used."));
}
void AAbyssGameMode::ToggleFlashlight()
{
    if (BatteryRemaining<=0) { bLight=false; Notify(TEXT("FLASHLIGHT BATTERY EMPTY. Find a spare battery.")); return; }
    bLight=!bLight;
}
void AAbyssGameMode::AdvanceBattery(float Dt)
{
    if (bLight && !bHidden && !bEnded)
    {
        float Before=BatteryRemaining;
        BatteryRemaining=FMath::Max(0.f,BatteryRemaining-Dt);
        if (Before>20 && BatteryRemaining<=20) Notify(TEXT("FLASHLIGHT LOW / less than 20 seconds. Press F to conserve power."));
        if (BatteryRemaining<=0)
        {
            bLight=false;
            if (AAbyssDiver* P=Cast<AAbyssDiver>(UGameplayStatics::GetPlayerPawn(this,0)))
            { P->Flashlight->SetVisibility(false); P->DiveGlow->SetIntensity(12); }
            Notify(TEXT("FLASHLIGHT BATTERY DEPLETED. Find a spare battery."));
        }
    }
}
void AAbyssGameMode::EndRun(bool bEscape, const FString& Reason)
{
    Conditions->Reset();
    bEnded=true; bWon=bEscape; Ending=Reason; SalvageSite=INDEX_NONE;
    UE_LOG(LogTemp,Display,TEXT("ABYSS_END: %s"),*Reason);
}

void AAbyssGameMode::Tick(float Dt)
{
    Super::Tick(Dt);
    if (bMenu) return;
    if (bSmokeTest) RunSmokeTest();
    AAbyssDiver* P = Cast<AAbyssDiver>(UGameplayStatics::GetPlayerPawn(this,0));
    APlayerController* PC = UGameplayStatics::GetPlayerController(this,0);
    if (!P || !PC || bEnded) return;
    Elapsed+=Dt; NoticeTime=FMath::Max(0.f,NoticeTime-Dt);
    if (!bSmokeTest) Conditions->Advance(Dt, Elapsed);
    FinsRemaining=FMath::Max(0.f,FinsRemaining-Dt);
    AdvanceBattery(Dt);
    AdvanceSalvage(Dt);
    UpdateBuilding();
    const float OxygenUseRate=OxygenRate(P->bSprinting);
    const float SuffocationTime=FMath::Clamp(Dt-Oxygen/OxygenUseRate,0.f,Dt);
    Oxygen=FMath::Max(0.f,Oxygen-Dt*OxygenUseRate);
    if (Oxygen<=0)
    {
        Health=FMath::Max(0.f,Health-SuffocationTime*5.f);
        if (Health<=0) { EndRun(false,TEXT("OXYGEN DEPLETED. The deep takes another diver.")); return; }
    }
    DamageCooldown=FMath::Max(0.f,DamageCooldown-Dt);
    float Darkness = FMath::Clamp(Elapsed/180.f,0.f,1.f);
    if (OceanLight) OceanLight->GetLightComponent()->SetIntensity((bImportedCity ? FMath::Lerp(1.4f,0.8f,Darkness) : FMath::Lerp(1.6f,0.12f,Darkness))*Conditions->LightMultiplier());
    if (Fog) Fog->GetComponent()->SetFogDensity(bImportedCity ? FMath::Lerp(0.012f,0.022f,Darkness) : FMath::Lerp(0.032f,0.075f,Darkness));
    int32 I=RepairSite;
    bool bRepairing=false;
    if (Sites.IsValidIndex(I) && FVector::Dist2D(P->GetActorLocation(),Sites[I].Position)<=150)
    {
        int32 K=NextRepairStage();
        if (K<3 && Sites[I].Kind==5+K && (!bWelderRequired || bHasWelder) && Inventory[K]>=2)
        {
            if (RepairSite!=I) RepairProgress=0;
            RepairSite=I; RepairProgress+=Dt; bRepairing=true; bHidden=false;
            if (RepairProgress>=4)
            {
                Inventory[K]-=2; Repaired[K]=true; RepairProgress=0; RepairSite=INDEX_NONE;
                if (NextRepairStage()==3) UpdateSubmarineModel();
                Notify(FString::Printf(TEXT("%s installed. %s"), Abyss::Items[K], Repaired[0]&&Repaired[1]&&Repaired[2] ? TEXT("All systems ONLINE. Board the escape hatch!") : TEXT("Subsystem restored.")));
            }
        }
    }
    if (!bRepairing) { RepairProgress=0; RepairSite=INDEX_NONE; }
    bChasing=false; Alert=FMath::FInterpTo(Alert,0.f,Dt,3);
    if (RoamingKraken)
    {
        const float Phase=Elapsed*0.032f;
        const FVector Next=AdvanceKraken(Dt);
        const FVector Heading(FMath::Cos(Phase)*18500.f,FMath::Cos(2*Phase)*33000.f,0);
        RoamingKraken->SetActorLocationAndRotation(Next,Heading.Rotation()+FRotator(0,90,0));
        const bool bSearching = !bInsideBuilding && !bHidden && FVector::DistSquared2D(Next, P->GetActorLocation()) < FMath::Square(1300.f);
        FCollisionQueryParams KrakenSight;
        KrakenSight.AddIgnoredActor(P);
        KrakenSight.AddIgnoredActor(RoamingKraken);
        const bool bAttack = DamageCooldown <= 0.f && bSearching &&
            FVector::Dist2D(Next, P->GetActorLocation()) < 650 && FMath::Abs(Next.Z-P->GetActorLocation().Z) < 900 &&
            !GetWorld()->LineTraceTestByChannel(Next,P->GetActorLocation(),ECC_Visibility,KrakenSight);
        UpdateCreatureAnimation(RoamingKraken, true, bSearching, bAttack);
        if (bAttack) DamageDiver(35);
    }
    TickHunter(Hunter,HunterLight,Memory,LastKnown,PatrolIndex,Dt,bRepairing);
    for (FAbyssHunter& Enemy:ExtraHunters)
        TickHunter(Enemy.Actor,Enemy.Light,Enemy.Memory,Enemy.LastKnown,Enemy.PatrolIndex,Dt,bRepairing);
}

float AAbyssGameMode::KrakenRadius() const
{
    const auto* Skin=RoamingKraken ? RoamingKraken->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
    if (!Skin || !Skin->GetSkeletalMeshAsset()) return 300.f;
    const FBoxSphereBounds Bounds=Skin->GetSkeletalMeshAsset()->GetBounds();
    return FMath::Clamp(FMath::Min(Bounds.BoxExtent.X,Bounds.BoxExtent.Y)*Skin->GetComponentScale().X*.6f,150.f,600.f);
}

FVector AAbyssGameMode::AdvanceKraken(float Dt)
{
    // Sweep both halves of the city and cross downtown on a figure-eight route.
    const float Phase=Elapsed*0.032f;
    FVector Next(18500.f*FMath::Sin(Phase),16500.f*FMath::Sin(2*Phase),550.f+60.f*FMath::Cos(Phase));
    // Rise over the tall buildings on the next few seconds of the route instead of swimming through them.
    FCollisionQueryParams Query;
    Query.AddIgnoredActor(RoamingKraken);
    if (APawn* P=UGameplayStatics::GetPlayerPawn(this,0)) Query.AddIgnoredActor(P);
    const float Radius=KrakenRadius();
    float Floor=Next.Z;
    for (float Ahead=-.025f;Ahead<=.15f;Ahead+=.025f)
    {
        const FVector At(18500.f*FMath::Sin(Phase+Ahead),16500.f*FMath::Sin(2*(Phase+Ahead)),0);
        FHitResult Roof;
        if (GetWorld()->SweepSingleByChannel(Roof,At+FVector(0,0,9000),At-FVector(0,0,200),FQuat::Identity,
            ECC_WorldStatic,FCollisionShape::MakeSphere(Radius),Query))
            Floor=FMath::Max(Floor,Roof.Location.Z+120.f);
    }
    KrakenHeight=KrakenHeight<0 ? Floor : FMath::FInterpConstantTo(KrakenHeight,Floor,Dt,Floor>KrakenHeight ? 1800.f : 500.f);
    Next.Z=KrakenHeight;
    return Next;
}

void AAbyssGameMode::UpdateBuilding()
{
    APawn* P=UGameplayStatics::GetPlayerPawn(this,0);
    if (!P) return;
    if (bImportedCity)
    {
        bInsideBuilding=false;
        const FVector Pos=P->GetActorLocation();
        for (FAbyssCityInterior& Interior : CityInteriors)
        {
            const FVector Offset=Pos-Interior.Center;
            const bool bInside=FMath::Abs(Offset.X)<Interior.Extent.X-20 &&
                FMath::Abs(Offset.Y)<Interior.Extent.Y-20 && Offset.Z<0 && Offset.Z>-1200;
            bInsideBuilding |= bInside;
            if (bInside==Interior.bInside) continue;
            Interior.bInside=bInside;
            Interior.Roof->SetActorHiddenInGame(bInside && !Interior.Roof->ActorHasTag(TEXT("AbyssInterior_SeaCave")));
            if (Interior.UpperStorey) Interior.UpperStorey->SetActorHiddenInGame(bInside);
            if (!CityWallGlass) continue;
            for (const FAbyssCityWall& Wall : Interior.Walls)
            {
                for (int32 I=0;I<Wall.OriginalMaterials.Num();++I)
                    if (Wall.OriginalMaterials[I]) Wall.Component->SetMaterial(I,bInside ? CityWallGlass.Get() : Wall.OriginalMaterials[I].Get());
                Wall.Component->SetCastShadow(!bInside && Wall.bCastShadow);
            }
        }
        return;
    }
    if (BuildingWalls.IsEmpty()) return;
    FVector D=P->GetActorLocation()-BuildingCenter;
    bool bInside=FMath::Abs(D.X)<BuildingExtent.X && FMath::Abs(D.Y)<BuildingExtent.Y;
    if (bInside==bInsideBuilding) return;
    bInsideBuilding=bInside;
    for (int32 I=0;I<BuildingWalls.Num();++I)
    {
        BuildingWalls[I]->SetMaterial(0,bInside && BuildingGlass ? BuildingGlass.Get() : BuildingMaterials[I].Get());
        BuildingWalls[I]->SetCastShadow(!bInside);
        // If the engine material is unavailable, use a cutaway without changing collision.
        BuildingWalls[I]->SetVisibility(!bInside || BuildingGlass!=nullptr);
    }
}

