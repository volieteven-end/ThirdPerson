#pragma once
#include "CoreMinimal.h"
#include "../GameMode/TPCGameMode.h"
#include "Blueprint/UserWidget.h"
#include "ArenaGameMode.generated.h"
class AArenaWaveDirector;
class ACountessBossCharacter;

/** 竞技场状态显示，只读取波次管理器的信息，不生成敌人或结算奖励。 */
UCLASS()
class THIRDPERSON_API UArenaHUDWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};

/** 竞技场规则入口：配置出生、HUD 与挑战生命周期，避免套用普通关卡的击杀通关条件。 */
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
