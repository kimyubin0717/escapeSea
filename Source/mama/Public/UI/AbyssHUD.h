#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AbyssHUD.generated.h"

UCLASS()
class MAMA_API AAbyssHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
