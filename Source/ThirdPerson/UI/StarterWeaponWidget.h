#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StarterWeaponWidget.generated.h"

class AStarterWeaponNPC;

/** 初始武器选择界面，展示可选配置并通知选择结果，不自行生成可战斗角色。 */
UCLASS()
class THIRDPERSON_API UStarterWeaponWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeSelection(AStarterWeaponNPC* InSourceNPC);

	UFUNCTION(BlueprintCallable, Category = "Starter Weapon")
	void ChooseMeleeWeapon();

	UFUNCTION(BlueprintCallable, Category = "Starter Weapon")
	void ChooseRangedWeapon();

	UFUNCTION(BlueprintCallable, Category = "Starter Weapon")
	void CancelSelection();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Starter Weapon")
	void RefreshWeaponChoices(const FText& MeleeName, const FText& RangedName);

private:
	void CloseSelection();

	UPROPERTY(Transient)
	TObjectPtr<AStarterWeaponNPC> SourceNPC;
};
