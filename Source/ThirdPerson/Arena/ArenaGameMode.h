#pragma once
#include "CoreMinimal.h"
#include "../GameMode/TPCGameMode.h"
#include "Blueprint/UserWidget.h"
#include "ArenaGameMode.generated.h"
class AArenaWaveDirector;
class ACountessBossCharacter;

UCLASS()
class THIRDPERSON_API UArenaHUDWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};

UCLASS()
class THIRDPERSON_API AArenaGameMode : public ATPCGameMode
{
    GENERATED_BODY()
public:
    AArenaGameMode();
    virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;
    virtual void Tick(float Delta) override;
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY(Transient) TObjectPtr<UArenaHUDWidget> ArenaHUD;
    TWeakObjectPtr<ACountessBossCharacter> Boss;
    bool bBossRewardSaved = false;
};
