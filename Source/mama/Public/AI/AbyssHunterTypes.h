#pragma once

#include "CoreMinimal.h"
#include "AbyssHunterTypes.generated.h"

class AActor;
class UPointLightComponent;
class UAnimSequence;

USTRUCT()
struct FAbyssCreatureAnimation
{
    GENERATED_BODY()

    UPROPERTY() TObjectPtr<UAnimSequence> Movement;
    UPROPERTY() TObjectPtr<UAnimSequence> Idle;
    UPROPERTY() TObjectPtr<UAnimSequence> Search;
    UPROPERTY() TObjectPtr<UAnimSequence> Attack;
    float AttackEnds = 0.f;
};

USTRUCT()
struct FAbyssHunter
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<AActor> Actor;
    UPROPERTY() TObjectPtr<UPointLightComponent> Light;
    float Memory=0;
    FVector LastKnown=FVector::ZeroVector;
    int32 PatrolIndex=0;
};
