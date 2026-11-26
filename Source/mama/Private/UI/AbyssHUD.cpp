#include "UI/AbyssHUD.h"
#include "UI/AbyssUIComponent.h"
#include "Player/AbyssController.h"
#include "Game/AbyssGameLookup.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

void AAbyssHUD::DrawHUD()
{
    Super::DrawHUD();
    const auto* Controller = Cast<AAbyssController>(GetOwningPlayerController());
    if (Controller && Controller->GameUI && Controller->GameUI->IsConnected()) return;
    const AAbyssGameMode* Game = Abyss::Game(this);
    if (!Canvas || !GEngine || !Game || Canvas->SizeX <= 0 || Canvas->SizeY <= 0) return;

    const float Scale = FMath::Min(Canvas->SizeX / 1600.f, Canvas->SizeY / 900.f);
    const float W = Canvas->SizeX / Scale, H = Canvas->SizeY / Scale;
    const FLinearColor Ink(.82f, .93f, .94f), Muted(.49f, .69f, .7f);
    const FLinearColor Panel(.008f, .023f, .033f, .94f), Line(.075f, .153f, .179f, 1);
    const FLinearColor Mint(.38f, .89f, .77f), Amber(.96f, .70f, .31f), Danger(1.f, .38f, .33f);
    UFont* Font = GEngine->GetMediumFont();
    const float TextScale = 1.35f * Scale;
    auto Box = [&](float X, float Y, float Width, float Height, FLinearColor Color)
    {
        DrawRect(Color, X*Scale, Y*Scale, Width*Scale, Height*Scale);
    };
    auto Text = [&](const FString& Label, float X, float Y, FLinearColor Color, float Size = 1.f)
    {
        DrawText(Label, Color, X*Scale, Y*Scale, Font, Size*TextScale);
    };
    auto Center = [&](const FString& Label, float X, float Y, FLinearColor Color, float Size = 1.f)
    {
        float Width = 0, Height = 0;
        GetTextSize(Label, Width, Height, Font, Size*TextScale);
        Text(Label, X-Width/(2*Scale), Y, Color, Size);
    };
    auto Card = [&](float X, float Y, float Width, float Height, FLinearColor Accent)
    {
        Box(X+2, Y+4, Width, Height, FLinearColor(0, .009f, .015f, .50f));
        Box(X, Y, Width, Height, Panel);
        Box(X, Y, Width, 1, Line);
        Box(X, Y, 2, Height, Accent);
    };
    auto Meter = [&](float X, float Y, float Width, float Value, FLinearColor Color)
    {
        Box(X, Y, Width, 4, Line);
        const float Fill = FMath::Clamp(Value, 0.f, 1.f);
        if (Fill > 0) Box(X, Y, Width*Fill, 4, Color);
    };
    const auto Critical = [&](float Percent) { return Percent <= 30 ? Danger : Mint; };
    const float OxygenPercent = 100.f * Game->Oxygen / Game->OxygenCapacity;
    const float Lamp = 100.f * Game->BatteryRemaining / FMath::Max(1.f, Game->FlashlightDuration);
    const int32 Seconds = FMath::Max(0, FMath::FloorToInt(Game->Elapsed));
    const FString Time = FString::Printf(TEXT("%02d:%02d"), Seconds/60, Seconds%60);

    if (Game->bEnded)
    {
        const FLinearColor Accent = Game->bWon ? Mint : Danger;
        Box(0, 0, W, H, FLinearColor(.002f, .007f, .012f, .86f));
        Card(W/2-365, H/2-94, 730, 188, Accent);
        Center(Game->bWon ? TEXT("Dive complete") : TEXT("Dive lost"), W/2, H/2-66, Accent, 1.8f);
        Center(Game->Ending, W/2, H/2-15, Ink);
        Center(FString::Printf(TEXT("%s    |    [R] Dive again"), *Time), W/2, H/2+40, Muted, .95f);
        return;
    }

    const int32 Stage = Game->NextRepairStage();
    const TCHAR* Names[] = { TEXT("Power cell"), TEXT("Hull alloy"), TEXT("Drive part") };
    Card(28, 28, 310, 144, Mint);
    Text(Stage<3 ? TEXT("RESTORE THE WHITE NAUTILUS MK2") : TEXT("ESCAPE HATCH OPEN"), 47, 41, Mint, .93f);
    for (int32 I=0; I<3; ++I)
    {
        const float Y = 70.f + 22.f*I;
        const FLinearColor Tint = Game->Repaired[I] || I==Stage ? Ink : Muted;
        Text(Names[I], 49, Y, Tint, .88f);
        Text(Game->Repaired[I] ? FString(TEXT("Online")) :
            FString::Printf(TEXT("%d / 2"), Game->Inventory[I]), 252, Y,
            Game->Repaired[I] ? Mint : Tint, .85f);
    }
    const APawn* Diver = UGameplayStatics::GetPlayerPawn(this, 0);
    const FAbyssSite* GuideSite = Game->FirstDiveTarget();
    FString Hint = Stage<3 ? TEXT("Unlock safes and crates. Bring parts to port.") : TEXT("Return to the submarine to escape.");
    if (GuideSite && Diver)
    {
        const TCHAR* Target = GuideSite->Kind == FAbyssSite::Fins ? TEXT("Dock fins") :
            GuideSite->Kind == FAbyssSite::Welder ? TEXT("Dock welder") : TEXT("Supply cache");
        Hint = FString::Printf(TEXT("%s  %d m  |  Follow gold marker"), Target,
            FMath::RoundToInt(FVector::Dist2D(Diver->GetActorLocation(),GuideSite->Position)/100.0));
    }
    Text(Hint, 47, 143, GuideSite ? Amber : Muted, .73f);
    Card(W/2-62, 28, 124, 44, Line);
    Center(Time, W/2, 37, Ink, 1.2f);

    const float StatusX = W-280;
    Card(StatusX, 28, 252, 124, OxygenPercent<=30 ? Danger : Line);
    auto Resource = [&](const TCHAR* Label, float Y, float Value, const FLinearColor& Color)
    {
        Text(Label, StatusX+14, Y, Muted, .8f);
        Text(FString::Printf(TEXT("%d%%"), FMath::CeilToInt(Value)), StatusX+192, Y, Color, .84f);
        Meter(StatusX+14, Y+17, 224, Value/100.f, Color);
    };
    Resource(TEXT("Oxygen"), 40, OxygenPercent, Critical(OxygenPercent));
    Resource(TEXT("Health"), 75, Game->Health, Critical(Game->Health));
    Resource(Game->bLight ? TEXT("Lamp") : TEXT("Lamp off"), 110, Lamp, Critical(Lamp));

    if (Game->Oxygen<=0 || Game->Alert>.15f)
    {
        Card(W/2-214, 84, 428, 44, Danger);
        Center(Game->Oxygen<=0 ? TEXT("Oxygen depleted  /  [Q] Use a reserve") :
            Game->bChasing ? TEXT("Hunter pursuing  /  Break line of sight") : TEXT("Sonar contact nearby"),
            W/2, 96, Danger, .92f);
    }
    if (Game->NoticeTime>0 && !Game->Notice.IsEmpty())
    {
        Card(28, 182, 655, 37, Line);
        Text(Game->Notice, 46, 191, Ink, .78f);
    }

    const float MapX = W-225, MapY = H-260;
    Card(MapX, MapY, 197, 188, Line);
    Text(Game->bImportedCity ? TEXT("ABYSSAL CITY") : TEXT("SEAFLOOR"), MapX+14, MapY+10, Mint, .83f);
    const float MX = MapX+98, MY = MapY+103;
    Text(TEXT("N"), MX-4, MY-73, Muted, .67f);
    for (int32 I=-2; I<=2; ++I)
    {
        const float Offset = I*25.f;
        Box(MX+Offset, MY-62, 1, 124, Line);
        Box(MX-72, MY+Offset, 144, 1, Line);
    }
    const float Extent = FMath::Max(1.f, Game->MapHalfExtent);
    auto Dot = [&](const FVector& Pos, float Size, FLinearColor Color)
    {
        const float X = MX + FMath::Clamp(Pos.Y / Extent, -1., 1.)*70;
        const float Y = MY - FMath::Clamp(Pos.X / Extent, -1., 1.)*59;
        Box(X-Size/2, Y-Size/2, Size, Size, Color);
    };
    for (const FAbyssSite& Site : Game->Sites)
    {
        if (Site.bUsed || Site.Kind == 4) continue;
        const bool bSalvage = Site.Kind < 3 || Site.Kind == FAbyssSite::LootCache;
        const bool bTerminal = Site.Kind >= 5 && Site.Kind <= 8;
        const FLinearColor DotColor = Site.ResourceDrop == EAbyssResourceDrop::Battery ? FLinearColor(.39f,.66f,1.f) : bSalvage ? Amber : Mint;
        Dot(Site.Position, bTerminal ? 5.f : 3.f, DotColor);
    }
    if (GuideSite)
    {
        Dot(GuideSite->Position, 12.f, Amber);
        Dot(GuideSite->Position, 6.f, Panel);
    }
    if (Diver)
    {
        const FVector Position = Diver->GetActorLocation();
        Dot(Position, 7, Ink);
        if (Game->Sites.IsValidIndex(Game->HarborHatchIndex))
        {
            const int32 Meters = FMath::RoundToInt(FVector::Dist2D(Position, Game->Sites[Game->HarborHatchIndex].Position)/100.f);
            Text(FString::Printf(TEXT("Nautilus hatch  %d m"), Meters), MapX+14, MapY+160, Mint, .78f);
        }
        const auto Sonar = [&](const AActor* Creature, float Range)
        {
            if (Creature && FVector::DistSquared2D(Position, Creature->GetActorLocation()) <= FMath::Square(Range))
                Dot(Creature->GetActorLocation(), 6, Danger);
        };
        Sonar(Game->Hunter, 950.f);
        for (const FAbyssHunter& Enemy : Game->ExtraHunters) Sonar(Enemy.Actor, 950.f);
        Sonar(Game->RoamingKraken, 1800.f);
    }

    Card(28, H-64, 470, 47, Line);
    Text(TEXT("WASD / ARROWS   Swim       MOUSE   Camera       V   View"), 41, H-57, Ink, .90f);
    Text(TEXT("SHIFT   Boost       E   Interact       F   Lamp       TAB   Gear"), 41, H-36, Muted, .84f);
    const FLinearColor OxygenTint = OxygenPercent<=30 ? Danger : Mint;
    Text(FString::Printf(TEXT("[Q] Reserve air   %d"), Game->Inventory[3]), W-211, H-51, OxygenTint, .94f);
    if (Game->FinsRemaining>0)
        Text(FString::Printf(TEXT("Fins +50%%  /  %ds"), FMath::CeilToInt(Game->FinsRemaining)), 30, H-110, Mint, .86f);
    if (Game->bHidden) Text(TEXT("Hidden in kelp"), 30, H-88, Mint, .86f);
    else if (Game->bInsideBuilding) Text(TEXT("Flooded shelter"), 30, H-88, Mint, .86f);
    const bool Salvaging = Game->SalvageSite != INDEX_NONE;
    const int32 Nearby = Game->NearestSite();
    if (Nearby != INDEX_NONE && !Salvaging && !Game->bInventory)
    {
        Card(W/2-313, H-130, 626, 48, Amber);
        Center(Game->SitePrompt(Nearby), W/2, H-117, Ink, 1.f);
    }
    if (Game->RepairProgress>0)
    {
        Center(TEXT("Installing subsystem"), W/2, H-75, Mint, .88f);
        Meter(W/2-170, H-52, 340, Game->RepairProgress/4.f, Mint);
    }

    if (Game->bInventory && !Salvaging)
    {
        const float X=W/2-320, Y=H/2-188;
        Box(0,0,W,H,FLinearColor(0,.007f,.012f,.67f));
        Card(X,Y,640,376,Mint);
        Text(TEXT("Dive inventory"),X+24,Y+15,Ink,1.45f);
        Text(TEXT("[Tab] Close"),X+504,Y+20,Muted,.86f);
        auto Slot=[&](int32 Column,float Row,const FString& Label,const FString& Value,FLinearColor Tint)
        {
            const float Left=X+24+Column*150;
            Box(Left,Row,140,78,FLinearColor(.026f,.06f,.076f));
            Box(Left,Row,140,2,Tint);
            Text(Label,Left+8,Row+10,Ink,.84f);
            Text(Value,Left+8,Row+43,Tint,.84f);
        };
        Text(TEXT("SUPPLIES"),X+24,Y+65,Mint,.85f);
        for (int32 K=0;K<3;++K)
            Slot(K,Y+92,Names[K],FString::Printf(TEXT("%d held  /  %s"),Game->Inventory[K],
                Game->Repaired[K] ? TEXT("fitted") : TEXT("need 2")),Game->Repaired[K] ? Mint : Amber);
        Slot(3,Y+92,TEXT("Air tanks"),FString::Printf(TEXT("%d held  [Q]"),Game->Inventory[3]),Mint);
        Text(TEXT("DIVE EQUIPMENT"),X+24,Y+192,Mint,.85f);
        Slot(0,Y+220,TEXT("Dive light"),FString::Printf(TEXT("%ds  [F]"),FMath::CeilToInt(Game->BatteryRemaining)),Mint);
        Slot(1,Y+220,TEXT("Swim fins"),Game->FinsRemaining>0 ? FString::Printf(TEXT("%ds left"),FMath::CeilToInt(Game->FinsRemaining)) : FString(TEXT("Inactive")),Game->FinsRemaining>0 ? Mint : Muted);
        Slot(2,Y+220,TEXT("Welder"),Game->bHasWelder ? TEXT("Equipped") : TEXT("At harbor"),Game->bHasWelder ? Mint : Amber);
        Text(Stage<3 ? FString::Printf(TEXT("Next: terminal %02d  /  %s"),Stage+1,Names[Stage]) : FString(TEXT("Repairs complete. Return to the hatch.")),X+24,Y+332,Ink,.92f);
    }
    if (Salvaging)
    {
        const float X = W/2-270, Y = H/2-72, Width = 492;
        Card(X, Y, 540, 144, Amber);
        Text(Game->Sites[Game->SalvageSite].Kind==FAbyssSite::LootCache ? FString::Printf(TEXT("Unlock %s"), *Game->Sites[Game->SalvageSite].Name) : FString(TEXT("Recover part")), X+24, Y+16, Ink, 1.25f);
        Text(TEXT("[Space] Stop the marker inside the green band."), X+24, Y+47, Muted, .88f);
        Box(X+24, Y+77, Width, 15, Line);
        Box(X+24+Width*Game->SalvageTarget, Y+77, Width*Game->SalvageWindow, 15, Mint);
        const float Phase = FMath::Clamp(Game->SalvageTime/FMath::Max(.01f, Game->SalvageDuration), 0.f, 1.f);
        Box(X+22+Width*Phase, Y+71, 4, 27, Ink);
        Text(TEXT("A miss can be heard by monsters within 28 m."), X+24, Y+110, Amber, .82f);
    }
}

