// Fill out your copyright notice in the Description page of Project Settings.


#include "InventroyWidget.h"
#include "../Save/TPCSaveSlots.h"
#include "../Components/InventoryComponent.h"
#include "../Components/HealthComponent.h"
#include "../GameMode/TPCGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"	
#include "../Components/InteractionComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Character/TPCPlayerController.h"
#include "Components/UniformGridPanel.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/UniformGridSlot.h"
#include "InventorySlotWidget.h"
#include "../Items/ItemDefinition.h"
#include "../Items/PickupActor.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "../Components/LevelComponent.h"
namespace
{
// Optional presentation widgets keep older Inventory blueprints compatible.
void RefreshForestItemPreview(UUserWidget* Widget, const UItemDefinition* Definition, bool bHasItem)
{
    if (UImage* Preview = Cast<UImage>(Widget->GetWidgetFromName(TEXT("ForestSelectedItemImage"))))
    {
        UTexture2D* Icon = Definition ? Definition->Icon.Get() : nullptr;
        Preview->SetBrushFromTexture(Icon);
        Preview->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    if (UWidget* Empty = Widget->GetWidgetFromName(TEXT("ForestEmptySelection")))
    {
        Empty->SetVisibility(bHasItem ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
}
}

void UInventoryWidget::SetInventory(UInventoryComponent* InInventory)
{
	if (InventoryComponent)
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this,&ThisClass::HandleInventoryChanged);
	}
	InventoryComponent = InInventory;

	if (InventoryComponent)
	{
		InventoryComponent->OnInventoryChanged.AddDynamic(this,&ThisClass::HandleInventoryChanged);
		HandleInventoryChanged();
	}
}

void UInventoryWidget::HandleInventoryChanged()
{
	if (!InventoryComponent)
	{
		return;
	}

	FString Contents;

	for (const FInventorySlot& InventorySlot :
		InventoryComponent->Slots)
	{
		Contents += FString::Printf(
			TEXT("%s x%d\n"),
			*InventorySlot.ItemId.ToString(),
			InventorySlot.Count);
	}

	if (Contents.IsEmpty())
	{
		Contents = TEXT("Inventory Empty");
	}

	RefreshInventoryText(FText::FromString(Contents));
    // Refresh count/icon even when inventory changes externally while the panel is open.
    HandleInventorySlotClicked(SelectedInventorySlotIndex);
}

void UInventoryWidget::NativeDestruct()
{
	if (InventoryComponent)
	{
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this,&ThisClass::HandleInventoryChanged);
	}
	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(this,&ThisClass::HandleHealthChanged);
	}

	if (GameMode)
	{
		GameMode->OnEnemyDefeated.RemoveDynamic(this,&ThisClass::HandleEnemyDefeated);
		GameMode->OnGameWon.RemoveDynamic(this,&ThisClass::HandleGameWon);
	}
	if (InteractionComponent)
	{
		InteractionComponent->OnInteractionPromptChanged.RemoveDynamic(this,&ThisClass::HandleInteractionPromptChanged);
	}
	if (StaminaComponent)
	{
		StaminaComponent->OnStaminaChanged.RemoveDynamic(this, &ThisClass::HandleStaminaChanged);
	}
	if (LevelComponent)
	{
		LevelComponent->OnLevelProgressChanged.RemoveDynamic(
			this, &ThisClass::HandleLevelProgressChanged);
		LevelComponent->OnUpgradeChoicesReady.RemoveDynamic(
			this, &ThisClass::HandleUpgradeChoicesReady);
	}
	Super::NativeDestruct();
}
void UInventoryWidget::SetHealthComponent(
	UHealthComponent* InHealthComponent)
{
	if (HealthComponent == InHealthComponent)
	{
		return;
	}

	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(
			this,
			&ThisClass::HandleHealthChanged);
	}

	HealthComponent = InHealthComponent;

	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.AddDynamic(
			this,
			&ThisClass::HandleHealthChanged);

		HandleHealthChanged(
			HealthComponent->GetCurrentHealth(),
			HealthComponent->GetMaxHealth());
	}
}void UInventoryWidget::HandleHealthChanged(
	float CurrentHealth,
	float MaxHealth)
{
	const FString Text = FString::Printf(
		TEXT("HP: %.0f / %.0f"),
		CurrentHealth,
		MaxHealth);

	RefreshHealthText(FText::FromString(Text));
    // Drive the actual fill as well as its text; Designer Percent is only a preview value.
    if (UProgressBar* Bar = Cast<UProgressBar>(GetWidgetFromName(TEXT("HPbar"))))
    {
        Bar->SetPercent(MaxHealth > 0.f ? FMath::Clamp(CurrentHealth / MaxHealth, 0.f, 1.f) : 0.f);
    }
}
void UInventoryWidget::SetGameMode(ATPCGameMode* InGameMode)
{
	if (GameMode == InGameMode)
	{
		return;
	}

	if (GameMode)
	{
		GameMode->OnEnemyDefeated.RemoveDynamic(this,&ThisClass::HandleEnemyDefeated);
		GameMode->OnGameWon.RemoveDynamic(this,&ThisClass::HandleGameWon);
	}

	GameMode = InGameMode;

	if (GameMode)
	{
		GameMode->OnEnemyDefeated.AddDynamic(this,&ThisClass::HandleEnemyDefeated);
		GameMode->OnGameWon.AddDynamic(this,&ThisClass::HandleGameWon);
		HandleEnemyDefeated(GameMode->GetEnemiesDefeated());
		if (GameMode->IsGameWon())
		{
			HandleGameWon();
		}
	}
	RefreshObjective();
}

void UInventoryWidget::HandleEnemyDefeated(int32 NewDefeatedCount)
{
	RefreshKillText(FText::FromString(FString::Printf(TEXT("Kills: %d"), NewDefeatedCount)));
	RefreshObjective();	
}
void UInventoryWidget::HandleGameWon()
{
	RefreshGameResult(FText::FromString(TEXT("Victory!")));

	if (APlayerController* PlayerController =GetOwningPlayer())
	{
		PlayerController->bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
	}
}
void UInventoryWidget::RestartLevel()
{
	if (ATPCPlayerController* PlayerController =
		Cast<ATPCPlayerController>(GetOwningPlayer()))
	{
		PlayerController->RestoreGameplayInput();
	}
	const FName CurrentLevelName = FName(*UGameplayStatics::GetCurrentLevelName(this,true));

	UGameplayStatics::OpenLevel(this,CurrentLevelName);
}
void UInventoryWidget::SetInteractionComponent(UInteractionComponent* InInteractionComponent)
{
	if (InteractionComponent == InInteractionComponent)
	{
		return;
	}

	if (InteractionComponent)
	{
		InteractionComponent->OnInteractionPromptChanged.RemoveDynamic(this,&ThisClass::HandleInteractionPromptChanged);
	}

	InteractionComponent = InInteractionComponent;

	if (InteractionComponent)
	{
		InteractionComponent->OnInteractionPromptChanged.AddDynamic(this,&ThisClass::HandleInteractionPromptChanged);
	}

	HandleInteractionPromptChanged(FText::GetEmpty());
}

void UInventoryWidget::HandleInteractionPromptChanged(const FText& PromptText)
{
	RefreshInteractionPrompt(PromptText,!PromptText.IsEmpty());
}
void UInventoryWidget::SetStaminaComponent(
	UStaminaComponent* InStaminaComponent)
{
	if (StaminaComponent == InStaminaComponent)
	{
		return;
	}

	if (StaminaComponent)
	{
		StaminaComponent->OnStaminaChanged.RemoveDynamic(this, &ThisClass::HandleStaminaChanged);
	}
	StaminaComponent = InStaminaComponent;
	if (StaminaComponent)
	{
		StaminaComponent->OnStaminaChanged.AddDynamic(this, &ThisClass::HandleStaminaChanged);
		HandleStaminaChanged(StaminaComponent->GetCurrentStamina(),StaminaComponent->GetMaxStamina());
	}
}

void UInventoryWidget::HandleStaminaChanged(float CurrentStamina,float MaxStamina)
{
	const FString Text = FString::Printf(TEXT("SP: %.0f / %.0f"),CurrentStamina,MaxStamina);
	RefreshStaminaText(FText::FromString(Text));
	const float Percent = MaxStamina > 0.f ? FMath::Clamp(CurrentStamina / MaxStamina, 0.f, 1.f) : 0.f;
	RefreshStaminaPercent(Percent);
	const bool bIsLowStamina = Percent <= 0.25f;
	if (bWasLowStamina != bIsLowStamina)
	{
		bWasLowStamina = bIsLowStamina;
		RefreshStaminaBarStyle(Percent, bIsLowStamina);
	}
    // The Forest material owns the normal jade color; don't multiply it by legacy green tint.
    if (GetWidgetFromName(TEXT("ForestStatusPanel")))
    {
        if (UProgressBar* Bar = Cast<UProgressBar>(GetWidgetFromName(TEXT("StaminaBar"))))
        {
            Bar->SetFillColorAndOpacity(bIsLowStamina ? FLinearColor(1.f, .62f, .32f, 1.f) : FLinearColor::White);
        }
    }
}
void UInventoryWidget::ClearSaveAndRestart()
{
	const bool bDeleted =UGameplayStatics::DeleteGameInSlot(TPCSaveSlots::Resolve(),0);

	UE_LOG(LogTemp,Warning,TEXT("Save deleted: %s"),bDeleted ? TEXT("Success") : TEXT("No save found"));

	RestartLevel();
}

void UInventoryWidget::CloseInventory()
{
	if (ATPCPlayerController* PlayerController =Cast<ATPCPlayerController>(GetOwningPlayer()))
	{
		PlayerController->ToggleInventory();
	}
}

void UInventoryWidget::RefreshInventoryGrid()
{
	if (!InventoryGrid ||!InventorySlotWidgetClass ||!InventoryComponent)
	{
		return;
	}
	InventoryGrid->ClearChildren();
	int32 DisplayIndex = 0;
	for (int32 SlotIndex = 0;SlotIndex < InventoryComponent->MaxSlots;++SlotIndex)
	{
		UInventorySlotWidget* SlotWidget =CreateWidget<UInventorySlotWidget>(GetOwningPlayer(),InventorySlotWidgetClass);
		if (!SlotWidget)
		{
			continue;
		}
		SlotWidget->SetSlotIndex(SlotIndex);
		SlotWidget->OnSlotClicked.AddDynamic(this,&ThisClass::HandleInventorySlotClicked);
		
		const FInventorySlot* InventorySlot =InventoryComponent->Slots.IsValidIndex(SlotIndex)? &InventoryComponent->Slots[SlotIndex]: nullptr;
		if (InventorySlot && InventorySlot->Count > 0)
		{
			const FText ItemName =InventorySlot->ItemDefinition? InventorySlot->ItemDefinition->DisplayName: FText::FromName(InventorySlot->ItemId);
			SlotWidget->SetItemData(ItemName,InventorySlot->Count,InventorySlot->ItemDefinition? InventorySlot->ItemDefinition->Icon: nullptr);
		}
		else
		{
			SlotWidget->SetEmpty();
		}
		SlotWidget->SetSelected(SlotIndex == SelectedInventorySlotIndex);
		if (UUniformGridSlot* GridSlot =InventoryGrid->AddChildToUniformGrid(SlotWidget))
		{
			GridSlot->SetRow(SlotIndex / InventoryGridColumns);
			GridSlot->SetColumn(SlotIndex % InventoryGridColumns);
		}
	}
}
void UInventoryWidget::HandleInventorySlotClicked(int32 ClickedSlotIndex)
{
	SelectedInventorySlotIndex = ClickedSlotIndex;
	if (InventoryComponent &&InventoryComponent->Slots.IsValidIndex(SelectedInventorySlotIndex))
	{
		const FInventorySlot& SelectedSlot =InventoryComponent->Slots[SelectedInventorySlotIndex];
		if (SelectedSlot.Count > 0)
		{
			const FText ItemName =SelectedSlot.ItemDefinition? SelectedSlot.ItemDefinition->DisplayName: FText::FromName(SelectedSlot.ItemId);
			const bool bCanUse =SelectedSlot.ItemDefinition &&SelectedSlot.ItemDefinition->ItemType ==EItemType::Consumable &&SelectedSlot.ItemDefinition->HealAmount > 0.f;
			RefreshSelectedItemInfo(ItemName,SelectedSlot.Count,true,bCanUse);
            RefreshForestItemPreview(this, SelectedSlot.ItemDefinition, true);
		}
		else
		{
			RefreshSelectedItemInfo(FText::GetEmpty(),0,false,false);
        RefreshForestItemPreview(this, nullptr, false);
		}
	}
	else
	{
		RefreshSelectedItemInfo(FText::GetEmpty(),0,false,false);
        RefreshForestItemPreview(this, nullptr, false);
	}
	RefreshInventoryGrid();
}
void UInventoryWidget::UseSelectedItem()
{
	if (!InventoryComponent ||
		!InventoryComponent->Slots.IsValidIndex(
			SelectedInventorySlotIndex))
	{
		return;
	}

	const FInventorySlot& SelectedSlot =
		InventoryComponent->Slots[
			SelectedInventorySlotIndex];

	UItemDefinition* ItemDefinition =
		SelectedSlot.ItemDefinition;

	if (!ItemDefinition ||
		ItemDefinition->ItemType != EItemType::Consumable ||
		ItemDefinition->HealAmount <= 0.f)
	{
		ShowInventoryHint(
			FText::FromString(TEXT("This item cannot be used")));

		return;
	}

	if (!HealthComponent ||
		HealthComponent->GetCurrentHealth() >=
			HealthComponent->GetMaxHealth())
	{
		ShowInventoryHint(
			FText::FromString(TEXT("Health is already full")));

		return;
	}

	if (InventoryComponent->UseItemAtSlot(
		SelectedInventorySlotIndex))
	{
		HandleInventorySlotClicked(
			SelectedInventorySlotIndex);
	}
}
void UInventoryWidget::DropSelectedItem()
{
	if (!InventoryComponent ||!InventoryComponent->Slots.IsValidIndex(SelectedInventorySlotIndex))
	{
		return;
	}

	const FInventorySlot& SelectedSlot =InventoryComponent->Slots[SelectedInventorySlotIndex];

	UItemDefinition* ItemDefinition =SelectedSlot.ItemDefinition;

	if (!ItemDefinition ||!ItemDefinition->WorldPickupClass)
	{
		ShowInventoryHint(FText::FromString(TEXT("This item cannot be dropped")));

		return;
	}

	APawn* PlayerPawn = GetOwningPlayerPawn();

	if (!PlayerPawn || !GetWorld())
	{
		return;
	}

	const FVector DropLocation =PlayerPawn->GetActorLocation() +PlayerPawn->GetActorForwardVector() * 120.f +FVector(0.f, 0.f, 30.f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride =ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APickupActor* DroppedPickup =GetWorld()->SpawnActor<APickupActor>(ItemDefinition->WorldPickupClass,DropLocation,PlayerPawn->GetActorRotation(),SpawnParams);

	if (!DroppedPickup)
	{
		ShowInventoryHint(FText::FromString(TEXT("Failed to drop item")));

		return;
	}
	InventoryComponent->RemoveItem(ItemDefinition->ItemId,1);
	SelectedInventorySlotIndex = INDEX_NONE;
	RefreshSelectedItemInfo(FText::GetEmpty(),0,false,false);
        RefreshForestItemPreview(this, nullptr, false);
}
void UInventoryWidget::RefreshObjective()
{
	if (!GameMode)
	{
		return;
	}

	const int32 Current = GameMode->GetEnemiesDefeated();
	const int32 Target = GameMode->GetVictoryKillTarget();
	const FText Text = Current >= Target? FText::FromString(TEXT("Objective: Go to the exit")): FText::FromString(FString::Printf(TEXT("Objective: Defeat enemies (%d / %d)"),Current,Target));

	RefreshObjectiveText(Text);
}

void UInventoryWidget::SetLevelComponent(ULevelComponent* InLevelComponent)
{
	if (LevelComponent == InLevelComponent)
	{
		return;
	}

	if (LevelComponent)
	{
		LevelComponent->OnLevelProgressChanged.RemoveDynamic(
			this, &ThisClass::HandleLevelProgressChanged);
		LevelComponent->OnUpgradeChoicesReady.RemoveDynamic(
			this, &ThisClass::HandleUpgradeChoicesReady);
	}

	LevelComponent = InLevelComponent;
	if (LevelComponent)
	{
		LevelComponent->OnLevelProgressChanged.AddDynamic(
			this, &ThisClass::HandleLevelProgressChanged);
		LevelComponent->OnUpgradeChoicesReady.AddDynamic(
			this, &ThisClass::HandleUpgradeChoicesReady);
		HandleLevelProgressChanged(
			LevelComponent->GetLevel(),
			LevelComponent->GetCurrentExperience(),
			LevelComponent->GetExperienceToNextLevel());
	}
}

void UInventoryWidget::HandleLevelProgressChanged(
	int32 Level,
	int32 CurrentExperience,
	int32 ExperienceToNextLevel)
{
	const FText LevelText = FText::FromString(FString::Printf(
		TEXT("Lv.%d  XP %d / %d"),
		Level,
		CurrentExperience,
		ExperienceToNextLevel));
	const float Percent = ExperienceToNextLevel > 0
		? static_cast<float>(CurrentExperience) /
			static_cast<float>(ExperienceToNextLevel)
		: 0.f;
	RefreshLevelProgress(LevelText, FMath::Clamp(Percent, 0.f, 1.f));
}

void UInventoryWidget::HandleUpgradeChoicesReady(
	const FLevelUpgradeChoice& ChoiceA,
	const FLevelUpgradeChoice& ChoiceB,
	const FLevelUpgradeChoice& ChoiceC)
{
	// The restored upgrade modal must accept keyboard focus after possession changes.
	SetIsFocusable(true);
	ShowLevelUpChoices(
		ChoiceA.DisplayName, ChoiceA.Description,
		ChoiceB.DisplayName, ChoiceB.Description,
		ChoiceC.DisplayName, ChoiceC.Description);

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetPause(true);
		PlayerController->bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
	}
}

void UInventoryWidget::ChooseLevelUpgrade(int32 ChoiceIndex)
{
	if (!LevelComponent || !LevelComponent->SelectUpgrade(ChoiceIndex))
	{
		return;
	}

	if (LevelComponent->HasPendingUpgradeChoice())
	{
		return;
	}

	HideLevelUpChoices();
    // The Forest layout always dismisses the modal, including blueprints without a Hide event.
    if (GetWidgetFromName(TEXT("ForestUpgradeFrame")))
    {
        if (UWidget* Panel = GetWidgetFromName(TEXT("LevelUpPanel")))
        {
            Panel->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetPause(false);
		PlayerController->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
	}
}
