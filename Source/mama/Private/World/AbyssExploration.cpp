#include "Game/AbyssGameMode.h"
#include "GameFramework/Pawn.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Animation/AnimSequence.h"
#include "Kismet/GameplayStatics.h"

void AAbyssGameMode::AppendExplorationCaches()
{
    TArray<AActor*> Caches, Opened, Roofs;
    UGameplayStatics::GetAllActorsWithTag(this, TEXT("AbyssExplorationCache"), Caches);
    UGameplayStatics::GetAllActorsWithTag(this, TEXT("AbyssCacheOpened"), Opened);
    UGameplayStatics::GetAllActorsWithTag(this, TEXT("AbyssInteriorRoof"), Roofs);
    Caches.Sort([](const AActor& A, const AActor& B){ return A.GetName() < B.GetName(); });
    // These sites follow all existing caches and equipment so old save indices stay valid.
    for (AActor* Cache : Caches)
    {
        FAbyssSite& Site = Sites.AddDefaulted_GetRef();
        Site.Kind = FAbyssSite::LootCache;
        Site.CacheAnchor = Cache->GetActorLocation();
        Site.Position = Site.CacheAnchor + FVector(0,0,60);
        Site.Visual = Cache;
        Site.Name = TEXT("TREASURE CHEST");
        for (const FName Tag : Cache->Tags)
        {
            if (Tag.ToString().StartsWith(TEXT("AbyssCacheId_")))
                for (AActor* Model : Opened) if (Model->ActorHasTag(Tag)) Site.OpenedVisual = Model;
            if (Tag.ToString().StartsWith(TEXT("AbyssInterior_")))
                for (AActor* Roof : Roofs) if (Roof->ActorHasTag(Tag)) Site.InteriorRoof = Roof;
        }
        if (Site.OpenedVisual)
        {
            Site.OpenedVisual->SetActorTransform(Cache->GetActorTransform());
            Site.OpenedVisual->SetActorHiddenInGame(true);
        }
    }
}

bool AAbyssGameMode::SpawnCacheAmbush(int32 SiteIndex)
{
    if (!Sites.IsValidIndex(SiteIndex) || Sites[SiteIndex].Kind != FAbyssSite::LootCache || !Sites[SiteIndex].bUsed ||
        !Sites[SiteIndex].bAmbushPlanned || CacheMonsters.Contains(SiteIndex) ||
        CacheMonsters.Num() >= MaximumCacheAmbushes) return false;
    const FAbyssSite& Site = Sites[SiteIndex];
    APawn* Player = UGameplayStatics::GetPlayerPawn(this,0);
    if (!Player) return false;
    FVector Away = (Site.Position - Player->GetActorLocation()).GetSafeNormal2D();
    if (Away.IsNearlyZero()) Away = FVector::ForwardVector;
    FCollisionQueryParams Query;
    Query.AddIgnoredActor(Site.Visual);
    Query.AddIgnoredActor(Site.OpenedVisual);
    Query.AddIgnoredActor(Player);
    FVector SpawnPoint;
    bool bFound = false;
    for (int32 I=0; I<8; ++I)
    {
        const FVector Candidate = Site.Position + Away.RotateAngleAxis(I*45.f,FVector::UpVector)*220.f;
        if (FVector::DistSquared2D(Candidate,Player->GetActorLocation()) < FMath::Square(180.f)) continue;
        FHitResult Floor;
        if (!GetWorld()->LineTraceSingleByChannel(Floor,Candidate+FVector(0,0,120),Candidate-FVector(0,0,450),ECC_WorldStatic,Query) ||
            Floor.ImpactNormal.Z < .65f || FMath::Abs(Floor.ImpactPoint.Z-Site.CacheAnchor.Z)>150.f) continue;
        SpawnPoint = Floor.ImpactPoint;
        if (GetWorld()->OverlapBlockingTestByChannel(SpawnPoint+FVector(0,0,55),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(45),Query)) continue;
        bFound = true;
        break;
    }
    if (!bFound) return false;
    auto* Model = LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/IntegratedCity/Monsters/GiantIsopod/SK_GiantIsopod"));
    auto* Walk = LoadObject<UAnimSequence>(nullptr,TEXT("/Game/IntegratedCity/Monsters/GiantIsopod/SK_GiantIsopod_Anim_Armature_IS_Walk"));
    if (!ensure(Model && Walk)) return false;
    const FBoxSphereBounds Bounds = Model->GetBounds();
    const float Scale = 140.f / FMath::Max(1.f,2*FMath::Max(Bounds.BoxExtent.X,Bounds.BoxExtent.Y));
    SpawnPoint.Z -= (Bounds.Origin.Z-Bounds.BoxExtent.Z)*Scale-5.f;
    auto* Monster = GetWorld()->SpawnActor<ASkeletalMeshActor>(SpawnPoint,(Player->GetActorLocation()-SpawnPoint).Rotation());
    if (!Monster) return false;
    Monster->Tags = {TEXT("AbyssExplorationHunter"),TEXT("AbyssBenthicHunter"),TEXT("AbyssCacheMonster")};
    Monster->SetActorScale3D(FVector(Scale));
    Monster->SetActorEnableCollision(false);
    auto* Mesh = Monster->GetSkeletalMeshComponent();
    Mesh->SetSkeletalMesh(Model);
    Mesh->bEnableUpdateRateOptimizations = true;
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    Mesh->OverrideAnimationData(Walk,true,true,0,1);
    auto* Light = NewObject<UPointLightComponent>(Monster);
    Light->SetupAttachment(Mesh);
    Light->SetCastShadows(false);
    Light->SetIntensity(350);
    Light->SetAttenuationRadius(300);
    Light->SetLightColor(FLinearColor(.25f,.75f,.7f));
    Light->RegisterComponent();
    FAbyssHunter& HunterState = ExtraHunters.AddDefaulted_GetRef();
    HunterState.Actor = Monster;
    HunterState.Light = Light;
    HunterState.Memory = 5.f;
    HunterState.LastKnown = Player->GetActorLocation();
    CacheMonsters.Add(SiteIndex,Monster);
    BindCreatureAnimation(Monster);
    UE_LOG(LogTemp,Display,TEXT("ABYSS_CACHE_AMBUSH site=%d total=%d"),SiteIndex,CacheMonsters.Num());
    return true;
}

void AAbyssGameMode::RestoreCacheAmbushes()
{
    for (auto It=CacheMonsters.CreateIterator(); It; ++It)
    {
        if (Sites.IsValidIndex(It.Key()) && Sites[It.Key()].bUsed && Sites[It.Key()].bAmbushPlanned) continue;
        AActor* Monster = It.Value();
        CreatureAnimations.Remove(Monster);
        ExplorationHomes.Remove(Monster);
        ExtraHunters.RemoveAll([Monster](const FAbyssHunter& Entry){ return Entry.Actor == Monster; });
        if (Monster) Monster->Destroy();
        It.RemoveCurrent();
    }
    for (int32 I=0; I<Sites.Num(); ++I)
        if (Sites[I].bUsed && Sites[I].bAmbushPlanned) SpawnCacheAmbush(I);
}
