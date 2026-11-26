#include "Save/AbyssSaveService.h"
#include "Common/AbyssShared.h"
#include "Inventory/AbyssInventoryComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
namespace {
FString Encode(const TSharedRef<FJsonObject>& O){FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
TSharedPtr<FJsonObject> Decode(const FString& S){TSharedPtr<FJsonObject> O;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),O);return O;}
}
FString FAbyssSaveService::SavePath(const UObject* Context) {return FPaths::ProjectSavedDir()/(Abyss::Game(Context) && Abyss::Game(Context)->bImportedCity ? (Abyss::Game(Context)->CityScale<1 ? TEXT("SaveGames/AbyssCompactRun.json") : TEXT("SaveGames/AbyssCityRun.json")) : TEXT("SaveGames/AbyssRun.json"));}
bool FAbyssSaveService::HasSave(const UObject* Context)
{
    FString S;if(!FFileHelper::LoadFileToString(S,*SavePath(Context)))return false;
    const auto O=Decode(S);double Version;
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
    S->SetArrayField(TEXT("counts"),Counts);S->SetArrayField(TEXT("repaired"),Repaired);S->SetArrayField(TEXT("used"),Used);
    if(auto P=UGameplayStatics::GetPlayerPawn(Context,0)){const auto Pos=P->GetActorLocation();S->SetNumberField(TEXT("x"),Pos.X);S->SetNumberField(TEXT("y"),Pos.Y);S->SetNumberField(TEXT("z"),Pos.Z);}
    O->SetObjectField(TEXT("state"),S);O->SetStringField(TEXT("cargo"),G->Cargo->ToJson());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(SavePath(Context)),true);
    return FFileHelper::SaveStringToFile(Encode(O),*SavePath(Context),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
bool FAbyssSaveService::LoadRun(const UObject* Context)
{
    auto G=Abyss::Game(Context);FString Json;if(!G || !FFileHelper::LoadFileToString(Json,*SavePath(Context)))return false;
    const auto O=Decode(Json);if(!O || !O->HasTypedField<EJson::Object>(TEXT("state")))return false;
    const auto S=O->GetObjectField(TEXT("state"));
    double Oxygen,Health,Battery,Elapsed,X,Y;bool Light;FString CargoJson;
    if(!S->TryGetNumberField(TEXT("oxygen"),Oxygen)||!S->TryGetNumberField(TEXT("health"),Health)||!S->TryGetNumberField(TEXT("battery"),Battery)||!S->TryGetNumberField(TEXT("elapsed"),Elapsed)||!S->TryGetNumberField(TEXT("x"),X)||!S->TryGetNumberField(TEXT("y"),Y)||!S->TryGetBoolField(TEXT("light"),Light)||!O->TryGetStringField(TEXT("cargo"),CargoJson))return false;
    if(!FMath::IsFinite(Oxygen)||!FMath::IsFinite(Health)||!FMath::IsFinite(Battery)||!FMath::IsFinite(Elapsed)||!FMath::IsFinite(X)||!FMath::IsFinite(Y)||Oxygen<0||Oxygen>100||Health<=0||Health>100||Battery<0||Battery>G->FlashlightDuration||Elapsed<0||Elapsed>1e8||FMath::Abs(X)>G->MapHalfExtent||FMath::Abs(Y)>G->MapHalfExtent)return false;
    const TArray<TSharedPtr<FJsonValue>> *Counts,*Repaired,*Used;
    if(!S->TryGetArrayField(TEXT("counts"),Counts)||Counts->Num()!=4||!S->TryGetArrayField(TEXT("repaired"),Repaired)||Repaired->Num()!=3||!S->TryGetArrayField(TEXT("used"),Used)||Used->Num()!=G->Sites.Num())return false;
    int32 Values[4];for(int32 I=0;I<4;++I){double V;if(!(*Counts)[I]->TryGetNumber(V)||V<0||V>49||V!=FMath::FloorToDouble(V))return false;Values[I]=int32(V);}
    for(const auto& V:*Repaired)if(V->Type!=EJson::Boolean)return false;
    for(const auto& V:*Used)if(V->Type!=EJson::Boolean)return false;
    if(!G->Cargo->Restore(CargoJson,Values))return false;
    for(int32 I=0;I<4;++I)G->Inventory[I]=Values[I];for(int32 I=0;I<3;++I)G->Repaired[I]=(*Repaired)[I]->AsBool();
    for(int32 I=0;I<G->Sites.Num();++I){auto& Site=G->Sites[I];Site.bUsed=(*Used)[I]->AsBool();if(Site.Kind<4){if(Site.Visual)Site.Visual->SetActorHiddenInGame(Site.bUsed);if(Site.LabelVisual)Site.LabelVisual->SetActorHiddenInGame(Site.bUsed);}}
    G->Oxygen=Oxygen;G->Health=Health;G->BatteryRemaining=Battery;G->Elapsed=Elapsed;G->bLight=Light;G->bMenu=false;G->bEnded=false;G->bWon=false;G->bInventory=false;G->SalvageSite=INDEX_NONE;G->RepairSite=INDEX_NONE;G->RepairProgress=0;
    double Z=70; S->TryGetNumberField(TEXT("z"),Z); if(!FMath::IsFinite(Z)||Z < -1000||Z>10000) Z=70;
    if(auto P=UGameplayStatics::GetPlayerPawn(Context,0))P->SetActorLocation(FVector(X,Y,Z));
    G->Notify(TEXT("Saved dive restored."));return true;
}

