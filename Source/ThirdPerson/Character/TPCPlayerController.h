
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TPCPlayerController.generated.h"

class APawn;
class UPlayerDeathWidget;
class UInventoryWidget;
class UHealthPotionWidget;
class UPauseMenuWidget;
/** 玩家输入与界面的协调层：管理 HUD、背包、暂停和死亡界面的焦点，以及重生后的重新绑定。 */
UCLASS()
class THIRDPERSON_API ATPCPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	void ToggleInventory();
	void TogglePauseMenu();
	/** Restores controller and pawn input after UI-only victory/restart states. */
	UFUNCTION(BlueprintCallable, Category = "Input")
	void RestoreGameplayInput();
    void ShowDeathScreen();
    UFUNCTION(BlueprintCallable, Category="Death") void RestartAfterDeath();
    UFUNCTION(BlueprintPure, Category="Death") bool IsDeathScreenOpen() const;
    UPlayerDeathWidget* GetDeathScreen() const { return DeathScreen; }
	UHealthPotionWidget* GetHealthPotionWidget() const { return HealthPotionWidget; }
protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UInventoryWidget> InventoryWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryWidget> InventoryWidget;
	
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UPauseMenuWidget> PauseMenuWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UPauseMenuWidget> PauseMenuWidget;

	bool bIsPauseMenuOpen = false;
private:
    UPROPERTY(Transient) TObjectPtr<UHealthPotionWidget> HealthPotionWidget;
    UPROPERTY(Transient) TObjectPtr<UPlayerDeathWidget> DeathScreen;
	void ConnectInventory(APawn* InPawn);
	bool bIsInventoryOpen = false;
};
