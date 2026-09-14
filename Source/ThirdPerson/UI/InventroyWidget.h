
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventroyWidget.generated.h"
class UInventoryComponent;
class UHealthComponent;
class ATPCGameMode;
class UInteractionComponent;
class UStaminaComponent;
class UUniformGridPanel;
class UInventorySlotWidget;
class ULevelComponent;
class AInventoryPreviewActor;
struct FLevelUpgradeChoice;
/** 游戏 HUD 与背包窗口入口：保持已有蓝图绑定，通过分页显示背包或属性，关闭窗口时停止角色预览捕获。 */
UCLASS()
class THIRDPERSON_API UInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Inventory") void SwitchPage(int32 PageIndex);
	void SetPresentationOpen(bool bOpen);
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void RefreshInventoryText(const FText& InventoryText);
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void SetInventoryPanelOpen(bool bOpen);
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void CloseInventory();
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void UseSelectedItem();
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void ShowInventoryHint(const FText& Message);
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void DropSelectedItem();
	void SetHealthComponent(UHealthComponent* InHealthComponent);
	void SetGameMode(ATPCGameMode* InGameMode);
	void SetInventory(UInventoryComponent* InInventory);
	void SetInteractionComponent(UInteractionComponent* InInteractionComponent);
	void SetStaminaComponent(UStaminaComponent* InStaminaComponent);
	void SetLevelComponent(ULevelComponent* InLevelComponent);
	UFUNCTION(BlueprintCallable, Category = "Level")
	void ChooseLevelUpgrade(int32 ChoiceIndex);
	
protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
	virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
	virtual void NativeDestruct() override;
	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth, float MaxHealth);

	UFUNCTION(BlueprintImplementableEvent)
	void RefreshHealthText(const FText& InText);
	UFUNCTION()
	void HandleEnemyDefeated(int32 NewDefeatedCount);

	UFUNCTION(BlueprintImplementableEvent)
	void RefreshKillText(const FText& InText);
	UFUNCTION()
	void HandleGameWon();

	UFUNCTION(BlueprintImplementableEvent)
	void RefreshGameResult(const FText& InText);
	UFUNCTION(BlueprintCallable)
	void RestartLevel();
	UFUNCTION(BlueprintCallable)
	void ClearSaveAndRestart();
	UFUNCTION()
	void HandleInteractionPromptChanged(const FText& PromptText);
	UFUNCTION(BlueprintImplementableEvent)
	void RefreshInteractionPrompt(const FText& PromptText,bool bVisible);
	
	UFUNCTION()
	void HandleStaminaChanged(float CurrentStamina, float MaxStamina);

	UFUNCTION(BlueprintImplementableEvent)
	void RefreshStaminaText(const FText& InText);
	
	UFUNCTION(BlueprintImplementableEvent)
	void RefreshStaminaPercent(float Percent);
	UFUNCTION(BlueprintImplementableEvent)
	void RefreshStaminaBarStyle(float Percent, bool bIsLowStamina);
	UFUNCTION()
	void HandleLevelProgressChanged(
		int32 Level,
		int32 CurrentExperience,
		int32 ExperienceToNextLevel);
	UFUNCTION()
	void HandleUpgradeChoicesReady(
		const FLevelUpgradeChoice& ChoiceA,
		const FLevelUpgradeChoice& ChoiceB,
		const FLevelUpgradeChoice& ChoiceC);
	UFUNCTION(BlueprintImplementableEvent, Category = "Level")
	void RefreshLevelProgress(const FText& LevelText, float ExperiencePercent);
	UFUNCTION(BlueprintImplementableEvent, Category = "Level")
	void ShowLevelUpChoices(
		const FText& ChoiceATitle,
		const FText& ChoiceADescription,
		const FText& ChoiceBTitle,
		const FText& ChoiceBDescription,
		const FText& ChoiceCTitle,
		const FText& ChoiceCDescription);
	UFUNCTION(BlueprintImplementableEvent, Category = "Level")
	void HideLevelUpChoices();
	UFUNCTION(BlueprintImplementableEvent)
	void RefreshSelectedItemInfo(const FText& ItemName,int32 ItemCount,bool bHasItem,bool bCanUse);
	UFUNCTION(BlueprintImplementableEvent)
	void RefreshObjectiveText(const FText& InText);
	void RefreshObjective();
private:
	UFUNCTION() void ShowBagPage();
	UFUNCTION() void ShowAttributesPage();
	UPROPERTY(Transient) TObjectPtr<AInventoryPreviewActor> Portrait;
	bool bPresentationOpen=false;
	double NextAttributeRefresh=0,NextPortraitCapture=0;
	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> InventoryComponent;
	UFUNCTION()
	void HandleInventoryChanged();
	UPROPERTY()
	TObjectPtr<UHealthComponent> HealthComponent;
	UPROPERTY(Transient)
	TObjectPtr<ATPCGameMode> GameMode;
	UPROPERTY(Transient)
	TObjectPtr<UInteractionComponent> InteractionComponent;
	UPROPERTY()
	TObjectPtr<UStaminaComponent> StaminaComponent;
	UPROPERTY()
	TObjectPtr<ULevelComponent> LevelComponent;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> InventoryGrid;
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSubclassOf<UInventorySlotWidget> InventorySlotWidgetClass;
	UPROPERTY(EditDefaultsOnly, Category = "Inventory",meta = (ClampMin = "1"))
	int32 InventoryGridColumns = 5;
	UFUNCTION()
	void HandleInventorySlotClicked(int32 ClickedSlotIndex);
	int32 SelectedInventorySlotIndex = INDEX_NONE;
	void RefreshInventoryGrid();
	bool bWasLowStamina = false;	
};
