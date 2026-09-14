

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "PauseMenuWidget.generated.h"

/** 暂停菜单和输入焦点处理，独立于背包分页，不承载物品操作。 */
UCLASS()
class THIRDPERSON_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void ResumeGame();
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry,const FKeyEvent& InKeyEvent) override;
};