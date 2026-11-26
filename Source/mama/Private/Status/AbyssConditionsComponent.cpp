#include "Status/AbyssConditionsComponent.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"

UAbyssConditionsComponent::UAbyssConditionsComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UAbyssConditionsComponent::Reset()
{
    State = FAbyssConditionState();
    Random.Initialize(static_cast<int32>(FPlatformTime::Cycles()));
}

int32 UAbyssConditionsComponent::GameDay(float RunElapsed) const
{
    return 1 + FMath::FloorToInt(FMath::Max(0.f, RunElapsed) / SecondsPerDay);
}

int32 UAbyssConditionsComponent::GameMinute(float RunElapsed) const
{
    return FMath::FloorToInt(FMath::Fmod(FMath::Max(0.f, RunElapsed), SecondsPerDay) / SecondsPerDay * 1440.f);
}

void UAbyssConditionsComponent::Restore(const FAbyssConditionState& Saved, float RunElapsed)
{
    State = RunElapsed < FirstEventTime() ? FAbyssConditionState() : Saved;
}

void UAbyssConditionsComponent::Advance(float DeltaSeconds, float RunElapsed)
{
    if (!FMath::IsFinite(DeltaSeconds) || !FMath::IsFinite(RunElapsed) || DeltaSeconds <= 0.f) return;
    if (RunElapsed < FirstEventTime())
    {
        State = FAbyssConditionState();
        return;
    }
    DeltaSeconds = FMath::Min(DeltaSeconds, FMath::Max(0.f, RunElapsed - FirstEventTime()));
    if (State.Active == EAbyssCondition::None && State.Pending == EAbyssCondition::None)
    {
        State.NextEvent = FMath::Max(0.f, State.NextEvent - DeltaSeconds);
        if (State.NextEvent == 0.f)
        {
            State.Pending = static_cast<EAbyssCondition>(Random.RandRange(1, 4));
            State.Remaining = WarningDuration;
        }
        return;
    }
    State.Remaining = FMath::Max(0.f, State.Remaining - DeltaSeconds);
    if (State.Remaining > 0.f) return;
    if (State.Pending != EAbyssCondition::None)
    {
        State.Active = State.Pending;
        State.Pending = EAbyssCondition::None;
        State.Remaining = Duration;
        return;
    }
    State.Active = EAbyssCondition::None;
    State.NextEvent = Random.FRandRange(MinimumInterval, FMath::Max(MinimumInterval, MaximumInterval));
}

float UAbyssConditionsComponent::MovementMultiplier() const
{
    return State.Active == EAbyssCondition::Cold ? ColdSpeed : 1.f;
}

float UAbyssConditionsComponent::OxygenMultiplier() const
{
    return State.Active == EAbyssCondition::Hyperventilation ? HyperventilationOxygen : 1.f;
}

float UAbyssConditionsComponent::LightMultiplier() const
{
    return State.Active == EAbyssCondition::Darkness ? 0.2f : 1.f;
}

float UAbyssConditionsComponent::InputDirection() const
{
    return State.Active == EAbyssCondition::Disorientation ? -1.f : 1.f;
}

TSharedRef<FJsonObject> UAbyssConditionsComponent::ToJson() const
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("active"), static_cast<uint8>(State.Active));
    Json->SetNumberField(TEXT("pending"), static_cast<uint8>(State.Pending));
    Json->SetNumberField(TEXT("remaining"), State.Remaining);
    Json->SetNumberField(TEXT("next"), State.NextEvent);
    return Json;
}

bool UAbyssConditionsComponent::Deserialize(const TSharedPtr<FJsonObject>& Json, FAbyssConditionState& Out)
{
    FAbyssConditionState Parsed;
    if (!Json) { Out = Parsed; return true; }
    double Active, Pending, Remaining, Next;
    if (!Json->TryGetNumberField(TEXT("active"), Active) ||
        !Json->TryGetNumberField(TEXT("pending"), Pending) ||
        !Json->TryGetNumberField(TEXT("remaining"), Remaining) ||
        !Json->TryGetNumberField(TEXT("next"), Next)) return false;
    if (!FMath::IsFinite(Active) || !FMath::IsFinite(Pending) ||
        !FMath::IsFinite(Remaining) || !FMath::IsFinite(Next) ||
        Active < 0 || Active > 4 || Active != FMath::FloorToDouble(Active) ||
        Pending < 0 || Pending > 4 || Pending != FMath::FloorToDouble(Pending) ||
        Remaining < 0 || Remaining > 30 || Next < 0 || Next > 300 ||
        (Active > 0 && Pending > 0) || ((Active > 0 || Pending > 0) != (Remaining > 0))) return false;
    Parsed.Active = static_cast<EAbyssCondition>(static_cast<uint8>(Active));
    Parsed.Pending = static_cast<EAbyssCondition>(static_cast<uint8>(Pending));
    Parsed.Remaining = Remaining;
    Parsed.NextEvent = Next;
    Out = Parsed;
    return true;
}
