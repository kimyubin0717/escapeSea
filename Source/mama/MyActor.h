#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyActor.generated.h"

UCLASS()
class MAMA_API AMyActor : public AActor
{
    GENERATED_BODY()

public:
    AMyActor();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"))
    int32 number1 = 30;
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float number2 = 3.14f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString name = TEXT("안녕");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool isReady = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool isFinished = false;

    float Add(float num1, float num2);
};
