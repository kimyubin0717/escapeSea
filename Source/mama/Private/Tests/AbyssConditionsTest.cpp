#include "Status/AbyssConditionsComponent.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbyssConditionsTest, "Abyss.Conditions",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAbyssConditionsTest::RunTest(const FString& Parameters)
{
    auto* Conditions = NewObject<UAbyssConditionsComponent>();
    Conditions->Reset();
    TestFalse(TEXT("No independent component tick"), Conditions->PrimaryComponentTick.bCanEverTick);
    TestEqual(TEXT("Day one at game start"), Conditions->GameDay(0.f), 1);
    TestEqual(TEXT("One day is ten real minutes"), Conditions->GameDay(600.f), 2);
    TestEqual(TEXT("Clock shows noon halfway through a day"), Conditions->GameMinute(300.f), 720);
    TestEqual(TEXT("Clock resets at midnight"), Conditions->GameMinute(600.f), 0);
    TestEqual(TEXT("Before fourteen full days"), Conditions->GameDay(8399.f), 14);
    TestEqual(TEXT("Fourteen full days starts day fifteen"), Conditions->GameDay(8400.f), 15);
    Conditions->Advance(8399.f, 8399.f);
    TestTrue(TEXT("Initial grace period"), Conditions->GetState().Pending == EAbyssCondition::None);
    Conditions->Advance(1.f, 8400.f);
    TestTrue(TEXT("Warning before effect"), Conditions->GetState().Pending != EAbyssCondition::None &&
        Conditions->GetState().Active == EAbyssCondition::None);
    TestEqual(TEXT("Warning has no movement penalty"), Conditions->MovementMultiplier(), 1.f);
    Conditions->Advance(3.f, 8403.f);
    TestTrue(TEXT("One active effect after warning"), Conditions->GetState().Active != EAbyssCondition::None &&
        Conditions->GetState().Pending == EAbyssCondition::None);
    Conditions->Advance(15.f, 8418.f);
    TestTrue(TEXT("Recovery and cooldown"), Conditions->GetState().Active == EAbyssCondition::None &&
        Conditions->GetState().NextEvent >= 60.f && Conditions->GetState().NextEvent <= 100.f);

    for (uint8 Kind = 1; Kind <= 4; ++Kind)
    {
        FAbyssConditionState State;
        State.Active = static_cast<EAbyssCondition>(Kind);
        State.Remaining = 10.f;
        Conditions->Restore(State, 8400.f);
        TestEqual(TEXT("Cold speed modifier"), Conditions->MovementMultiplier(), Kind == 4 ? .7f : 1.f);
        TestEqual(TEXT("Hyperventilation oxygen modifier"), Conditions->OxygenMultiplier(), Kind == 3 ? 1.5f : 1.f);
        TestEqual(TEXT("Darkness light modifier"), Conditions->LightMultiplier(), Kind == 1 ? .2f : 1.f);
        TestEqual(TEXT("Direction reversal modifier"), Conditions->InputDirection(), Kind == 2 ? -1.f : 1.f);
        FAbyssConditionState Loaded;
        TestTrue(TEXT("Save state deserializes"), UAbyssConditionsComponent::Deserialize(Conditions->ToJson(), Loaded));
        TestTrue(TEXT("Active effect and remaining time survive save"), Loaded.Active == State.Active && Loaded.Remaining == 10.f);
        Conditions->Advance(10.f, 8410.f);
        TestEqual(TEXT("Movement recovers"), Conditions->MovementMultiplier(), 1.f);
        TestEqual(TEXT("Oxygen recovers"), Conditions->OxygenMultiplier(), 1.f);
        TestEqual(TEXT("Light recovers"), Conditions->LightMultiplier(), 1.f);
        TestEqual(TEXT("Input recovers"), Conditions->InputDirection(), 1.f);
    }
    FAbyssConditionState Loaded;
    TestTrue(TEXT("Older saves without condition data are accepted"), UAbyssConditionsComponent::Deserialize(nullptr, Loaded));
    TestTrue(TEXT("Older saves inherit the calendar gate"), Loaded.Active == EAbyssCondition::None && Loaded.NextEvent == 0.f);
    FAbyssConditionState OldActive;
    OldActive.Active = EAbyssCondition::Cold;
    OldActive.Remaining = 10.f;
    Conditions->Restore(OldActive, 600.f);
    TestEqual(TEXT("Old active effects are cleared before day fifteen"), Conditions->MovementMultiplier(), 1.f);
    Conditions->Restore(OldActive, 9000.f);
    TestEqual(TEXT("Eligible saved effects are retained"), Conditions->MovementMultiplier(), .7f);
    auto Invalid = Conditions->ToJson();
    Invalid->SetNumberField(TEXT("active"), 4);
    Invalid->SetNumberField(TEXT("pending"), 2);
    Invalid->SetNumberField(TEXT("remaining"), 10);
    TestFalse(TEXT("Overlapping conditions are rejected"), UAbyssConditionsComponent::Deserialize(Invalid, Loaded));
    Invalid->SetNumberField(TEXT("pending"), 0);
    Invalid->SetNumberField(TEXT("remaining"), -1);
    TestFalse(TEXT("Negative duration is rejected"), UAbyssConditionsComponent::Deserialize(Invalid, Loaded));
    Invalid->SetNumberField(TEXT("remaining"), 10);
    Invalid->SetNumberField(TEXT("active"), 1.5);
    TestFalse(TEXT("Fractional effect IDs are rejected"), UAbyssConditionsComponent::Deserialize(Invalid, Loaded));
    return true;
}
#endif
