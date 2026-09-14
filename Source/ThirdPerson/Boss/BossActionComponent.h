#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossDefinition.h"
#include "../Components/CombatHitTypes.h"
#include "BossActionComponent.generated.h"

class ACountessBossCharacter;
class UHealthComponent;
class UParticleSystemComponent;
class ABossTelegraph;
class UBossStatusWidget;
class AArenaBounds;

DECLARE_MULTICAST_DELEGATE(FOnBossStatusChanged);

/** Boss 遭遇与动作状态机：统一处理选招、对峙、阶段、破韧、伤害窗口和重置，防止多个状态同时控制角色。 */
UCLASS(ClassGroup=(Boss), meta=(BlueprintSpawnableComponent))
class THIRDPERSON_API UBossActionComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UBossActionComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") TObjectPtr<UBossDefinition> Definition;
 /** 绑定场地后以边界判断脱战，替代距离与遮挡超时规则；未绑定的 Boss 保持旧规则。 */
 UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Boss|Arena") TObjectPtr<AArenaBounds> ArenaBoundary;
 UPROPERTY(EditAnywhere, Category="Boss|Arena", meta=(ClampMin="0.1")) float ArenaExitGrace = 2.f;
 UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Boss") EBossState State = EBossState::Dormant;
 UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Boss") int32 Phase = 1;
 UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Boss") float Poise = 100.f;
 UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Boss") bool bPhasePending = false;
 UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Boss") EBossAction CurrentAction = EBossAction::None;
 UFUNCTION(BlueprintPure, Category="Boss") bool IsActionActive() const { return CurrentAction != EBossAction::None; }
 UFUNCTION(BlueprintPure, Category="Boss") AActor* GetTarget() const { return Target.Get(); }
 UFUNCTION(BlueprintPure, Category="Boss") float GetMaxPoise() const;
 UFUNCTION(BlueprintPure, Category="Boss") bool IsEncounterActive() const;
 UFUNCTION(BlueprintPure, Category="Boss") bool CanMove() const;
 UFUNCTION(BlueprintCallable, Category="Boss") bool BeginEncounter(AActor* Player);
 UFUNCTION(BlueprintCallable, Category="Boss") void RequestReset();
 UFUNCTION(BlueprintCallable, Category="Boss") bool TryStartAction(EBossAction Action);
 UFUNCTION(BlueprintCallable, Category="Boss") void CancelAction();
 void OnParried(AActor* Player);
 void Die();
 void ConsiderPhase(float Current, float Max);
 void UpdateContext(float Delta);
 EBossAction SelectAction(float Distance, bool bLOS);
 bool IsActionLegal(EBossAction Action, float Distance, bool bLOS) const;
 void DriveDecision();
 void CompleteReset();
 void FaceTarget(float Delta, float DegreesPerSecond);
 void OnLanded();
 FVector GetHomeLocation() const { return HomeLocation; }
 FVector GetLastKnownLocation() const { return LastKnownLocation; }
 bool HasTargetLOS() const { return bHasLOS; }
 int64 GetActionSerial() const { return ActionSerial; }
 bool IsCurrentNotify(const UAnimSequenceBase* Animation, int32 InstanceId) const;
 void NotifyWindow(bool bOpen, const UAnimSequenceBase* Animation, int32 InstanceId);
 FOnBossStatusChanged OnStatusChanged;
 const UBossDefinition* GetDefinition() const;
 float GetStateElapsed() const;
 float GetHitReactionAlpha() const;
 float GetHitReactionTime() const { return FMath::Max(0.f,static_cast<float>(Now()-LastHitReaction)); }
 bool IsStrafing() const { return bStrafing; }
 UFUNCTION(BlueprintPure, Category="Boss") bool IsStandoffActive() const { return bStandoff; }
 float GetFacingDelta() const;
 int32 GetContextRevision() const { return ContextRevision; }
 int32 GetHitReactionDirection() const { return HitReactionDirection; }
private:
 friend struct FCombatVFXTestAccess;
 friend struct FCountessBossTestAccess;
 friend struct FCountessPIETestAccess;
 friend struct FCountessReadableMovementAccess;
 friend struct FArenaTestAccess;
 friend struct FCombatUIAccess;
 bool bStandoff=false;
 bool bStandoffRolled=false;
 int32 StandoffPathFailures=0;
 double StandoffUntil=0,NextStandoffAt=0;
 bool TickStandoff(float Distance);
 enum class EActionStep : uint8 { None, Telegraph, Playing, Recovery };
 UPROPERTY(Transient) TObjectPtr<ACountessBossCharacter> Boss;
 UPROPERTY(Transient) TObjectPtr<UHealthComponent> Health;
 UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActiveMontage;
 UPROPERTY(Transient) TArray<TObjectPtr<UParticleSystemComponent>> Effects;
 UPROPERTY(Transient) TObjectPtr<ABossTelegraph> TelegraphActor;
 UPROPERTY(Transient) TObjectPtr<UBossStatusWidget> StatusWidget;
 TWeakObjectPtr<AActor> Target;
 FVector HomeLocation=FVector::ZeroVector;
 FRotator HomeRotation=FRotator::ZeroRotator;
 FVector LastKnownLocation=FVector::ZeroVector;
 FVector LockedDirection=FVector::ForwardVector;
 FVector PreviousBlade[4];
 TSet<TWeakObjectPtr<AActor>> WindowHits;
 TMap<EBossAction,double> CooldownUntil;
 FRandomStream Random;
 EBossAction LastSpecial=EBossAction::None;
 EActionStep Step=EActionStep::None;
 int64 ActionSerial=0;
 int32 MontageInstanceId=INDEX_NONE;
 int32 StageIndex=0;
 int32 HitReactionDirection=0;
 int32 ContextRevision=0;
 uint16 MoveSourceId=0;
 double StateStarted=0;
 double StepEnd=0;
 double StageStarted=0;
 double LastPoiseHit=-100;
 double PoiseImmuneUntil=0;
 double RecoilUntil=0;
 double LastHitReaction=-100;
 float HitReactionStrength=.32f;
 float HitReactionDuration=.35f;
 double NextMoveRequest=0;
 double LastSeen=0;
 double OutsideSince=-1;
 float PreviousClipTime=-KINDA_SMALL_NUMBER;
 bool bInitialized=false;
 bool bHasLOS=false;
 bool bIntroSeen=false;
 bool bWindowOpen=false;
 bool bOneShotFired=false;
 bool bMoveStarted=false;
 bool bFacingCommitted=false;
 bool bTrailsStarted=false;
 bool bMovementSaved=false;
 bool bSavedOrient=false;
 bool bSavedControllerYaw=false;
 bool bSavedRVO=false;
 bool bReversingOrbit=false;
 bool bStrafing=true;
 double Now() const;
 void SetState(EBossState NewState);
 void LockMovement();
 void RestoreMovement();
 void RemoveOwnRootMotion();
 void ClearEffects();
 void ShowTelegraph();
 void UpdateTelegraphTransform();
 void StartStage();
 void FinishStage();
 void StartRecovery();
 void TickAction(float Delta);
 void SampleDamage(float OldTime, float NewTime);
 bool ReadBladePoints(FVector* Out) const;
 void SweepBlades();
 void DamageArea();
 void ReleaseWave();
 void ApplyHit(AActor* Victim,const FVector& Point);
 bool HasLineOfSightTo(AActor* Actor) const;
 void StartRush();
 bool ValidateRush(FVector& Destination) const;
 void PlayUtility(UAnimSequence* Sequence,float PlayRate=1.f);
 void OnMontageEnded(UAnimMontage* Montage,bool bInterrupted,int64 Serial);
 void OnResolvedHit(const FCombatHitSpec& Spec,const FCombatHitResult& Result,AActor* Source);
 void ReducePoise(float Amount);
 void StartPhaseTransition();
 void MoveToGoal(const FVector& Goal,float Speed,bool bStrafe);
 void TickReset();
};
