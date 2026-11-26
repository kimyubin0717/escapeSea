#pragma once

#include "CoreMinimal.h"
#include "AbyssCityTypes.generated.h"

class AActor;
class UStaticMeshComponent;
class UMaterialInterface;

USTRUCT()
struct FAbyssCityWall
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Component;
    UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;
    bool bCastShadow=true;
};

USTRUCT()
struct FAbyssCityInterior
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<AActor> Roof;
    UPROPERTY() TObjectPtr<AActor> UpperStorey;
    UPROPERTY() TArray<FAbyssCityWall> Walls;
    FVector Center=FVector::ZeroVector;
    FVector Extent=FVector::ZeroVector;
    bool bInside=false;
};
