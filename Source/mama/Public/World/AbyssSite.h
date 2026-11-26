#pragma once

#include "CoreMinimal.h"
#include "AbyssSite.generated.h"

class AActor;
class UStaticMeshComponent;

enum class EAbyssResourceDrop : uint8 { ReserveTank, Oxygen, Battery };

USTRUCT()
struct FAbyssSite
{
    GENERATED_BODY()
    static constexpr int32 LootCache = 9;
    static constexpr int32 Fins=10, Welder=11;
    EAbyssResourceDrop ResourceDrop = EAbyssResourceDrop::ReserveTank;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> BatteryVisual;
    int32 CacheReward=0;
    FVector CacheAnchor=FVector::ZeroVector;
    FVector Position = FVector::ZeroVector;
    FString Name;
    int32 Kind = 0; // 0-2 salvage, 3 oxygen, 4 kelp, 5-7 repair, 8 exit
    bool bUsed = false;
    bool bAmbushPlanned = false;
    UPROPERTY() TObjectPtr<AActor> Visual;
    UPROPERTY() TObjectPtr<AActor> LabelVisual;
    UPROPERTY() TObjectPtr<AActor> InteriorRoof;
    UPROPERTY() TObjectPtr<AActor> OpenedVisual;
};
