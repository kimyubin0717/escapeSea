#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbyssUIComponent.generated.h"
class SWebBrowser;
class SWidget;
class AAbyssGameMode;

/** Hosts the same HTML used by the preview and synchronizes it with the real game. */
UCLASS()
class MAMA_API UAbyssUIComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UAbyssUIComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Dt,ELevelTick TickType,FActorComponentTickFunction* ThisTick) override;
    void HandleAction(const FString& Action,const FString& Payload=TEXT(""));
    bool IsConnected() const { return bReady; }
    void SendState();
    bool SaveRun();
    bool LoadRun();
    bool HasSave() const;
private:
    TSharedPtr<SWebBrowser> Browser;
    TSharedPtr<SWidget> ViewportWidget;
    bool bReady=false,bInteractive=false;
    float SyncClock=0,SaveClock=0;
    AAbyssGameMode* Game() const;
};
