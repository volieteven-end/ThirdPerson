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
class UWeaponDefinition;
class AWeaponActor;
UENUM(BlueprintType)
enum class EEnemyLaunchPhase : uint8 { None, Airborne, LandImpact, DownIdle, DownHit, GetUp, Dead };
UCLASS()
class THIRDPERSON_API AEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Lock On")
	void SetLockOnIndicatorVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Lock On")
	FVector GetLockOnAimPoint() const;
	virtual void ApplyParryStagger(AActor* ParryingActor);
	virtual void ApplyUppercutHit(AActor* AttackingActor);
	UFUNCTION(BlueprintPure, Category = "Combat")
	EEnemyLaunchPhase GetLaunchPhase() const { return LaunchPhase; }
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsParryStaggered() const { return bParryStaggered; }
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsHitReacting() const { return bHitReacting; }

    /** Ranged patrol samples reachable points around the spawn/HomeLocation when PatrolPoints is empty. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Patrol", meta=(ClampMin="100.0"))
    float RangedPatrolRadius = 800.f;
    AActor* GetCurrentPatrolPoint() const;
	void AdvancePatrolPoint();
	UFUNCTION()
	virtual void HandleHealthChanged(float CurrentHealth,float MaxHealth);
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;
	/** Recovery gate when no ordinary hit montage can be played. */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Hit Reaction", meta = (ClampMin = "0.05"))
	float HitReactFallbackDuration = 0.35f;
	float PreviousHealth = 0.f;
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> DeathMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> ParryStaggerMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation", meta = (ClampMin = "0.1"))
	float ParryStaggerFallbackDuration = 1.4f;
    /** Minimum punish window, including the authored recoil animation. */
    UPROPERTY(EditDefaultsOnly, Category="Animation", meta=(ClampMin="0.1", Units="s"))
    float ParryStaggerMinimumDuration = 1.4f;
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
	/** This complete pair uses Down_01; never mix it with the reversed Down_02 pose. */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Uppercut") TObjectPtr<UAnimMontage> UppercutDownIdleMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Uppercut") TObjectPtr<UAnimMontage> UppercutDownHitMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Uppercut", meta=(ClampMin="0")) float UppercutDownDuration = .6f;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Uppercut", meta=(ClampMin="1",ClampMax="3")) int32 MaxLaunchesPerFlight = 2;
protected:
	virtual void BeginPlay() override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;
	UFUNCTION()
	virtual void HandleDeath();
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
    friend struct FTPCSwordPIETestAccess;
	friend struct FMeleeAITestAccess;
	void IgnoreCameraCollision();
	UFUNCTION()
	void HandleEquippedWeaponChanged(UWeaponDefinition* Definition, AWeaponActor* Weapon);
	int32 CurrentPatrolIndex = 0;
	void PositionLockOnIndicator();
	FVector GetLockOnCameraReferencePoint() const;
	FVector CachedLockOnCameraReferenceLocal = FVector::ZeroVector;
	bool bHasLockOnCameraReference = false;
	void EndParryStagger();
	void TryEndUppercutStun();
	void StartUppercutGetUp();
	void StartUppercutDownIdle();
	void FinishUppercutStun();
	void SetAIStunned(bool bStunned);
	void BeginHitReaction();
	void FinishHitReaction();
	void ClearHitReaction();
	bool bHitReacting = false;
	bool bHitReactionDisabledMovement = false;
	bool bCombatEnabledBeforeHit = false;
	uint8 MovementModeBeforeHit = 0;
	FTimerHandle HitReactionTimerHandle;
	bool bParryStaggered = false;
	bool bUppercutStunned = false;
	bool bUppercutLandingRecovery = false;
	bool bReceivedUppercutLanding = false;
	int32 LaunchesThisFlight = 0;
	EEnemyLaunchPhase LaunchPhase = EEnemyLaunchPhase::None;
	bool bDead = false;
	FTimerHandle ParryStaggerTimerHandle;
	FTimerHandle UppercutRecoveryTimerHandle;
};
