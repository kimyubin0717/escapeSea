#include "UI/AbyssUIComponent.h"
#include "Status/AbyssConditionsComponent.h"
#include "Game/AbyssGameMode.h"
#include "GameFramework/Pawn.h"
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
FString EncodeUIState(const TSharedRef<FJsonObject>& O){FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
TSharedPtr<FJsonObject> DecodeUIState(const FString& S){TSharedPtr<FJsonObject> O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),O);return O;}
}
void UAbyssUIComponent::SendState()
{
    auto G=Game();if(!G || !Browser.IsValid() || !bReady)return;
    auto O=MakeShared<FJsonObject>();
    const auto& Condition = G->Conditions->GetState();
    O->SetNumberField(TEXT("gameDay"), G->Conditions->GameDay(G->Elapsed));
    O->SetNumberField(TEXT("gameMinute"), G->Conditions->GameMinute(G->Elapsed));
    O->SetNumberField(TEXT("condition"), static_cast<uint8>(Condition.Active != EAbyssCondition::None ? Condition.Active : Condition.Pending));
    O->SetNumberField(TEXT("conditionRemaining"), Condition.Remaining);
    O->SetBoolField(TEXT("conditionWarning"), Condition.Pending != EAbyssCondition::None);
    O->SetNumberField(TEXT("oxygen"),100.f*G->Oxygen/G->OxygenCapacity);O->SetNumberField(TEXT("health"),G->Health);
    O->SetNumberField(TEXT("battery"),G->BatteryRemaining);O->SetNumberField(TEXT("tanks"),G->Inventory[3]);
    O->SetNumberField(TEXT("elapsed"),G->Elapsed);O->SetNumberField(TEXT("repair"),G->RepairProgress);
    O->SetNumberField(TEXT("phase"),G->SalvageTime/G->SalvageDuration);O->SetNumberField(TEXT("salvageTarget"),G->SalvageTarget);O->SetNumberField(TEXT("salvageWindow"),G->SalvageWindow);
    O->SetBoolField(TEXT("inventory"),G->bInventory);O->SetBoolField(TEXT("menu"),G->bMenu);O->SetBoolField(TEXT("hasSave"),HasSave());
    O->SetBoolField(TEXT("hidden"),G->bHidden);O->SetBoolField(TEXT("light"),G->bLight);O->SetBoolField(TEXT("inside"),G->bInsideBuilding);
    O->SetBoolField(TEXT("alert"),G->Alert>0.15f);O->SetBoolField(TEXT("chasing"),G->bChasing);
    O->SetBoolField(TEXT("salvage"),G->SalvageSite!=INDEX_NONE);O->SetBoolField(TEXT("ended"),G->bEnded);O->SetBoolField(TEXT("won"),G->bWon);
    O->SetStringField(TEXT("reason"),G->Ending);O->SetStringField(TEXT("prompt"),G->SitePrompt(G->NearestSite()));
    FString Guide;
    if (const FAbyssSite* Target=G->FirstDiveTarget())
    {
        const APawn* Player=UGameplayStatics::GetPlayerPawn(this,0);
        const TCHAR* Task=Target->Kind==FAbyssSite::Fins ? TEXT("부두 오리발을 [E]로 챙기세요") :
            Target->Kind==FAbyssSite::Welder ? TEXT("부두 용접기를 [E]로 챙기세요") : TEXT("청록색 입구 안 보급함을 [E]로 열고 [SPACE]로 잠금을 푸세요");
        Guide=FString::Printf(TEXT("%s · %dm"),Task,FMath::RoundToInt(FVector::Dist2D(Player->GetActorLocation(),Target->Position)/100.0));
    }
    O->SetStringField(TEXT("guide"),Guide);
    O->SetStringField(TEXT("notice"),G->NoticeTime>0 ? G->Notice : FString());O->SetNumberField(TEXT("noticeSerial"),G->NoticeSerial);O->SetStringField(TEXT("noticeKind"),G->NoticeKind);
    TArray<TSharedPtr<FJsonValue>> Parts,Repaired;
    for(int32 I=0;I<3;++I){Parts.Add(MakeShared<FJsonValueNumber>(G->Inventory[I]));Repaired.Add(MakeShared<FJsonValueBoolean>(G->Repaired[I]));}
    O->SetArrayField(TEXT("parts"),Parts);O->SetArrayField(TEXT("repaired"),Repaired);
    O->SetNumberField(TEXT("missingPart"), G->MissingPartsUntil>G->Elapsed ? G->MissingPart : INDEX_NONE);
    const FString Cargo=G->Cargo ? G->Cargo->ToJson() : TEXT("[]");
    Browser->ExecuteJavascript(TEXT("window.abyssRuntime && window.abyssRuntime.receive(")+EncodeUIState(O)+TEXT(",")+Cargo+TEXT(");"));
}
