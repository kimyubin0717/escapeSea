#include "UI/AbyssUIComponent.h"
#include "AbyssGame.h"
#include "Inventory/AbyssInventoryComponent.h"
#include "SWebBrowser.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
FString Encode(const TSharedRef<FJsonObject>& O){FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
TSharedPtr<FJsonObject> Decode(const FString& S){TSharedPtr<FJsonObject> O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),O);return O;}
}
UAbyssUIComponent::UAbyssUIComponent(){PrimaryComponentTick.bCanEverTick=true;}
AAbyssGameMode* UAbyssUIComponent::Game() const {return Cast<AAbyssGameMode>(UGameplayStatics::GetGameMode(this));}
void UAbyssUIComponent::BeginPlay()
{
    Super::BeginPlay();
    if(IsRunningCommandlet() || FParse::Param(FCommandLine::Get(),TEXT("nullrhi")))return;
    if(!GEngine || !GEngine->GameViewport)return;
    FString Path=FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()/TEXT("UI/index.html"));
    Path.ReplaceInline(TEXT("\\"),TEXT("/"));Path.ReplaceInline(TEXT(" "),TEXT("%20"));
    Browser=SNew(SWebBrowser).InitialURL(TEXT("file:///")+Path+TEXT("?runtime=1"))
        .ShowControls(false).ShowAddressBar(false).ShowInitialThrobber(false)
        .SupportsTransparency(true).BackgroundColor(FColor::Transparent).BrowserFrameRate(60)
        .OnBeforePopup_Lambda([](FString,FString){return true;})
        .OnConsoleMessage_Lambda([this](const FString& Message,const FString&,int32,EWebBrowserConsoleLogSeverity)
        {
            if(Message==TEXT("ABYSS_UI_READY"))
            {
                bReady=true;UE_LOG(LogTemp,Display,TEXT("ABYSS_UI_CONNECTED: HTML menu, HUD, inventory and runtime bridge ready"));SendState();return;
            }
            const FString Prefix=TEXT("ABYSS_ACTION:");
            if(Message.StartsWith(Prefix))
            {
                const auto O=Decode(Message.Mid(Prefix.Len()));FString Action,Payload;
                if(O && O->TryGetStringField(TEXT("action"),Action)){O->TryGetStringField(TEXT("payload"),Payload);HandleAction(Action,Payload);}
            }
            else if(Message.StartsWith(TEXT("ABYSS_UI_CHECK:")))UE_LOG(LogTemp,Display,TEXT("%s"),*Message);
        });
    ViewportWidget=Browser;
    GEngine->GameViewport->AddViewportWidgetContent(ViewportWidget.ToSharedRef(),100);
    UE_LOG(LogTemp,Display,TEXT("ABYSS_UI_LOADING: %s"),*Path);
}
void UAbyssUIComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Reason!=EEndPlayReason::Quit)SaveRun();
    if(GEngine && GEngine->GameViewport && ViewportWidget.IsValid())GEngine->GameViewport->RemoveViewportWidgetContent(ViewportWidget.ToSharedRef());
    Browser.Reset();ViewportWidget.Reset();Super::EndPlay(Reason);
}
void UAbyssUIComponent::TickComponent(float Dt,ELevelTick TickType,FActorComponentTickFunction* ThisTick)
{
    Super::TickComponent(Dt,TickType,ThisTick);
    auto G=Game();if(!G)return;
    if(G->Cargo)G->Cargo->Sync(G->Inventory);
    if(Browser.IsValid())
    {
        const bool Interactive=G->bMenu || G->bInventory || G->bEnded;
        if(Interactive!=bInteractive || !bReady)
        {
            bInteractive=Interactive;Browser->SetVisibility(Interactive ? EVisibility::Visible : EVisibility::HitTestInvisible);
            if(FSlateApplication::IsInitialized())
            {
                if(Interactive)FSlateApplication::Get().SetKeyboardFocus(Browser);
                else FSlateApplication::Get().SetAllUserFocusToGameViewport();
            }
        }
        SyncClock+=Dt;if(SyncClock>=0.1f){SyncClock=0;SendState();}
    }
    SaveClock+=Dt;if(SaveClock>=5){SaveClock=0;SaveRun();}
}
#include "Save/AbyssSaveService.h"
bool UAbyssUIComponent::HasSave() const { return FAbyssSaveService::HasSave(this); }
bool UAbyssUIComponent::SaveRun() { return FAbyssSaveService::SaveRun(this); }
bool UAbyssUIComponent::LoadRun() { return FAbyssSaveService::LoadRun(this); }
