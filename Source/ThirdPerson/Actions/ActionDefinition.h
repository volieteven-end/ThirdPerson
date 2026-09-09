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

/** Immutable authored move. Runtime state and hit sets belong to the owning components. */
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
    /** Montage timeline seconds, NOT elapsed wall time; playback speed stays independent. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float CancelStart = -1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float CancelEnd = -1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<ETPCActionIntent> CancelIntents;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float FacingEnd = 0.18f;
    /** Independent of PlayRate: bound large native displacement without speeding the animation. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0",ClampMax="1")) float RootMotionTranslationScale = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float CommitTime = -1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float InvulnerabilityStart = -1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float InvulnerabilityEnd = -1.f;
    /** Dodge montage time when its displacement is effectively finished and control returns.
     * The visual recovery blends out independently; -1 uses the character's legacy fallback. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(Units="s")) float ControlReturnTime = -1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0.02", ClampMax="0.25", Units="s")) float ControlReturnBlendTime = .10f;
};

/** Weapon-owned style. The old weapon montage array is used only when no action set exists. */
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
    /** Forward, backward, left, right, matching the character-local quantization. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<TObjectPtr<UActionDefinition>> Dodges;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TArray<TObjectPtr<UActionDefinition>> Turns;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UAnimMontage> DoubleJumpMontage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) bool bAllowDoubleJump = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0")) float BuffDuration = 8.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1")) float BuffDamageMultiplier = 1.2f;
    const UActionDefinition* FindByMontage(const UAnimMontage* Montage) const;
};
