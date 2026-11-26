#include "Game/AbyssGameMode.h"
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
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"

namespace
{
bool FindSalvagePlacement(UWorld* World, const FVector& Authored, FVector& Placement)
{
    // Check the model footprint and the standing height of the diver at the pickup.
    constexpr float Spacing = 150.f;
    for (int32 Ring = 0; Ring <= 4; ++Ring)
    {
        const int32 Samples = Ring ? 8 : 1;
        for (int32 Direction = 0; Direction < Samples; ++Direction)
        {
            const float Angle = Direction*PI/4.f;
            const FVector Point = Authored + Ring*Spacing*FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0);
            FHitResult Floor;
            if (!World->LineTraceSingleByChannel(Floor, Point+FVector(0,0,105), Point-FVector(0,0,500), ECC_WorldStatic) ||
                Floor.ImpactNormal.Z < .7f) continue;
            const FVector Standing = Floor.ImpactPoint+FVector(0,0,70);
            if (World->OverlapBlockingTestByChannel(Standing, FQuat::Identity, ECC_Pawn,
                    FCollisionShape::MakeSphere(45))) continue;
            Placement = Floor.ImpactPoint;
            return true;
        }
    }
    return false;
}
void DressSalvageSites(TArray<FAbyssSite>& Sites)
{
    const TArray<TArray<FString>> Kits[]={
        {{TEXT("Battery"),TEXT("CopperWire")},{TEXT("CircuitBoard"),TEXT("CableSpool")},{TEXT("FuseModule")}},
        {{TEXT("HullPatch"),TEXT("SealantCan")},{TEXT("Sub2_HullPatch"),TEXT("GlassPowder"),TEXT("DomeGlass")},
         {TEXT("DomeShard_1"),TEXT("DomeShard_2"),TEXT("DomeShard_3"),TEXT("DomeShard_4"),TEXT("DomeShard_5"),TEXT("DomeShard_6")}},
        {{TEXT("MotorRotor"),TEXT("RepairGear")},{TEXT("RepairPipe"),TEXT("PropBlade")},{TEXT("Sub2_PropBlade")}},
        {{TEXT("ScubaTank")},{TEXT("ScubaTank"),TEXT("TankBand")},{TEXT("ScubaTank"),TEXT("ScrubberCartridge")}}
    };
    int32 Counts[4]={};
    for (FAbyssSite& Site : Sites)
    {
        if (Site.Kind<0 || Site.Kind>=4 || !Site.Visual) continue;
        const auto& Family=Kits[Site.Kind];
        const auto& Parts=Family[Counts[Site.Kind]++ % Family.Num()];
        auto* Root=CastChecked<UStaticMeshComponent>(Site.Visual->GetRootComponent());
        Root->SetStaticMesh(nullptr);
        Root->SetWorldScale3D(FVector::OneVector);
        for (int32 I=0;I<Parts.Num();++I)
        {
            const FString Path=TEXT("/Game/IntegratedCity/Salvage/Meshes/SM_")+Parts[I];
            auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path);
            if (!ensureMsgf(Mesh,TEXT("Missing salvage model %s"),*Path)) continue;
            auto* Part=NewObject<UStaticMeshComponent>(Site.Visual);
            Part->SetupAttachment(Root);
            Part->SetStaticMesh(Mesh);
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            const float X=(I%3-(FMath::Min(Parts.Num(),3)-1)*.5f)*34.f;
            const float Y=(I/3)*34.f;
            Part->SetRelativeLocation(FVector(X,Y,(Site.Kind<3 ? -9.f : -25.f)-Mesh->GetBoundingBox().Min.Z));
            Part->RegisterComponent();
        }
    }
    UE_LOG(LogTemp,Display,TEXT("DIVE_KIT modeled sites power=%d hull=%d drive=%d oxygen=%d"),Counts[0],Counts[1],Counts[2],Counts[3]);
}
}
UStaticMeshComponent* AAbyssGameMode::Shape(const FVector& Position, const FVector& Scale, const FLinearColor& Color, bool bSolid, const FString& Mesh)
{
    AActor* A = GetWorld()->SpawnActor<AActor>(Position, FRotator::ZeroRotator);
    UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A);
    A->SetRootComponent(C);
    C->SetMobility(EComponentMobility::Movable);
    C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), *Mesh, *Mesh)));
    C->SetCollisionEnabled(bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    C->SetCollisionResponseToAllChannels(ECR_Block);
    C->RegisterComponent();
    A->SetActorLocation(Abyss::Map(Position));
    A->SetActorScale3D(FVector(Scale.X*Abyss::MapScale,Scale.Y*Abyss::MapScale,Scale.Z));
    if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(Base, A);
        Mat->SetVectorParameterValue(TEXT("Color"), Color);
        C->SetMaterial(0, Mat);
    }
    return C;
}

AActor* AAbyssGameMode::Label(const FVector& Position, const FString& Text, const FColor& Color, float Size)
{
    AActor* A = GetWorld()->SpawnActor<AActor>(Position, FRotator::ZeroRotator);
    UTextRenderComponent* T = NewObject<UTextRenderComponent>(A);
    A->SetRootComponent(T);
    T->SetText(FText::FromString(Text));
    T->SetTextRenderColor(Color);
    T->SetWorldSize(Size*0.8f);
    T->SetHorizontalAlignment(EHTA_Center);
    T->RegisterComponent();
    A->SetActorLocation(Abyss::Map(Position));
    A->SetActorRotation(FRotator(68, 180, 0));
    T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    return A;
}

void AAbyssGameMode::AddSite(const FVector& Position, int32 Kind, const FString& Name)
{
    FAbyssSite S;
    S.Position = Abyss::Map(Position);
    S.Kind = Kind;
    S.Name = Name;
    if (Kind<3)
        Shape(Position+FVector(0,0,8),FVector(1.25,1.25,0.16),FLinearColor(0.08,0.18,0.22),false,TEXT("Cylinder"));
    FLinearColor Color = Kind == 3 ? FLinearColor(0.15, 1, 0.55) : (Kind == 4 ? FLinearColor(0.04, 0.4, 0.15) : Abyss::Gold);
    if (Kind >= 5) Color = Abyss::Teal;
    UStaticMeshComponent* C = Shape(Position + FVector(0,0,25), Kind == 4 ? FVector(1.8,1.8,0.2) : FVector(0.45,0.45,0.45), Color, false, Kind == 3 ? TEXT("Cylinder") : TEXT("Cube"));
    S.Visual = C->GetOwner();
    if (bImportedCity && Kind>=5 && Kind<=8) C->SetVisibility(false);
    UPointLightComponent* L = NewObject<UPointLightComponent>(S.Visual);
    L->SetupAttachment(C);
    L->SetRelativeLocation(FVector(0,0,150));
    L->SetIntensity(Kind >= 5 ? 350 : 1000);
    L->SetLightColor(Color);
    L->SetAttenuationRadius(400);
    L->RegisterComponent();
    if (Kind != 4) S.LabelVisual = Label(Position + FVector(0,0,140), Name, Color.ToFColor(true), Kind >= 5 ? 38 : 29);
    Sites.Add(S);
}

void AAbyssGameMode::BuildSeafloor()
{
    bImportedCity = GetWorld()->GetMapName().Contains(TEXT("LV_AbyssalCity_Dense"));
    if (bImportedCity) { BuildImportedCity(); return; }
    Shape(FVector(0,0,-70), FVector(74,74,1), FLinearColor(0.035,0.08,0.11));
    // Four flooded streets meet at the escape submarine. All objectives remain reachable.
    for (int32 I=-3; I<=3; ++I)
    {
        Shape(FVector(I*1000,0,-14), FVector(0.08,70,0.03), Abyss::Teal, false);
        Shape(FVector(0,I*1000,-14), FVector(70,0.08,0.03), Abyss::Teal, false);
    }
    FRandomStream Random(9271);
    for (int32 X=-3; X<=3; ++X) for (int32 Y=-3; Y<=3; ++Y)
    {
        if ((FMath::Abs(X)<=1 && FMath::Abs(Y)<=1) || X==0 || Y==0) continue;
        FVector P(X*880.f + 300, Y*880.f + 260, 0);
        if (FMath::Abs(P.X+1700)<950 && FMath::Abs(P.Y-900)<850) continue;
        float H = Random.FRandRange(160,410);
        Shape(P + FVector(0,0,H/2), FVector(Random.FRandRange(2,3.3), Random.FRandRange(2,3.3), H/100), FLinearColor(0.07,0.12,0.16));
        for (int32 W=-1; W<=1; ++W)
            Shape(P + FVector(-170,W*64,100), FVector(0.05,0.32,0.65), Abyss::Teal, false);
        Shape(P + FVector(0,0,H+8), FVector(3.5,3.5,0.15), FLinearColor(0.12,0.18,0.2), false);
    }
    for (int32 I=0; I<65; ++I)
    {
        FVector P(Random.FRandRange(-3300,3300), Random.FRandRange(-3300,3300), -4);
        if (FMath::Abs(P.X)<340 || FMath::Abs(P.Y)<340) continue;
        Shape(P, FVector(Random.FRandRange(0.5,1.5), Random.FRandRange(0.5,1.5), 0.4), FLinearColor(0.08,0.16,0.17), false, TEXT("Sphere"));
    }
    // Submarine silhouette, conning tower, portholes, propeller and docking pad.
    Shape(FVector(-300,0,0), FVector(12,9,0.12), FLinearColor(0.1,0.16,0.2), false, TEXT("Cylinder"));
    Shape(FVector(-470,0,155), FVector(7.5,2.8,2.7), FLinearColor(0.12,0.25,0.29), true, TEXT("Sphere"));
    Shape(FVector(-400,0,320), FVector(1.5,1.4,1.9), FLinearColor(0.1,0.2,0.24), true, TEXT("Cylinder"));
    Shape(FVector(-400,0,440), FVector(0.2,0.2,1.2), Abyss::Gold, false, TEXT("Cylinder"));
    for (int32 I=0; I<4; ++I) Shape(FVector(-700+I*135, -132,185), FVector(0.5,0.18,0.5), Abyss::Teal, false, TEXT("Sphere"));
    Shape(FVector(-870,0,150), FVector(0.2,3.4,0.35), Abyss::Gold, false);
    Label(FVector(-500,0,530), TEXT("N A U T I L U S  /  0 7"), FColor(130,220,230), 48);
    AddSite(FVector(50,-380,0),5,TEXT("SERVICE 01"));
    AddSite(FVector(50,0,0),6,TEXT("SERVICE 02"));
    AddSite(FVector(50,380,0),7,TEXT("SERVICE 03"));
    AddSite(FVector(-370,420,0),8,TEXT("ESCAPE HATCH"));
    Label(FVector(1850,-1850,80), TEXT("01 / FLOODED LAB"), FColor(80,200,235), 64);
    Label(FVector(-2050,-1900,80), TEXT("02 / SHIP GRAVEYARD"), FColor(235,175,80), 64);
    Label(FVector(1750,2050,80), TEXT("03 / PUMP STATION"), FColor(140,225,165), 64);
    const FVector Centers[] = {FVector(2000,-1800,0), FVector(-2000,-1800,0), FVector(1800,1800,0)};
    for (int32 K=0; K<3; ++K)
    {
        for (int32 J=0; J<3; ++J) AddSite(Centers[K]+FVector((J-1)*250, J%2*270, 0), K, Abyss::Items[K]);
        AddSite(Centers[K]+FVector(-380,450,0),3,TEXT("OXYGEN"));
        AddSite(Centers[K]+FVector(380,-440,0),3,TEXT("OXYGEN"));
        Shape(Centers[K]+FVector(0,520,90), FVector(5,0.35,1.8), FLinearColor(0.15,0.24,0.27));
    }
    const FVector Hides[] = {FVector(800,-800,0), FVector(-850,-700,0), FVector(700,1000,0), FVector(-1500,950,0), FVector(2400,750,0), FVector(100,-2200,0)};
    for (const FVector& P : Hides)
    {
        AddSite(P,4,TEXT("KELP / HIDE"));
        for (int32 I=0; I<9; ++I)
        {
            FVector Offset(Random.FRandRange(-130,130), Random.FRandRange(-130,130), 110);
            UStaticMeshComponent* Kelp = Shape(P+Offset,FVector(0.12,0.42,Random.FRandRange(2,3.5)), FLinearColor(0.02,0.3,0.19),false);
            Kelp->GetOwner()->SetActorRotation(FRotator(Random.FRandRange(-20,20),Random.FRandRange(0,180),0));
        }
    }
    AddSite(FVector(480,650,0),3,TEXT("OXYGEN"));
    Hunter = Shape(FVector(-2300,1800,100),FVector(1.6,0.8,0.65),FLinearColor(1.f,0.005f,0.008f),false,TEXT("Sphere"))->GetOwner();
    HunterLight = NewObject<UPointLightComponent>(Hunter);
    HunterLight->SetupAttachment(Hunter->GetRootComponent());
    HunterLight->SetRelativeLocation(FVector(0,0,90));
    HunterLight->SetIntensity(6500);
    HunterLight->SetAttenuationRadius(650);
    HunterLight->SetLightColor(FLinearColor(1,0.015,0.03));
    HunterLight->RegisterComponent();
    for (int32 I=0;I<2;++I)
    {
        FAbyssHunter Enemy;
        Enemy.Actor=Shape(I==0 ? FVector(2800,-2600,100) : FVector(-2700,-2700,100),FVector(1.6,0.8,0.65),FLinearColor(1,0.005,0.008),false,TEXT("Sphere"))->GetOwner();
        Enemy.Light=NewObject<UPointLightComponent>(Enemy.Actor);
        Enemy.Light->SetupAttachment(Enemy.Actor->GetRootComponent());
        Enemy.Light->SetIntensity(4500); Enemy.Light->SetAttenuationRadius(500);
        Enemy.Light->SetLightColor(FLinearColor(1,0.005,0.008)); Enemy.Light->RegisterComponent();
        Enemy.PatrolIndex=I+1; ExtraHunters.Add(Enemy);
    }
    auto Wall=[&](FVector Pos,FVector Scale)
    {
        UStaticMeshComponent* C=Shape(Pos,Scale,FLinearColor(0.08,0.19,0.26));
        BuildingWalls.Add(C); BuildingMaterials.Add(C->GetMaterial(0));
    };
    Shape(FVector(-1700,900,-6),FVector(14,12,0.2),FLinearColor(0.05,0.25,0.3),false);
    Wall(FVector(-2400,900,180),FVector(0.35,12,3.6));
    Wall(FVector(-1700,300,180),FVector(14,0.35,3.6));
    Wall(FVector(-1700,1500,180),FVector(14,0.35,3.6));
    // A 220cm entrance is left in the north wall; collision stays active when transparent.
    Wall(FVector(-1000,500,180),FVector(0.35,4,3.6));
    Wall(FVector(-1000,1300,180),FVector(0.35,4,3.6));
    Wall(FVector(-1700,900,370),FVector(14,12,0.2));
    if (UMaterialInterface* GlassBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent.Widget3DPassThrough_Translucent")))
    {
        BuildingGlass=UMaterialInstanceDynamic::Create(GlassBase,this);
        BuildingGlass->SetVectorParameterValue(TEXT("TintColorAndOpacity"),FLinearColor(0.08,0.3,0.45,0.12));
        BuildingGlass->SetScalarParameterValue(TEXT("OpacityFromTexture"),0);
        BuildingGlass->SetTextureParameterValue(TEXT("SlateUI"),LoadObject<UTexture>(nullptr,TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture")));
    }
    Label(FVector(-950,900,65),TEXT("FLOODED SHELTER / ENTRANCE"),FColor(90,210,240),32);
    AddSite(FVector(-1900,700,0),1,TEXT("SHELTER / HULL ALLOY"));
    AddSite(FVector(-2000,1200,0),3,TEXT("SHELTER / OXYGEN"));
    UPointLightComponent* Interior=NewObject<UPointLightComponent>(this);
    Interior->SetIntensity(1800); Interior->SetAttenuationRadius(650);
    Interior->SetLightColor(FLinearColor(0.08,0.4,1)); Interior->RegisterComponent();
    Interior->SetWorldLocation(BuildingCenter+FVector(0,0,240));
    Patrol = {FVector(-2300,1800,100), FVector(1800,1800,100), FVector(2100,-1800,100), FVector(-2000,-1800,100)};
    for (FVector& Point:Patrol) Point=Abyss::Map(Point);

    OceanLight = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,1500), FRotator(-65,-30,0));
    OceanLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    OceanLight->GetLightComponent()->SetIntensity(1.6);
    OceanLight->GetLightComponent()->SetLightColor(FLinearColor(0.19,0.48,0.65));
    Fog = GetWorld()->SpawnActor<AExponentialHeightFog>();
    Fog->GetComponent()->SetFogDensity(0.032);
    Fog->GetComponent()->SetFogHeightFalloff(0.035);
    Fog->GetComponent()->SetFogInscatteringColor(FLinearColor(0.015,0.075,0.105));
    Fog->GetComponent()->SetVolumetricFog(true);
    APostProcessVolume* PP = GetWorld()->SpawnActor<APostProcessVolume>();
    PP->bUnbound = true;
    PP->Settings.bOverride_AutoExposureMethod = true;
    PP->Settings.AutoExposureMethod = AEM_Manual;
    PP->Settings.bOverride_AutoExposureBias = true;
    PP->Settings.AutoExposureBias = 1.0;
    PP->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    PP->Settings.AutoExposureApplyPhysicalCameraExposure = false;
    PP->Settings.bOverride_VignetteIntensity = true;
    PP->Settings.VignetteIntensity = 0.55;
    PP->Settings.bOverride_SceneColorTint = true;
    PP->Settings.SceneColorTint = FLinearColor(0.6,0.85,1);
    PP->Settings.bOverride_BloomIntensity = true;
    PP->Settings.BloomIntensity = 0.45;
}

void AAbyssGameMode::BuildImportedCity()
{
    CityScale=GetWorld()->GetMapName().Contains(TEXT("Compact")) ? 0.7f : 1.f;
    MapHalfExtent = (GetWorld()->GetMapName().EndsWith(TEXT("_Mk2")) ? 23650.f : 24500.f)*CityScale;
    // Keep the authored city, seabed and lighting. Only create gameplay objects.
    for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It) { OceanLight=*It; break; }
    for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It) { Fog=*It; break; }
    const auto PlaceSite = [&](FVector WorldPosition, int32 Kind)
    {
        if (Kind < 4 && RoamingKraken)
        {
            FVector Placement;
            if (FindSalvagePlacement(GetWorld(), WorldPosition, Placement))
            {
                if (FVector::DistSquared2D(Placement, WorldPosition) > 1.f)
                    UE_LOG(LogTemp, Display, TEXT("ABYSS_SITE_RELOCATED %d %s -> %s"), Kind,
                        *WorldPosition.ToString(), *Placement.ToString());
                WorldPosition = Placement;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("ABYSS_SITE_PLACEMENT_UNVERIFIED %d %s"), Kind, *WorldPosition.ToString());
            }
        }
        // AddSite uses the legacy test-map transform; invert it for authored positions.
        AddSite(FVector(WorldPosition.X/Abyss::MapScale, WorldPosition.Y/Abyss::MapScale, WorldPosition.Z), Kind,
            Kind<4 ? FString(Abyss::Items[Kind]) : Kind==4 ? TEXT("KELP / HIDE") :
            Kind==8 ? TEXT("ESCAPE HATCH") : FString::Printf(TEXT("SERVICE %02d"),Kind-4));
    };
    struct FAuthoredSite
    {
        FVector Position;
        int32 Kind;
        int32 SavedOrder;
    };
    TArray<FAuthoredSite> AuthoredSites;
    TArray<AActor*> Caches, Roofs, Opened, Walls, Uppers;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(TEXT("AbyssRoamingKraken"))) RoamingKraken=*It;
        if ((It->ActorHasTag(TEXT("AbyssCacheSafe")) || It->ActorHasTag(TEXT("AbyssCacheCrate"))) &&
            !It->ActorHasTag(TEXT("AbyssExplorationCache"))) Caches.Add(*It);
        if (It->ActorHasTag(TEXT("AbyssUpperStorey"))) Uppers.Add(*It);
        if (It->ActorHasTag(TEXT("AbyssInteriorRoof"))) Roofs.Add(*It);
        if (It->ActorHasTag(TEXT("AbyssInteriorWall"))) Walls.Add(*It);
        if (It->ActorHasTag(TEXT("AbyssCutawayMaterial")))
            if (const UStaticMeshComponent* Mesh=It->FindComponentByClass<UStaticMeshComponent>()) CityWallGlass=Mesh->GetMaterial(0);
        if (It->ActorHasTag(TEXT("AbyssCacheOpened"))) Opened.Add(*It);
        for (int32 Kind=0; Kind<=8; ++Kind)
        {
            if (!It->ActorHasTag(FName(*FString::Printf(TEXT("AbyssSite_%d"),Kind)))) continue;
            int32 SavedOrder=INDEX_NONE;
            for (const FName Tag : It->Tags)
            {
                const FString Value=Tag.ToString();
                if (Value.StartsWith(TEXT("AbyssSiteOrder_")))
                {
                    SavedOrder=FCString::Atoi(*Value.Mid(15));
                    break;
                }
            }
            AuthoredSites.Add({It->GetActorLocation(), Kind, SavedOrder});
        }
    }
    // Stable authored order is required by saved site flags and the interaction integration test.
    AuthoredSites.Sort([](const auto& A, const auto& B)
    {
        if (A.SavedOrder != B.SavedOrder) return A.SavedOrder < B.SavedOrder;
        const int32 AK=A.Kind>=5 ? A.Kind-5 : A.Kind+4;
        const int32 BK=B.Kind>=5 ? B.Kind-5 : B.Kind+4;
        if (AK!=BK) return AK<BK;
        if (A.Position.X!=B.Position.X) return A.Position.X<B.Position.X;
        return A.Position.Y<B.Position.Y;
    });
    for (const auto& Site : AuthoredSites) PlaceSite(Site.Position, Site.Kind);
    DressSalvageSites(Sites);
    for (AActor* Roof : Roofs)
    {
        if (Roof->ActorHasTag(TEXT("AbyssInterior_SeaCave"))) Walls.AddUnique(Roof);
        FAbyssCityInterior& Interior=CityInteriors.AddDefaulted_GetRef();
        Interior.Roof=Roof;
        for (FName Tag : Roof->Tags)
            if (Tag.ToString().StartsWith(TEXT("AbyssInterior_Lobby_")))
                for (AActor* Upper : Uppers) if (Upper->ActorHasTag(Tag)) Interior.UpperStorey=Upper;
        Roof->GetActorBounds(false,Interior.Center,Interior.Extent);
        for (AActor* Wall : Walls)
        {
            const bool bSameRoom=Roof->Tags.ContainsByPredicate([&](FName Tag)
            {
                return Tag.ToString().StartsWith(TEXT("AbyssInterior_")) && Wall->ActorHasTag(Tag);
            });
            UStaticMeshComponent* Mesh=bSameRoom ? Wall->FindComponentByClass<UStaticMeshComponent>() : nullptr;
            if (!Mesh) continue;
            FAbyssCityWall& Entry=Interior.Walls.AddDefaulted_GetRef();
            Entry.Component=Mesh;
            Entry.bCastShadow=Mesh->CastShadow;
            const bool bShell=Wall->ActorHasTag(TEXT("AbyssInteriorShell"));
            const TArray<FName> SlotNames=Mesh->GetMaterialSlotNames();
            for (int32 I=0;I<Mesh->GetNumMaterials();++I)
            {
                const FString Slot=SlotNames.IsValidIndex(I) ? SlotNames[I].ToString() : FString();
                const bool bWallSlot=Slot.EndsWith(TEXT("Limestone")) || Slot.EndsWith(TEXT("Brick")) ||
                    Slot.EndsWith(TEXT("Bronze")) || Slot.EndsWith(TEXT("PaintedSteel")) ||
                    Slot.EndsWith(TEXT("BlueGlass")) || Slot.EndsWith(TEXT("Enamel"));
                Entry.OriginalMaterials.Add(!bShell || bWallSlot ? Mesh->GetMaterial(I) : nullptr);
            }
        }
    }
    // Append after the authored site list to preserve existing mission and save indices.
    Caches.Sort([](const AActor& A, const AActor& B)
    {
        const bool NewA=A.ActorHasTag(TEXT("AbyssNewCache")), NewB=B.ActorHasTag(TEXT("AbyssNewCache"));
        return NewA!=NewB ? !NewA : A.GetName()<B.GetName();
    });
    for (AActor* Cache : Caches)
    {
        FAbyssSite& Site = Sites.AddDefaulted_GetRef();
        Site.Kind = FAbyssSite::LootCache;
        Site.CacheAnchor=Cache->GetActorLocation();
        Site.Position = Site.CacheAnchor + FVector(0,0,60);
        Site.Name = Cache->ActorHasTag(TEXT("AbyssCacheSafe")) ? TEXT("SAFE") : TEXT("SUPPLY CRATE");
        Site.Visual = Cache;
        for (const FName Tag : Cache->Tags)
        {
            if (!Tag.ToString().StartsWith(TEXT("AbyssInterior_"))) continue;
            for (AActor* Roof : Roofs) if (Roof->ActorHasTag(Tag)) Site.InteriorRoof = Roof;
            for (AActor* Model : Opened) if (Model->ActorHasTag(Tag)) Site.OpenedVisual = Model;
        }
        if (Site.OpenedVisual)
        {
            Site.OpenedVisual->SetActorTransform(Cache->GetActorTransform());
            Site.OpenedVisual->SetActorHiddenInGame(true);
        }
    }
    if (GetWorld()->GetMapName().EndsWith(TEXT("_Mk2")))
    {
        bWelderRequired=true;
        for (int32 Kind : {FAbyssSite::Fins, FAbyssSite::Welder})
        {
            const bool Fins=Kind==FAbyssSite::Fins;
            const FString Name=Fins ? TEXT("SM_Fin") : TEXT("SM_Welder");
            auto* Asset=LoadObject<UStaticMesh>(nullptr,*(TEXT("/Game/IntegratedCity/Equipment/Meshes/")+Name));
            if (!ensure(Asset)) continue;
            FVector Deck(-740,Fins ? 19040 : 19300,4);
            FHitResult Ground;
            if (GetWorld()->LineTraceSingleByChannel(Ground,Deck+FVector(0,0,140),Deck-FVector(0,0,200),ECC_WorldStatic))
                Deck.Z=Ground.ImpactPoint.Z;
            auto* Actor=GetWorld()->SpawnActor<AActor>();
            auto* Mesh=NewObject<UStaticMeshComponent>(Actor);
            Actor->SetRootComponent(Mesh);
            Mesh->SetStaticMesh(Asset);
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Mesh->RegisterComponent();
            Actor->SetActorLocation(Deck-FVector(0,0,Asset->GetBoundingBox().Min.Z));
            FAbyssSite& Gear=Sites.AddDefaulted_GetRef();
            Gear.Kind=Kind;
            Gear.Position=Deck+FVector(0,0,55);
            Gear.Name=Fins ? TEXT("SWIM FINS") : TEXT("UNDERWATER WELDER");
            Gear.Visual=Actor;
            Gear.LabelVisual=Label(FVector(Deck.X/Abyss::MapScale,Deck.Y/Abyss::MapScale,Deck.Z+100),Gear.Name,FColor::Cyan,24);
        }
    }
    AppendExplorationCaches();
    SetCacheLayout(FMath::Rand());
    SpawnCityHunters();
}


void AAbyssGameMode::SetResourceDropLayout(int32 Seed)
{
    if (!bImportedCity) return;
    auto* BatteryModel = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/IntegratedCity/Salvage/Meshes/SM_Battery"));
    if (!ensure(BatteryModel)) return;

    TArray<int32> Slots;
    for (int32 Index = 0; Index < Sites.Num(); ++Index)
        if (Sites[Index].Kind == 3 && Sites[Index].Visual) Slots.Add(Index);
    FRandomStream Random(Seed ^ 0x5a5164c);
    for (int32 Index = Slots.Num()-1; Index > 0; --Index) Slots.Swap(Index, Random.RandRange(0, Index));

    const int32 DropsPerType = FMath::Min(8, Slots.Num()/3);
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        FAbyssSite& Site = Sites[Slots[Index]];
        Site.ResourceDrop = Index < DropsPerType ? EAbyssResourceDrop::Oxygen :
            Index < DropsPerType*2 ? EAbyssResourceDrop::Battery : EAbyssResourceDrop::ReserveTank;
        const bool bBattery = Site.ResourceDrop == EAbyssResourceDrop::Battery;
        if (bBattery && !Site.BatteryVisual)
        {
            Site.BatteryVisual = NewObject<UStaticMeshComponent>(Site.Visual);
            Site.BatteryVisual->SetupAttachment(Site.Visual->GetRootComponent());
            Site.BatteryVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Site.BatteryVisual->SetStaticMesh(BatteryModel);
            Site.BatteryVisual->SetRelativeLocation(FVector(0, 0, -25.f-BatteryModel->GetBoundingBox().Min.Z));
            Site.BatteryVisual->RegisterComponent();
        }
        TInlineComponentArray<UStaticMeshComponent*> Parts(Site.Visual);
        for (UStaticMeshComponent* Part : Parts)
            Part->SetVisibility(Part == Site.BatteryVisual ? bBattery : !bBattery);
        if (Site.LabelVisual)
        {
            auto* Label = Site.LabelVisual->FindComponentByClass<UTextRenderComponent>();
            const TCHAR* Text = bBattery ? TEXT("FLASHLIGHT BATTERY") :
                Site.ResourceDrop == EAbyssResourceDrop::Oxygen ? TEXT("OXYGEN REFILL") : TEXT("RESERVE OXYGEN TANK");
            if (Label) Label->SetText(FText::FromString(Text));
        }
    }
}
void AAbyssGameMode::SetCacheLayout(int32 Seed)
{
    CacheLayoutSeed=Seed;
    SetResourceDropLayout(Seed);
    FRandomStream Random(Seed);
    FRandomStream AmbushRandom(Seed ^ 0x35A18721);
    int32 PlannedAmbushes = 0;
    TArray<int32> Order;
    for (int32 I=0;I<Sites.Num();++I)
        if (Sites[I].Kind==FAbyssSite::LootCache && !Sites[I].Visual->ActorHasTag(TEXT("AbyssExplorationCache"))) Order.Add(I);
    for (int32 I=Order.Num()-1;I>0;--I) Order.Swap(I,Random.RandRange(0,I));
    for (int32 I=0;I<Sites.Num();++I)
        if (Sites[I].Kind==FAbyssSite::LootCache && Sites[I].Visual->ActorHasTag(TEXT("AbyssExplorationCache"))) Order.Add(I);
    for (int32 I=0;I<Order.Num();++I)
    {
        FAbyssSite& Site=Sites[Order[I]];
        Site.CacheReward=I%3;
        Site.bAmbushPlanned = PlannedAmbushes < MaximumCacheAmbushes && AmbushRandom.FRand() < CacheAmbushChance;
        if (Site.bAmbushPlanned) ++PlannedAmbushes;
        const bool bSafe=I<2 && !Site.Visual->ActorHasTag(TEXT("AbyssTreasureChest"));
        FString Base=TEXT("/Game/Models/AbyssalCity/Interiors/");
        Base+=bSafe ? TEXT("SM_CityBankSafe") : TEXT("SM_CityMarketCrate");
        for (bool Open : {false,true})
        {
            AActor* Actor=Open ? Site.OpenedVisual.Get() : Site.Visual.Get();
            if (!Actor) continue;
            auto* Part=Actor->FindComponentByClass<UStaticMeshComponent>();
            auto* Model=LoadObject<UStaticMesh>(nullptr,*(Base+(Open ? TEXT("Open") : TEXT("Closed"))));
            if (!Part || !Model) continue;
            Part->SetMobility(EComponentMobility::Movable);
            Part->SetWorldScale3D(FVector(1.38f));
            Part->EmptyOverrideMaterials();
            Part->SetStaticMesh(Model);
        }
        // Use only nearby floor positions with clearance for both the container and its approach.
        FVector Destination=Site.CacheAnchor;
        FCollisionQueryParams Query;
        Query.AddIgnoredActor(Site.Visual);
        Query.AddIgnoredActor(Site.OpenedVisual);
        Query.AddIgnoredActor(UGameplayStatics::GetPlayerPawn(this,0));
        const FVector From=Site.CacheAnchor+FVector(0,0,65);
        const float Side=Random.RandRange(0,1) ? 1.f : -1.f;
        for (float Sign : {Side,-Side})
        {
            const FVector Offset(0,Sign*85.f,0);
            FHitResult Ground;
            if (!GetWorld()->LineTraceSingleByChannel(Ground,From+Offset,From+Offset-FVector(0,0,135),ECC_Visibility,Query)
                || FMath::Abs(Ground.ImpactPoint.Z-Site.CacheAnchor.Z)>20 || Ground.ImpactNormal.Z<0.8f) continue;
            const FCollisionShape Clearance=FCollisionShape::MakeBox(FVector(42,42,40));
            if (GetWorld()->SweepTestByChannel(From,From+Offset,FQuat::Identity,ECC_Pawn,Clearance,Query)) continue;
            if (GetWorld()->OverlapBlockingTestByChannel(From+Offset-FVector(70,0,0),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(36),Query)) continue;
            Destination=Site.CacheAnchor+Offset;
            Destination.Z=Ground.ImpactPoint.Z;
            break;
        }
        Site.Visual->SetActorLocation(Destination);
        if (Site.OpenedVisual) Site.OpenedVisual->SetActorLocation(Destination);
        Site.Position=Destination+FVector(0,0,60);
        Site.Name=bSafe ? TEXT("SAFE") : TEXT("SUPPLY CRATE");
    }
}
