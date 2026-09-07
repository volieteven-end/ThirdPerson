// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnemyCharacter.generated.h"
class APickupActor;
class UHealthComponent;
class UWidgetComponent;
class UEnemyHealthWidget;
class UCombatComponent;
class UEquipmentComponent;
class AActor;
class UAnimMontage;
UCLASS()
class THIRDPERSON_API AEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Lock On")
	void SetLockOnIndicatorVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Lock On")
	FVector GetLockOnAimPoint() const;
	void ApplyParryStagger(AActor* ParryingActor);
	void ApplyUppercutHit(AActor* AttackingActor);
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsParryStaggered() const { return bParryStaggered; }

    /** Ranged patrol samples reachable points around the spawn/HomeLocation when PatrolPoints is empty. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Patrol", meta=(ClampMin="100.0"))
    float RangedPatrolRadius = 800.f;
    AActor* GetCurrentPatrolPoint() const;
	void AdvancePatrolPoint();
	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth,float MaxHealth);
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;
	float PreviousHealth = 0.f;
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> DeathMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> ParryStaggerMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation", meta = (ClampMin = "0.1"))
	float ParryStaggerFallbackDuration = 0.7f;
	UPROPERTY(EditDefaultsOnly, Category = "Animation",meta = (ClampMin = "0.0"))
	float DeathDestroyDelay = 0.5f;
	UPROPERTY(EditDefaultsOnly, Category = "Animation", meta = (ClampMin = "0.1"))
	float MinimumDeathLifeSpan = 2.f;
	/** Airborne reaction used when this enemy is hit by the player's uppercut. */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Uppercut")
	TObjectPtr<UAnimMontage> UppercutHitMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Uppercut", meta = (ClampMin = "0.0"))
	float UppercutLaunchVelocity = 620.f;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Uppercut", meta = (ClampMin = "0.0"))
	float UppercutHorizontalVelocity = 180.f;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Uppercut", meta = (ClampMin = "0.1"))
	float UppercutStunFallbackDuration = 1.f;
	/** Optional landing/get-up montage. AI remains stunned until it finishes. */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Uppercut")
	TObjectPtr<UAnimMontage> UppercutLandMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Uppercut", meta = (ClampMin = "0.0"))
	float UppercutLandingRecoveryDuration = 0.35f;
	/** Separate get-up phase played after the landing reaction. */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Uppercut")
	TObjectPtr<UAnimMontage> UppercutGetUpMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Uppercut", meta = (ClampMin = "0.0"))
	float UppercutGetUpFallbackDuration = 0.65f;
protected:
	virtual void BeginPlay() override;
	virtual void Landed(const FHitResult& Hit) override;
	UFUNCTION()
	void HandleDeath();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,Category = "Components")
	TObjectPtr<UHealthComponent> HealthComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> HealthBarWidget;
	UPROPERTY(EditInstanceOnly, Category = "AI")
	TArray<TObjectPtr<AActor>> PatrolPoints;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCombatComponent> CombatComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UEquipmentComponent> EquipmentComponent;
	UPROPERTY(EditDefaultsOnly, Category = "Drops")
	TSubclassOf<APickupActor> DropPickupClass;
	UPROPERTY(EditDefaultsOnly, Category = "Drops",meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DropChance = 1.f;
	UPROPERTY(EditDefaultsOnly, Category = "Rewards", meta = (ClampMin = "0"))
	int32 ExperienceReward = 50;

	/** Each enemy owns and positions its own lock marker. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lock On")
	TObjectPtr<UWidgetComponent> LockOnIndicatorWidget;

	/** Visual marker follows the chest bone; missing bones fall back to the capsule's upper body. */
	UPROPERTY(EditDefaultsOnly, Category = "Lock On")
	FName LockOnIndicatorSocketName = TEXT("spine_03");

	/** Added after socket/bounds placement, configurable for every enemy subtype. */
	UPROPERTY(EditDefaultsOnly, Category = "Lock On")
	FVector LockOnIndicatorOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Lock On", meta = (ClampMin = "0.01"))
	float LockOnIndicatorSize = 24.f;

	UPROPERTY(EditDefaultsOnly, Category = "Lock On", meta = (ClampMin = "0.0"))
	float LockOnIndicatorPulseAmount = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Lock On", meta = (ClampMin = "0.0"))
	float LockOnIndicatorPulseSpeed = 3.f;

	/** Independent camera reference preserves the previous framing when the visual marker moves. */
	UPROPERTY(EditDefaultsOnly, Category = "Lock On|Camera")
	FName LockOnCameraReferenceSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "Lock On|Camera")
	FVector LockOnCameraReferenceOffset = FVector(0.f, 0.f, 28.f);

	/** Legacy camera tuning, now measured below the independent camera reference, not the orb. */
	UPROPERTY(EditDefaultsOnly, Category = "Lock On", meta = (ClampMin = "0.0"))
	float LockOnCameraAimBelowIndicator = 70.f;
private:
	int32 CurrentPatrolIndex = 0;
	void PositionLockOnIndicator();
	FVector GetLockOnCameraReferencePoint() const;
	FVector CachedLockOnCameraReferenceLocal = FVector::ZeroVector;
	bool bHasLockOnCameraReference = false;
	void EndParryStagger();
	void TryEndUppercutStun();
	void StartUppercutGetUp();
	void FinishUppercutStun();
	void SetAIStunned(bool bStunned);
	bool bParryStaggered = false;
	bool bUppercutStunned = false;
	bool bUppercutLandingRecovery = false;
	bool bDead = false;
	FTimerHandle ParryStaggerTimerHandle;
	FTimerHandle UppercutRecoveryTimerHandle;
};
