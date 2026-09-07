// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StaminaComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChanged, float, CurrentStamina, float, MaxStamina);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THIRDPERSON_API UStaminaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStaminaComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
	bool TryConsume(float Amount);
	float GetCurrentStamina() const { return CurrentStamina; }
	float GetMaxStamina() const { return MaxStamina; }
	void SetCurrentStamina(float NewStamina);
	void AddMaxStamina(float Amount, bool bRestoreAddedAmount);
	UPROPERTY(BlueprintAssignable)
	FOnStaminaChanged OnStaminaChanged;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Stamina",meta = (ClampMin = "1.0"))
	float MaxStamina = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Stamina",meta = (ClampMin = "0.0"))
	float RecoveryPerSecond = 25.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Stamina",meta = (ClampMin = "0.0"))
	float RecoveryDelay = 1.f;

	float RecoveryResumeTime = 0.f;
private:
	float CurrentStamina = 0.f;
};
