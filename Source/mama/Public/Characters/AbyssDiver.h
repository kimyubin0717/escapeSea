#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AbyssDiver.generated.h"

class USphereComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UAnimSequence;
class UCameraComponent;
class USpringArmComponent;
class USpotLightComponent;
class UPointLightComponent;
class UFloatingPawnMovement;

UCLASS()
class MAMA_API AAbyssDiver : public APawn
{
    GENERATED_BODY()
public:
    AAbyssDiver();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY() TObjectPtr<USphereComponent> Collision;
    UPROPERTY() TObjectPtr<UFloatingPawnMovement> Movement;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> DiverMesh;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> HandLamp;
    UPROPERTY() TObjectPtr<UAnimSequence> IdleClip;
    UPROPERTY() TObjectPtr<UAnimSequence> SwimClip;
    UPROPERTY() TObjectPtr<UAnimSequence> SprintClip;
    UPROPERTY() TObjectPtr<UAnimSequence> CurrentClip;
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY() TObjectPtr<USpotLightComponent> Flashlight;
    UPROPERTY() TObjectPtr<UPointLightComponent> DiveGlow;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Bubbles;
    TArray<float> BubbleAges;
    TArray<FVector> BubbleOrigins;
    float BubbleClock = 0;
    float BubbleTime = 0;
    int32 NextBubble = 0;
    int32 BubbleBursts = 0;
    void UpdateBubbles(float DeltaSeconds, bool bBreathing);
    bool bAngledView=true;
    float ViewYaw=180.f;
    float ViewPitch=-50.f;
    void Orbit(float DeltaX, float DeltaY=0.f);
    void ToggleView();
    void MoveDiver(const FVector& Delta);
    bool bSprinting = false;
private:
    bool bHadGameplayFocus=false;
};
