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
void UAbyssUIComponent::HandleAction(const FString& Action,const FString& Payload)
{
    auto G=Game();if(!G)return;
    if(Action==TEXT("ready")){bReady=true;SendState();return;}
    if(Action==TEXT("menu")){SaveRun();G->bMenu=true;G->bInventory=false;}
    else if(Action==TEXT("new"))
    {
        // Starting a new run after playing reloads the world as well as all interaction sites.
        if(G->Elapsed>0 || G->bEnded){UGameplayStatics::OpenLevel(this,FName(*UGameplayStatics::GetCurrentLevelName(this)),true,TEXT("game=/Script/mama.AbyssGameMode?AbyssNew=1"));return;}
        G->bMenu=false;SaveRun();
    }
    else if(Action==TEXT("continue")){if(!LoadRun())G->Notify(TEXT("SAVE LOAD FAILED / start a new dive."));}
    else if(Action==TEXT("quit")){SaveRun();if(auto PC=Cast<APlayerController>(GetOwner()))PC->ConsoleCommand(TEXT("quit"));return;}
    else if(Action==TEXT("cargo") && G->bInventory && !G->bMenu)
    {
        const auto O=Decode(Payload);FString Id;double X,Y;
        if(O && O->TryGetStringField(TEXT("id"),Id) && O->TryGetNumberField(TEXT("x"),X) && O->TryGetNumberField(TEXT("y"),Y) && X>=0 && X<=6 && Y>=0 && Y<=6 && X==FMath::FloorToDouble(X) && Y==FMath::FloorToDouble(Y))G->Cargo->Move(Id,int32(X),int32(Y));
    }
    else if(!G->bMenu)
    {
        if(Action==TEXT("r")){UGameplayStatics::OpenLevel(this,FName(*UGameplayStatics::GetCurrentLevelName(this)),true,TEXT("game=/Script/mama.AbyssGameMode?AbyssNew=1"));return;}
        if(!G->bEnded)
        {
            if(Action==TEXT("tab"))G->bInventory=!G->bInventory;
            if(Action==TEXT("q"))G->UseOxygen();
            if(Action==TEXT("f"))G->ToggleFlashlight();
            if(Action==TEXT("e"))G->Interact();
            if(Action==TEXT("space"))G->SubmitSalvage();
        }
    }
    SendState();
}
