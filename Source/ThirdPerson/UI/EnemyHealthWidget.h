
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHealthWidget.generated.h"

class UHealthComponent;

/** 普通敌人血条显示，绑定生命组件并在角色更换或销毁时解除旧绑定。 */
UCLASS()
class THIRDPERSON_API UEnemyHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetHealthComponent(UHealthComponent* InHealthComponent);

protected:
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth, float MaxHealth);

	UFUNCTION(BlueprintImplementableEvent)
	void RefreshHealthPercent(float Percent);

private:
	UPROPERTY(Transient)
	TObjectPtr<UHealthComponent> HealthComponent;
};