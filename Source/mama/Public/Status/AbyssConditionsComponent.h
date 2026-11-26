#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbyssConditionsComponent.generated.h"

class FJsonObject;

UENUM()
enum class EAbyssCondition : uint8
{
    None, Darkness, Disorientation, Hyperventilation, Cold
};

struct FAbyssConditionState
{
    EAbyssCondition Active = EAbyssCondition::None;
    EAbyssCondition Pending = EAbyssCondition::None;
    float Remaining = 0.f;
    float NextEvent = 0.f;
};

UCLASS()
class MAMA_API UAbyssConditionsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UAbyssConditionsComponent();
    void Reset();
    void Advance(float DeltaSeconds, float RunElapsed);
    const FAbyssConditionState& GetState() const { return State; }
    void Restore(const FAbyssConditionState& Saved, float RunElapsed);
    int32 GameDay(float RunElapsed) const;
    int32 GameMinute(float RunElapsed) const;
    float FirstEventTime() const { return SecondsPerDay * GraceDays; }
    float MovementMultiplier() const;
    float OxygenMultiplier() const;
    float LightMultiplier() const;
    float InputDirection() const;
    TSharedRef<FJsonObject> ToJson() const;
    static bool Deserialize(const TSharedPtr<FJsonObject>& Json, FAbyssConditionState& Out);

    UPROPERTY(EditDefaultsOnly, Category="Calendar", meta=(ClampMin="1", ClampMax="86400"))
    float SecondsPerDay = 600.f;
    UPROPERTY(EditDefaultsOnly, Category="Calendar", meta=(ClampMin="0", ClampMax="365"))
    int32 GraceDays = 14;
    UPROPERTY(EditDefaultsOnly, Category="Conditions", meta=(ClampMin="1", ClampMax="30"))
    float Duration = 15.f;
    UPROPERTY(EditDefaultsOnly, Category="Conditions", meta=(ClampMin="1", ClampMax="10"))
    float WarningDuration = 3.f;
    UPROPERTY(EditDefaultsOnly, Category="Conditions", meta=(ClampMin="5", ClampMax="300"))
    float MinimumInterval = 60.f;
    UPROPERTY(EditDefaultsOnly, Category="Conditions", meta=(ClampMin="5", ClampMax="300"))
    float MaximumInterval = 100.f;
    UPROPERTY(EditDefaultsOnly, Category="Conditions", meta=(ClampMin="0.1", ClampMax="1"))
    float ColdSpeed = 0.7f;
    UPROPERTY(EditDefaultsOnly, Category="Conditions", meta=(ClampMin="1", ClampMax="3"))
    float HyperventilationOxygen = 1.5f;
private:
    FAbyssConditionState State;
    FRandomStream Random;
};
