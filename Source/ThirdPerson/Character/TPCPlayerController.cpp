// Fill out your copyright notice in the Description page of Project Settings.


#include "TPCPlayerController.h"
#include "../Components/InventoryComponent.h"
#include "../UI/InventroyWidget.h"
#include "../Components/HealthComponent.h"
#include "../GameMode/TPCGameMode.h"
#include "../Components/InteractionComponent.h"
#include "../Components/StaminaComponent.h"
#include "../Components/LevelComponent.h"
#include "../UI/PauseMenuWidget.h"
void ATPCPlayerController::BeginPlay()
{
	Super::BeginPlay();
	RestoreGameplayInput();
	if (!IsLocalController()||!InventoryWidgetClass)
	{
		return;
	}
	InventoryWidget=CreateWidget<UInventoryWidget>(this, InventoryWidgetClass);
	if (!InventoryWidget)
	{
		return;
	}
	InventoryWidget->AddToViewport();
	if (ATPCGameMode* GameMode =Cast<ATPCGameMode>(GetWorld()->GetAuthGameMode()))
	{
		InventoryWidget->SetGameMode(GameMode);
	}
	ConnectInventory(GetPawn());
	
	
}

void ATPCPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	RestoreGameplayInput();
	ConnectInventory(InPawn);
}

void ATPCPlayerController::RestoreGameplayInput()
{
	SetPause(false);
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	bShowMouseCursor = false;
	bIsPauseMenuOpen = false;
	bIsInventoryOpen = false;
	if (PauseMenuWidget)
	{
		PauseMenuWidget->RemoveFromParent();
	}
	if (InventoryWidget)
	{
		InventoryWidget->SetInventoryPanelOpen(false);
	}
	FInputModeGameOnly InputMode;
	SetInputMode(InputMode);
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->EnableInput(this);
	}
}

void ATPCPlayerController::ConnectInventory(APawn* InPawn)
{
	if (!IsLocalController()||!InventoryWidget||!InPawn)
	{
		return;
	}
	if (UInventoryComponent* Inventory=InPawn->FindComponentByClass<UInventoryComponent>())
	{
		InventoryWidget->SetInventory(Inventory);
	}
	if (UHealthComponent* Health=InPawn->FindComponentByClass<UHealthComponent>())
	{
		InventoryWidget->SetHealthComponent(Health);
	}
	if (UInteractionComponent* Interaction =InPawn->FindComponentByClass<UInteractionComponent>())
	{
		InventoryWidget->SetInteractionComponent(Interaction);
	}
	if (UStaminaComponent* Stamina =InPawn->FindComponentByClass<UStaminaComponent>())
	{
		InventoryWidget->SetStaminaComponent(Stamina);
	}
	if (ULevelComponent* Level = InPawn->FindComponentByClass<ULevelComponent>())
	{
		InventoryWidget->SetLevelComponent(Level);
	}
}
void ATPCPlayerController::ToggleInventory()
{
	if (!InventoryWidget)
	{
		return;
	}
	bIsInventoryOpen = !bIsInventoryOpen;
	InventoryWidget->SetInventoryPanelOpen(bIsInventoryOpen);
	if (bIsInventoryOpen)
	{
		bShowMouseCursor = true;
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(InventoryWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	else
	{
		bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
	}
}
void ATPCPlayerController::TogglePauseMenu()
{
	if (!IsLocalController() || !PauseMenuWidgetClass)
	{
		return;
	}

	bIsPauseMenuOpen = !bIsPauseMenuOpen;

	if (bIsPauseMenuOpen)
	{
		if (!PauseMenuWidget)
		{
			PauseMenuWidget =CreateWidget<UPauseMenuWidget>(this,PauseMenuWidgetClass);
		}
		if (!PauseMenuWidget)
		{
			bIsPauseMenuOpen = false;
			return;
		}
		PauseMenuWidget->AddToViewport();
		SetPause(true);
		bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	else
	{
		if (PauseMenuWidget)
		{
			PauseMenuWidget->RemoveFromParent();
		}

		SetPause(false);
		bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
	}
}
