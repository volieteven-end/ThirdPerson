#include "PauseMenuWidget.h"

#include "../Character/TPCPlayerController.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
void UPauseMenuWidget::ResumeGame()
{
	if (ATPCPlayerController* PlayerController =Cast<ATPCPlayerController>(GetOwningPlayer()))
	{
		PlayerController->TogglePauseMenu();
	}
}
FReply UPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry,const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		ResumeGame();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry,InKeyEvent);
}