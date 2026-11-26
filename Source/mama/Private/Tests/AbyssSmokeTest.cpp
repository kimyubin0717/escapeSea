#include "Characters/AbyssDiver.h"
#include "Game/AbyssGameMode.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
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

void AAbyssGameMode::RunSmokeTest()
{
    APawn* P=UGameplayStatics::GetPlayerPawn(this,0);
    APlayerController* PC=UGameplayStatics::GetPlayerController(this,0);
    if (!P || !PC || !PC->PlayerInput) return;
    if (FParse::Param(FCommandLine::Get(), TEXT("AbyssMovementCheck")))
    {
        RunMovementCheck();
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("AbyssAuditCheck")))
    {
        RunAuditCheck();
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("AbyssExplorationCheck")))
    {
        RunExplorationCheck();
        return;
    }
    const bool bStreetscape=GetWorld()->GetMapName().Contains(TEXT("Streetscape"));
    int32 StationaryHunters=Hunter && Hunter->ActorHasTag(TEXT("AbyssBobbitWorm")) ? 1 : 0;
    for (const FAbyssHunter& Enemy:ExtraHunters) StationaryHunters+=Enemy.Actor->ActorHasTag(TEXT("AbyssBobbitWorm"));
    Hunter->SetActorLocation(FVector(3200,3200,100));
    Memory=0;
    for (FAbyssHunter& Enemy:ExtraHunters) { Enemy.Actor->SetActorLocation(FVector(3200,-3200,100)); Enemy.Memory=0; }
    auto Check=[&](bool bOK,const TCHAR* What)
    {
        UE_LOG(LogTemp,Display,TEXT("ABYSS_TEST %s: %s"),bOK ? TEXT("PASS") : TEXT("FAIL"),What);
        if (!bOK) { FPlatformMisc::RequestExitWithStatus(false,1); bSmokeTest=false; }
        return bOK;
    };
    if (Elapsed>65) { Check(false,TEXT("Gameplay integration timeout")); return; }
    if (FParse::Param(FCommandLine::Get(), TEXT("AbyssInputCheck")))
    {
        auto* Diver = CastChecked<AAbyssDiver>(P);
        if (SmokeStage == -3 && Elapsed > 1)
        {
            if (!Check(!bMenu, TEXT("Gameplay starts with input enabled"))) return;
            SmokeStart = P->GetActorLocation();
            P->SetActorRotation(FRotator::ZeroRotator);
            PC->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseX, IE_Axis, 55.f, 1));
            PC->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseY, IE_Axis, 40.f, 1));
            SmokeDeadline = Elapsed + .15f;
            SmokeStage = -4;
        }
        else if (SmokeStage == -4 && Elapsed > SmokeDeadline)
        {
            const FRotator CameraRotation = Diver->Camera->GetComponentRotation();
            if (!Check(FMath::Abs(FRotator::NormalizeAxis(CameraRotation.Yaw + 180.f)) > 5.f &&
                       FMath::Abs(CameraRotation.Pitch + 50.f) > 5.f,
                       TEXT("Mouse X and Y rotate the active camera without a mouse button"))) return;
            if (!Check(P->GetActorRotation().IsNearlyZero() && P->GetActorLocation().Equals(SmokeStart, 1.f),
                       TEXT("Mouse does not turn or move the diver"))) return;
            P->SetActorLocation(FVector(0, 0, 74.f));
            SmokeStart = P->GetActorLocation();
            PC->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Right, IE_Pressed, 1.f));
            SmokeDeadline = Elapsed + .2f;
            SmokeStage = -5;
        }
        else if (SmokeStage == -5 && Elapsed > SmokeDeadline)
        {
            PC->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Right, IE_Released, 0.f));
            if (!Check(Diver->SwimClip && Diver->CurrentClip == Diver->SwimClip && Diver->DiverMesh->IsPlaying(),
                       TEXT("Direction key plays the swimming animation"))) return;
            const FVector Displacement = P->GetActorLocation() - SmokeStart;
            const FVector CameraRight = FRotator(0, Diver->ViewYaw, 0).RotateVector(FVector::RightVector);
            if (!Check(FVector::DotProduct(Displacement.GetSafeNormal2D(), CameraRight) > .95f &&
                       FVector::DotProduct(P->GetActorForwardVector(), CameraRight) > .99f,
                       TEXT("Right arrow moves and faces the diver toward screen right"))) return;
            if (!Check(FMath::IsNearlyEqual(Diver->Camera->GetComponentRotation().Yaw, Diver->ViewYaw, .1f),
                       TEXT("Diver turning does not rotate the active camera"))) return;
            bSmokeTest = false;
            FPlatformMisc::RequestExitWithStatus(false, 0);
        }
        return;
    }
    if (SmokeStage==-3 && Elapsed>1)
    {
        const bool bHarborStart=GetWorld()->GetMapName().Contains(TEXT("Streetscape_Integrated"));
        if (bHarborStart)
        {
            if (!Check(FVector::Dist(P->GetActorLocation(),FVector(-560,19100,GetWorld()->GetMapName().EndsWith(TEXT("_Harbor_Mk2")) ? 74 : 274))<100,
                       TEXT("Diver starts at the service area beside the submarine"))) return;
            if (GetWorld()->GetMapName().Contains(TEXT("Mk2")))
            {
                TArray<AActor*> Hulls, Wrecks;
                UGameplayStatics::GetAllActorsWithTag(this,TEXT("AbyssMk2Hull"),Hulls);
                UGameplayStatics::GetAllActorsWithTag(this,TEXT("AbyssShipwreck"),Wrecks);
                if (!Check(RoamingKraken && Wrecks.Num()==1 && Hulls.Num()==1 &&
                    Hulls[0]->FindComponentByClass<UStaticMeshComponent>()->GetStaticMesh()->GetName()==TEXT("SM_Sub2") &&
                    FVector::Dist2D(RoamingKraken->GetActorLocation(),FVector(-17500,-17000,640))>100,
                    TEXT("White Mk2, shipwreck and roaming kraken active"))) return;
            }
            if (GetWorld()->GetMapName().EndsWith(TEXT("_Mk2")))
            {
                if (!Check(CreatureAnimations.Num() == 10+ExplorationHomes.Num()+StationaryHunters, TEXT("Patrol hunters, stationary worms, exploration residents and kraken have runtime animation sets"))) return;
                for (auto& Pair : CreatureAnimations)
                {
                    AActor* Creature = Pair.Key;
                    auto* Mesh = Creature->FindComponentByClass<USkeletalMeshComponent>();
                    auto& Clips = Pair.Value;
                    auto* Instance = Mesh ? Mesh->GetSingleNodeInstance() : nullptr;
                    UE_LOG(LogTemp, Display, TEXT("ABYSS_CREATURE_STATE %s model=%s material=%s instance=%d playing=%d clips=%s/%s/%s/%s"),
                        *GetNameSafe(Creature), *GetNameSafe(Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr),
                        *GetNameSafe(Mesh ? Mesh->GetMaterial(0) : nullptr), Instance != nullptr, Mesh && Mesh->IsPlaying(),
                        *GetNameSafe(Clips.Movement), *GetNameSafe(Clips.Idle), *GetNameSafe(Clips.Search), *GetNameSafe(Clips.Attack));
                    if (!Check(Instance && Mesh->GetSkeletalMeshAsset() && Mesh->GetMaterial(0) &&
                               Clips.Idle && Clips.Movement && Clips.Search && Clips.Attack && Mesh->IsPlaying(),
                               TEXT("Monster mesh, material and four behavior clips are active"))) return;
                    const FVector RootScale = Mesh->GetBoneTransform(0, FTransform::Identity).GetScale3D();
                    const FVector RefScale = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetRefBonePose()[0].GetScale3D();
                    UE_LOG(LogTemp, Display, TEXT("ABYSS_CREATURE %s %s root=%s ref=%s"), *GetNameSafe(Creature),
                           *GetNameSafe(Mesh->GetSkeletalMeshAsset()), *RootScale.ToString(), *RefScale.ToString());
                    if (!Check(RootScale.Equals(RefScale, .1f), TEXT("Animated monster retains reference scale"))) return;
                    const auto& Skeleton = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
                    TArray<FTransform> Initial = Mesh->GetComponentSpaceTransforms();
                    Instance->SetPosition(Clips.Movement->GetPlayLength() * .41f, false);
                    Mesh->RefreshBoneTransforms();
                    const auto& Animated = Mesh->GetComponentSpaceTransforms();
                    int32 MovingBones = 0;
                    for (int32 Bone = 1; Bone < FMath::Min(Initial.Num(), Animated.Num()); ++Bone)
                        if (!Animated[Bone].GetRotation().Equals(Initial[Bone].GetRotation(), .001f)) ++MovingBones;
                    UE_LOG(LogTemp, Display, TEXT("ABYSS_RIG %s reference_bones=%d moving_bones=%d"),
                           *GetNameSafe(Mesh->GetSkeletalMeshAsset()), Skeleton.GetNum(), MovingBones);
                    if (!Check(MovingBones >= 3, TEXT("Monster rig deforms several non-root bones at runtime"))) return;
                    UpdateCreatureAnimation(Creature, true, true, false);
                    if (!Check(Instance->GetCurrentAsset() == Clips.Search, TEXT("Monster plays search animation"))) return;
                    UpdateCreatureAnimation(Creature, true, false, true);
                    if (!Check(Instance->GetCurrentAsset() == Clips.Attack && Mesh->IsPlaying(),
                               TEXT("Monster plays its attack animation"))) return;
                    Clips.AttackEnds = 0.f;
                    UpdateCreatureAnimation(Creature, true, false, false);
                    if (!Check(Instance->GetCurrentAsset() == Clips.Movement, TEXT("Monster returns to its movement loop"))) return;
                }
            }
            if (RoamingKraken)
            {
                TArray<AActor*> Crawlers;
                UGameplayStatics::GetAllActorsWithTag(this,TEXT("AbyssBenthicHunter"),Crawlers);
                int32 Residents=0;
                for (const auto& Home:ExplorationHomes) Residents+=Home.Key->ActorHasTag(TEXT("AbyssBenthicHunter"));
                if (!Check(Crawlers.Num()==3+Residents+StationaryHunters,TEXT("Patrol isopods, exploration residents and benthic worms are active"))) return;
            }
            if (!Check(ExtraHunters.Num()==(RoamingKraken ? 8 : 5)+ExplorationHomes.Num()+StationaryHunters && Hunter->ActorHasTag(TEXT("AbyssPatrolCreature")) &&
                       ExtraHunters.ContainsByPredicate([](const FAbyssHunter& H){return !H.Actor->ActorHasTag(TEXT("AbyssPatrolCreature"));})==false,
                       TEXT("All authored creatures are bound to hunter AI"))) return;
        }
        else if (!Check(P->GetActorLocation().Z<200,TEXT("Diver spawns near the street, below the top-down camera"))) return;
        auto* Diver=CastChecked<AAbyssDiver>(P);
        if (!Check(Diver->IdleClip && Diver->CurrentClip==Diver->IdleClip && Diver->DiverMesh->IsPlaying(),TEXT("Idle swim animation playing"))) return;
        if (!Check(Diver->bAngledView && Diver->DiverMesh->GetSkeletalMeshAsset()->GetName()==TEXT("SK_Diver"),TEXT("Default quarter view possesses diver mesh"))) return;
        if (!Check(FVector::Dist(Diver->DiverMesh->GetSocketLocation(TEXT("hand_r")),Diver->DiverMesh->GetComponentLocation())>45.f,TEXT("Animated diver retains full body scale"))) return;
        const FName Elbow(TEXT("lowerarm_l"));
        const FReferenceSkeleton& DiverSkeleton = Diver->DiverMesh->GetSkeletalMeshAsset()->GetRefSkeleton();
        const FQuat BindElbow = DiverSkeleton.GetRefBonePose()[DiverSkeleton.FindBoneIndex(Elbow)].GetRotation();
        const FQuat ActiveElbow = Diver->DiverMesh->GetSocketTransform(Elbow, RTS_ParentBoneSpace).GetRotation();
        if (!Check(BindElbow.AngularDistance(ActiveElbow) > .25f, TEXT("Idle mesh elbow visibly departs from the bind pose"))) return;
        UE_LOG(LogTemp,Display,TEXT("DIVER_SCREEN mesh extent %s scale %s"),*Diver->DiverMesh->Bounds.BoxExtent.ToString(),*Diver->DiverMesh->GetComponentScale().ToString());
        if (!Check(Diver->HandLamp->GetComponentScale().Equals(Diver->DiverMesh->GetComponentScale(),.01f) && Diver->HandLamp->Bounds.BoxExtent.GetMax()<25.f,TEXT("Hand lamp retains handheld size during animation"))) return;
        Diver->Orbit(0,10);
        if (!Check(FMath::IsNearlyEqual(Diver->CameraBoom->GetComponentRotation().Pitch,-43.5,.1),TEXT("Vertical orbit changes quarter-view pitch"))) return;
        Diver->Orbit(40,1000);
        if (!Check(FMath::IsNearlyEqual(Diver->CameraBoom->GetComponentRotation().Pitch,-10.,.1),TEXT("Quarter view upper pitch limit"))) return;
        Diver->Orbit(0,-1000);
        if (!Check(FMath::IsNearlyEqual(Diver->CameraBoom->GetComponentRotation().Pitch,-85.,.1),TEXT("Quarter view lower pitch limit"))) return;
        if (!Check(FMath::IsNearlyEqual(Diver->ViewYaw,-154.f,.1f),TEXT("Mouse orbit changes camera azimuth"))) return;
        if (FParse::Param(FCommandLine::Get(), TEXT("AbyssLobbyCheck")))
        {
            int32 Checked = 0;
            FCollisionQueryParams Query;
            Query.AddIgnoredActor(P);
            const auto Probe = FCollisionShape::MakeSphere(36);
            for (FAbyssSite& Site : Sites)
            {
                if (!Site.Visual || !Site.Visual->ActorHasTag(TEXT("AbyssLobbySupply"))) continue;
                if (!Check(Site.InteriorRoof && Site.OpenedVisual, TEXT("Lobby supply has room and opened model"))) return;
                FVector Center, Extent;
                Site.InteriorRoof->GetActorBounds(false, Center, Extent);
                const FVector Forward = Site.InteriorRoof->GetActorForwardVector();
                const float Reach = FMath::Abs(Forward.X)*Extent.X + FMath::Abs(Forward.Y)*Extent.Y;
                const FVector Inside(Center.X, Center.Y, 74);
                const FVector Outside = Inside - Forward*(Reach+120);
                if (!Check(!GetWorld()->SweepTestByChannel(Outside, Inside, FQuat::Identity, ECC_Pawn, Probe, Query),
                        TEXT("Diver fits through the lobby entrance"))) return;
                bool bReachable = false;
                for (const FVector& Offset : {FVector(-70,0,0), FVector(70,0,0), FVector(0,-70,0), FVector(0,70,0)})
                {
                    const FVector Approach = FVector(Site.Position.X, Site.Position.Y, 74) + Offset;
                    if (GetWorld()->SweepTestByChannel(Inside, Approach, FQuat::Identity, ECC_Pawn, Probe, Query)) continue;
                    P->SetActorLocation(Approach);
                    const int32 Nearest = NearestSite();
                    if (Sites.IsValidIndex(Nearest) && &Sites[Nearest] == &Site)
                    {
                        bReachable = true;
                        break;
                    }
                }
                if (!bReachable) UE_LOG(LogTemp, Error, TEXT("Lobby supply unreachable: %s at %s"), *Site.InteriorRoof->GetName(), *Site.Position.ToString());
                if (!Check(bReachable, TEXT("Diver reaches the supply and can select it"))) return;
                UpdateBuilding();
                if (!Check(Site.InteriorRoof->IsHidden(), TEXT("Lobby cutaway reveals the supply"))) return;
                const int32 Reward = Site.CacheReward;
                const int32 Before = Inventory[Reward];
                Interact();
                if (!Check(Sites.IsValidIndex(SalvageSite) && &Sites[SalvageSite] == &Site, TEXT("Interact selects lobby supply"))) return;
                SalvageTime = (SalvageTarget+SalvageWindow/2)*SalvageDuration;
                SubmitSalvage();
                if (!Check(Site.bUsed && Inventory[Reward] == Before+1 && !Site.OpenedVisual->IsHidden(),
                        TEXT("Opening lobby supply yields one repair part"))) return;
                P->SetActorLocation(Outside);
                UpdateBuilding();
                if (!Check(!Site.InteriorRoof->IsHidden(), TEXT("Lobby exterior restores on exit"))) return;
                ++Checked;
            }
            if (!Check(Checked == 16, TEXT("All 16 lobby supplies verified"))) return;
            bSmokeTest = false;
            FPlatformMisc::RequestExitWithStatus(false, 0);
            return;
        }
        Diver->ViewYaw=0; Diver->Orbit(0);
        PC->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::V,IE_Pressed,1.f));
        P->SetActorLocation(GetWorld()->GetMapName().EndsWith(TEXT("_Harbor_Mk2"))
            ? FVector(-560,18000,74) : FVector(0,14900*CityScale,74));
        PC->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::D,IE_Pressed,1.f));
        SmokeDeadline=Elapsed+3.5f;
        SmokeStage=-2;
    }
    else if (SmokeStage==-2 && Elapsed>SmokeDeadline)
    {
        PC->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::D,IE_Released,0.f));
        auto* Diver=CastChecked<AAbyssDiver>(P);
        if (!Check(!Diver->bAngledView && FMath::IsNearlyEqual(Diver->CameraBoom->GetComponentRotation().Pitch,-90.,.1) &&
            Diver->SwimClip && Diver->CurrentClip==Diver->SwimClip && Diver->DiverMesh->IsPlaying(),TEXT("V switches camera and normal swimming clip plays"))) return;
        PC->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::V,IE_Released,0.f));
        Diver->Orbit(0,100);
        Diver->ToggleView();
        if (!Check(FMath::IsNearlyEqual(Diver->CameraBoom->GetComponentRotation().Pitch,-85.,.1),TEXT("Quarter view restores selected pitch after overhead view"))) return;
        Diver->ViewPitch=-50.f; Diver->Orbit(0);
        UE_LOG(LogTemp,Display,TEXT("ABYSS_CITY_PIER: %s"),*P->GetActorLocation().ToString());
        if (GetWorld()->GetMapName().EndsWith(TEXT("_Harbor_Mk2")))
        {
            if (!Check(P->GetActorLocation().Y>19100 && FMath::Abs(P->GetActorLocation().Z-74)<20,
                       TEXT("Diver swims continuously through the grounded service area"))) return;
        }
        else if (!Check(P->GetActorLocation().Y>15600*CityScale && P->GetActorLocation().Z>200*CityScale+60,TEXT("D input climbs the city pier access steps and follows the raised deck"))) return;
        P->SetActorLocation(FVector(0,-12000*CityScale,74));
        SmokeStage=0;
    }
    else if (SmokeStage==0 && Elapsed>1)
    {
        SmokeStart=P->GetActorLocation();
        SmokeOxygen=Oxygen;
        PC->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W,IE_Pressed,1.f));
        PC->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftShift,IE_Pressed,1.f));
        SmokeDeadline=Elapsed+1;
        SmokeStage=1;
    }
    else if (SmokeStage==1 && Elapsed>SmokeDeadline)
    {
        PC->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::W,IE_Released,0.f));
        PC->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftShift,IE_Released,0.f));
        auto* Swimming=CastChecked<AAbyssDiver>(P);
        if (!Check(Swimming->SprintClip && Swimming->CurrentClip==Swimming->SprintClip && Swimming->DiverMesh->IsPlaying(),TEXT("Fast swimming animation playing"))) return;
        if (!Check(P->GetActorLocation().X-SmokeStart.X>100,TEXT("W input moves diver north"))) return;
        UE_LOG(LogTemp,Display,TEXT("ABYSS_SPRINT: oxygen used=%f, sprint rate=%f, normal rate=%f"),SmokeOxygen-Oxygen,OxygenRate(true),OxygenRate(false));
        if (!Check(SmokeOxygen-Oxygen>1.5f && FMath::IsNearlyEqual(OxygenRate(true),3*OxygenRate(false),0.001f),TEXT("Shift movement consumes oxygen at triple normal rate"))) return;
        if (!Check(ExtraHunters.Num()==(RoamingKraken ? 8 : bStreetscape ? 5 : 2)+ExplorationHomes.Num()+StationaryHunters,TEXT("Expected number of patrol, resident and stationary hunters spawned"))) return;
        if (bImportedCity)
        {
            if (!Check(FMath::IsNearlyEqual(MapHalfExtent,(RoamingKraken ? 23650 : 24500)*CityScale) && Sites.Num()>=40 && OceanLight && Fog && FMath::Abs(P->GetActorLocation().Y)>1900,TEXT("Authored city loads with matching scaled bounds, lighting and mapped objectives"))) return;
            bool bFixedGround=false;
            for (TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)
            {
                const UStaticMeshComponent* Mesh=It->GetStaticMeshComponent();
                if (Mesh && Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetName()==TEXT("SM_SeabedBase"))
                    bFixedGround=It->GetActorTransform().Equals(FTransform(FRotator::ZeroRotator,FVector(0,0,-50*CityScale),FVector(500*CityScale,500*CityScale,CityScale)),0.1f);
            }
            if (!Check(bFixedGround,TEXT("The seabed remains level and fixed at its authored transform while the player moves"))) return;
            if (bStreetscape)
            {
                const FVector Return=P->GetActorLocation();
                int32 CacheParts[3]={};
                int32 CheckedCaches=0, CheckedInteriors=0, CheckedWarehouses=0;
                for (FAbyssSite& Site : Sites)
                {
                    if (Site.Kind!=FAbyssSite::LootCache || (Site.Visual && (Site.Visual->ActorHasTag(TEXT("AbyssLobbySupply")) || Site.Visual->ActorHasTag(TEXT("AbyssExplorationCache"))))) continue;
                    if (!Check(Site.Visual && Site.OpenedVisual && Site.OpenedVisual->IsHidden(),
                               TEXT("Cache has distinct closed and opened visuals"))) return;
                    const bool bWarehouse=Site.Visual->ActorHasTag(TEXT("AbyssWarehouseCache"));
                    const float TravelZ=bWarehouse ? Site.CacheAnchor.Z+70.f : 74.f;
                    if (Site.InteriorRoof)
                    {
                        FVector Center, Extent;
                        Site.InteriorRoof->GetActorBounds(false,Center,Extent);
                        const FVector Outside=bWarehouse ? FVector(Center.X,Center.Y-Extent.Y-150,TravelZ) :
                                                           FVector(Center.X-Extent.X-150,Center.Y,TravelZ);
                        const FVector Inside(Center.X,Center.Y,TravelZ);
                        FCollisionQueryParams Query; Query.AddIgnoredActor(P);
                        FHitResult Barrier;
                        const auto Probe=FCollisionShape::MakeSphere(36);
                        if (!Check(!GetWorld()->SweepSingleByChannel(Barrier,Outside,Inside,FQuat::Identity,ECC_Pawn,Probe,Query),
                                   bWarehouse ? TEXT("Warehouse entry is passable from the harbor approach") : TEXT("Diver fits through the street entrance"))) return;
                        const FVector Back=bWarehouse ? FVector(Center.X,Center.Y+Extent.Y+100,TravelZ) :
                                                       FVector(Center.X+Extent.X+100,Center.Y,TravelZ);
                        if (!Check(GetWorld()->SweepSingleByChannel(Barrier,Inside,Back,FQuat::Identity,ECC_Pawn,Probe,Query),
                                   TEXT("The rear wall still blocks the diver"))) return;
                        P->SetActorLocation(Outside); UpdateBuilding();
                        if (!Check(!Site.InteriorRoof->IsHidden(),TEXT("Roof is visible outside"))) return;
                        ++CheckedInteriors;
                        if (bWarehouse) ++CheckedWarehouses;
                    }
                    P->SetActorLocation(FVector(Site.Position.X-70,Site.Position.Y,TravelZ)); UpdateBuilding();
                    if (Site.InteriorRoof)
                    {
                        if (!Check(Site.InteriorRoof->IsHidden() && bInsideBuilding,TEXT("Top-down cutaway reveals the explored interior"))) return;
                        const FAbyssCityInterior* Room=CityInteriors.FindByPredicate([&](const FAbyssCityInterior& R){return R.Roof==Site.InteriorRoof;});
                        bool bClearWall=false;
                        if (Room && CityWallGlass && CityWallGlass->GetBlendMode()==BLEND_Translucent)
                            for (const FAbyssCityWall& Wall : Room->Walls)
                                for (int32 I=0;I<Wall.OriginalMaterials.Num();++I)
                                    bClearWall |= Wall.OriginalMaterials[I] && Wall.Component->GetMaterial(I)==CityWallGlass &&
                                        Wall.Component->GetCollisionEnabled()!=ECollisionEnabled::NoCollision;
                        if (!Check(bClearWall,TEXT("Interior walls are translucent with collision intact"))) return;
                    }
                    int32 Before[4];
                    for (int32 I=0; I<4; ++I) Before[I]=Inventory[I];
                    Interact(); SubmitSalvage();
                    if (!Check(!Site.bUsed && Site.OpenedVisual->IsHidden(),TEXT("Missed lock stays closed"))) return;
                    Interact();
                    if (!Check(SalvageSite!=INDEX_NONE,TEXT("Cache lock can be retried"))) return;
                    SalvageTime=(SalvageTarget+SalvageWindow/2)*SalvageDuration;
                    SubmitSalvage(); Interact();
                    for(int32 K=0;K<3;++K) CacheParts[K]+=Inventory[K]-Before[K];
                    int32 Granted=0;
                    for (int32 I=0; I<4; ++I) { Granted+=Inventory[I]-Before[I]; Inventory[I]=Before[I]; }
                    if (!Check(Granted==1 && Site.bUsed && Site.Visual->IsHidden() && !Site.OpenedVisual->IsHidden(),
                               TEXT("Cache grants one item, opens, and cannot grant loot twice"))) return;
                    if (Site.InteriorRoof)
                    {
                        P->SetActorLocation(Return); UpdateBuilding();
                        const FAbyssCityInterior* Room=CityInteriors.FindByPredicate([&](const FAbyssCityInterior& R){return R.Roof==Site.InteriorRoof;});
                        bool bRestored=Room && !Room->Roof->IsHidden();
                        if (Room)
                            for (const FAbyssCityWall& Wall : Room->Walls)
                                for (int32 I=0;I<Wall.OriginalMaterials.Num();++I)
                                    if (Wall.OriginalMaterials[I]) bRestored &= Wall.Component->GetMaterial(I)==Wall.OriginalMaterials[I];
                        if (!Check(bRestored,TEXT("Leaving restores the original roof and wall materials"))) return;
                    }
                    ++CheckedCaches;
                }
                P->SetActorLocation(Return); UpdateBuilding();
                if (!Check((CacheParts[0]==2 && CacheParts[1]==2 && CacheParts[2]==2) || GetWorld()->GetMapName().EndsWith(TEXT("_Mk2")),TEXT("Cache rewards alone cover all three repairs"))) return;
                if (!Check(CheckedCaches==6 && CheckedInteriors==4 && CheckedWarehouses==2,TEXT("Four explorable interiors and two street caches verified"))) return;
            }
            if (CityScale<1)
            {
                TArray<AActor*> Blocks;
                UGameplayStatics::GetAllActorsWithTag(this,TEXT("CompactSolidBuilding"),Blocks);
                if (!Check(Blocks.Num()>=25,TEXT("Compact city uses individual solid building models"))) return;
                if (!Check(GetWorld()->OverlapBlockingTestByChannel(Blocks[0]->GetActorLocation(),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(36)),TEXT("Building interiors are solid collision volumes"))) return;
            }
        }
        else
        {
        FVector Outside=P->GetActorLocation();
        P->SetActorLocation(BuildingCenter+FVector(0,0,70)); UpdateBuilding();
        if (!Check(bInsideBuilding && BuildingGlass && BuildingWalls[0]->GetMaterial(0)==BuildingGlass && BuildingWalls[0]->GetCollisionEnabled()!=ECollisionEnabled::NoCollision,TEXT("Entering building makes walls transparent while preserving collision"))) return;
        P->SetActorLocation(Outside); UpdateBuilding();
        if (!Check(!bInsideBuilding && BuildingWalls[0]->GetMaterial(0)==BuildingMaterials[0],TEXT("Leaving building restores opaque exterior"))) return;
        }
        AAbyssDiver* Diver=Cast<AAbyssDiver>(P);
        if (!Check(Diver && PC->GetViewTarget()==P && Diver->Camera->GetAttachParent()==Diver->CameraBoom && Diver->CameraBoom->GetAttachParent()==P->GetRootComponent(),TEXT("Third-person top-down camera is attached to the possessed player through a fixed-heading boom"))) return;
        const FVector CameraOffset=Diver->Camera->GetComponentLocation()-P->GetActorLocation();
        const FVector ExpectedOffset=-FRotator(-50,0,0).Vector()*1800 + FVector(0,0,55);
        if (!Check(CameraOffset.Equals(ExpectedOffset,0.1f) && Diver->Camera->ProjectionMode==ECameraProjectionMode::Perspective,TEXT("Camera follows player movement while preserving the third-person top-down perspective"))) return;
        if (RoamingKraken)
        {
            const FVector Return=P->GetActorLocation();
            int32 Tested=0;
            for (const auto& Room : CityInteriors)
            {
                if (!Room.UpperStorey) continue;
                const FVector Forward=Room.Roof->GetActorForwardVector();
                const float Reach=FMath::Abs(Forward.X)*Room.Extent.X+FMath::Abs(Forward.Y)*Room.Extent.Y;
                const FVector Middle(Room.Center.X,Room.Center.Y,74);
                const FVector Outside=Middle-Forward*(Reach+120);
                FCollisionQueryParams Query; Query.AddIgnoredActor(P);
                FHitResult Hit;
                if (GetWorld()->SweepSingleByChannel(Hit,Outside,Middle,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(36),Query))
                    UE_LOG(LogTemp,Error,TEXT("LOBBY_BLOCKED %s by %s"),*Room.Roof->GetName(),*GetNameSafe(Hit.GetActor()));
                if (!Check(!Hit.bBlockingHit,TEXT("Street entrance passes diver collision"))) return;
                P->SetActorLocation(Middle); UpdateBuilding();
                if (!Check(Room.bInside && Room.UpperStorey->IsHidden() && Room.Roof->IsHidden(),TEXT("Upper facade and ceiling clear inside lobby"))) return;
                P->SetActorLocation(Outside); UpdateBuilding();
                if (!Check(!Room.bInside && !Room.UpperStorey->IsHidden(),TEXT("Lobby exterior restored after exit"))) return;
                ++Tested;
            }
            if (!Check(Tested==16 && CityInteriors.Num()>=21,TEXT("Sixteen converted lobbies and five original interiors"))) return;
            P->SetActorLocation(Return); UpdateBuilding();
        }
        const FRotator BeforeAim=P->GetActorRotation();
        P->SetActorRotation(FRotator(0,137,0));
        if (!Check(Diver->Camera->GetComponentRotation().Quaternion().Equals(FRotator(-50,0,0).Quaternion(),0.001f) && (Diver->Camera->GetComponentLocation()-P->GetActorLocation()).Equals(CameraOffset,0.1f),TEXT("Aiming keeps camera heading and offset fixed without rotating or tilting the map"))) return;
        P->SetActorRotation(BeforeAim);
        if (!Check(Diver && Diver->Bubbles.Num()==24 && Diver->BubbleBursts>0,TEXT("Breathing periodically emits bubbles from a bounded pool"))) return;
        BatteryRemaining=10; bLight=false;
        AdvanceBattery(1);
        if (!Check(BatteryRemaining==10,TEXT("Switched-off flashlight conserves battery"))) return;
        bLight=true; bHidden=true; AdvanceBattery(1);
        if (!Check(BatteryRemaining==10,TEXT("Hidden flashlight conserves battery"))) return;
        bHidden=false; AdvanceBattery(1);
        if (!Check(BatteryRemaining==9,TEXT("Active flashlight consumes battery time"))) return;
        BatteryRemaining=0.01f; AdvanceBattery(0.1f); ToggleFlashlight();
        if (!Check(BatteryRemaining==0 && !bLight && !Diver->Flashlight->IsVisible() && Diver->DiveGlow->Intensity==12,TEXT("Empty battery disables flashlight and dims local light; cannot toggle back on"))) return;
        if (bImportedCity)
        {
            const FVector Return = P->GetActorLocation();
            const float SavedOxygen = Oxygen;
            int32 AirDrops = 0, Batteries = 0, SupplySlots = 0;
            for (const auto& Site : Sites)
            {
                SupplySlots += Site.Kind == 3;
                AirDrops += Site.ResourceDrop == EAbyssResourceDrop::Oxygen;
                Batteries += Site.ResourceDrop == EAbyssResourceDrop::Battery;
            }
            const int32 Expected = FMath::Min(8, SupplySlots/3);
            if (!Check(OxygenCapacity == 200.f && AirDrops == Expected && Batteries == Expected,
                       TEXT("Doubled oxygen capacity and correct randomized supply counts"))) return;
            const auto ReadLayout = [&]()
            {
                TArray<EAbyssResourceDrop> Layout;
                for (const auto& Site : Sites)
                    if (Site.Kind == 3) Layout.Add(Site.ResourceDrop);
                return Layout;
            };
            const auto OriginalLayout = ReadLayout();
            SetResourceDropLayout(CacheLayoutSeed ^ 0x3493771);
            const bool bVaries = ReadLayout() != OriginalLayout;
            SetResourceDropLayout(CacheLayoutSeed);
            if (!Check(bVaries && ReadLayout() == OriginalLayout,
                       TEXT("Resource locations vary by seed and restore deterministically"))) return;
            const auto* Air = Sites.FindByPredicate([](const FAbyssSite& Site)
            {
                return Site.ResourceDrop == EAbyssResourceDrop::Oxygen;
            });
            const auto* Battery = Sites.FindByPredicate([](const FAbyssSite& Site)
            {
                return Site.ResourceDrop == EAbyssResourceDrop::Battery;
            });
            if (!Check(Air && Battery && Battery->BatteryVisual && Battery->BatteryVisual->IsVisible(),
                       TEXT("Supply drops have an oxygen tank and battery model"))) return;

            P->SetActorLocation(Air->Position+FVector(0,0,70));
            Oxygen=OxygenCapacity;
            Interact();
            if (!Check(!Air->bUsed, TEXT("Full oxygen preserves the pickup"))) return;
            Oxygen=OxygenCapacity-20.f;
            Interact();
            if (!Check(Air->bUsed && Oxygen==OxygenCapacity && Air->Visual->IsHidden(),
                       TEXT("Oxygen pickup recharges immediately and caps at 200"))) return;

            P->SetActorLocation(Battery->Position+FVector(0,0,70));
            BatteryRemaining=0.f;
            Interact();
            if (!Check(Battery->bUsed && BatteryRemaining==BatteryRefill && Battery->Visual->IsHidden(),
                       TEXT("Battery pickup recharges an empty flashlight"))) return;
            ToggleFlashlight();
            if (!Check(bLight, TEXT("A recharged flashlight can be switched back on"))) return;
            Interact();
            if (!Check(BatteryRemaining==BatteryRefill, TEXT("Used battery cannot recharge twice"))) return;
            Oxygen=SavedOxygen;
            P->SetActorLocation(Return);
        }
        BatteryRemaining=FlashlightDuration; bLight=true;
        const int32 TestSite=Sites.IndexOfByPredicate([](const FAbyssSite& Site){return Site.Kind==0;});
        P->SetActorLocation(Sites[TestSite].Position+FVector(0,0,70));
        Hunter->SetActorLocation(Sites[TestSite].Position+FVector(900,0,0));
        ExtraHunters[0].Actor->SetActorLocation(Sites[TestSite].Position+FVector(4000,0,0));
        const float DistantMemory=ExtraHunters[0].Memory;
        Interact(); SubmitSalvage();
        if (!Check(ExtraHunters[0].Memory==DistantMemory,TEXT("Distant hunter ignores lock noise"))) return;
        if (!Check(Inventory[0]==0 && !Sites[TestSite].bUsed && bChasing && Memory==18 && FVector::Dist2D(LastKnown,Sites[TestSite].Position)<1,TEXT("Early timing failure keeps part and alerts hunter to salvage location"))) return;
        Interact(); AdvanceSalvage(SalvageDuration+0.1f);
        if (!Check(SalvageSite==INDEX_NONE && Inventory[0]==0 && !Sites[TestSite].bUsed,TEXT("Timing check timeout fails without granting or consuming part"))) return;
        Interact(); P->SetActorLocation(Sites[TestSite].Position+FVector(400,0,70)); AdvanceSalvage(0.1f);
        if (!Check(SalvageSite==INDEX_NONE && Memory==18,TEXT("Leaving salvage terrain cancels check and makes noise"))) return;
        for (const FAbyssSite& Site:Sites)
        {
            if (Site.Kind<3 && Inventory[Site.Kind]<2)
            {
                P->SetActorLocation(Site.Position+FVector(0,0,70)); Interact();
                if (!Check(SalvageSite!=INDEX_NONE && !Site.bUsed,TEXT("Salvage interaction requires timing check before giving item"))) return;
                SalvageTime=(SalvageTarget+SalvageWindow/2)*SalvageDuration;
                SubmitSalvage();
            }
        }
        if (!Check(Inventory[0]==2&&Inventory[1]==2&&Inventory[2]==2,TEXT("Collect 6 regional parts into inventory"))) return;
        int32 OldTanks=Inventory[3];
        for (const FAbyssSite& Site:Sites) if (Site.Kind==3 && Site.ResourceDrop==EAbyssResourceDrop::ReserveTank)
        { P->SetActorLocation(Site.Position+FVector(0,0,70)); Interact(); break; }
        Oxygen=20; UseOxygen();
        if (!Check(Inventory[3]==OldTanks && FMath::IsNearlyEqual(Oxygen,75.f),TEXT("Collect tank then consume it to replenish oxygen"))) return;
        for (const FAbyssSite& Site:Sites) if (Site.Kind==4)
        { P->SetActorLocation(Site.Position+FVector(0,0,70)); Interact(); break; }
        if (!Check(bHidden,TEXT("Kelp interaction enables hiding"))) return;
        Interact();
        if (!Check(!bHidden,TEXT("Kelp interaction exits hiding"))) return;
        P->SetActorLocation(Sites[0].Position+FVector(0,0,70));
        if (bWelderRequired)
        {
            Inventory[0]=2; Interact();
            if (!Check(!bHasWelder && RepairSite==INDEX_NONE,TEXT("Materials alone cannot start repairs"))) return;
            for (int32 Kind : {FAbyssSite::Fins, FAbyssSite::Welder})
            {
                int32 Id=Sites.IndexOfByPredicate([Kind](const FAbyssSite& S){return S.Kind==Kind;});
                if (!Check(Id!=INDEX_NONE,TEXT("Harbor tool spawned"))) return;
                P->SetActorLocation(Sites[Id].Position);
                Interact();
                if (!Check(Sites[Id].bUsed && Sites[Id].Visual->IsHidden(),TEXT("Tool collected through interact"))) return;
            }
            if (!Check(bHasWelder && FinsRemaining==60 && SwimSpeed(false)==567.f && SwimSpeed(true)==918.f,TEXT("Welder acquired and fins boost both speeds for 60 seconds"))) return;
            FinsRemaining=.01f;
            P->SetActorLocation(Sites[0].Position+FVector(0,0,70));
        }
        Inventory[0]=2;
        P->SetActorLocation(Sites[1].Position+FVector(0,0,70));
        Interact();
        if (!Check(RepairSite==INDEX_NONE && Inventory[0]==2,TEXT("Hull terminal rejects out-of-order repair without consuming power cells"))) return;
        P->SetActorLocation(Sites[0].Position+FVector(0,0,70));
        Inventory[0]=1; Interact();
        if (!Check(RepairSite==INDEX_NONE && MissingPart==0 && bInventory,TEXT("Missing parts block stage and highlight inventory"))) return;
        Inventory[0]=2;
        SmokeDeadline=Elapsed+1;
        SmokeStage=2;
    }
    else if (SmokeStage==2 && Elapsed>SmokeDeadline)
    {
        if (!Check(FinsRemaining==0 && SwimSpeed(false)==378 && SwimSpeed(true)==612,TEXT("Expired fins restore normal speed"))) return;
        if (!Check(NextRepairStage()==0 && !Repaired[0],TEXT("Repair stages begin locked in sequence"))) return;
        Interact();
        SmokeStage=3;
    }
    else if (SmokeStage==3)
    {
        if (Repaired[SmokeRepairStage])
        {
            if (!Check(Inventory[SmokeRepairStage]==0 && NextRepairStage()==SmokeRepairStage+1,TEXT("E interaction consumes parts automatically and unlocks next repair stage"))) return;
            ++SmokeRepairStage;
            if (SmokeRepairStage<3) { P->SetActorLocation(Sites[SmokeRepairStage].Position+FVector(0,0,70)); Interact(); }
            else
            {
                P->SetActorLocation(Sites[3].Position+FVector(0,0,70));
                Interact(); SmokeStage=4;
            }
        }
    }
    else if (SmokeStage==4)
    {
        if (!Check(bEnded && bWon && Repaired[0]&&Repaired[1]&&Repaired[2],TEXT("All three repairs unlock submarine escape and victory"))) return;
        if (RoamingKraken)
        {
            int32 Hinges=0, Restored=0;
            for (TActorIterator<AStaticMeshActor> It(GetWorld());It;++It)
            {
                const UStaticMesh* Mesh=It->GetStaticMeshComponent()->GetStaticMesh();
                if (!Mesh) continue;
                const FString Name=Mesh->GetName();
                if (It->ActorHasTag(TEXT("AbyssMk2Hull")) || It->ActorHasTag(TEXT("AbyssMk2Cabin")))
                    Restored+=Name.EndsWith(TEXT("_Repaired"));
                if (It->GetAttachParentActor() && (Name==TEXT("SM_Sub_BayDoor") || Name==TEXT("SM_Sub_HatchLid") ||
                    Name==TEXT("SM_Sub2_LockerDoor") || Name==TEXT("SM_Sub2_PowerLever")))
                {
                    if (!Check(It->GetActorScale3D().Equals(FVector(1.7),.01),TEXT("Repaired vessel hardware retains real-world scale"))) return;
                    ++Hinges;
                }
            }
            if (!Check(Hinges==4 && Restored==2,TEXT("All four fittings and both repaired meshes verified at escape"))) return;
        }
        bEnded=false; bWon=false;
        DamageDiver(35); DamageDiver(35);
        if (!Check(Health==65,TEXT("Monster damage reduces health with hit cooldown"))) return;
        Oxygen=0.0001; SmokeStage=5;
    }
    else if (SmokeStage==5)
    {
        if (!Check(!bEnded && Oxygen==0 && Health>0 && Health<65,TEXT("Oxygen depletion gradually drains health without instant death"))) return;
        const float HealthBeforeRecovery=Health;
        ++Inventory[3]; UseOxygen();
        if (!Check(Oxygen>0 && Health==HealthBeforeRecovery,TEXT("Emergency oxygen restores breathing without resetting health"))) return;
        SmokeOxygen=Health; SmokeStage=6;
    }
    else if (SmokeStage==6)
    {
        if (!Check(!bEnded && Health==SmokeOxygen,TEXT("Health stops draining after oxygen recovery"))) return;
        Oxygen=0; Health=0.0001f; SmokeStage=7;
    }
    else if (SmokeStage==7)
    {
        if (!Check(bEnded && !bWon && Oxygen==0 && Health==0,TEXT("Suffocation triggers loss only after health reaches zero"))) return;
        UE_LOG(LogTemp,Display,TEXT("ABYSS_TEST ALL PASSED"));
        bSmokeTest=false;
        FPlatformMisc::RequestExitWithStatus(false,0);
    }
}

