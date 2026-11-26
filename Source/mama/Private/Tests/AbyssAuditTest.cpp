#include "Game/AbyssGameMode.h"
#include "Characters/AbyssDiver.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

// Whole-map audit for the items left after the 2026-11-25 pass. It reports every finding by name;
// only objective defects (stuck inside geometry, missing meshes, kraken clipping) fail the run.
void AAbyssGameMode::RunAuditCheck()
{
    if (SmokeStage!=-3 || Elapsed<1.f) return;
    auto* Player=Cast<AAbyssDiver>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!Player) return;
    bool Passed=true;
    auto Check=[&](bool OK,const FString& Name)
    {
        UE_LOG(LogTemp,Display,TEXT("AUDIT_TEST %s: %s"),OK ? TEXT("PASS") : TEXT("FAIL"),*Name);
        Passed&=OK;
    };
    const FVector PlayerStart=Player->GetActorLocation();
    TSet<const AActor*> Creatures={Player,Hunter.Get(),RoamingKraken.Get()};
    for (const FAbyssHunter& Enemy:ExtraHunters) Creatures.Add(Enemy.Actor);
    FCollisionQueryParams Query;
    Query.AddIgnoredActor(Player);
    auto GroundZ=[&](const FVector& At,FVector::FReal& Z)
    {
        FHitResult Floor;
        if (!GetWorld()->LineTraceSingleByChannel(Floor,FVector(At.X,At.Y,At.Z+150),FVector(At.X,At.Y,At.Z-600),ECC_WorldStatic,Query)
            || Floor.ImpactNormal.Z<.65f) return false;
        Z=Floor.ImpactPoint.Z+70;
        return true;
    };

    // 1. Every placed static mesh: missing mesh/material, non-blocking solids, pass-through and stuck checks.
    int32 Components=0,Missing=0,NoMaterial=0,Passable=0,Probed=0,Crossed=0,Stuck=0,NoShape=0;
    for (TActorIterator<AActor> It(GetWorld());It;++It)
    {
        if (Creatures.Contains(*It) || It->IsHidden()) continue;
        TArray<UStaticMeshComponent*> Meshes;
        It->GetComponents(Meshes);
        for (UStaticMeshComponent* Mesh:Meshes)
        {
            if (!Mesh->IsVisible() || !Mesh->IsRegistered()) continue;
            ++Components;
            const FString Name=It->GetActorNameOrLabel()+TEXT(".")+Mesh->GetName();
            // Salvage sites keep an empty root and attach their part models under it.
            if (!Mesh->GetStaticMesh() && Mesh->GetNumChildrenComponents()==0)
            {
                ++Missing;
                UE_LOG(LogTemp,Display,TEXT("AUDIT_MISSING_MESH %s at %s"),*Name,*Mesh->GetComponentLocation().ToString());
                continue;
            }
            if (!Mesh->GetStaticMesh()) continue;
            for (int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot)
                if (!Mesh->GetMaterial(Slot))
                {
                    ++NoMaterial;
                    UE_LOG(LogTemp,Display,TEXT("AUDIT_MISSING_MATERIAL %s slot=%d"),*Name,Slot);
                }
            if (Mesh->GetCollisionEnabled()!=ECollisionEnabled::NoCollision && Mesh->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block)
                if (const UBodySetup* Body=Mesh->GetStaticMesh()->GetBodySetup();
                    !Body || Body->AggGeom.GetElementCount()==0 && Body->CollisionTraceFlag!=CTF_UseComplexAsSimple)
                {
                    ++NoShape;
                    UE_LOG(LogTemp,Display,TEXT("AUDIT_NO_COLLISION_SHAPE %s mesh=%s at=%s"),*Name,*Mesh->GetStaticMesh()->GetPathName(),
                        *Mesh->GetComponentLocation().ToString());
                }
            const FBox Box=Mesh->Bounds.GetBox();
            const FVector Center=Box.GetCenter(),Extent=Box.GetExtent();
            // Terrain, roads and whole-city shells are covered by the building route checks.
            if (FMath::Max(Extent.X,Extent.Y)>2500.f || FMath::Min(Extent.X,Extent.Y)<15.f || Extent.Z<25.f) continue;
            if (FMath::Abs(Center.X)>MapHalfExtent || FMath::Abs(Center.Y)>MapHalfExtent) continue;
            FVector::FReal SwimZ=0;
            const FVector Probe(Center.X-Extent.X-120,Center.Y,Box.Max.Z);
            if (!GroundZ(Probe,SwimZ) || Box.Min.Z>SwimZ+30 || Box.Max.Z<SwimZ-30) continue;
            if (Mesh->GetCollisionEnabled()==ECollisionEnabled::NoCollision || Mesh->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block)
            {
                ++Passable;
                UE_LOG(LogTemp,Display,TEXT("AUDIT_PASSABLE %s mesh=%s extent=%s"),*Name,*Mesh->GetStaticMesh()->GetName(),*Extent.ToString());
                continue;
            }
            ++Probed;
            for (const FVector& Dir:{FVector(1,0,0),FVector(-1,0,0),FVector(0,1,0),FVector(0,-1,0)})
            {
                const float Reach=FMath::Abs(Dir.X)*Extent.X+FMath::Abs(Dir.Y)*Extent.Y;
                FVector Start=Center-Dir*(Reach+90);
                if (!GroundZ(Start,Start.Z)) continue;
                if (GetWorld()->OverlapBlockingTestByChannel(Start,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(37),Query)) continue;
                Player->SetActorLocation(Start);
                const int32 Steps=FMath::CeilToInt((2*Reach+180)/24.f);
                for (int32 I=0;I<Steps;++I) Player->MoveDiver(Dir*24.f);
                const FVector End=Player->GetActorLocation();
                FCollisionQueryParams Self;Self.AddIgnoredActor(Player);
                TArray<FOverlapResult> Hits;
                if (GetWorld()->OverlapMultiByChannel(Hits,End,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(34),Self) &&
                    Hits.ContainsByPredicate([&](const FOverlapResult& H){return H.GetComponent()==Mesh;}))
                {
                    ++Stuck;
                    UE_LOG(LogTemp,Display,TEXT("AUDIT_STUCK %s dir=%s end=%s"),*Name,*Dir.ToString(),*End.ToString());
                }
                else if (FVector::DotProduct(End-Center,Dir)>Reach+20)
                {
                    ++Crossed;
                    UE_LOG(LogTemp,Display,TEXT("AUDIT_CROSSED %s mesh=%s dir=%s start=%s end=%s"),*Name,
                        *Mesh->GetStaticMesh()->GetName(),*Dir.ToString(),*Start.ToString(),*End.ToString());
                }
            }
        }
    }
    UE_LOG(LogTemp,Display,TEXT("AUDIT_OBJECTS components=%d missingMesh=%d missingMaterial=%d passable=%d probed=%d crossed=%d stuck=%d"),
        Components,Missing,NoMaterial,Passable,Probed,Crossed,Stuck);
    Check(Missing==0,TEXT("No visible component lost its static mesh"));
    Check(NoShape==0,TEXT("Every blocking prop has collision geometry"));
    Check(Stuck==0,TEXT("Swimming into any solid prop never leaves the diver inside it"));
    Player->SetActorLocation(PlayerStart);

    // 2. Every creature kind: indoor pursuit (patrol kinds) or walled-habitat pursuit (resident kinds).
    struct FEntry { AActor* Actor; UPointLightComponent* Light; };
    TArray<FEntry> All;
    if (Hunter) All.Add({Hunter,HunterLight});
    for (const FAbyssHunter& Enemy:ExtraHunters) All.Add({Enemy.Actor,Enemy.Light});
    TMap<FString,FEntry> Kinds;
    for (const FEntry& E:All)
    {
        if (E.Actor->ActorHasTag(TEXT("AbyssBobbitWorm"))) continue;
        const auto* Skin=E.Actor->FindComponentByClass<USkeletalMeshComponent>();
        const FString Kind=(Skin && Skin->GetSkeletalMeshAsset() ? Skin->GetSkeletalMeshAsset()->GetName() : TEXT("Placeholder"))+
            (ExplorationHomes.Contains(E.Actor) ? TEXT("/Resident") : TEXT("/Patrol"));
        if (!Kinds.Contains(Kind)) Kinds.Add(Kind,E);
    }
    UE_LOG(LogTemp,Display,TEXT("AUDIT_CREATURE_KINDS %d"),Kinds.Num());
    TArray<const FAbyssCityInterior*> Rooms;
    for (const auto& Room:CityInteriors) if (!Room.Roof->ActorHasTag(TEXT("AbyssInterior_SeaCave"))) Rooms.Add(&Room);
    const bool OldHidden=bHidden,OldLight=bLight;
    const float OldCooldown=DamageCooldown,OldHealth=Health;
    for (auto& Pair:Kinds)
    {
        AActor* Enemy=Pair.Value.Actor;
        const FVector Original=Enemy->GetActorLocation();
        const FRotator Turn=Enemy->GetActorRotation();
        FCollisionQueryParams EnemyQuery;EnemyQuery.AddIgnoredActor(Player);EnemyQuery.AddIgnoredActor(Enemy);
        const bool Benthic=Enemy->ActorHasTag(TEXT("AbyssBenthicHunter"));
        auto Inside=[&]{return GetWorld()->OverlapBlockingTestByChannel(Enemy->GetActorLocation()+FVector(0,0,Benthic ? 65.f : 0.f),
            FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(55),EnemyQuery);};
        DamageCooldown=1000.f;bHidden=false;bLight=true;
        if (FVector* Home=ExplorationHomes.Find(Enemy))
        {
            // Rebuild the resident's habitat in an empty test arena above the city.
            const FVector OldHome=*Home,Base(0,0,4500);
            TArray<AActor*> Fixtures;
            auto Box=[&](FVector Offset,FVector Extent)
            {
                auto* Actor=GetWorld()->SpawnActor<AActor>();
                auto* Shape=NewObject<UBoxComponent>(Actor);
                Actor->SetRootComponent(Shape);Shape->SetBoxExtent(Extent);
                Shape->SetCollisionProfileName(TEXT("BlockAll"));Shape->RegisterComponent();
                Actor->SetActorLocation(Base+Offset);Fixtures.Add(Actor);
            };
            Box(FVector(0,0,-25),FVector(1500,1500,25));
            Box(FVector(220,0,150),FVector(20,400,300));
            float Lift=5.f;
            if (const auto* Skin=Enemy->FindComponentByClass<USkeletalMeshComponent>(); Skin && Skin->GetSkeletalMeshAsset())
            {
                const auto Bounds=Skin->GetSkeletalMeshAsset()->GetBounds();
                Lift-=(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Skin->GetComponentScale().Z;
            }
            const FVector Spawn=Base+FVector(0,0,Benthic ? Lift : 95.f);
            Enemy->SetActorLocation(Spawn);*Home=Spawn;
            Player->SetActorLocation(Base+FVector(450,0,70));
            float Recall=3;FVector Known=Player->GetActorLocation();int32 Route=0;bool Clear=true;
            for (int32 I=0;I<400;++I)
            {
                TickHunter(Enemy,Pair.Value.Light,Recall,Known,Route,.05f,true);
                Clear&=!Inside();
            }
            const float Gap=FVector::Dist2D(Enemy->GetActorLocation(),Player->GetActorLocation());
            UE_LOG(LogTemp,Display,TEXT("AUDIT_RESIDENT %s gap=%.0f end=%s"),*Pair.Key,Gap,*(Enemy->GetActorLocation()-Base).ToString());
            Check(Clear,Pair.Key+TEXT(": resident pursuit never enters a wall"));
            Check(Gap<200,Pair.Key+TEXT(": resident navigates around a wall inside its habitat"));
            *Home=OldHome;
            for (AActor* Actor:Fixtures) Actor->Destroy();
        }
        else
        {
            int32 Reached=0,Tried=0;bool Clear=true;
            for (const FAbyssCityInterior* Room:Rooms)
            {
                // Same doorway direction as the movement check's verified entries.
                const auto* Cache=Sites.FindByPredicate([&](const FAbyssSite& Site){return Site.InteriorRoof==Room->Roof;});
                if (!Cache) continue;
                const bool Lobby=Cache->Visual->ActorHasTag(TEXT("AbyssLobbySupply"));
                const bool Warehouse=Cache->Visual->ActorHasTag(TEXT("AbyssWarehouseCache"));
                const FVector Forward=Lobby ? Room->Roof->GetActorForwardVector() : Warehouse ? FVector(0,1,0) : FVector(1,0,0);
                const float Reach=FMath::Abs(Forward.X)*Room->Extent.X+FMath::Abs(Forward.Y)*Room->Extent.Y;
                FVector Target=Room->Center,Door=Room->Center-Forward*(Reach+300);
                if (!GroundZ(Target,Target.Z)) continue;
                Door.Z=95.f;
                if (GetWorld()->OverlapBlockingTestByChannel(Door,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(60),EnemyQuery)) continue;
                ++Tried;
                Player->SetActorLocation(Target);
                Enemy->SetActorLocation(Door);
                float Recall=3;FVector Known=Target;int32 Route=0;
                for (int32 I=0;I<600;++I)
                {
                    TickHunter(Enemy,Pair.Value.Light,Recall,Known,Route,.05f,false);
                    Clear&=!Inside();
                }
                const float Gap=FVector::Dist2D(Enemy->GetActorLocation(),Target);
                Reached+=Gap<250;
                UE_LOG(LogTemp,Display,TEXT("AUDIT_INDOOR %s room=%s gap=%.0f memory=%.1f door=%s end=%s"),*Pair.Key,*Room->Roof->GetName(),Gap,
                    Recall,*Door.ToString(),*Enemy->GetActorLocation().ToString());
            }
            UE_LOG(LogTemp,Display,TEXT("AUDIT_INDOOR_SUMMARY %s reached=%d/%d"),*Pair.Key,Reached,Tried);
            Check(Clear,Pair.Key+TEXT(": indoor pursuit never enters a wall"));
        }
        Enemy->SetActorLocationAndRotation(Original,Turn);
    }
    bHidden=OldHidden;bLight=OldLight;DamageCooldown=OldCooldown;Health=OldHealth;

    // 3. Roaming kraken figure-eight against every tall building it crosses.
    if (RoamingKraken)
    {
        // Drive the real route code through one full figure-eight at 20 fps.
        FCollisionQueryParams KrakenQuery;KrakenQuery.AddIgnoredActor(RoamingKraken);KrakenQuery.AddIgnoredActor(Player);
        const float Radius=FMath::Min(KrakenRadius()*.5f,400.f);
        const float OldElapsed=Elapsed,OldHeight=KrakenHeight;
        int32 Samples=0,Clipped=0;TSet<FString> Buildings;float Highest=0,MaxStep=0,LastZ=-1;
        KrakenHeight=-1.f;
        for (Elapsed=0;Elapsed<2*PI/.032f;Elapsed+=.05f,++Samples)
        {
            const FVector At=AdvanceKraken(.05f);
            Highest=FMath::Max(Highest,At.Z);
            if (LastZ>=0) MaxStep=FMath::Max(MaxStep,FMath::Abs(At.Z-LastZ));
            LastZ=At.Z;
            TArray<FOverlapResult> Hits;
            if (GetWorld()->OverlapMultiByChannel(Hits,At,FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeSphere(Radius),KrakenQuery))
            {
                ++Clipped;
                for (const FOverlapResult& H:Hits) if (H.GetActor()) Buildings.Add(H.GetActor()->GetActorNameOrLabel());
            }
        }
        Elapsed=OldElapsed;KrakenHeight=OldHeight;
        UE_LOG(LogTemp,Display,TEXT("AUDIT_KRAKEN radius=%.0f probe=%.0f samples=%d clipped=%d highest=%.0f maxStepPerFrame=%.1f actors=%s"),
            KrakenRadius(),Radius,Samples,Clipped,Highest,MaxStep,*FString::Join(Buildings.Array(),TEXT(",")));
        Check(Clipped==0,TEXT("Roaming kraken route stays clear of buildings and props"));
    }

    // 4. Resource budget for a straight dive: start -> every exploration cache -> repair sites -> hatch.
    TArray<FVector> Stops;
    for (const FAbyssSite& Site:Sites)
        if (Site.Visual && Site.Visual->ActorHasTag(TEXT("AbyssExplorationCache"))) Stops.Add(Site.Position);
    for (int32 Kind=5;Kind<=8;++Kind)
        if (const FAbyssSite* Site=Sites.FindByPredicate([&](const FAbyssSite& S){return S.Kind==Kind;})) Stops.Add(Site->Position);
    FVector At=PlayerStart;float Length=0;
    for (const FVector& Stop:Stops) { Length+=FVector::Dist2D(At,Stop); At=Stop; }
    int32 OxygenSites=0;for (const FAbyssSite& Site:Sites) OxygenSites+=Site.Kind==3;
    const float Seconds=Length*1.3f/SwimSpeed(false);
    UE_LOG(LogTemp,Display,TEXT("AUDIT_BUDGET stops=%d straight=%.0fm detoured=%.0fs oxygenUse=%.0f/%.0f battery=%.0f/%.0fs oxygenSites=%d reserveTanks=%d"),
        Stops.Num(),Length/100,Seconds,Seconds*OxygenRate(false),OxygenCapacity,Seconds,FlashlightDuration,OxygenSites,Inventory[3]);

    Player->SetActorLocation(PlayerStart);
    bSmokeTest=false;
    UE_LOG(LogTemp,Display,TEXT("AUDIT_TEST %s"),Passed ? TEXT("ALL PASSED") : TEXT("FAILED"));
    FPlatformMisc::RequestExitWithStatus(false,Passed ? 0 : 1);
}
