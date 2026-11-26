#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AI/AbyssHunterTypes.h"
#include "World/AbyssCityTypes.h"
#include "World/AbyssSite.h"
#include "AbyssGameMode.generated.h"

class UAbyssInventoryComponent;
class UAbyssConditionsComponent;
class ADirectionalLight;
class AExponentialHeightFog;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMeshComponent;

UCLASS()
class MAMA_API AAbyssGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AAbyssGameMode();
    UPROPERTY() TObjectPtr<UAbyssInventoryComponent> Cargo;
    UPROPERTY() TObjectPtr<UAbyssConditionsComponent> Conditions;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void Interact();
    void UseOxygen();
    void ToggleFlashlight();
    void AdvanceBattery(float DeltaSeconds);
    void AdvanceSalvage(float DeltaSeconds);
    void SubmitSalvage();
    void FinishSalvage(bool bSuccess);
    void Notify(const FString& Message);
    int32 NearestSite() const;
    const FAbyssSite* FirstDiveTarget() const;
    FString SitePrompt(int32 Index) const;
    UStaticMeshComponent* Shape(const FVector& Position, const FVector& Scale, const FLinearColor& Color, bool bSolid = true, const FString& Mesh = TEXT("Cube"));
    AActor* Label(const FVector& Position, const FString& Text, const FColor& Color, float Size = 55);
    void AddSite(const FVector& Position, int32 Kind, const FString& Name);
    void BuildSeafloor();
    void BuildImportedCity();
    int32 HarborHatchIndex=INDEX_NONE;
    float FinsRemaining=0;
    bool bWelderRequired=false;
    bool bHasWelder=false;
    float SwimSpeed(bool Sprint) const;
    void SpawnCityHunters();
    void UpdateSubmarineModel();
    bool bImportedCity = false;
    float CityScale = 1.f;
    float MapHalfExtent = 3450.f * 0.55f;
    void EndRun(bool bEscape, const FString& Reason);
    void RunSmokeTest();
    int32 NextRepairStage() const;
    void DamageDiver(float Amount);
    void UpdateBuilding();
    void TickHunter(AActor* Enemy, UPointLightComponent* Light, float& EnemyMemory, FVector& Known, int32& Route, float Dt, bool bRepairing);
    float OxygenRate(bool bSprint) const;

    UPROPERTY() TArray<FAbyssSite> Sites;
    UPROPERTY() TObjectPtr<AActor> RoamingKraken;
    float KrakenHeight=-1.f;
    float KrakenRadius() const;
    FVector AdvanceKraken(float Dt);
    int32 CacheLayoutSeed=0;
    void SetCacheLayout(int32 Seed);
    void AppendExplorationCaches();
    bool SpawnCacheAmbush(int32 SiteIndex);
    void RestoreCacheAmbushes();
    void RunExplorationCheck();
    void RunMovementCheck();
    void RunAuditCheck();
    UPROPERTY(EditDefaultsOnly, Category="Exploration", meta=(ClampMin="0", ClampMax="1"))
    float CacheAmbushChance = 0.15f;
    UPROPERTY(EditDefaultsOnly, Category="Exploration", meta=(ClampMin="0", ClampMax="3"))
    int32 MaximumCacheAmbushes = 3;
    UPROPERTY() TMap<int32, TObjectPtr<AActor>> CacheMonsters;
    UPROPERTY() TMap<TObjectPtr<AActor>, FVector> ExplorationHomes;
    void SetResourceDropLayout(int32 Seed);
    UPROPERTY() TArray<FAbyssCityInterior> CityInteriors;
    UPROPERTY() TObjectPtr<UMaterialInterface> CityWallGlass;
    UPROPERTY() TObjectPtr<AActor> Hunter;
    UPROPERTY() TObjectPtr<UPointLightComponent> HunterLight;
    UPROPERTY() TObjectPtr<ADirectionalLight> OceanLight;
    UPROPERTY() TObjectPtr<AExponentialHeightFog> Fog;
    UPROPERTY() TArray<FAbyssHunter> ExtraHunters;
    UPROPERTY() TMap<TObjectPtr<AActor>, FAbyssCreatureAnimation> CreatureAnimations;
    void BindCreatureAnimation(AActor* Creature);
    void UpdateCreatureAnimation(AActor* Creature, bool bMoving, bool bSearching, bool bAttacking);
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BuildingWalls;
    UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> BuildingMaterials;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BuildingGlass;
    FVector BuildingCenter=FVector(-935,495,0);
    FVector BuildingExtent=FVector(385,330,0);
    bool bInsideBuilding=false;
    TArray<FVector> Patrol;
    int32 PatrolIndex = 0;
    int32 Inventory[4] = {0, 0, 0, 2};
    bool Repaired[3] = {false, false, false};
    float Health = 100;
    float DamageCooldown = 0;
    float MissingPartsUntil = 0;
    int32 MissingPart = INDEX_NONE;
    int32 SmokeRepairStage = 0;
    static constexpr float OxygenCapacity = 200.f;
    static constexpr float OxygenRefill = 55.f;
    static constexpr float BatteryRefill = 60.f;
    float Oxygen = OxygenCapacity;
    UPROPERTY(EditDefaultsOnly, Category="Survival", meta=(ClampMin="1"))
    float FlashlightDuration = 120;
    UPROPERTY(EditDefaultsOnly, Category="Survival", meta=(ClampMin="0.1"))
    float BubbleInterval = 1.2f;
    float BatteryRemaining = 120;
    int32 SalvageSite = INDEX_NONE;
    float SalvageTime = 0;
    float SalvageTarget = 0.45f;
    UPROPERTY(EditDefaultsOnly, Category="Salvage", meta=(ClampMin="0.5"))
    float SalvageDuration = 2.4f;
    UPROPERTY(EditDefaultsOnly, Category="Salvage", meta=(ClampMin="0.05", ClampMax="0.5"))
    float SalvageWindow = 0.17f;
    float Elapsed = 0;
    float RepairProgress = 0;
    int32 RepairSite = INDEX_NONE;
    float Alert = 0;
    float Memory = 0;
    FVector LastKnown = FVector::ZeroVector;
    float NoticeTime = 0;
    FString Notice;
    FString Ending;
    bool bInventory = false;
    bool bMenu = false;
    int32 NoticeSerial = 0;
    FString NoticeKind;
    bool bHidden = false;
    bool bEnded = false;
    bool bWon = false;
    bool bLight = true;
    bool bChasing = false;
    bool bSmokeTest = false;
    int32 SmokeStage = 0;
    float SmokeDeadline = 0;
    FVector SmokeStart = FVector::ZeroVector;
    float SmokeOxygen = 0;
};
