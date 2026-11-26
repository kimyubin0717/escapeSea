#include "Game/AbyssGameMode.h"
#include "Characters/AbyssDiver.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void AAbyssGameMode::RunMovementCheck()
{
    if (SmokeStage!=-3 || Elapsed<1.f) return;
    auto* Player=Cast<AAbyssDiver>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!Player) return;
    bool Passed=true;
    auto Check=[&](bool OK,const TCHAR* Name)
    {
        UE_LOG(LogTemp,Display,TEXT("MOVEMENT_TEST %s: %s"),OK ? TEXT("PASS") : TEXT("FAIL"),Name);
        Passed&=OK;
    };
    const FVector Base(0,0,4500);
    TArray<AActor*> Fixtures;
    auto Box=[&](FVector Offset,FVector Extent)
    {
        auto* Actor=GetWorld()->SpawnActor<AActor>();
        auto* Shape=NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Shape);
        Shape->SetBoxExtent(Extent);
        Shape->SetCollisionProfileName(TEXT("BlockAll"));
        Shape->RegisterComponent();
        Actor->SetActorLocation(Base+Offset);
        Fixtures.Add(Actor);
        return Actor;
    };
    Box(FVector(0,0,-25),FVector(1800,1800,25));
    AActor* Wall=Box(FVector(300,0,200),FVector(20,600,200));
    Player->SetActorLocation(Base+FVector(0,0,70));
    for (int32 I=0;I<90;++I) Player->MoveDiver(FVector(8,4,0));
    FVector Local=Player->GetActorLocation()-Base;
    Check(Local.X<246 && Local.Y>250,TEXT("Diagonal movement slides along a solid wall without crossing it"));
    for (int32 I=0;I<100;++I) Player->MoveDiver(FVector(8,0,0));
    Check(Player->GetActorLocation().Z<Base.Z+71,TEXT("Holding against a tall wall cannot climb it"));
    for (int32 I=0;I<60;++I) Player->MoveDiver(FVector(-8,0,0));
    Check(Player->GetActorLocation().X<Base.X+20,TEXT("Backing away releases contact without sticking"));
    Player->SetActorLocation(Base+FVector(0,0,70));
    Player->MoveDiver(FVector(2000,0,0));
    Check(Player->GetActorLocation().X<Base.X+246,TEXT("Large frame displacement cannot tunnel through a thin wall"));
    Player->SetActorLocation(Base+FVector(270,0,70));
    Player->MoveDiver(FVector(-50,0,0));
    FCollisionQueryParams Query;
    Query.AddIgnoredActor(Player);
    Check(!GetWorld()->OverlapBlockingTestByChannel(Player->GetActorLocation(),FQuat::Identity,ECC_Pawn,
        FCollisionShape::MakeSphere(35),Query),TEXT("An initial shallow overlap can resolve without remaining trapped"));
    Wall->SetActorEnableCollision(false);
    Box(FVector(250,0,15),FVector(120,200,15));
    Player->SetActorLocation(Base+FVector(0,0,70));
    for (int32 I=0;I<30;++I) Player->MoveDiver(FVector(8,0,0));
    Local=Player->GetActorLocation()-Base;
    Check(Local.X>200 && FMath::Abs(Local.Z-100)<1,TEXT("A supported low step remains traversable"));
    Player->SetActorLocation(Base+FVector(0,-400,70));
    AActor* Corner=Box(FVector(200,-250,200),FVector(20,180,200));
    Box(FVector(0,-80,200),FVector(220,20,200));
    for (int32 I=0;I<60;++I) Player->MoveDiver(FVector(8,8,0));
    for (int32 I=0;I<40;++I) Player->MoveDiver(FVector(-8,-8,0));
    Local=Player->GetActorLocation()-Base;
    Check(Local.X<30 && Local.Y<-330,TEXT("Two-wall contact permits a diagonal escape"));
    Corner->SetActorEnableCollision(false);
    for (AActor* Actor:Fixtures) Actor->Destroy();
    Fixtures.Reset();
    auto FloorPosition=[&](FVector Position,const FAbyssCityInterior& Room)
    {
        FCollisionQueryParams GroundQuery;
        GroundQuery.AddIgnoredActor(Player);
        GroundQuery.AddIgnoredActor(Room.Roof);
        GroundQuery.AddIgnoredActor(Room.UpperStorey);
        FHitResult Floor;
        // Probe the ground route, rather than spawning on a lobby canopy above its door.
        if (GetWorld()->LineTraceSingleByChannel(Floor,FVector(Position.X,Position.Y,150),
            FVector(Position.X,Position.Y,-500),ECC_WorldStatic,GroundQuery) && Floor.ImpactNormal.Z>.65f)
            Position.Z=Floor.ImpactPoint.Z+70;
        return Position;
    };
    auto MoveTo=[&](const FVector& Destination)
    {
        const int32 Limit=FMath::CeilToInt(FVector::Dist2D(Player->GetActorLocation(),Destination)/8)+80;
        for (int32 I=0;I<Limit && FVector::Dist2D(Player->GetActorLocation(),Destination)>9;++I)
            Player->MoveDiver((Destination-Player->GetActorLocation()).GetSafeNormal2D()*8.f);
        return FVector::Dist2D(Player->GetActorLocation(),Destination)<15;
    };
    int32 Rooms=0;
    for (const auto& Room:CityInteriors)
    {
        if (Room.Roof->ActorHasTag(TEXT("AbyssInterior_SeaCave"))) continue;
        const auto* Cache=Sites.FindByPredicate([&](const FAbyssSite& Site){return Site.InteriorRoof==Room.Roof;});
        if (!Cache)
        {
            UE_LOG(LogTemp,Display,TEXT("MOVEMENT_ROOM_NO_CACHE %s"),*Room.Roof->GetName());
            continue;
        }
        const bool Lobby=Cache->Visual->ActorHasTag(TEXT("AbyssLobbySupply"));
        const bool Warehouse=Cache->Visual->ActorHasTag(TEXT("AbyssWarehouseCache"));
        const FVector Forward=Lobby ? Room.Roof->GetActorForwardVector() : Warehouse ? FVector(0,1,0) : FVector(1,0,0);
        const FVector Right(-Forward.Y,Forward.X,0);
        const float Reach=FMath::Abs(Forward.X)*Room.Extent.X+FMath::Abs(Forward.Y)*Room.Extent.Y;
        const float Width=FMath::Abs(Right.X)*Room.Extent.X+FMath::Abs(Right.Y)*Room.Extent.Y;
        const FVector Outside=FloorPosition(Room.Center-Forward*(Reach+180),Room);
        const FVector Inside=FloorPosition(Room.Center,Room);
        Player->SetActorLocation(Outside);
        const bool Entered=MoveTo(Inside);
        UE_LOG(LogTemp,Display,TEXT("MOVEMENT_ROOM %s center=%s extent=%s forward=%s outside=%s inside=%s reached=%s"),
            *Room.Roof->GetName(),*Room.Center.ToString(),*Room.Extent.ToString(),*Forward.ToString(),
            *Outside.ToString(),*Inside.ToString(),*Player->GetActorLocation().ToString());
        UpdateBuilding();
        Check(Entered && Room.bInside,*FString::Printf(TEXT("%s: actual movement enters through the doorway"),*Room.Roof->GetName()));
        const bool Left=MoveTo(Outside);
        UpdateBuilding();
        Check(Left && !Room.bInside,*FString::Printf(TEXT("%s: movement exits and restores its exterior"),*Room.Roof->GetName()));
        for (float Side:{-1.f,1.f})
        {
            Player->SetActorLocation(FloorPosition(Room.Center-Forward*(Reach+90)+Right*(Width*.7f*Side),Room));
            const FVector RubStart=Player->GetActorLocation();
            FHitResult Surface;
            FCollisionQueryParams RubQuery;RubQuery.AddIgnoredActor(Player);
            GetWorld()->SweepSingleByChannel(Surface,RubStart,RubStart+Forward*500,FQuat::Identity,ECC_Pawn,
                FCollisionShape::MakeSphere(36),RubQuery);
            for (int32 I=0;I<40;++I) Player->MoveDiver(Forward*8+Right*(FMath::Min(2.f,Width*.002f)*Side));
            const float Contact=FVector::DotProduct(Player->GetActorLocation()-Room.Center,Forward);
            UE_LOG(LogTemp,Display,TEXT("MOVEMENT_WALL %s side=%.0f start=%s obstacle=%s point=%s normal=%s end=%s"),
                *Room.Roof->GetName(),Side,*RubStart.ToString(),*GetNameSafe(Surface.GetActor()),*Surface.ImpactPoint.ToString(),
                *Surface.ImpactNormal.ToString(),*Player->GetActorLocation().ToString());
            const float WallPlane=FVector::DotProduct(Surface.ImpactPoint-Room.Center,Forward);
            // Bevelled facade details can end before the straight sweep's impact plane.
            const bool Clear=!GetWorld()->OverlapBlockingTestByChannel(Player->GetActorLocation(),FQuat::Identity,
                ECC_Pawn,FCollisionShape::MakeSphere(35),RubQuery);
            Check(Surface.bBlockingHit && Contact<=WallPlane-24 && Clear,
                *FString::Printf(TEXT("%s: diagonal wall rubbing stays outside"),*Room.Roof->GetName()));
            for (int32 I=0;I<30;++I) Player->MoveDiver(-Forward*8);
            Check(FVector::DotProduct(Player->GetActorLocation()-Room.Center,Forward)<Contact-150,
                *FString::Printf(TEXT("%s: reversing releases wall contact"),*Room.Roof->GetName()));
        }
        ++Rooms;
    }
    UE_LOG(LogTemp,Display,TEXT("MOVEMENT_ROOMS_CHECKED %d"),Rooms);
    Check(Rooms>=20,TEXT("All authored bank, market, warehouse and lobby interiors exercised"));
    FAbyssHunter* Swimmer=ExtraHunters.FindByPredicate([](const FAbyssHunter& Entry)
    {
        return Entry.Actor->ActorHasTag(TEXT("AbyssWreckResident"));
    });
    Check(Swimmer!=nullptr,TEXT("A shipwreck swimmer is available for pursuit verification"));
    if (Swimmer)
    {
        const FVector Original=Swimmer->Actor->GetActorLocation();
        const FVector OriginalHome=ExplorationHomes.FindChecked(Swimmer->Actor);
        Swimmer->Actor->SetActorLocation(Base);
        ExplorationHomes[Swimmer->Actor]=Base;
        Box(FVector(300,0,0),FVector(20,500,300));
        Player->SetActorLocation(Base+FVector(600,0,0));
        FCollisionQueryParams EnemyQuery;
        EnemyQuery.AddIgnoredActor(Player);EnemyQuery.AddIgnoredActor(Swimmer->Actor);
        float Recall=3;FVector Known=Player->GetActorLocation();int32 Route=0;
        bool Clear=true;DamageCooldown=100.f;bHidden=false;bLight=true;
        for (int32 I=0;I<300;++I)
        {
            TickHunter(Swimmer->Actor,Swimmer->Light,Recall,Known,Route,.05f,true);
            Clear&=!GetWorld()->OverlapBlockingTestByChannel(Swimmer->Actor->GetActorLocation(),FQuat::Identity,ECC_Pawn,
                FCollisionShape::MakeSphere(59),EnemyQuery);
        }
        Check(Clear,TEXT("Pursuit never places the swimmer inside a blocking wall"));
        Check(FVector::Dist2D(Swimmer->Actor->GetActorLocation(),Player->GetActorLocation())<200,
            TEXT("The swimmer navigates around a blocking wall to reach its target"));
        Check(FVector::Dist2D(Swimmer->Actor->GetActorLocation(),Base)<=1001,
            TEXT("Wall avoidance stays within the swimmer habitat"));
        Swimmer->Actor->SetActorLocation(Original);
        ExplorationHomes[Swimmer->Actor]=OriginalHome;
        for (AActor* Actor:Fixtures) Actor->Destroy();
        Fixtures.Reset();
    }
    bSmokeTest=false;
    UE_LOG(LogTemp,Display,TEXT("MOVEMENT_TEST %s"),Passed ? TEXT("ALL PASSED") : TEXT("FAILED"));
    FPlatformMisc::RequestExitWithStatus(false,Passed ? 0 : 1);
}
