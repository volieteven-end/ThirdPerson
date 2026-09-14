#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ActionDefinition.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class ETPCActionState : uint8 { Free, Attack, Dodge, Guard, HitReact, Knockdown, GetUp, Turn, Skill, Dead };
UENUM(BlueprintType)
enum class ETPCMovementContext : uint8 { Ground, Air };
UENUM(BlueprintType)
enum class ETPCMovementPolicy : uint8 { Input, RootMotionXY, Ballistic, Locked };
UENUM(BlueprintType)
enum class ETPCRotationPolicy : uint8 { Movement, CameraAnticipation, Root, Frozen };
UENUM(BlueprintType)
enum class ETPCActionIntent : uint8 { None, PrimaryAttack, Dodge, Guard, Jump, Dive, Buff, ToggleWeapon };
UENUM(BlueprintType)
enum class ETPCHitReactionProfile : uint8 { Light, Launch, Knockback, Knockdown };

/** 单个招式的只读配置：保存动画、消耗、倍率与动作窗口；播放状态和命中记录由组件持有，不写回资产。 */
UCLASS(BlueprintType)
class THIRDPERSON_API UActionDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName ActionId;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UAnimMontage> Montage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName EntrySection;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) ETPCActionState State = ETPCActionState::Attack;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) ETPCMovementPolicy MovementPolicy = ETPCMovementPolicy::RootMotionXY;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) ETPCRotationPolicy RotationPolicy = ETPCRotationPolicy::CameraAnticipation;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0.1")) float PlayRate = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0")) float StaminaCost = 0.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0")) float DamageMultiplier = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0")) float PoiseDamage = 10.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) ETPCHitReactionProfile HitReactionProfile = ETPCHitReactionProfile::Light;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<FName> AllowedNextActions;
    /** 使用蒙太奇时间轴上的秒数，不是真实经过时间；调整播放速度不会改变窗口在动画中的位置。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float CancelStart = -1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float CancelEnd = -1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<ETPCActionIntent> CancelIntents;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float FacingEnd = 0.18f;
    /** 独立于播放速度缩放根运动位移，限制位移量时不加快动画。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="1")) float RootMotionTranslationScale = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float CommitTime = -1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float InvulnerabilityStart = -1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float InvulnerabilityEnd = -1.f;
    /** 闪避位移结束并归还控制权的蒙太奇时刻；视觉收招独立混出，-1 沿用角色兼容回退。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(Units="s")) float ControlReturnTime = -1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0.02", ClampMax="0.25", Units="s")) float ControlReturnBlendTime = .10f;
};

/** 武器动作集合：将攻击意图和连招阶段映射到招式资产，不负责播放动画或结算伤害。 */
UCLASS(BlueprintType)
class THIRDPERSON_API UActionSet : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<TObjectPtr<UActionDefinition>> GroundCombo;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<TObjectPtr<UActionDefinition>> AirCombo;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UActionDefinition> Rising;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UActionDefinition> Dive;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UActionDefinition> SprintAttack;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UActionDefinition> ParryCounter;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UActionDefinition> Buff;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UActionDefinition> DrawWeapon;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UActionDefinition> SheatheWeapon;
    /** 按前、后、左、右排列，与角色局部方向的量化结果一致。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<TObjectPtr<UActionDefinition>> Dodges;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<TObjectPtr<UActionDefinition>> Turns;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UAnimMontage> DoubleJumpMontage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) bool bAllowDoubleJump = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0")) float BuffDuration = 8.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1")) float BuffDamageMultiplier = 1.2f;
    const UActionDefinition* FindByMontage(const UAnimMontage* Montage) const;
};
