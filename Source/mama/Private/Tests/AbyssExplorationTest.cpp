#include "Game/AbyssGameMode.h"
#include "Characters/AbyssDiver.h"
#include "Status/AbyssConditionsComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Save/AbyssSaveService.h"
#include "Inventory/AbyssInventoryComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void AAbyssGameMode::RunExplorationCheck()
{
    auto* Player=Cast<AAbyssDiver>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!Player) return;
    auto Check=[this](bool OK, const TCHAR* Message)
    {
        UE_LOG(LogTemp,Display,TEXT("EXPLORATION_TEST %s: %s"),OK ? TEXT("PASS") : TEXT("FAIL"),Message);
        if (!OK) { bSmokeTest=false; FPlatformMisc::RequestExitWithStatus(false,1); }
        return OK;
    };
    if (Elapsed>65.f) { Check(false,TEXT("Exploration test timeout")); return; }
    if (SmokeStage==-3 && Elapsed>1.f)
    {
        SmokeStart=Player->GetActorLocation();
        const FAbyssSite* Guide=FirstDiveTarget();
        if (!Check(Guide && Guide->Kind==FAbyssSite::Fins,TEXT("First-dive guide begins with dock fins"))) return;
        Sites[66].bUsed=true;
        Guide=FirstDiveTarget();
        if (!Check(Guide && Guide->Kind==FAbyssSite::Welder,TEXT("First-dive guide advances to the dock welder"))) return;
        bHasWelder=true;
        Guide=FirstDiveTarget();
        if (!Check(Guide && Guide->Kind==FAbyssSite::LootCache,TEXT("Equipped diver is guided to the nearest supply cache"))) return;
        bHasWelder=false;
        Sites[66].bUsed=false;
        if (!Check(Sites.Num()==71 && Sites[66].Kind==FAbyssSite::Fins && Sites[67].Kind==FAbyssSite::Welder,
            TEXT("Old site and equipment save indices preserved; three caches appended"))) return;
        int32 NewCaches=0, Planned=0, CaveSite=INDEX_NONE, Crawlers=0, Swimmers=0;
        for (int32 I=0; I<Sites.Num(); ++I)
        {
            const auto& Site=Sites[I];
            if (Site.bAmbushPlanned) ++Planned;
            if (Site.Visual && Site.Visual->ActorHasTag(TEXT("AbyssExplorationCache")))
            {
                ++NewCaches;
                if (!Check(Site.Kind==FAbyssSite::LootCache && Site.OpenedVisual,
                    TEXT("New treasure has open model and legacy-compatible cache kind"))) return;
                if (Site.Visual->ActorHasTag(TEXT("AbyssInterior_SeaCave"))) CaveSite=I;
                bool Reachable=false;
                FCollisionQueryParams Query;
                Query.AddIgnoredActor(Player);
                Query.AddIgnoredActor(Site.Visual);
                Query.AddIgnoredActor(Site.OpenedVisual);
                for (int32 Direction=0; Direction<8; ++Direction)
                {
                    const FVector Probe=Site.CacheAnchor+FVector(240,0,0).RotateAngleAxis(Direction*45.f,FVector::UpVector);
                    FHitResult Floor;
                    if (!GetWorld()->LineTraceSingleByChannel(Floor,Probe+FVector(0,0,40),Probe-FVector(0,0,250),ECC_WorldStatic,Query) || Floor.ImpactNormal.Z<.65f) continue;
                    const FVector From=Floor.ImpactPoint+FVector(0,0,70);
                    const FVector To=Site.CacheAnchor+FVector(0,0,70);
                    Reachable |= !GetWorld()->SweepTestByChannel(From,To,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(36),Query);
                }
                if (!Check(Reachable,TEXT("New cache has a floor-supported approach with diver clearance"))) return;
            }
        }
        for (const auto& Entry : ExplorationHomes)
        {
            const AActor* Actor=Entry.Key;
            const auto* Mesh=Actor->FindComponentByClass<USkeletalMeshComponent>();
            const UAnimationAsset* Clip=Mesh ? Mesh->AnimationData.AnimToPlay.Get() : nullptr;
            if (Actor->ActorHasTag(TEXT("AbyssCaveResident")))
            {
                ++Crawlers;
                if (!Check(Actor->ActorHasTag(TEXT("AbyssBenthicHunter")) && Clip && Clip->GetName().EndsWith(TEXT("_Walk")),
                    TEXT("Cave resident is grounded and plays crawling walk"))) return;
            }
            if (Actor->ActorHasTag(TEXT("AbyssWreckResident"))) ++Swimmers;
        }
        if (!Check(NewCaches==3 && Crawlers==2 && Swimmers==1 && CaveSite!=INDEX_NONE && Planned<=3,
            TEXT("Habitat population and ambush cap are bounded"))) return;
        if (!Check(Player->CameraBoom->TargetArmLength>=1800 && Player->Camera->FieldOfView>=72,
            TEXT("Gameplay camera shows a wider building view"))) return;
        auto& Site=Sites[CaveSite];
        const bool PlannedBefore=Site.bAmbushPlanned;
        int32 AmbushSeed=0;
        for (; AmbushSeed<1000; ++AmbushSeed)
        {
            SetCacheLayout(AmbushSeed);
            if (Site.bAmbushPlanned) break;
        }
        if (!Check(AmbushSeed<1000,TEXT("A saved layout seed can reproduce the cave ambush"))) return;
        Player->SetActorLocation(Site.CacheAnchor+FVector(100,0,70));
        UpdateBuilding();
        const auto* CaveMesh=Site.InteriorRoof ? Site.InteriorRoof->FindComponentByClass<UStaticMeshComponent>() : nullptr;
        if (!Check(CaveMesh && !Site.InteriorRoof->IsHidden() && CaveMesh->GetMaterial(0)==CityWallGlass &&
            CaveMesh->GetCollisionEnabled()!=ECollisionEnabled::NoCollision,TEXT("Cave becomes translucent while retaining its silhouette and collision"))) return;
        int32 RewardBefore=Inventory[Site.CacheReward];
        SalvageSite=CaveSite;
        FinishSalvage(true);
        if (!Check(CacheMonsters.Num()==1 && Site.bUsed && Inventory[Site.CacheReward]==RewardBefore+1,
            TEXT("Successful opening awards loot and spawns one monster"))) return;
        SalvageSite=CaveSite;
        FinishSalvage(true);
        RestoreCacheAmbushes();
        if (!Check(CacheMonsters.Num()==1 && Inventory[Site.CacheReward]==RewardBefore+1,
            TEXT("Repeated opening and restoring cannot duplicate monster or reward"))) return;
        Site.bUsed=false;
        RestoreCacheAmbushes();
        if (!Check(CacheMonsters.IsEmpty(),TEXT("Restoring an unopened cache removes its stale ambush"))) return;
        SalvageSite=CaveSite;
        FinishSalvage(false);
        if (!Check(CacheMonsters.IsEmpty() && !Site.bUsed,TEXT("Failed opening does not create a monster"))) return;
        {
            const FString Path=FAbyssSaveService::SavePath(this);
            const bool HadSave=IFileManager::Get().FileExists(*Path);
            TArray<uint8> Original;
            if (!Check(!HadSave || FFileHelper::LoadFileToArray(Original,*Path),TEXT("Existing save preserved before roundtrip"))) return;
            ON_SCOPE_EXIT
            {
                if (HadSave) FFileHelper::SaveArrayToFile(Original,*Path);
                else IFileManager::Get().Delete(*Path);
                bSmokeTest=true;
            };
            Inventory[Site.CacheReward]=RewardBefore;
            SalvageSite=CaveSite;
            FinishSalvage(true);
            Cargo->Sync(Inventory);
            bSmokeTest=false;
            const bool Saved=FAbyssSaveService::SaveRun(this);
            bSmokeTest=true;
            if (!Check(Saved,TEXT("Opened cache state saved through the real save service"))) return;
            Site.bUsed=false;
            RestoreCacheAmbushes();
            Inventory[Site.CacheReward]=0;
            if (!Check(FAbyssSaveService::LoadRun(this) && Site.bUsed && CacheMonsters.Num()==1 &&
                Inventory[Site.CacheReward]==RewardBefore+1,TEXT("Save load restores reward, opened state and exactly one ambush"))) return;
            if (!Check(FAbyssSaveService::LoadRun(this) && CacheMonsters.Num()==1,
                TEXT("Repeated save load cannot duplicate an ambush"))) return;
            FString Json;
            TSharedPtr<FJsonObject> Document;
            if (!Check(FFileHelper::LoadFileToString(Json,*Path) &&
                FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Document) && Document.IsValid(),
                TEXT("Saved cache document can be decoded"))) return;
            const auto State=Document->GetObjectField(TEXT("state"));
            auto Used=State->GetArrayField(TEXT("used"));
            Used.SetNum(68);
            State->SetArrayField(TEXT("used"),Used);
            Json.Reset();
            FJsonSerializer::Serialize(Document.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
            if (!Check(FFileHelper::SaveStringToFile(Json,*Path) && FAbyssSaveService::LoadRun(this) &&
                !Sites[68].bUsed && !Sites[69].bUsed && !Sites[70].bUsed && CacheMonsters.IsEmpty(),
                TEXT("Legacy save keeps old indices and initializes appended caches unopened"))) return;
        }
        Inventory[Site.CacheReward]=RewardBefore;
        Site.bAmbushPlanned=PlannedBefore;
        Site.Visual->SetActorHiddenInGame(false);
        Site.OpenedVisual->SetActorHiddenInGame(true);
        DamageCooldown=100.f;
        for (FAbyssHunter& Enemy:ExtraHunters) Enemy.Memory=0;
        if (FParse::Param(FCommandLine::Get(),TEXT("nullrhi")))
        {
            bSmokeTest=false;
            FPlatformMisc::RequestExitWithStatus(false,0);
            return;
        }
        Player->SetActorLocation(SmokeStart);
        UpdateBuilding();
        if (!Check(CaveMesh->GetMaterial(0)!=CityWallGlass,TEXT("Leaving cave restores its original rock material"))) return;
        Player->bAngledView=true;
        Player->ViewPitch=-65.f;
        Player->ViewYaw=0;
        Player->Orbit(0);
        if (APlayerController* PC=UGameplayStatics::GetPlayerController(this,0)) PC->ConsoleCommand(TEXT("DisableAllScreenMessages"));
        SmokeStage=0;
        SmokeDeadline=Elapsed+3.f;
    }
    else if (SmokeStage>=0 && SmokeStage<22 && Elapsed>SmokeDeadline)
    {
        if (SmokeStage%2==0)
        {
            static const TCHAR* Names[]={TEXT("Intro"),TEXT("Bank_Exterior"),TEXT("Bank_Interior"),TEXT("Cave"),TEXT("Shipwreck"),TEXT("Seabed_Edge"),TEXT("Darkness"),TEXT("Disorientation"),TEXT("Hyperventilation"),TEXT("Cold"),TEXT("Recovery")};
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/Exploration_")+Names[SmokeStage/2]+TEXT(".png"),true,false);
            SmokeDeadline=Elapsed+.5f;
        }
        else
        {
            if (SmokeStage==1) Player->SetActorLocation(FVector(3400,2900,74));
            if (SmokeStage==3) Player->SetActorLocation(FVector(4300,2900,74));
            if (SmokeStage==5) Player->SetActorLocation(FVector(-18050,-15650,267));
            if (SmokeStage==7) Player->SetActorLocation(FVector(-16800,17680,320));
            if (SmokeStage==9)
            {
                Player->SetActorLocation(FVector(24700,0,74));
                Player->ViewPitch=-90;Player->Orbit(0);
                // Isolate the material seam outside the playable border from its perimeter shadows.
                if (auto* PC=UGameplayStatics::GetPlayerController(this,0)) PC->ConsoleCommand(TEXT("viewmode unlit"));
            }
            if (SmokeStage>=11)
            {
                Player->SetActorLocation(FVector(3400,2900,74));
                Player->ViewPitch=-65;Player->Orbit(0);
                if (auto* PC=UGameplayStatics::GetPlayerController(this,0)) PC->ConsoleCommand(TEXT("viewmode lit"));
                FAbyssConditionState Effect;
                Effect.Active=SmokeStage<19 ? static_cast<EAbyssCondition>((SmokeStage-11)/2+1) : EAbyssCondition::None;
                Effect.Remaining=Effect.Active==EAbyssCondition::None ? 0 : 15;
                Conditions->Restore(Effect,9000.f);
            }
            SmokeDeadline=Elapsed+3.5f;
        }
        ++SmokeStage;
    }
    else if (SmokeStage==22 && Elapsed>SmokeDeadline)
    {
        bSmokeTest=false;
        FPlatformMisc::RequestExitWithStatus(false,0);
    }
}
