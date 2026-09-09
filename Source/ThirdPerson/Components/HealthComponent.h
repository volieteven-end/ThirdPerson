// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatHitTypes.h"
#include "HealthComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnHealthChanged,
	float, CurrentHealth,
	float, MaxHealth);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnCombatHitResolved, const FCombatHitSpec&, const FCombatHitResult&, AActor*);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THIRDPERSON_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	UFUNCTION(BlueprintCallable, Category = "Health")
	void ApplyDamage(float Damage);
	void ApplyDamageFrom(float Damage, AActor* DamageSource);
	UFUNCTION(BlueprintCallable, Category="Health")
	FCombatHitResult ApplyCombatHit(const FCombatHitSpec& Spec, AActor* DamageSource);
	const FCombatHitResult& GetLastCombatHitResult() const { return LastHitResult; }
	FOnCombatHitResolved OnCombatHitResolved;
	/** Separate encounter immunity; does not overwrite the player's timed dodge immunity. */
	void SetEncounterInvulnerable(bool bEnabled) { bEncounterInvulnerable = bEnabled; }
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
	/** Copy permanent attributes only; death, hit and immunity state stay fresh. */
	void RestoreRespawnAttributes(const UHealthComponent& Source);
	float GetCurrentHealth() const { return CurrentHealth; }
	float GetMaxHealth() const { return MaxHealth; }
protected:
	virtual void BeginPlay() override;
	
private:
	bool bIsInvulnerable = false;
	bool bEncounterInvulnerable = false;
	FCombatHitResult LastHitResult;

	FTimerHandle InvulnerabilityTimerHandle;
	float DamageReceivedMultiplier = 1.f;
	TWeakObjectPtr<AActor> LastDamageSource;

	void ClearInvulnerability();
};
