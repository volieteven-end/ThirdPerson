// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Animation/CombatActionRules.h"
#include "CombatComponent.generated.h"

class UAnimMontage;
class AWeaponActor;
class UAnimSequenceBase;
class UWeaponDefinition;
class USkeletalMeshComponent;
class UParticleSystem;

DECLARE_MULTICAST_DELEGATE(FOnMeleeAttackStartedNative);

enum class EActiveCombatAttackType : uint8
{
	Normal,
	Uppercut,
	Air,
	AirDive
};
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THIRDPERSON_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent();
	/** Native hook used by the owning character for facing and target magnetism. */
	FOnMeleeAttackStartedNative OnMeleeAttackStarted;

	void TryAttack();
	/** AI-facing ranged entry point. The projectile is aimed at TargetActor. */
	bool TryRangedAttackAt(AActor* TargetActor);
	/** Called by the ranged release AnimNotify at the authored bow-release frame. */
	void ReleaseRangedProjectile();
	bool IsRangedAttackInProgress() const { return bRangedAttackInProgress; }
	void TryUppercutAttack();
	void TryAirAttack();
	/** A separate input action; normal air attacks never force a downward launch. */
	void TryAirDiveAttack();
	void HandleOwnerLanded();
	void CancelActiveAttack(float BlendOutTime = 0.1f);

    // Only the first dash of a normal ground combo may preserve its next stage.
    void CancelAttackForDash(float BlendOutTime);
    bool FinishDashForCombo(bool bInterrupted, float ContinueWindow);
    bool QueueAttackDuringDash();
    bool TryResumeComboAfterDash();
    void ClearDashCombo();
	void ReachComboChainPoint(UAnimSequenceBase* Animation, int32 MontageInstanceId = INDEX_NONE);
	bool IsCurrentAttackNotify(const UAnimSequenceBase* Animation, int32 MontageInstanceId) const;
	void StartBlock();
	void StopBlock();
	UFUNCTION(BlueprintPure, Category = "Combat|Block")
	bool IsBlocking() const { return bIsBlocking; }
	UFUNCTION(BlueprintPure, Category = "Combat|Block")
	bool IsParryWindowActive() const { return bParryWindowActive; }
	void SetCombatEnabled(bool bEnabled);
	float ModifyIncomingDamage(float IncomingDamage, const AActor* DamageSource);
	void PerformAttackHit();
	void StartAttackWindow(FName InAttackBoneName,float InTraceRadius);
    void EndAttackWindow();
	void OpenComboInputWindow();
	void CloseComboInputWindow();
	void AddDamageBonus(float Amount);
	void MultiplyDamage(float Multiplier);
	void MultiplyAttackCooldown(float Multiplier);
	void MultiplyMeleeReach(float Multiplier);
	bool IsMeleeAttackInProgress() const { return bMeleeAttackInProgress; }
	UFUNCTION(BlueprintPure, Category = "Combat|Combo")
	bool HasBufferedComboInput() const { return ComboBuffer.bQueued; }
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float Damage = 25.f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float AttackRange = 150.f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float AttackRadius = 50.f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float AttackCooldown = 0.5f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	TArray<TObjectPtr<UAnimMontage>> AttackMontages;
	/** Used for unarmed attacks or weapons without their own hit effect. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Effects")
	TObjectPtr<UParticleSystem> DefaultMeleeHitEffect;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Special Attacks")
	TObjectPtr<UAnimMontage> UppercutMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Special Attacks")
	TObjectPtr<UAnimMontage> AirAttackMontage;
	/** Optional ordered air combo. Empty uses the existing single AirAttackMontage. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Air Attacks")
	TArray<TObjectPtr<UAnimMontage>> AirAttackMontages;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Air Attacks")
	TObjectPtr<UAnimMontage> AirDiveMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Air Attacks")
	FName AirDiveStartSection = TEXT("Start");
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Air Attacks")
	FName AirDiveLoopSection = TEXT("Loop");
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Air Attacks")
	FName AirDiveLandSection = TEXT("Land");
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Air Attacks", meta = (ClampMin = "0.0"))
	float AirDiveDamageMultiplier = 1.75f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Special Attacks", meta = (ClampMin = "0.0"))
	float UppercutDamageMultiplier = 1.5f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Special Attacks", meta = (ClampMin = "0.0"))
	float AirAttackDamageMultiplier = 1.25f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Special Attacks", meta = (ClampMin = "0.0"))
	float UppercutLaunchVelocity = 520.f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Special Attacks", meta = (ClampMin = "0.0"))
	/** Legacy property name retained for BP values; applies ONLY to Air Dive. */
	float AirAttackDownwardVelocity = 650.f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlockDamageMultiplier = 0.25f;
	/** Total frontal blocking arc in degrees. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float BlockArcDegrees = 120.f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.0"))
	float BlockStaminaCostPerHit = 15.f;
	/** Initial perfect-parry window. A quick tap keeps this window alive after release. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float PerfectParryWindow = 0.22f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	float ComboResetTime = 1.f;
	/** Accept the next attack press before the montage's ComboInput window opens. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Combo")
	bool bAllowEarlyComboBuffer = true;
	/** Migration switch only. Prefer an explicit Combo Chain Point notify. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Combo")
	bool bChainOnLegacyComboWindowEnd = false;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Debug")
	bool bDrawHandTraceDebug = true;
	
private:
	const UWeaponDefinition* GetEquippedWeaponDefinition() const;
	float GetEffectiveDamage() const;
	float GetEffectiveAttackCooldown() const;
	float GetEffectiveAttackRange() const;
	float GetEffectiveAttackRadius() const;
	const TArray<TObjectPtr<UAnimMontage>>& GetEffectiveAttackMontages() const;
	bool BeginRangedAttack(const UWeaponDefinition& WeaponDefinition, AActor* TargetActor);
	void PerformRangedAttack(
		const UWeaponDefinition& WeaponDefinition,
		AActor* TargetActor);
	void HandleRangedMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	bool StartMeleeMontageAttack();
	bool StartAirMontageAttack();
	bool CanChainAttack() const;
	void ResetMeleeAction();
	void StopAttackEffects();
	void HandleAttackBlendingOut(UAnimMontage* Montage, bool bInterrupted, uint64 Generation);
	bool StartSpecialMontageAttack(
		UAnimMontage* Montage,
		float DamageScale,
		EActiveCombatAttackType AttackType);
	void SpawnMeleeHitEffect(const FHitResult& Hit) const;
	void ApplySpecialHitReaction(AActor* HitActor) const;
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint64 Generation);
	bool TryCommitBufferedCombo();
	void EndParryWindow();
	void CancelBlock();

	float LastAttackTime = -BIG_NUMBER;
	int32 NextAttackIndex = 0;
	int32 NextAirAttackIndex = 0;
	int32 ActiveAttackInstanceId = INDEX_NONE;
	uint64 AttackGeneration = 0;
	TPCActionRules::FComboBuffer ComboBuffer;
    TPCActionRules::FDashComboWindow DashComboWindow;
    bool bDashUsedInCombo = false;
    TWeakObjectPtr<const UWeaponDefinition> DashComboWeapon;
    TWeakObjectPtr<AWeaponActor> DashComboWeaponActor;
    TArray<TWeakObjectPtr<UAnimMontage>> DashComboMontages;
    bool IsDashComboContextValid() const;
	bool bAirDiveLanded = false;
	friend struct FTPActionTestAccess;
	bool bAttackWindowActive = false;
	FVector PreviousAttackLocation = FVector::ZeroVector;
	FVector PreviousBladeBaseLocation = FVector::ZeroVector;
	FVector PreviousBladeTipLocation = FVector::ZeroVector;
	TSet<TObjectPtr<AActor>> HitActors;
	virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
	FName ActiveAttackBoneName = NAME_None;
	float ActiveTraceRadius = 18.f;
	bool bUseWeaponBladeTrace = false;
	FName ActiveBladeBaseSocketName = NAME_None;
	FName ActiveBladeTipSocketName = NAME_None;
	TWeakObjectPtr<USkeletalMeshComponent> ActiveWeaponTraceMesh;
	TWeakObjectPtr<UAnimMontage> ActiveAttackMontage;
	bool bMeleeAttackInProgress = false;
	bool bRangedAttackInProgress = false;
	bool bRangedProjectileReleased = false;
	TWeakObjectPtr<AActor> PendingRangedTarget;
	TWeakObjectPtr<UWeaponDefinition> PendingRangedWeapon;
	TWeakObjectPtr<UAnimMontage> ActiveRangedMontage;
	bool bComboInputWindowOpen = false;
	bool bIsBlocking = false;
	bool bBlockInputHeld = false;
	bool bParryWindowActive = false;
	bool bCombatEnabled = true;
	FTimerHandle ParryWindowTimerHandle;
	float ActiveDamageMultiplier = 1.f;
	EActiveCombatAttackType ActiveAttackType = EActiveCombatAttackType::Normal;
	float LevelDamageBonus = 0.f;
	float DamageMultiplier = 1.f;
	float AttackCooldownMultiplier = 1.f;
	float MeleeReachMultiplier = 1.f;
};
