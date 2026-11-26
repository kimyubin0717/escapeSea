#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbyssInventoryComponent.generated.h"

struct FAbyssCargoItem
{
    FString Id;
    int32 Kind=0, X=0, Y=0;
    int32 Width() const { return Kind==3 ? 2 : Kind==4 ? 3 : 1; }
    int32 Height() const { return Kind==3 ? 2 : 1; }
};

UCLASS()
class MAMA_API UAbyssInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    TArray<FAbyssCargoItem> Items;
    int32 Serial=0;
    bool Fits(const FAbyssCargoItem& Item,int32 X,int32 Y) const;
    bool Add(int32 Kind);
    bool Move(const FString& Id,int32 X,int32 Y);
    void Sync(const int32* Counts);
    FString ToJson() const;
    bool Restore(const FString& Json,const int32* Counts);
};
