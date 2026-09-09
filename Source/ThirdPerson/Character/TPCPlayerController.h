// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TPCPlayerController.generated.h"

class APawn;
class UPlayerDeathWidget;
class UInventoryWidget;
class UHealthPotionWidget;
class UPauseMenuWidget;
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
