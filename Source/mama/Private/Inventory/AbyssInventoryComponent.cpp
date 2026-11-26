#include "Inventory/AbyssInventoryComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace { const TCHAR* Kinds[]={TEXT("power"),TEXT("hull"),TEXT("drive"),TEXT("tank"),TEXT("flashlight")}; }
bool UAbyssInventoryComponent::Fits(const FAbyssCargoItem& Item,int32 X,int32 Y) const
{
    if(X<0 || Y<0 || X+Item.Width()>7 || Y+Item.Height()>7) return false;
    for(const auto& Other:Items)
        if(Other.Id!=Item.Id && X<Other.X+Other.Width() && X+Item.Width()>Other.X && Y<Other.Y+Other.Height() && Y+Item.Height()>Other.Y) return false;
    return true;
}
bool UAbyssInventoryComponent::Add(int32 Kind)
{
    if(Kind<0 || Kind>4) return false;
    FAbyssCargoItem Item;Item.Id=FString::Printf(TEXT("cargo-%d"),++Serial);Item.Kind=Kind;
    for(int32 Y=0;Y<7;++Y) for(int32 X=0;X<7;++X)
        if(Fits(Item,X,Y)){Item.X=X;Item.Y=Y;Items.Add(Item);return true;}
    return false;
}
bool UAbyssInventoryComponent::Move(const FString& Id,int32 X,int32 Y)
{
    for(auto& Item:Items) if(Item.Id==Id && Fits(Item,X,Y)){Item.X=X;Item.Y=Y;return true;}
    return false;
}
void UAbyssInventoryComponent::Sync(const int32* Counts)
{
    for(int32 K=0;K<5;++K)
    {
        const int32 Wanted=K==4 ? 1 : Counts[K];
        int32 Have=Items.FilterByPredicate([K](const auto& I){return I.Kind==K;}).Num();
        for(int32 I=Items.Num()-1;I>=0 && Have>Wanted;--I) if(Items[I].Kind==K){Items.RemoveAt(I);--Have;}
    }
    // Place the large flashlight first on initial creation.
    for(int32 K=4;K>=0;--K)
    {
        int32 Have=Items.FilterByPredicate([K](const auto& I){return I.Kind==K;}).Num();
        const int32 Wanted=K==4 ? 1 : Counts[K];
        while(Have<Wanted && Add(K)) ++Have;
    }
}
FString UAbyssInventoryComponent::ToJson() const
{
    TArray<TSharedPtr<FJsonValue>> Values;
    for(const auto& I:Items)
    {
        auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("id"),I.Id); O->SetStringField(TEXT("kind"),Kinds[I.Kind]);
        O->SetNumberField(TEXT("x"),I.X);O->SetNumberField(TEXT("y"),I.Y);O->SetNumberField(TEXT("w"),I.Width());O->SetNumberField(TEXT("h"),I.Height());
        Values.Add(MakeShared<FJsonValueObject>(O));
    }
    FString Json; FJsonSerializer::Serialize(Values,TJsonWriterFactory<>::Create(&Json)); return Json;
}
bool UAbyssInventoryComponent::Restore(const FString& Json,const int32* Counts)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Values) || Values.Num()>49) return false;
    auto Old=Items;Items.Reset();int32 Actual[5]={0,0,0,0,0};
    for(const auto& V:Values)
    {
        const auto O=V->AsObject(); FString Id,Kind;double X,Y;
        if(!O || !O->TryGetStringField(TEXT("id"),Id) || !O->TryGetStringField(TEXT("kind"),Kind) || !O->TryGetNumberField(TEXT("x"),X) || !O->TryGetNumberField(TEXT("y"),Y)) {Items=Old;return false;}
        int32 K=INDEX_NONE;for(int32 I=0;I<5;++I) if(Kind==Kinds[I]) K=I;
        if(K==INDEX_NONE || X!=FMath::FloorToDouble(X) || Y!=FMath::FloorToDouble(Y) || X<0 || X>6 || Y<0 || Y>6 || Id.IsEmpty() || Items.ContainsByPredicate([&](const auto& I){return I.Id==Id;})){Items=Old;return false;}
        FAbyssCargoItem Item;Item.Id=Id;Item.Kind=K;Item.X=int32(X);Item.Y=int32(Y);
        if(!Fits(Item,Item.X,Item.Y)){Items=Old;return false;} Items.Add(Item);++Actual[K];
    }
    for(int32 K=0;K<5;++K) if(Actual[K]!=(K==4 ? 1 : Counts[K])){Items=Old;return false;}
    Serial+=100;return true;
}
