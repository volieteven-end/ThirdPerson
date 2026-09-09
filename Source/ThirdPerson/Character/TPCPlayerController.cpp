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
#include "../Components/ActionComponent.h"
#include "TPCCharacter.h"
#include "../UI/PlayerDeathWidget.h"
#include "../UI/HealthPotionWidget.h"
void ATPCPlayerController::BeginPlay()
{
	Super::BeginPlay();
	RestoreGameplayInput();
	if (!IsLocalController())
	{
		return;
	}
	HealthPotionWidget = CreateWidget<UHealthPotionWidget>(this, UHealthPotionWidget::StaticClass());
	if (HealthPotionWidget) HealthPotionWidget->AddToViewport(5);
	if (InventoryWidgetClass)
	{
		InventoryWidget = CreateWidget<UInventoryWidget>(this, InventoryWidgetClass);
		if (InventoryWidget)
		{
			InventoryWidget->AddToViewport();
			if (auto* GameMode = Cast<ATPCGameMode>(GetWorld()->GetAuthGameMode()))
				InventoryWidget->SetGameMode(GameMode);
		}
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
    if (DeathScreen) { DeathScreen->RemoveFromParent(); DeathScreen = nullptr; }
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
		if (auto* Actions = ControlledPawn->FindComponentByClass<UActionComponent>()) Actions->SetInputSuppressed(false);
	}
}

void ATPCPlayerController::ConnectInventory(APawn* InPawn)
{
	if (!IsLocalController()||!InPawn)
	{
		return;
	}
	if (HealthPotionWidget) HealthPotionWidget->SetInventory(InPawn->FindComponentByClass<UInventoryComponent>());
	if (!InventoryWidget) return;
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
    if (IsDeathScreenOpen()) return;
	if (!InventoryWidget)
	{
		return;
	}
	bIsInventoryOpen = !bIsInventoryOpen;
	if (GetPawn()) if (auto* Actions = GetPawn()->FindComponentByClass<UActionComponent>())
		Actions->SetInputSuppressed(bIsInventoryOpen || bIsPauseMenuOpen);
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
    if (IsDeathScreenOpen()) return;
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
		if (GetPawn()) if (auto* Actions = GetPawn()->FindComponentByClass<UActionComponent>()) Actions->SetInputSuppressed(true);
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
		if (GetPawn()) if (auto* Actions = GetPawn()->FindComponentByClass<UActionComponent>()) Actions->SetInputSuppressed(bIsInventoryOpen);
		bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
	}
}

bool ATPCPlayerController::IsDeathScreenOpen() const { return DeathScreen && DeathScreen->IsInViewport(); }
void ATPCPlayerController::ShowDeathScreen()
{
    if (!IsLocalController() || IsDeathScreenOpen()) return;
    if (PauseMenuWidget) PauseMenuWidget->RemoveFromParent();
    if (InventoryWidget) InventoryWidget->SetInventoryPanelOpen(false);
    bIsPauseMenuOpen = bIsInventoryOpen = false;
    DeathScreen = CreateWidget<UPlayerDeathWidget>(this, UPlayerDeathWidget::StaticClass());
    if (!DeathScreen) return;
    DeathScreen->AddToViewport(100);
    SetIgnoreMoveInput(true); SetIgnoreLookInput(true); bShowMouseCursor = true;
    FInputModeUIOnly Input; Input.SetWidgetToFocus(DeathScreen->TakeWidget()); Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Input); SetPause(true);
}
void ATPCPlayerController::RestartAfterDeath()
{
    if (!IsDeathScreenOpen()) return;
    SetPause(false);
    if (auto* P = Cast<ATPCCharacter>(GetPawn())) P->RestartAfterDeath();
}
