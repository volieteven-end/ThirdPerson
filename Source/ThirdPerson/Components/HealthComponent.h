// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnHealthChanged,
	float, CurrentHealth,
	float, MaxHealth);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THIRDPERSON_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	UFUNCTION(BlueprintCallable, Category = "Health")
	void ApplyDamage(float Damage);
	void ApplyDamageFrom(float Damage, AActor* DamageSource);
	AActor* GetLastDamageSource() const { return LastDamageSource.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float Amount);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health",
		meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health")
	float CurrentHealth = 100.f;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDeath OnDeath;
	void SetInvulnerableFor(float Duration);
	void SetCurrentHealth(float NewHealth);
	void AddMaxHealth(float Amount, bool bHealAddedAmount);
	void MultiplyDamageReceived(float Multiplier);
	float GetCurrentHealth() const { return CurrentHealth; }
	float GetMaxHealth() const { return MaxHealth; }
protected:
	virtual void BeginPlay() override;
	
private:
	bool bIsInvulnerable = false;

	FTimerHandle InvulnerabilityTimerHandle;
	float DamageReceivedMultiplier = 1.f;
	TWeakObjectPtr<AActor> LastDamageSource;

	void ClearInvulnerability();
};
