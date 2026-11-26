#include "MyActor.h"
#include "CodingTest.h"

AMyActor::AMyActor()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AMyActor::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("%d"), number1);
    UE_LOG(LogTemp, Warning, TEXT("%.2f"), number2);
    UE_LOG(LogTemp, Warning, TEXT("%s"), *name);

    float result = Add(number1, number2);
    UE_LOG(LogTemp, Warning, TEXT("%.2f"), result);

    for (int32 i = 0; i < 100; i++)
    {
        number1++;
        UE_LOG(LogTemp, Warning, TEXT("%d"), number1);
    }

}

void AMyActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

float AMyActor::Add(float num1, float num2)
{
    return num1 + num2;
}