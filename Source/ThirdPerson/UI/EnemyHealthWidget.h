// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHealthWidget.generated.h"

class UHealthComponent;

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