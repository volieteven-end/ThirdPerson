#include "StarterWeaponWidget.h"

#include "../NPC/StarterWeaponNPC.h"
#include "GameFramework/PlayerController.h"

void UStarterWeaponWidget::InitializeSelection(AStarterWeaponNPC* InSourceNPC)
{
	SourceNPC = InSourceNPC;
	if (SourceNPC)
	{
		RefreshWeaponChoices(
			SourceNPC->GetMeleeWeaponName(),
			SourceNPC->GetRangedWeaponName());
	}
}

void UStarterWeaponWidget::ChooseMeleeWeapon()
{
	if (SourceNPC && SourceNPC->CompleteWeaponChoice(EStarterWeaponChoice::Melee))
	{
		CloseSelection();
	}
}

void UStarterWeaponWidget::ChooseRangedWeapon()
{
	if (SourceNPC && SourceNPC->CompleteWeaponChoice(EStarterWeaponChoice::Ranged))
	{
		CloseSelection();
	}
}

void UStarterWeaponWidget::CancelSelection()
{
	if (SourceNPC)
	{
		SourceNPC->CancelWeaponSelection();
	}
	CloseSelection();
}

void UStarterWeaponWidget::CloseSelection()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->bShowMouseCursor = false;
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
	}

	SourceNPC = nullptr;
	RemoveFromParent();
}
