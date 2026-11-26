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
void UAbyssUIComponent::SendState()
{
    auto G=Game();if(!G || !Browser.IsValid() || !bReady)return;
    auto O=MakeShared<FJsonObject>();
    O->SetNumberField(TEXT("oxygen"),G->Oxygen);O->SetNumberField(TEXT("health"),G->Health);
    O->SetNumberField(TEXT("battery"),G->BatteryRemaining);O->SetNumberField(TEXT("tanks"),G->Inventory[3]);
    O->SetNumberField(TEXT("elapsed"),G->Elapsed);O->SetNumberField(TEXT("repair"),G->RepairProgress);
    O->SetNumberField(TEXT("phase"),G->SalvageTime/G->SalvageDuration);O->SetNumberField(TEXT("salvageTarget"),G->SalvageTarget);O->SetNumberField(TEXT("salvageWindow"),G->SalvageWindow);
    O->SetBoolField(TEXT("inventory"),G->bInventory);O->SetBoolField(TEXT("menu"),G->bMenu);O->SetBoolField(TEXT("hasSave"),HasSave());
    O->SetBoolField(TEXT("hidden"),G->bHidden);O->SetBoolField(TEXT("light"),G->bLight);O->SetBoolField(TEXT("inside"),G->bInsideBuilding);
    O->SetBoolField(TEXT("alert"),G->Alert>0.15f);O->SetBoolField(TEXT("chasing"),G->bChasing);
    O->SetBoolField(TEXT("salvage"),G->SalvageSite!=INDEX_NONE);O->SetBoolField(TEXT("ended"),G->bEnded);O->SetBoolField(TEXT("won"),G->bWon);
    O->SetStringField(TEXT("reason"),G->Ending);O->SetStringField(TEXT("prompt"),G->SitePrompt(G->NearestSite()));
    O->SetStringField(TEXT("notice"),G->NoticeTime>0 ? G->Notice : FString());O->SetNumberField(TEXT("noticeSerial"),G->NoticeSerial);O->SetStringField(TEXT("noticeKind"),G->NoticeKind);
    TArray<TSharedPtr<FJsonValue>> Parts,Repaired;
    for(int32 I=0;I<3;++I){Parts.Add(MakeShared<FJsonValueNumber>(G->Inventory[I]));Repaired.Add(MakeShared<FJsonValueBoolean>(G->Repaired[I]));}
    O->SetArrayField(TEXT("parts"),Parts);O->SetArrayField(TEXT("repaired"),Repaired);
    O->SetNumberField(TEXT("missingPart"), G->MissingPartsUntil>G->Elapsed ? G->MissingPart : INDEX_NONE);
    const FString Cargo=G->Cargo ? G->Cargo->ToJson() : TEXT("[]");
    Browser->ExecuteJavascript(TEXT("window.abyssRuntime && window.abyssRuntime.receive(")+Encode(O)+TEXT(",")+Cargo+TEXT(");"));
}
