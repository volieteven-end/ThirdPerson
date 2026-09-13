#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BossDefinition.generated.h"

class UAnimSequence;
class UAnimMontage;
class UParticleSystem;
class UMaterialInterface;
class AWeaponProjectile;
class UBehaviorTree;
class UBossStatusWidget;

UENUM(BlueprintType)
enum class EBossAction : uint8 { None, Combo, DelayedSlash, Siphon, ShadowRush, BloodWave, BloodFeast };
UENUM(BlueprintType)
enum class EBossState : uint8 { Dormant, Intro, Combat, Action, PoiseBroken, PhaseTransition, Resetting, Dead };
UENUM(BlueprintType)
enum class EBossHitShape : uint8 { Blades, Radial, Projectile };
UENUM(BlueprintType)
enum class EBossLocomotionState : uint8 { Idle, Start, Moving, Stop, Pivot, Turn };

USTRUCT(BlueprintType)
struct THIRDPERSON_API FBossActionStage
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> Sequence;
 /** Optional authored montage. Empty creates a DefaultSlot montage from Sequence at runtime. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimMontage> Montage;
 /** Readable body motion AFTER the entry blend, before damage (before movement for a rush). */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", Units="s")) float MinReadableWindup = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimSequence> RecoverySequence;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UAnimMontage> RecoveryMontage;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", Units="s")) float EntryBlendTime = .08f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float Damage = 16.f;
 /** Bootstrap values used only when creating a new Montage. An assigned Montage's Notify is authoritative. */
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HitStart = .15f;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float HitEnd = .37f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float FacingCommitTime = .10f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) EBossHitShape Shape = EBossHitShape::Blades;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float Radius = 260.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bCanBeBlocked = true;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bCanBeParried = true;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float MoveStart = -1.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float MoveEnd = -1.f;
 float Length() const;
 bool ReadHitWindow(float& Start,float& End) const;
};

USTRUCT(BlueprintType)
struct THIRDPERSON_API FBossActionDefinition
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadOnly) EBossAction Id = EBossAction::None;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FBossActionStage> Stages;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float MinRange = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaxRange = 220.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float Telegraph = .4f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float Recovery = .55f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float Cooldown = 2.5f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float PhaseOneWeight = 45.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly) float PhaseTwoWeight = 35.f;
};

/** First playable tuning; all source animations remain in the Paragon package. */
UCLASS(BlueprintType)
class THIRDPERSON_API UBossDefinition : public UDataAsset
{
 GENERATED_BODY()
public:
 UBossDefinition();
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") FText DisplayName;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss", meta=(ClampMin="1")) float MaxHealth = 1500.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") int32 Experience = 300;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float PhaseThreshold = .5f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float PoiseOne = 100.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float PoiseTwo = 120.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float PoiseRegenDelay = 5.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float PoiseRegenPerSecond = 12.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float BreakDuration = 2.4f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float PostBreakPoiseImmunity = .8f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float ParryPoiseDamage = 40.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float ParryRecoil = .7f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float EngageRadius = 900.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float HomeLeash = 1600.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float LostSightGrace = 5.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float LeashGrace = 2.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float ChaseSpeed = 400.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float ChaseSpeedTwo = 450.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float OrbitSpeed = 180.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Standoff", meta=(ClampMin="0",ClampMax="1")) float StandoffChance=.3f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Standoff") FVector2D StandoffDuration=FVector2D(.8,1.4);
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Standoff", meta=(ClampMin="0")) float StandoffCooldown=5.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float RetreatSpeed = 220.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float ComboGap = .35f;
 /** Versioned, targeted editor migration. Never rebuilds the encounter map or vendor package. */
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Animation") int32 AnimationRevision = 0;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float RushMaxDistance = 500.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float RushStopDistance = 120.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float BladeTraceRadius = 22.f;
 /** Horizontal melee reach; radial skills and projectiles keep their authored sizes. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss", meta=(ClampMin="0.1")) float MeleeReachScale = 1.15f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") float AreaVerticalTolerance = 140.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") int32 RandomSeed = 137;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Actions") TArray<FBossActionDefinition> Actions;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") TObjectPtr<UBehaviorTree> BehaviorTree;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") TSubclassOf<UBossStatusWidget> StatusWidgetClass;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TObjectPtr<UAnimSequence> Intro;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TObjectPtr<UAnimSequence> PhaseCast;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TObjectPtr<UAnimSequence> StunStart;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TObjectPtr<UAnimSequence> StunLoop;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TObjectPtr<UAnimSequence> Death;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TObjectPtr<UAnimSequence> Idle;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TObjectPtr<UAnimSequence> Relaxed;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TObjectPtr<UAnimSequence> Falling;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TObjectPtr<UAnimSequence> Landing;
 /** Forward, backward, left, right. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TArray<TObjectPtr<UAnimSequence>> Jog;
 /** Measured planted-foot travel at play rate 1, forward/backward/left/right, cm/s. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TArray<float> JogReferenceSpeeds={300.f,300.f,300.f,300.f};
 /** All directional arrays use forward, backward, left, right. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TArray<TObjectPtr<UAnimSequence>> MoveStarts;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TArray<TObjectPtr<UAnimSequence>> MoveStops;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TArray<TObjectPtr<UAnimSequence>> MovePivots;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TArray<TObjectPtr<UAnimSequence>> CircleLeft;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TArray<TObjectPtr<UAnimSequence>> CircleRight;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TArray<float> CircleLeftReferenceSpeeds;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TArray<float> CircleRightReferenceSpeeds;
 /** Left 90, right 90, left 180, right 180; root-neutral clips, body yaw has a single owner. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TArray<TObjectPtr<UAnimSequence>> Turns;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") float MovementBlendTime = .15f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") float MoveStartThreshold = 35.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") float MoveStopThreshold = 12.f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") TArray<TObjectPtr<UAnimSequence>> HitReactions;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UParticleSystem> ImpactEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UParticleSystem> SiphonEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UParticleSystem> RushEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UParticleSystem> RushBeginEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UParticleSystem> RushArriveEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UParticleSystem> WaveCastEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UParticleSystem> FeastEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UParticleSystem> PhaseEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UParticleSystem> TrailEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UParticleSystem> SiphonCastEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UParticleSystem> SiphonHitEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UParticleSystem> RushSlashEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UParticleSystem> FeastSlashEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UParticleSystem> WaveFlightEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UParticleSystem> WaveImpactEffect;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects", meta=(ClampMin=".1", ClampMax="2")) float SkillAccentScale = .65f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects", meta=(ClampMin=".1", ClampMax="2")) float WaveEffectScale = .55f;
 /** Cosmetic only. Impact is intentionally independent of the flight silhouette and collision sphere. */
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects", meta=(ClampMin=".1", ClampMax="2")) float WaveImpactScale = .85f;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TObjectPtr<UMaterialInterface> WarningMaterial;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effects") TSubclassOf<AWeaponProjectile> WaveClass;
 const FBossActionDefinition* FindAction(EBossAction Id) const;
 bool Validate(FString& Error) const;
};
