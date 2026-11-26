#include "Characters/AbyssDiver.h"
#include "Game/AbyssGameMode.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
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
#include "UnrealClient.h"
#include "InputKeyEventArgs.h"
#include "Engine/Texture.h"

#include "Common/AbyssShared.h"

void AAbyssGameMode::BindCreatureAnimation(AActor* Creature)
{
    if (!Creature) return;
    if (Creature->ActorHasTag(TEXT("AbyssExplorationHunter")))
        ExplorationHomes.Add(Creature,Creature->GetActorLocation());
    auto* Mesh = Creature->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh || !Mesh->GetSkeletalMeshAsset()) return;
    auto* Movement = Cast<UAnimSequence>(Mesh->AnimationData.AnimToPlay);
    if (!Movement) return;

    FString Prefix = Movement->GetOutermost()->GetName();
    const int32 Suffix = Prefix.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    if (Suffix == INDEX_NONE) return;
    Prefix.LeftInline(Suffix + 1);

    FAbyssCreatureAnimation& Animation = CreatureAnimations.Add(Creature);
    Animation.Movement = Movement;
    Animation.Idle = LoadObject<UAnimSequence>(nullptr, *(Prefix + TEXT("Idle")));
    Animation.Search = LoadObject<UAnimSequence>(nullptr, *(Prefix + TEXT("Search")));
    const bool bBites = Movement->GetName().Contains(TEXT("_EEL_")) || Movement->GetName().Contains(TEXT("_GT_"));
    Animation.Attack = LoadObject<UAnimSequence>(nullptr, *(Prefix + (bBites ? TEXT("Bite") : TEXT("Attack"))));

    Mesh->SkinCacheUsage.Init(ESkinCacheUsage::Disabled, Mesh->GetSkeletalMeshAsset()->GetLODNum());
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->PlayAnimation(Movement, true);
}

void AAbyssGameMode::UpdateCreatureAnimation(AActor* Creature, bool bMoving, bool bSearching, bool bAttacking)
{
    FAbyssCreatureAnimation* Animation = CreatureAnimations.Find(Creature);
    if (!Animation) return;
    auto* Mesh = Creature->FindComponentByClass<USkeletalMeshComponent>();
    if (!Mesh) return;

    if (bAttacking && Animation->Attack && Elapsed >= Animation->AttackEnds)
    {
        Animation->AttackEnds = Elapsed + Animation->Attack->GetPlayLength();
        Mesh->PlayAnimation(Animation->Attack, false);
        return;
    }
    if (Elapsed < Animation->AttackEnds) return;

    UAnimSequence* Loop = Animation->Movement;
    if (bSearching && Animation->Search) Loop = Animation->Search;
    else if (!bMoving && Animation->Idle) Loop = Animation->Idle;

    UAnimSingleNodeInstance* Instance = Mesh->GetSingleNodeInstance();
    if (Loop && (!Instance || Instance->GetCurrentAsset() != Loop || !Mesh->IsPlaying()))
        Mesh->PlayAnimation(Loop, true);
}
void AAbyssGameMode::SpawnCityHunters()
{
    Patrol={FVector(0,-11000,100),FVector(12000,-11000,100),FVector(12000,0,100),FVector(12000,11000,100),FVector(0,11000,100),FVector(-12000,11000,100),FVector(-12000,0,100),FVector(-12000,-11000,100)};
    for (FVector& Point:Patrol) { Point.X*=CityScale; Point.Y*=CityScale; }
    TArray<FVector> Starts={FVector(-12000,0,100),FVector(12000,-11000,100),FVector(0,11000,100)};
    for (FVector& Point:Starts) { Point.X*=CityScale; Point.Y*=CityScale; }

    TArray<AActor*> AuthoredHunters;
    UGameplayStatics::GetAllActorsWithTag(this, TEXT("AbyssPatrolCreature"), AuthoredHunters);
    AuthoredHunters.Sort([](const AActor& A, const AActor& B){ return A.GetName()<B.GetName(); });
    const bool bUseAuthoredStarts=GetWorld()->GetMapName().EndsWith(TEXT("_Mk2")) && !AuthoredHunters.IsEmpty();
    if (bUseAuthoredStarts)
    {
        Starts.Reset(AuthoredHunters.Num());
        for (const AActor* Enemy : AuthoredHunters) Starts.Add(Enemy->GetActorLocation());
    }
    const bool bRandomSpawns=!bUseAuthoredStarts && GetWorld()->GetMapName().Contains(TEXT("Streetscape"));
    if (bRandomSpawns)
    {
        FVector PlayerStart(6000,3500,74);
        TArray<AActor*> StartMarkers;
        UGameplayStatics::GetAllActorsWithTag(this,TEXT("AbyssPlayerSpawn"),StartMarkers);
        if (!StartMarkers.IsEmpty()) PlayerStart=StartMarkers[0]->GetActorLocation();

        TArray<FVector> Candidates;
        const int32 Avenues[]={-18000,-12000,0,12000,18000};
        const int32 Along[]={-22000,-14500,-8500,-4500,4500,8500,14500,22000};
        const int32 CrossStreets[]={-17000,-11000,0,11000,17000};
        for (int32 X:Avenues) for (int32 Y:Along) Candidates.Emplace(X,Y,95);
        for (int32 Y:CrossStreets) for (int32 X:Along) Candidates.Emplace(X,Y,95);

        TArray<FVector> Chosen;
        int32 DistrictCounts[4]={};
        FCollisionQueryParams Query;
        if (const APawn* Player=UGameplayStatics::GetPlayerPawn(this,0)) Query.AddIgnoredActor(Player);
        const FCollisionShape Clearance=FCollisionShape::MakeSphere(60);
        const int32 HunterCount=RoamingKraken ? 9 : 6;
        while (!Candidates.IsEmpty() && Chosen.Num()<HunterCount)
        {
            const int32 Pick=FMath::RandRange(0,Candidates.Num()-1);
            const FVector Spot=Candidates[Pick];
            Candidates.RemoveAtSwap(Pick);
            const int32 District = (Spot.X >= 0 ? 1 : 0) + (Spot.Y >= 0 ? 2 : 0);
            if (DistrictCounts[District] >= (HunterCount+3)/4 ||
                FVector::DistSquared2D(Spot,PlayerStart)<FMath::Square(3000.f)) continue;
            const bool bNearEntrance = CityInteriors.ContainsByPredicate([&](const FAbyssCityInterior& Room)
            {
                if (!Room.UpperStorey) return false;
                const FVector Forward = Room.Roof->GetActorForwardVector();
                const float Reach = FMath::Abs(Forward.X)*Room.Extent.X + FMath::Abs(Forward.Y)*Room.Extent.Y;
                return FVector::DistSquared2D(Spot, Room.Center - Forward*(Reach+120.f)) < FMath::Square(750.f);
            });
            if (bNearEntrance) continue;
            if (Chosen.ContainsByPredicate([&](const FVector& Other){return FVector::DistSquared2D(Spot,Other)<FMath::Square(2000.f);})) continue;
            if (Sites.ContainsByPredicate([&](const FAbyssSite& Site){return FVector::DistSquared2D(Spot,Site.Position)<FMath::Square(900.f);})) continue;
            FHitResult Floor;
            if (!GetWorld()->LineTraceSingleByChannel(Floor,Spot,Spot-FVector(0,0,145),ECC_WorldStatic,Query) ||
                Floor.ImpactNormal.Z<.65f || Floor.ImpactPoint.Z>25) continue;
            if (GetWorld()->OverlapBlockingTestByChannel(Spot,FQuat::Identity,ECC_Visibility,Clearance,Query) ||
                GetWorld()->OverlapBlockingTestByChannel(Spot,FQuat::Identity,ECC_Pawn,Clearance,Query)) continue;
            Chosen.Add(Spot);
            ++DistrictCounts[District];
        }
        if (!Chosen.IsEmpty()) Starts=MoveTemp(Chosen);
        if (Starts.Num()!=HunterCount) UE_LOG(LogTemp,Warning,TEXT("Only %d usable hunter starts found"),Starts.Num());
    }


    for (int32 I=0; I<Starts.Num(); ++I)
    {
        const FVector P=Starts[I];
        int32 FirstPatrol=I%Patrol.Num();
        if (bRandomSpawns || bUseAuthoredStarts)
        {
            FirstPatrol=0;
            for (int32 J=1;J<Patrol.Num();++J)
                if (FVector::DistSquared2D(P,Patrol[J])<FVector::DistSquared2D(P,Patrol[FirstPatrol])) FirstPatrol=J;
            UE_LOG(LogTemp,Display,TEXT("ABYSS_HUNTER_SPAWN %d: %s"),I,*P.ToString());
        }
        AActor* Enemy=nullptr;
        if (AuthoredHunters.IsValidIndex(I))
        {
            Enemy=AuthoredHunters[I];
            Enemy->SetActorLocation(P);
        }
        else
        {
            Enemy=Shape(FVector(P.X/Abyss::MapScale,P.Y/Abyss::MapScale,P.Z),FVector(1.6,0.8,0.65),FLinearColor(1,0.005,0.008),false,TEXT("Sphere"))->GetOwner();
        }
        UPointLightComponent* Light=NewObject<UPointLightComponent>(Enemy);
        Light->SetupAttachment(Enemy->GetRootComponent());
        Light->SetIntensity(4500); Light->SetAttenuationRadius(650);
        Light->SetLightColor(FLinearColor(1,0.015,0.03)); Light->RegisterComponent();
        if (Enemy->ActorHasTag(TEXT("AbyssExplorationHunter")))
        {
            Light->SetCastShadows(false);
            Light->SetIntensity(350);
            Light->SetAttenuationRadius(300);
        }
        if (I==0) { Hunter=Enemy; HunterLight=Light; PatrolIndex=FirstPatrol; }
        else { FAbyssHunter H; H.Actor=Enemy; H.Light=Light; H.PatrolIndex=FirstPatrol; ExtraHunters.Add(H); }
    }
    BindCreatureAnimation(Hunter);
    for (const FAbyssHunter& Enemy : ExtraHunters) BindCreatureAnimation(Enemy.Actor);
    BindCreatureAnimation(RoamingKraken);
    for (int32 I=Starts.Num(); I<AuthoredHunters.Num(); ++I) AuthoredHunters[I]->Destroy();
    UE_LOG(LogTemp,Display,TEXT("ABYSS_CITY_READY: %.0fm city, %d interaction sites, %d authored hunters active."),500*CityScale,Sites.Num(),FMath::Min(Starts.Num(),AuthoredHunters.Num()));
}

void AAbyssGameMode::TickHunter(AActor* Enemy,UPointLightComponent* Light,float& EnemyMemory,FVector& Known,int32& Route,float Dt,bool bRepairing)
{
    APawn* P=UGameplayStatics::GetPlayerPawn(this,0);
    if (!P || !Enemy || !Light || bEnded || Patrol.IsEmpty()) return;
    AAbyssDiver* Diver=Cast<AAbyssDiver>(P);
    FVector HP=Enemy->GetActorLocation(), DP=P->GetActorLocation();
    float Distance=FVector::Dist2D(HP,DP);
    const FVector* Home = ExplorationHomes.Find(Enemy);
    if (Home && Distance>4500.f) { Light->SetVisibility(false); EnemyMemory=0; return; }
    FCollisionQueryParams Query;
    Query.AddIgnoredActor(P);
    Query.AddIgnoredActor(Enemy);
    const bool bBenthic = Enemy->ActorHasTag(TEXT("AbyssBenthicHunter"));
    const FVector Eye = HP + FVector(0,0,bBenthic ? 55.f : 0.f);
    float Vision=(bLight ? 1300.f : 670.f)*Abyss::MapScale;
    const float HabitatRadius=Enemy->ActorHasTag(TEXT("AbyssCaveResident")) ? 600.f : 1000.f;
    const bool bInHabitat = !Home || FVector::DistSquared2D(DP,*Home)<FMath::Square(HabitatRadius);
    const bool bSees=bInHabitat && !bHidden && Distance<Vision &&
        !GetWorld()->LineTraceTestByChannel(Eye,DP,ECC_Visibility,Query);
    if (Enemy->ActorHasTag(TEXT("AbyssBobbitWorm")))
    {
        // The worm stays rooted in the seabed while its upper body attacks.
        const bool bAttack = bSees && Distance < 180.f && FMath::Abs(HP.Z-DP.Z)<180.f && DamageCooldown <= 0.f;
        Alert = FMath::Max(Alert, bSees && Distance < 450.f ? 0.7f : 0.f);
        UpdateCreatureAnimation(Enemy, false, bSees && Distance < 450.f, bAttack);
        if (bAttack) DamageDiver(35);
        return;
    }
    bool bHears=bInHabitat && ((Diver && Diver->bSprinting && Distance<1700*Abyss::MapScale) || (bRepairing && Distance<2300*Abyss::MapScale));
    if (bSees || bHears) { EnemyMemory=FMath::Max(EnemyMemory,3.f); Known=DP; }
    else EnemyMemory=FMath::Max(0.f,EnemyMemory-Dt);
    bool bPursuing=EnemyMemory>0;
    bChasing|=bPursuing;
    Alert=FMath::Max(Alert,bPursuing ? 1.f : FMath::Clamp(1-Distance/(1700*Abyss::MapScale),0.f,0.7f));
    Route=FMath::Clamp(Route,0,Patrol.Num()-1);
    FVector Target = bPursuing ? Known : Patrol[Route];
    if (Home)
    {
        const float Angle=(Route%4)*HALF_PI;
        const FVector LocalPatrol=*Home+FVector(FMath::Cos(Angle)*300.f,FMath::Sin(Angle)*300.f,0);
        Target=bPursuing ? *Home+(Known-*Home).GetClampedToMaxSize2D(HabitatRadius*.95f) : LocalPatrol;
    }
    Target.Z=HP.Z;
    if (FVector::Dist2D(HP,Target)<70 && !bPursuing) Route=(Route+1)%Patrol.Num();
    FVector Direction = (Target-HP).GetSafeNormal2D();
    // A seabed hunter's origin is at its feet. Probe above the floor, along its body.
    const FVector ProbeStart = HP + FVector(0,0,bBenthic ? 65.f : 0.f);
    const FCollisionShape Probe = FCollisionShape::MakeSphere(60);
    auto Clear = [&](const FVector& Heading)
    {
        return !GetWorld()->SweepTestByChannel(ProbeStart, ProbeStart+Heading*230.f,
            FQuat::Identity, ECC_Pawn, Probe, Query);
    };
    if (!Direction.IsNearlyZero() && !Clear(Direction))
    {
        const FVector Desired=Direction;
        Direction=FVector::ZeroVector;
        const float Side=(GetTypeHash(Enemy->GetFName())&1) ? 1.f : -1.f;
        for (float Angle:{45.f,90.f,135.f,-45.f,-90.f,-135.f,180.f})
        {
            const FVector Candidate=Desired.RotateAngleAxis(Angle*Side,FVector::UpVector);
            if (Clear(Candidate)) { Direction=Candidate; break; }
        }
    }

    FVector Destination=HP+Direction*(bPursuing ? 270.f : 140.f)*Dt;
    if (Home)
    {
        const FVector Limit=*Home+(Destination-*Home).GetClampedToMaxSize2D(HabitatRadius);
        Destination.X=Limit.X; Destination.Y=Limit.Y;
    }
    Destination.X=FMath::Clamp(Destination.X,-MapHalfExtent,MapHalfExtent);
    Destination.Y=FMath::Clamp(Destination.Y,-MapHalfExtent,MapHalfExtent);
    if (bBenthic)
    {
        FHitResult Floor;
        float Lift=5.f;
        const auto* Mesh=Enemy->FindComponentByClass<USkeletalMeshComponent>();
        if (Mesh && Mesh->GetSkeletalMeshAsset())
        {
            const auto Bounds=Mesh->GetSkeletalMeshAsset()->GetBounds();
            Lift-=(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Mesh->GetComponentScale().Z;
        }
        if (GetWorld()->LineTraceSingleByChannel(Floor,Destination+FVector(0,0,120),Destination-FVector(0,0,500),ECC_WorldStatic,Query) &&
            Floor.ImpactNormal.Z>.65f && FMath::Abs(Floor.ImpactPoint.Z+Lift-HP.Z)<=45.f)
            Destination.Z=Floor.ImpactPoint.Z+Lift;
        else Destination=HP;
    }
    else Destination.Z=(Home ? Home->Z : 95.f)+FMath::Sin(Elapsed*2)*12.f;
    FHitResult Barrier;
    if (GetWorld()->SweepSingleByChannel(Barrier,ProbeStart,Destination+FVector(0,0,bBenthic ? 65.f : 0.f),
        FQuat::Identity,ECC_Pawn,Probe,Query))
        Destination=HP+(Destination-HP)*FMath::Max(0.f,Barrier.Time-.01f);
    const bool bMoved=FVector::DistSquared(HP,Destination)>1.f;
    HP=Destination;
    Enemy->SetActorLocation(HP);
    const bool bAttack = DamageCooldown <= 0.f && FVector::Dist2D(HP,DP)<110 && FMath::Abs(HP.Z-DP.Z)<180.f && (!bHidden || EnemyMemory > 0) &&
        !GetWorld()->LineTraceTestByChannel(HP+FVector(0,0,bBenthic ? 55.f : 0.f), DP, ECC_Visibility, Query);
    const FVector Facing = bAttack ? (DP-HP).GetSafeNormal2D() : Direction;
    if (!Facing.IsNearlyZero())
        Enemy->SetActorRotation(FMath::RInterpTo(Enemy->GetActorRotation(), Facing.Rotation(), Dt, 7.f));
    Light->SetIntensity(Home ? 350.f : 4500+FMath::Sin(Elapsed*5)*1800);
    if (Home) Light->SetVisibility(Distance<2500.f);
    UpdateCreatureAnimation(Enemy, bMoved, bPursuing && !bSees, bAttack);
    if (bAttack) DamageDiver(35);
}

