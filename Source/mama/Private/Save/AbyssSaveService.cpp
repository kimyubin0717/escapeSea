#include "Save/AbyssSaveService.h"
#include "Status/AbyssConditionsComponent.h"
#include "Game/AbyssGameLookup.h"
#include "Inventory/AbyssInventoryComponent.h"
#include "GameFramework/Pawn.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
namespace {
FString EncodeRun(const TSharedRef<FJsonObject>& O){FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
TSharedPtr<FJsonObject> DecodeRun(const FString& S){TSharedPtr<FJsonObject> O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),O);return O;}
}
FString FAbyssSaveService::SavePath(const UObject* Context) {return FPaths::ProjectSavedDir()/(Abyss::Game(Context) && Abyss::Game(Context)->bImportedCity ? (Abyss::Game(Context)->CityScale<1 ? TEXT("SaveGames/AbyssCompactRun.json") : TEXT("SaveGames/AbyssCityRun.json")) : TEXT("SaveGames/AbyssRun.json"));}

bool FAbyssSaveService::HasSave(const UObject* Context)
{
    FString S;if(!FFileHelper::LoadFileToString(S,*SavePath(Context)))return false;
    const auto O=DecodeRun(S);double Version;
    return O && O->TryGetNumberField(TEXT("version"),Version) && Version==1 && O->HasTypedField<EJson::Object>(TEXT("state"));
}

bool FAbyssSaveService::SaveRun(const UObject* Context)
{
    auto G=Abyss::Game(Context);if(!G || G->bMenu || G->bEnded || G->bSmokeTest)return false;
    auto O=MakeShared<FJsonObject>();auto S=MakeShared<FJsonObject>();O->SetNumberField(TEXT("version"),1);
    S->SetNumberField(TEXT("oxygen"),G->Oxygen);S->SetNumberField(TEXT("health"),G->Health);S->SetNumberField(TEXT("battery"),G->BatteryRemaining);S->SetNumberField(TEXT("elapsed"),G->Elapsed);S->SetBoolField(TEXT("light"),G->bLight);
    TArray<TSharedPtr<FJsonValue>> Counts,Repaired,Used;
    for(int32 I=0;I<4;++I)Counts.Add(MakeShared<FJsonValueNumber>(G->Inventory[I]));
    for(int32 I=0;I<3;++I)Repaired.Add(MakeShared<FJsonValueBoolean>(G->Repaired[I]));
    for(const auto& Site:G->Sites)Used.Add(MakeShared<FJsonValueBoolean>(Site.bUsed));
    S->SetNumberField(TEXT("oxygenCapacity"),G->OxygenCapacity);
    S->SetNumberField(TEXT("finsRemaining"),G->FinsRemaining);
    S->SetNumberField(TEXT("cacheLayoutSeed"),G->CacheLayoutSeed);
    S->SetObjectField(TEXT("conditions"),G->Conditions->ToJson());
    S->SetArrayField(TEXT("counts"),Counts);S->SetArrayField(TEXT("repaired"),Repaired);S->SetArrayField(TEXT("used"),Used);
    if(auto P=UGameplayStatics::GetPlayerPawn(Context,0)){const auto Pos=P->GetActorLocation();S->SetNumberField(TEXT("x"),Pos.X);S->SetNumberField(TEXT("y"),Pos.Y);S->SetNumberField(TEXT("z"),Pos.Z);}
    O->SetObjectField(TEXT("state"),S);O->SetStringField(TEXT("cargo"),G->Cargo->ToJson());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(SavePath(Context)),true);
    return FFileHelper::SaveStringToFile(EncodeRun(O),*SavePath(Context),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool FAbyssSaveService::LoadRun(const UObject* Context)
{
    auto G=Abyss::Game(Context);FString Json;if(!G || !FFileHelper::LoadFileToString(Json,*SavePath(Context)))return false;
    const auto O=DecodeRun(Json);if(!O || !O->HasTypedField<EJson::Object>(TEXT("state")))return false;
    const auto S=O->GetObjectField(TEXT("state"));
    double Oxygen,Health,Battery,Elapsed,X,Y;bool Light;FString CargoJson;
    if(!S->TryGetNumberField(TEXT("oxygen"),Oxygen)||!S->TryGetNumberField(TEXT("health"),Health)||!S->TryGetNumberField(TEXT("battery"),Battery)||!S->TryGetNumberField(TEXT("elapsed"),Elapsed)||!S->TryGetNumberField(TEXT("x"),X)||!S->TryGetNumberField(TEXT("y"),Y)||!S->TryGetBoolField(TEXT("light"),Light)||!O->TryGetStringField(TEXT("cargo"),CargoJson))return false;
    double SavedCapacity=100.f;
    S->TryGetNumberField(TEXT("oxygenCapacity"),SavedCapacity);
    if (!FMath::IsFinite(SavedCapacity) || SavedCapacity <= 0 || Oxygen > SavedCapacity) return false;
    Oxygen *= G->OxygenCapacity/SavedCapacity;
    if(!FMath::IsFinite(Oxygen)||!FMath::IsFinite(Health)||!FMath::IsFinite(Battery)||!FMath::IsFinite(Elapsed)||!FMath::IsFinite(X)||!FMath::IsFinite(Y)||Oxygen<0||Oxygen>G->OxygenCapacity||Health<=0||Health>100||Battery<0||Battery>G->FlashlightDuration||Elapsed<0||Elapsed>1e8||FMath::Abs(X)>G->MapHalfExtent||FMath::Abs(Y)>G->MapHalfExtent)return false;
    const TArray<TSharedPtr<FJsonValue>> *Counts,*Repaired,*Used;
    if(!S->TryGetArrayField(TEXT("counts"),Counts)||Counts->Num()!=4||!S->TryGetArrayField(TEXT("repaired"),Repaired)||Repaired->Num()!=3||!S->TryGetArrayField(TEXT("used"),Used)||Used->Num()>G->Sites.Num())return false;
    for(int32 I=Used->Num();I<G->Sites.Num();++I)if (G->Sites[I].Kind<FAbyssSite::LootCache)return false;
    int32 Values[4];for(int32 I=0;I<4;++I){double V;if(!(*Counts)[I]->TryGetNumber(V)||V<0||V>49||V!=FMath::FloorToDouble(V))return false;Values[I]=int32(V);}
    for(const auto& V:*Repaired)if(V->Type!=EJson::Boolean)return false;
    for(const auto& V:*Used)if(V->Type!=EJson::Boolean)return false;
    double Fins=0;
    S->TryGetNumberField(TEXT("finsRemaining"),Fins);
    if(!FMath::IsFinite(Fins) || Fins<0 || Fins>60)return false;
    FAbyssConditionState Conditions;
    TSharedPtr<FJsonObject> ConditionJson;
    if (S->HasField(TEXT("conditions")))
    {
        if (!S->HasTypedField<EJson::Object>(TEXT("conditions"))) return false;
        ConditionJson = S->GetObjectField(TEXT("conditions"));
    }
    if (!UAbyssConditionsComponent::Deserialize(ConditionJson, Conditions)) return false;
    if(!G->Cargo->Restore(CargoJson,Values))return false;
    for(int32 I=0;I<4;++I)G->Inventory[I]=Values[I];for(int32 I=0;I<3;++I)G->Repaired[I]=(*Repaired)[I]->AsBool();
    int32 Seed;
    if(S->TryGetNumberField(TEXT("cacheLayoutSeed"),Seed)) G->SetCacheLayout(Seed);
    for(int32 I=0;I<G->Sites.Num();++I){auto& Site=G->Sites[I];Site.bUsed=Used->IsValidIndex(I)&&(*Used)[I]->AsBool();if(Site.Kind==FAbyssSite::LootCache && Site.OpenedVisual){if(Site.Visual)Site.Visual->SetActorHiddenInGame(Site.bUsed);Site.OpenedVisual->SetActorHiddenInGame(!Site.bUsed);}if(Site.Kind<4){if(Site.Visual)Site.Visual->SetActorHiddenInGame(Site.bUsed);if(Site.LabelVisual)Site.LabelVisual->SetActorHiddenInGame(Site.bUsed);}}
    G->FinsRemaining=Fins;
    G->Conditions->Restore(Conditions, Elapsed);
    G->bHasWelder=false;
    for (auto& Site:G->Sites)
    {
        if (Site.Kind!=FAbyssSite::Fins && Site.Kind!=FAbyssSite::Welder) continue;
        if (Site.Visual) Site.Visual->SetActorHiddenInGame(Site.bUsed);
        if (Site.LabelVisual) Site.LabelVisual->SetActorHiddenInGame(Site.bUsed);
        if (Site.Kind==FAbyssSite::Welder) G->bHasWelder=Site.bUsed;
    }
    G->Oxygen=Oxygen;G->Health=Health;G->BatteryRemaining=Battery;G->Elapsed=Elapsed;G->bLight=Light;G->bMenu=false;G->bEnded=false;G->bWon=false;G->bInventory=false;G->SalvageSite=INDEX_NONE;G->RepairSite=INDEX_NONE;G->RepairProgress=0;
    double Z=70; S->TryGetNumberField(TEXT("z"),Z); if(!FMath::IsFinite(Z)||Z < -1000||Z>10000) Z=70;
    if(auto P=UGameplayStatics::GetPlayerPawn(Context,0))P->SetActorLocation(FVector(X,Y,Z));
    G->UpdateSubmarineModel();
    G->RestoreCacheAmbushes();
    G->Notify(TEXT("Saved dive restored."));return true;
}

