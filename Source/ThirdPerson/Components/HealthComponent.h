
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

/** 统一生命与受击结算入口：处理免疫、格挡／弹反后的实际伤害，广播生命变化和死亡结果。 */
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
	/** 遭遇期间的独立免疫开关，不覆盖玩家闪避的限时无敌。 */
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
