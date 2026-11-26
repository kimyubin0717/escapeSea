#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AbyssController.generated.h"

class UAbyssUIComponent;

UCLASS()
class MAMA_API AAbyssController : public APlayerController
{
    GENERATED_BODY()
public:
    AAbyssController();
    virtual void OnPossess(APawn* InPawn) override;
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaSeconds) override;
    UPROPERTY() TObjectPtr<UAbyssUIComponent> GameUI;
};
