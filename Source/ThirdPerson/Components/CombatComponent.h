
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Animation/CombatActionRules.h"
#include "CombatHitTypes.h"
#include "CombatComponent.generated.h"

class UAnimMontage;
class AWeaponActor;
class UAnimSequenceBase;
class UWeaponDefinition;
class USkeletalMeshComponent;
class UParticleSystem;
class UActionComponent;
class UActionDefinition;
class UActionSet;

DECLARE_MULTICAST_DELEGATE(FOnMeleeAttackStartedNative);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnMeleeHitResolvedNative,AActor*,const FCombatHitSpec&,const FCombatHitResult&);

enum class EActiveCombatAttackType : uint8
{
	Normal,
	Uppercut,
	Air,
	AirDive,
	Special,
    Buff,
    DrawWeapon,
    SheatheWeapon
};
/** 战斗执行层：播放攻击、管理防御与命中窗口、计算有效伤害并发起命中；最终扣血统一由生命组件处理。 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THIRDPERSON_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
    void SetAttackTraceDebugVisible(bool bVisible) { bDrawHandTraceDebug = bVisible; }
	UCombatComponent();
	/** 供角色监听的原生事件，用于攻击朝向与目标吸附。 */
	FOnMeleeAttackStartedNative OnMeleeAttackStarted;
	FOnMeleeHitResolvedNative OnMeleeHitResolved;
	UFUNCTION(BlueprintPure, Category="Combat|Attributes") float GetPresentationDamage() const { return GetEffectiveDamage(); }

	void TryAttack();
	/** AI 发起远程攻击的入口，投射物瞄准 TargetActor。 */
	bool TryRangedAttackAt(AActor* TargetActor);
	/** 由远程攻击动画通知在松弦帧调用，不在起手时提前发射。 */
	void ReleaseRangedProjectile();
	bool IsRangedAttackInProgress() const { return bRangedAttackInProgress; }
	void TryUppercutAttack();
	bool TrySprintAttack();
	bool TryParryCounter();
    bool TryBuff();
    bool TryToggleWeapon();
    UFUNCTION(BlueprintPure, Category="Combat|Buff") float GetSwordBuffMultiplier() const;
    void ClearSwordBuff() { SwordBuffExpiresAt = -1.f; SwordBuffMultiplier = 1.f; }
    /** 在蒙太奇提交通知中调用，丢弃已过期实例的回调。 */
    void NotifyActionCommit(UAnimSequenceBase* Animation, int32 MontageInstanceId);
	void CommitSpecialMovement();
	void UpdateDiveApproach();
	const UActionSet* GetActionSet() const;
	void TryAirAttack();
	/** 独立的下砸输入；普通空中攻击不会强制向下发射角色。 */
	void TryAirDiveAttack();
	/** 真实接触地面前保持下落状态；敌人胶囊体不能被当作下砸终点。 */
	bool IsAirDiveDescending() const
	{
		return bCombatEnabled && bMeleeAttackInProgress &&
			ActiveAttackType == EActiveCombatAttackType::AirDive && !bAirDiveLanded;
	}
	void HandleOwnerLanded();
	void CancelActiveAttack(float BlendOutTime = 0.1f);

    // 普通地面连招仅第一次闪避可以保留待续阶段。
    void CancelAttackForDash(float BlendOutTime);
    bool FinishDashForCombo(bool bInterrupted, float ContinueWindow);
    bool QueueAttackDuringDash();
    bool TryResumeComboAfterDash();
    void ClearDashCombo();
	void ReachComboChainPoint(UAnimSequenceBase* Animation, int32 MontageInstanceId = INDEX_NONE);
	bool IsCurrentAttackNotify(const UAnimSequenceBase* Animation, int32 MontageInstanceId) const;
	void StartBlock();
	void StopBlock();
	void CancelGuard() { CancelBlock(); }
	UFUNCTION(BlueprintPure, Category = "Combat|Block")
	bool IsBlocking() const { return bIsBlocking; }
	UFUNCTION(BlueprintPure, Category = "Combat|Block")
	bool IsParryWindowActive() const { return bParryWindowActive; }
	void SetCombatEnabled(bool bEnabled);
	bool IsCombatEnabled() const { return bCombatEnabled; }
	float ModifyIncomingDamage(float IncomingDamage, const AActor* DamageSource);
	float ResolveIncomingHit(const FCombatHitSpec& Spec, const AActor* Source, FCombatHitResult& Result);
	FCombatHitSpec MakeCurrentHitSpec(const FVector& ImpactPoint) const;
	void PerformAttackHit();
	void StartAttackWindow(FName InAttackBoneName,float InTraceRadius, FName HitGroup = TEXT("Primary"));
	void EndAttackWindow();
    void FinishAuthoredDamageWindow(UAnimSequenceBase* Animation, int32 MontageInstanceId);
	void OpenComboInputWindow();
	void CloseComboInputWindow();
	void AddDamageBonus(float Amount);
	void MultiplyDamage(float Multiplier);
	void MultiplyAttackCooldown(float Multiplier);
	void MultiplyMeleeReach(float Multiplier);
	/** 基础配置，与永久升级倍率分开保存和计算。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.1"))
	float BaseMeleeReachScale = 1.f;
	void RestoreRespawnAttributes(const UCombatComponent& Source);
	bool IsMeleeAttackInProgress() const { return bMeleeAttackInProgress; }
	UFUNCTION(BlueprintPure, Category = "Combat|Combo")
	bool HasBufferedComboInput() const;
protected:
	/** 无可用武器时的基础伤害回退，不与武器基础伤害相加。 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float Damage = 25.f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float AttackRange = 150.f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float AttackRadius = 50.f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float AttackCooldown = 0.5f;
	/** 保留蓝图序列化字段；当前原生实现未读取此值，不作为现行调参入口。 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	TArray<TObjectPtr<UAnimMontage>> AttackMontages;
	/** 用于徒手攻击或没有专属命中特效的武器。 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Effects")
	TObjectPtr<UParticleSystem> DefaultMeleeHitEffect;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Special Attacks")
	TObjectPtr<UAnimMontage> UppercutMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Special Attacks")
	TObjectPtr<UAnimMontage> AirAttackMontage;
	/** 可选的有序空中连招；为空时回退到原有单个空中攻击蒙太奇。 */
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
	/** 为保留蓝图已存数值而沿用旧字段名；该向下速度只作用于下砸。 */
	float AirAttackDownwardVelocity = 650.f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlockDamageMultiplier = 0.25f;
	/** 正面可格挡扇形的总角度，单位为度。 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float BlockArcDegrees = 120.f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.0"))
	float BlockStaminaCostPerHit = 15.f;
	/** 起始完美弹反窗口；快速点按松开后仍保留尚未结束的窗口。 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float PerfectParryWindow = 0.22f;
	/** 保留蓝图序列化字段；当前原生实现未读取此值，不作为现行调参入口。 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Animation")
	float ComboResetTime = 1.f;
	/** 允许在动画的连招输入窗口打开前缓存下一次攻击按键。 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Combo")
	bool bAllowEarlyComboBuffer = true;
	/** 仅用于兼容旧资源；新动作使用明确的连招衔接点通知。 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Combo")
	bool bChainOnLegacyComboWindowEnd = false;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Debug")
	bool bDrawHandTraceDebug = false;
	
private:
	friend struct FTPCRespawnTestAccess;
	UActionComponent* GetActions() const;
	TPCActionRules::FComboBuffer& GetComboBuffer();
	uint64 GetComboGeneration() const;
	bool StartDefinedAttack(const UActionDefinition* Definition, EActiveCombatAttackType Type);
	const UActionDefinition* GetActiveDefinition() const;
    float SwordBuffExpiresAt = -1.f;
    float SwordBuffMultiplier = 1.f;
    bool bPosePolicySaved = false;
    uint8 PreviousPosePolicy = 0;
	uint64 OwnedActionId = 0;
	uint64 GuardActionId = 0;
	float ParryCounterExpiresAt = -1.f;
	bool bSpecialMovementCommitted = false;
	bool bDiveApproachStarted = false;
	mutable TArray<TObjectPtr<UAnimMontage>> ResolvedGroundMontages;
	const UWeaponDefinition* GetEquippedWeaponDefinition() const;
	bool IsUnarmedPlayer() const;
	float GetMeleeTraceScale() const { return BaseMeleeReachScale * MeleeReachMultiplier; }
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
		EActiveCombatAttackType AttackType,
        const UActionDefinition* AuthoredDefinition = nullptr);
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
	friend struct FCombatVFXTestAccess;
	friend struct FMeleeAITestAccess;
    friend struct FTPCSwordPIETestAccess;
    friend struct FTPCSwordActionTestAccess;
    friend struct FTutorialImpactTestAccess;
 friend struct FCountessBossTestAccess;
	bool bAttackWindowActive = false;
	FVector PreviousAttackLocation = FVector::ZeroVector;
	FVector PreviousBladeBaseLocation = FVector::ZeroVector;
	FVector PreviousBladeTipLocation = FVector::ZeroVector;
    float PreviousTraceMontagePosition = 0.f;
    FTransform PreviousTraceMeshTransform;
	TSet<TObjectPtr<AActor>> HitActors;
	TMap<FName, TSet<TObjectPtr<AActor>>> HitGroups;
	FName ActiveHitGroup = TEXT("Primary");
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
