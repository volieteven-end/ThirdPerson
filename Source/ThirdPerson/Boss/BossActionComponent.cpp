#include "BossActionComponent.h"
#include "../Weapons/CombatVFXSettings.h"
#include "CountessBossCharacter.h"
#include "BossTelegraph.h"
#include "BossStatusWidget.h"
#include "../Components/HealthComponent.h"
#include "../Weapons/ProjectilePoolSubsystem.h"
#include "../Weapons/WeaponProjectile.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RootMotionSource.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "../Animation/MeleeTraceGeometry.h"
#include "../Arena/ArenaActors.h"

static TAutoConsoleVariable<int32> CVarCountessDebug(TEXT("tp.Boss.Debug"),0,TEXT("Show Countess state and exact blade sweep endpoints."));

UBossActionComponent::UBossActionComponent()
{
 PrimaryComponentTick.bCanEverTick=true;
 PrimaryComponentTick.TickGroup=TG_PostPhysics;
}
const UBossDefinition* UBossActionComponent::GetDefinition() const
{ return Definition?Definition.Get():GetDefault<UBossDefinition>(); }
double UBossActionComponent::Now() const { return GetWorld()?GetWorld()->GetTimeSeconds():0.; }
float UBossActionComponent::GetMaxPoise() const { return Phase==2?GetDefinition()->PoiseTwo:GetDefinition()->PoiseOne; }
float UBossActionComponent::GetStateElapsed() const { return Now()-StateStarted; }
bool UBossActionComponent::IsEncounterActive() const
{ return State!=EBossState::Dormant && State!=EBossState::Dead && State!=EBossState::Resetting; }
bool UBossActionComponent::CanMove() const { return State==EBossState::Combat && !IsActionActive() && Now()>=RecoilUntil; }
void UBossActionComponent::BeginPlay()
{
 Super::BeginPlay();
 Boss=Cast<ACountessBossCharacter>(GetOwner());
 Health=Boss?Boss->FindComponentByClass<UHealthComponent>():nullptr;
 FString Error;
 if (!Boss || !Health || !GetDefinition()->Validate(Error))
 { UE_LOG(LogTemp,Error,TEXT("Countess Boss configuration: %s"),*Error); SetComponentTickEnabled(false); return; }
 Health->MaxHealth=GetDefinition()->MaxHealth; Health->SetCurrentHealth(Health->MaxHealth);
 Health->OnCombatHitResolved.AddUObject(this,&ThisClass::OnResolvedHit);
 HomeLocation=Boss->GetActorLocation(); HomeRotation=Boss->GetActorRotation();
 Poise=GetMaxPoise(); Random.Initialize(GetDefinition()->RandomSeed); StateStarted=Now();
 AddTickPrerequisiteComponent(Boss->GetMesh());
 bInitialized=true;
}
void UBossActionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 CancelAction();
 if (Health) Health->OnCombatHitResolved.RemoveAll(this);
 if (StatusWidget) { StatusWidget->RemoveFromParent(); StatusWidget=nullptr; }
 Super::EndPlay(Reason);
}
void UBossActionComponent::SetState(EBossState NewState)
{
 bStandoff=false;
 if (NewState==EBossState::Combat) bStandoffRolled=false;
 State=NewState; StateStarted=Now(); OnStatusChanged.Broadcast();
 UE_LOG(LogTemp,Verbose,TEXT("Countess State=%d Action=%d Serial=%lld Phase=%d Poise=%.1f MoveSource=%u"),static_cast<int32>(State),static_cast<int32>(CurrentAction),ActionSerial,Phase,Poise,MoveSourceId);
}
void UBossActionComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 if (!bInitialized || State==EBossState::Dead) return;
 if (CVarCountessDebug.GetValueOnGameThread())
 {
  DrawDebugString(GetWorld(),Boss->GetActorLocation()+FVector(0,0,150),FString::Printf(TEXT("State %d Action %d Serial %lld Phase %d Poise %.0f RMS %u"),static_cast<int32>(State),static_cast<int32>(CurrentAction),ActionSerial,Phase,Poise,MoveSourceId),nullptr,FColor::White,0,true);
  FVector Points[4]; if (ReadBladePoints(Points)) for (int32 I=0;I<2;++I)
  { DrawDebugLine(GetWorld(),Points[I*2],Points[I*2+1],bWindowOpen?FColor::Red:FColor::Cyan,false,0,0,2); DrawDebugSphere(GetWorld(),Points[I*2+1],GetDefinition()->BladeTraceRadius,8,FColor::Yellow,false,0); }
 }
 if (State==EBossState::Intro && Now()>=StepEnd)
 { Health->SetEncounterInvulnerable(false); ClearEffects(); RestoreMovement(); SetState(EBossState::Combat); }
 else if (State==EBossState::PhaseTransition && GetStateElapsed()>=1.5f)
 { Phase=2; bPhasePending=false; Poise=GetMaxPoise(); PoiseImmuneUntil=Now()+.8; Health->SetEncounterInvulnerable(false); ClearEffects(); RestoreMovement(); SetState(EBossState::Combat); }
 else if (State==EBossState::PoiseBroken && GetStateElapsed()>=GetDefinition()->BreakDuration)
 { Poise=GetMaxPoise(); PoiseImmuneUntil=Now()+GetDefinition()->PostBreakPoiseImmunity; RestoreMovement(); SetState(EBossState::Combat); }
 if (State==EBossState::Action) TickAction(Delta);
 if (State==EBossState::Combat && Now()>=RecoilUntil)
 {
  if (RecoilUntil>0)
  {
   if (ActiveMontage && Boss->GetMesh()->GetAnimInstance()) Boss->GetMesh()->GetAnimInstance()->Montage_Stop(.08f,ActiveMontage);
   ActiveMontage=nullptr; RecoilUntil=0;
  }
  RestoreMovement();
  if (bPhasePending) StartPhaseTransition();
  else if (bStrafing || Boss->GetVelocity().Size2D()<35.f)
  {
   // Chase uses CMC/path rotation. Strafing/standing has this one yaw owner.
   const float Speed=Boss->GetVelocity().Size2D();
   FaceTarget(Delta,Speed<35.f && GetDefinition()->AnimationRevision>=2 ? 95.f : 180.f);
  }
 }
 if (IsEncounterActive() && State!=EBossState::PoiseBroken && State!=EBossState::PhaseTransition &&
     Poise<GetMaxPoise() && Now()-LastPoiseHit>=GetDefinition()->PoiseRegenDelay)
 { Poise=FMath::Min(GetMaxPoise(),Poise+Delta*GetDefinition()->PoiseRegenPerSecond); OnStatusChanged.Broadcast(); }
}
bool UBossActionComponent::HasLineOfSightTo(AActor* Actor) const
{
 if (!Boss || !IsValid(Actor)) return false;
 FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(BossLOS),false,Boss); Q.AddIgnoredActor(Actor);
 TArray<AActor*> Attached; Actor->GetAttachedActors(Attached,true,true); Q.AddIgnoredActors(Attached);
 return !GetWorld()->LineTraceSingleByChannel(Hit,Boss->GetActorLocation()+FVector(0,0,40),Actor->GetActorLocation()+FVector(0,0,40),ECC_Visibility,Q);
}
bool UBossActionComponent::BeginEncounter(AActor* Player)
{
 const APawn* Pawn=Cast<APawn>(Player);
 const UHealthComponent* PlayerHealth=IsValid(Player)?Player->FindComponentByClass<UHealthComponent>():nullptr;
 if (!bInitialized || State!=EBossState::Dormant || !Pawn || !Pawn->IsPlayerControlled() || !PlayerHealth || PlayerHealth->CurrentHealth<=0) return false;
 if (ArenaBoundary && !ArenaBoundary->ContainsActor(Player)) return false;
 Target=Player; LastKnownLocation=Player->GetActorLocation(); LastSeen=Now(); bHasLOS=HasLineOfSightTo(Player);
 Health->SetEncounterInvulnerable(true); LockMovement(); SetState(EBossState::Intro);
 StepEnd=Now()+(bIntroSeen?.75f:GetDefinition()->Intro->GetPlayLength());
 if (!bIntroSeen) PlayUtility(GetDefinition()->Intro);
 bIntroSeen=true;
 if (APlayerController* PC=Cast<APlayerController>(Pawn->GetController()); PC && PC->IsLocalController())
 {
  if (!StatusWidget) StatusWidget=CreateWidget<UBossStatusWidget>(PC,GetDefinition()->StatusWidgetClass?GetDefinition()->StatusWidgetClass.Get():UBossStatusWidget::StaticClass());
  if (StatusWidget) { StatusWidget->BindBoss(Boss); StatusWidget->AddToViewport(20); }
 }
 return true;
}
void UBossActionComponent::UpdateContext(float Delta)
{
 ++ContextRevision;
 if (!bInitialized || State==EBossState::Dead) return;
 if (State==EBossState::Resetting) { TickReset(); return; }
 if (State==EBossState::Dormant)
 {
  APawn* Player=UGameplayStatics::GetPlayerPawn(this,0); // Local player lookup, not an actor scan.
  if (Player && (ArenaBoundary ? ArenaBoundary->ContainsActor(Player) :
      FVector::DistSquared2D(Player->GetActorLocation(),Boss->GetActorLocation())<=FMath::Square(GetDefinition()->EngageRadius) && HasLineOfSightTo(Player))) BeginEncounter(Player);
  return;
 }
 AActor* Player=Target.Get();
 const UHealthComponent* H=Player?Player->FindComponentByClass<UHealthComponent>():nullptr;
 if (!Player || !H || H->CurrentHealth<=0) { RequestReset(); return; }
 bHasLOS=HasLineOfSightTo(Player);
 if (ArenaBoundary)
 {
  const bool bInside = ArenaBoundary->ContainsActor(Player);
  if (bInside)
  {
   OutsideSince=-1; LastSeen=Now();
   // Keep pursuing/repathing inside the encounter; attacks still use bHasLOS.
   LastKnownLocation=Player->GetActorLocation();
  }
  else if (OutsideSince<0) OutsideSince=Now();
  if (OutsideSince>=0 && Now()-OutsideSince>=ArenaExitGrace) RequestReset();
  return;
 }
 if (bHasLOS) { LastSeen=Now(); LastKnownLocation=Player->GetActorLocation(); }
 const bool bOutside=FVector::DistSquared2D(Boss->GetActorLocation(),HomeLocation)>FMath::Square(GetDefinition()->HomeLeash) ||
     FVector::DistSquared2D(Player->GetActorLocation(),HomeLocation)>FMath::Square(GetDefinition()->HomeLeash);
 if (!bOutside) OutsideSince=-1; else if (OutsideSince<0) OutsideSince=Now();
 if (Now()-LastSeen>=GetDefinition()->LostSightGrace || (OutsideSince>=0 && Now()-OutsideSince>=GetDefinition()->LeashGrace)) RequestReset();
}
void UBossActionComponent::ConsiderPhase(float Current,float Max)
{
 if (Phase==1 && Current>0 && Current<=Max*GetDefinition()->PhaseThreshold && IsEncounterActive()) bPhasePending=true;
 OnStatusChanged.Broadcast();
}
void UBossActionComponent::OnResolvedHit(const FCombatHitSpec& Spec,const FCombatHitResult& Result,AActor* Source)
{
 if (State==EBossState::Dead || Result.ActualDamage<=0) return;
 if (State==EBossState::Dormant) BeginEncounter(Source);
 ConsiderPhase(Health->CurrentHealth,Health->MaxHealth);
 if (State!=EBossState::Intro)
 {
  LastHitReaction=Now();
  HitReactionStrength=.32f; HitReactionDuration=.35f;
  FVector Local=Boss->GetActorTransform().InverseTransformVectorNoScale(Source?Source->GetActorLocation()-Boss->GetActorLocation():Boss->GetActorForwardVector());
  HitReactionDirection=FMath::Abs(Local.X)>=FMath::Abs(Local.Y)?(Local.X>=0?0:1):(Local.Y<0?2:3);
  ReducePoise(Spec.PoiseDamage);
 }
}
float UBossActionComponent::GetHitReactionAlpha() const
{
 if (State!=EBossState::Combat && State!=EBossState::Action) return 0;
 const float Age=GetHitReactionTime();
 return HitReactionStrength*FMath::Clamp(Age/.035f,0.f,1.f)*FMath::Clamp(1.f-Age/HitReactionDuration,0.f,1.f);
}
void UBossActionComponent::ReducePoise(float Amount)
{
 if (!FMath::IsFinite(Amount) || Amount<=0 || !IsEncounterActive() || State==EBossState::Intro ||
     State==EBossState::PhaseTransition || State==EBossState::PoiseBroken || Now()<PoiseImmuneUntil) return;
 LastPoiseHit=Now(); Poise=FMath::Max(0.f,Poise-Amount);
 if (Poise<=0)
 { CancelAction(); LockMovement(); SetState(EBossState::PoiseBroken); PlayUtility(GetDefinition()->StunStart); }
 OnStatusChanged.Broadcast();
}
void UBossActionComponent::OnParried(AActor* Player)
{
 if (!IsActionActive() || State!=EBossState::Action) return;
 CancelAction(); ReducePoise(GetDefinition()->ParryPoiseDamage);
 if (State!=EBossState::PoiseBroken)
 {
  const float Recoil=GetDefinition()->ParryRecoil;
  RecoilUntil=Now()+Recoil; LockMovement();
  const auto* Animation=GetDefinition()->StunStart.Get();
  PlayUtility(GetDefinition()->StunStart,Animation?Animation->GetPlayLength()/FMath::Max(.1f,Recoil):1.f);
 }
}
void UBossActionComponent::StartPhaseTransition()
{
 if (State!=EBossState::Combat || IsActionActive() || !bPhasePending || Phase!=1 || Health->CurrentHealth<=0) return;
 LockMovement(); Health->SetEncounterInvulnerable(true); SetState(EBossState::PhaseTransition); PlayUtility(GetDefinition()->PhaseCast);
 if (auto* FX=UGameplayStatics::SpawnEmitterAttached(GetDefinition()->PhaseEffect,Boss->GetMesh(),TEXT("spine_03"))) Effects.Add(FX);
}
void UBossActionComponent::LockMovement()
{
 if (!Boss) return;
 auto* M=Boss->GetCharacterMovement();
 if (!bMovementSaved)
 { bSavedOrient=M->bOrientRotationToMovement; bSavedControllerYaw=Boss->bUseControllerRotationYaw; bSavedRVO=M->bUseRVOAvoidance; bMovementSaved=true; }
 if (auto* AI=Cast<AAIController>(Boss->GetController())) { AI->StopMovement(); AI->ClearFocus(EAIFocusPriority::Gameplay); }
 M->StopMovementImmediately(); M->bOrientRotationToMovement=false; Boss->bUseControllerRotationYaw=false; M->SetAvoidanceEnabled(false);
}
void UBossActionComponent::RestoreMovement()
{
 if (!Boss || !bMovementSaved || State==EBossState::Dead) return;
 auto* M=Boss->GetCharacterMovement(); M->bOrientRotationToMovement=bSavedOrient; M->SetAvoidanceEnabled(bSavedRVO);
 Boss->bUseControllerRotationYaw=bSavedControllerYaw; bMovementSaved=false;
}
void UBossActionComponent::RemoveOwnRootMotion()
{
 if (Boss && MoveSourceId)
 { Boss->GetCharacterMovement()->RemoveRootMotionSourceByID(MoveSourceId); Boss->GetCharacterMovement()->StopMovementImmediately(); }
 MoveSourceId=0;
}
void UBossActionComponent::ClearEffects()
{
 if (TelegraphActor) { TelegraphActor->Destroy(); TelegraphActor=nullptr; }
 for (UParticleSystemComponent* FX:Effects) if (IsValid(FX)) { FX->EndTrails(); FX->DeactivateSystem(); FX->DestroyComponent(); }
 Effects.Reset();
}
void UBossActionComponent::CancelAction()
{
 ++ActionSerial; bWindowOpen=false; WindowHits.Reset(); Step=EActionStep::None; CurrentAction=EBossAction::None;
 MontageInstanceId=INDEX_NONE;
 UAnimMontage* Old=ActiveMontage; ActiveMontage=nullptr;
 if (Boss && Boss->GetMesh()->GetAnimInstance() && Old) Boss->GetMesh()->GetAnimInstance()->Montage_Stop(.1f,Old);
 RemoveOwnRootMotion(); ClearEffects(); RestoreMovement();
 if (auto* Pool=GetWorld()?GetWorld()->GetSubsystem<UProjectilePoolSubsystem>():nullptr) Pool->ReleaseForSource(GetOwner());
 if (State==EBossState::Action) SetState(EBossState::Combat);
}
bool UBossActionComponent::IsActionLegal(EBossAction Id,float Distance,bool bLOS) const
{
 const auto* A=GetDefinition()->FindAction(Id);
 const float Reach = (Id==EBossAction::Combo || Id==EBossAction::DelayedSlash) ? GetDefinition()->MeleeReachScale : 1.f;
 if (!A || !bLOS || !CanMove() || bPhasePending || Distance<A->MinRange || Distance>A->MaxRange*Reach) return false;
 if ((Phase==2?A->PhaseTwoWeight:A->PhaseOneWeight)<=0) return false;
 if (const double* Until=CooldownUntil.Find(Id); Until && Now()<*Until) return false;
 return true;
}
EBossAction UBossActionComponent::SelectAction(float Distance,bool bLOS)
{
 TArray<const FBossActionDefinition*> Legal; float Total=0;
 for (const auto& A:GetDefinition()->Actions)
 {
  if (!IsActionLegal(A.Id,Distance,bLOS) || (A.Id!=EBossAction::Combo && A.Id==LastSpecial)) continue;
  Legal.Add(&A); Total+=Phase==2?A.PhaseTwoWeight:A.PhaseOneWeight;
 }
 if (Total<=0) return EBossAction::None;
 float Choice=Random.FRandRange(0,Total);
 for (const auto* A:Legal) { Choice-=Phase==2?A->PhaseTwoWeight:A->PhaseOneWeight; if (Choice<=0) return A->Id; }
 return Legal.Last()->Id;
}
bool UBossActionComponent::TryStartAction(EBossAction Id)
{
 if (!Boss || !Target.IsValid() || !IsActionLegal(Id,FVector::Dist2D(Boss->GetActorLocation(),Target->GetActorLocation()),HasLineOfSightTo(Target.Get()))) return false;
 if (!Boss->GetMesh()->GetAnimInstance()) return false;
 const auto* A=GetDefinition()->FindAction(Id);
 ++ActionSerial; CurrentAction=Id; StageIndex=0; Step=EActionStep::Telegraph;
 CooldownUntil.Add(Id,Now()+A->Cooldown); LastSpecial=Id==EBossAction::Combo?EBossAction::None:Id;
 LockMovement(); SetState(EBossState::Action); StepEnd=Now()+A->Telegraph;
 ShowTelegraph();
 // Migrated actions contain the actual anticipation in their montage, not an idle timer.
 if (GetDefinition()->AnimationRevision>=2) StartStage();
 return IsActionActive();
}
float UBossActionComponent::GetFacingDelta() const
{
 return Boss && Target.IsValid() ? FMath::FindDeltaAngleDegrees(Boss->GetActorRotation().Yaw,
     (Target->GetActorLocation()-Boss->GetActorLocation()).Rotation().Yaw) : 0.f;
}
void UBossActionComponent::FaceTarget(float Delta,float Speed)
{
 if (!Boss || !Target.IsValid()) return;
 const float Yaw=(Target->GetActorLocation()-Boss->GetActorLocation()).Rotation().Yaw;
 Boss->SetActorRotation(FRotator(0,FMath::FixedTurn(Boss->GetActorRotation().Yaw,Yaw,Speed*Delta),0));
}
void UBossActionComponent::ShowTelegraph()
{
 ClearEffects();
 if (!Boss) return;
 const bool bCircle=CurrentAction==EBossAction::Siphon || CurrentAction==EBossAction::BloodFeast;
 const bool bLong=CurrentAction==EBossAction::ShadowRush || CurrentAction==EBossAction::BloodWave;
 const float Size=bCircle?(CurrentAction==EBossAction::BloodFeast?330.f:260.f):(bLong?500.f:200.f);
 FActorSpawnParameters P; P.Owner=Boss; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 TelegraphActor=GetWorld()->SpawnActor<ABossTelegraph>(Boss->GetActorLocation(),Boss->GetActorRotation(),P);
 if (TelegraphActor) TelegraphActor->Configure(bCircle,Size,bLong?70.f:80.f,(bCircle || CurrentAction==EBossAction::BloodWave)?FLinearColor(1,.02f,.035f):FLinearColor::White,GetDefinition()->WarningMaterial);
 UpdateTelegraphTransform();
}
void UBossActionComponent::UpdateTelegraphTransform()
{
 if (!Boss || !TelegraphActor) return;
 const bool bCircle=CurrentAction==EBossAction::Siphon || CurrentAction==EBossAction::BloodFeast;
 const bool bLong=CurrentAction==EBossAction::ShadowRush || CurrentAction==EBossAction::BloodWave;
 FVector Location=Boss->GetActorLocation()-FVector(0,0,Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-3);
 if (!bCircle) Location+=Boss->GetActorForwardVector()*(bLong?250.f:100.f);
 FRotator Rotation=Boss->GetActorRotation(); FHitResult Floor;
 FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(BossTelegraphGround),false,Boss);
 if (Target.IsValid())
 {
  GroundQuery.AddIgnoredActor(Target.Get()); TArray<AActor*> Attached;
  Target->GetAttachedActors(Attached,true,true); GroundQuery.AddIgnoredActors(Attached);
 }
 if (GetWorld()->LineTraceSingleByChannel(Floor,Location+FVector(0,0,180),Location-FVector(0,0,300),ECC_Visibility,GroundQuery))
 { Location=Floor.ImpactPoint+Floor.ImpactNormal*3.f; Rotation=FRotationMatrix::MakeFromZX(Floor.ImpactNormal,Boss->GetActorForwardVector()).Rotator(); }
 TelegraphActor->SetActorLocationAndRotation(Location,Rotation);
}
void UBossActionComponent::StartStage()
{
 const auto* A=GetDefinition()->FindAction(CurrentAction);
 if (!A || !A->Stages.IsValidIndex(StageIndex)) { CancelAction(); return; }
 const auto& S=A->Stages[StageIndex]; auto* Anim=Boss->GetMesh()->GetAnimInstance();
 if (!Anim) { CancelAction(); return; }
 if (GetDefinition()->AnimationRevision<2 && TelegraphActor && CurrentAction!=EBossAction::BloodFeast && CurrentAction!=EBossAction::Siphon)
 { TelegraphActor->Destroy(); TelegraphActor=nullptr; }
 ActiveMontage=S.Montage?S.Montage.Get():UAnimMontage::CreateSlotAnimationAsDynamicMontage(S.Sequence,TEXT("DefaultSlot"),.08f,.12f,1,1);
 if (!ActiveMontage || Anim->Montage_Play(ActiveMontage,1.f)<=0) { CancelAction(); return; }
 if (FAnimMontageInstance* Instance=Anim->GetActiveInstanceForMontage(ActiveMontage)) MontageInstanceId=Instance->GetInstanceID();
 FOnMontageEnded End; End.BindUObject(this,&ThisClass::OnMontageEnded,ActionSerial); Anim->Montage_SetEndDelegate(End,ActiveMontage);
 Step=EActionStep::Playing; StageStarted=Now(); StepEnd=Now()+S.Length()+.5;
 PreviousClipTime=-KINDA_SMALL_NUMBER; bOneShotFired=false; bMoveStarted=false; bFacingCommitted=false; bWindowOpen=false; bTrailsStarted=false; WindowHits.Reset();
 LockedDirection=Boss->GetActorForwardVector();
 ReadBladePoints(PreviousBlade);
}
void UBossActionComponent::OnMontageEnded(UAnimMontage* Montage,bool bInterrupted,int64 Serial)
{
 if (Serial!=ActionSerial || Montage!=ActiveMontage || !IsActionActive()) return;
 if (bInterrupted) { CancelAction(); return; }
 // Tick consumes any crossed final event before advancing. Never start the next clip inside this delegate.
}
void UBossActionComponent::TickAction(float Delta)
{
 if (Step==EActionStep::Telegraph)
 {
  FaceTarget(Delta,120.f);
  // The warning follows the committed caster transform, never snaps the player/camera.
  UpdateTelegraphTransform();
  if (Now()>=StepEnd) StartStage();
  return;
 }
 if (Step==EActionStep::Recovery)
 {
  if (Now()>=StepEnd)
  { CurrentAction=EBossAction::None; ActiveMontage=nullptr; Step=EActionStep::None; RestoreMovement(); SetState(EBossState::Combat); }
  return;
 }
 const auto* A=GetDefinition()->FindAction(CurrentAction);
 if (!A || !A->Stages.IsValidIndex(StageIndex)) { CancelAction(); return; }
 const auto& S=A->Stages[StageIndex];
 auto* Anim=Boss->GetMesh()->GetAnimInstance();
 const bool bPlaying=Anim && Anim->Montage_IsActive(ActiveMontage);
 const float Elapsed=static_cast<float>(Now()-StageStarted);
 // IsActive becomes false at blend-out start, not at the last source frame. Do not shorten clips/gaps by BlendOutTime.
 const float Time=FMath::Max(PreviousClipTime,bPlaying?Anim->Montage_GetPosition(ActiveMontage):FMath::Min(S.Length(),Elapsed));
 if (!bFacingCommitted && Time<S.FacingCommitTime) FaceTarget(Delta,120.f);
 else if (!bFacingCommitted) { LockedDirection=Boss->GetActorForwardVector(); bFacingCommitted=true; }
 if (TelegraphActor)
 {
  UpdateTelegraphTransform();
  float HitStart=0,HitEnd=0; S.ReadHitWindow(HitStart,HitEnd);
  if (Time>=HitStart && S.Shape!=EBossHitShape::Radial) { TelegraphActor->Destroy(); TelegraphActor=nullptr; }
 }
 const int64 Serial=ActionSerial;
 if (!bMoveStarted && S.MoveStart>=0 && Time>=S.MoveStart)
 { bMoveStarted=true; StartRush(); if (Serial!=ActionSerial || Step!=EActionStep::Playing) return; }
 const auto Source=MoveSourceId?Boss->GetCharacterMovement()->GetRootMotionSourceByID(MoveSourceId):TSharedPtr<FRootMotionSource>();
 if (MoveSourceId && (!Source || Source->GetTime()>=Source->Duration-KINDA_SMALL_NUMBER || Time>S.MoveEnd+.1f || Boss->GetCharacterMovement()->IsFalling()))
 {
  RemoveOwnRootMotion();
  if (auto* FX=UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),GetDefinition()->RushArriveEffect,Boss->GetActorLocation()-FVector(0,0,Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()))) Effects.Add(FX);
 }
 SampleDamage(PreviousClipTime,Time); if (Serial!=ActionSerial) return;
 PreviousClipTime=Time;
 if (Time>=S.Length()-KINDA_SMALL_NUMBER || Now()>=StepEnd) FinishStage();
}
void UBossActionComponent::FinishStage()
{
 const auto* A=GetDefinition()->FindAction(CurrentAction); if (!A) { CancelAction(); return; }
 bWindowOpen=false; WindowHits.Reset(); RemoveOwnRootMotion(); ClearEffects();
 const bool bCombo=CurrentAction==EBossAction::Combo && StageIndex+1<(Phase==2?3:2);
 const bool bRush=CurrentAction==EBossAction::ShadowRush && StageIndex==0 && Phase==2 && Target.IsValid() &&
     FVector::DistSquared2D(Boss->GetActorLocation(),Target->GetActorLocation())<=FMath::Square(240.f);
 if ((bCombo || bRush) && A->Stages.IsValidIndex(StageIndex+1))
 {
  ++StageIndex; ShowTelegraph();
  if (GetDefinition()->AnimationRevision>=2) StartStage();
  else { Step=EActionStep::Telegraph; StepEnd=Now()+GetDefinition()->ComboGap; }
 }
 else StartRecovery();
}
void UBossActionComponent::StartRecovery()
{
 const auto* A=GetDefinition()->FindAction(CurrentAction); if (!A) { CancelAction(); return; }
 Step=EActionStep::Recovery; bWindowOpen=false; WindowHits.Reset();
 float Duration=FMath::Max(Phase==2?.45f:.55f,A->Recovery);
 const auto& S=A->Stages[StageIndex];
 if (S.RecoveryMontage && Boss->GetMesh()->GetAnimInstance())
 {
  ActiveMontage=S.RecoveryMontage;
  Boss->GetMesh()->GetAnimInstance()->Montage_Play(ActiveMontage,1.f);
  Duration=FMath::Max(Duration,ActiveMontage->GetPlayLength());
 }
 StepEnd=Now()+Duration;
}
bool UBossActionComponent::IsCurrentNotify(const UAnimSequenceBase* Animation,int32 Id) const
{
 return State==EBossState::Action && Step==EActionStep::Playing && ActiveMontage && Animation==ActiveMontage && Id!=INDEX_NONE && Id==MontageInstanceId;
}
void UBossActionComponent::NotifyWindow(bool bOpen,const UAnimSequenceBase* Animation,int32 Id)
{
 if (IsCurrentNotify(Animation,Id)) bWindowOpen=bOpen;
 // Hit timing remains driven by montage position, so skipped/low-weight notifies cannot create missing damage or stale windows.
}
void UBossActionComponent::SampleDamage(float OldTime,float NewTime)
{
 const auto* A=GetDefinition()->FindAction(CurrentAction); if (!A || !A->Stages.IsValidIndex(StageIndex)) return;
 const auto& S=A->Stages[StageIndex];
 float Start=0,End=0;
 if (!S.ReadHitWindow(Start,End) || NewTime<Start || OldTime>=End) { bWindowOpen=false; ReadBladePoints(PreviousBlade); return; }
 bWindowOpen=true;
 if (S.Shape==EBossHitShape::Blades)
 {
  if (!bTrailsStarted)
  {
   bTrailsStarted=true;
   if (TPCCombatVFX::IsEnabled() && CurrentAction==EBossAction::ShadowRush)
    if (auto* FX=UGameplayStatics::SpawnEmitterAttached(GetDefinition()->RushSlashEffect,Boss->GetMesh(),TEXT("weapon_r"),
        FVector::ZeroVector,FRotator::ZeroRotator,FVector(GetDefinition()->SkillAccentScale))) Effects.Add(FX);
   for (const TCHAR* Side:{TEXT("L"),TEXT("R")})
    if (auto* FX=UGameplayStatics::SpawnEmitterAttached(GetDefinition()->TrailEffect,Boss->GetMesh()))
    {
     FX->BeginTrails(FName(*FString::Printf(TEXT("BladeBase_%s"),Side)),FName(*FString::Printf(TEXT("BladeTip_%s"),Side)),ETrailWidthMode_FromFirst,1.f);
     Effects.Add(FX);
    }
  }
  SweepBlades();
 }
 else if (!bOneShotFired) { bOneShotFired=true; if (S.Shape==EBossHitShape::Radial) DamageArea(); else ReleaseWave(); }
 if (NewTime>=End)
 { bWindowOpen=false; for (UParticleSystemComponent* FX:Effects) if (IsValid(FX)) FX->EndTrails(); }
}
bool UBossActionComponent::ReadBladePoints(FVector* Out) const
{
 if (!Boss || !Boss->GetMesh()) return false;
 static const FName Names[]={TEXT("BladeBase_L"),TEXT("BladeTip_L"),TEXT("BladeBase_R"),TEXT("BladeTip_R")};
 for (int32 I=0;I<4;++I)
 {
  if (!Boss->GetMesh()->DoesSocketExist(Names[I])) return false;
  Out[I]=TPCMeleeTrace::Extend(Boss->GetMesh()->GetSocketLocation(Names[I]),Boss->GetActorLocation(),GetDefinition()->MeleeReachScale);
 }
 return true;
}
void UBossActionComponent::SweepBlades()
{
 FVector Points[4]; if (!ReadBladePoints(Points)) return;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BossBlade),false,Boss);
 const int64 Serial=ActionSerial;
 for (int32 Blade=0;Blade<2;++Blade)
  for (int32 Sample=0;Sample<7;++Sample)
  {
   float Alpha=Sample/6.f;
   FVector Start=FMath::Lerp(PreviousBlade[Blade*2],PreviousBlade[Blade*2+1],Alpha);
   FVector End=FMath::Lerp(Points[Blade*2],Points[Blade*2+1],Alpha);
   TArray<FHitResult> Hits;
   GetWorld()->SweepMultiByObjectType(Hits,Start,End,FQuat::Identity,FCollisionObjectQueryParams(ECC_Pawn),FCollisionShape::MakeSphere(GetDefinition()->BladeTraceRadius*GetDefinition()->MeleeReachScale),Q);
   for (const auto& Hit:Hits) { ApplyHit(Hit.GetActor(),Hit.ImpactPoint); if (Serial!=ActionSerial) return; }
  }
 for (int32 I=0;I<4;++I) PreviousBlade[I]=Points[I];
}
void UBossActionComponent::DamageArea()
{
 if (TelegraphActor) { TelegraphActor->Destroy(); TelegraphActor=nullptr; }
 const auto& S=GetDefinition()->FindAction(CurrentAction)->Stages[StageIndex];
 // Reached once per authored window, even when a frame crosses the entire window.
 if (TPCCombatVFX::IsEnabled()) if (auto* FX=UGameplayStatics::SpawnEmitterAttached(
     CurrentAction==EBossAction::Siphon?GetDefinition()->SiphonCastEffect:GetDefinition()->FeastSlashEffect,
     Boss->GetMesh(),TEXT("weapon_r"),FVector::ZeroVector,FRotator::ZeroRotator,FVector(GetDefinition()->SkillAccentScale))) Effects.Add(FX);
 TArray<FOverlapResult> Hits; FCollisionQueryParams Q(SCENE_QUERY_STAT(BossArea),false,Boss);
 GetWorld()->OverlapMultiByObjectType(Hits,Boss->GetActorLocation(),FQuat::Identity,FCollisionObjectQueryParams(ECC_Pawn),
     FCollisionShape::MakeBox(FVector(S.Radius,S.Radius,GetDefinition()->AreaVerticalTolerance)),Q);
 if (auto* FX=UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),CurrentAction==EBossAction::Siphon?GetDefinition()->SiphonEffect:GetDefinition()->FeastEffect,
     Boss->GetActorLocation()-FVector(0,0,Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()))) Effects.Add(FX);
 const int64 Serial=ActionSerial;
 for (const auto& Hit:Hits)
 {
  AActor* Other=Hit.GetActor();
  if (Other && FMath::Abs(Other->GetActorLocation().Z-Boss->GetActorLocation().Z)<=GetDefinition()->AreaVerticalTolerance &&
      FVector::DistSquared2D(Other->GetActorLocation(),Boss->GetActorLocation())<=FMath::Square(S.Radius)) ApplyHit(Other,Other->GetActorLocation());
  if (Serial!=ActionSerial) return;
 }
}
void UBossActionComponent::ApplyHit(AActor* Victim,const FVector& Point)
{
 if (!IsValid(Victim) || Victim==Boss || Victim->IsA<AEnemyCharacter>() || WindowHits.Contains(Victim) || !HasLineOfSightTo(Victim)) return;
 auto* H=Victim->FindComponentByClass<UHealthComponent>(); if (!H) return;
 const auto& S=GetDefinition()->FindAction(CurrentAction)->Stages[StageIndex];
 WindowHits.Add(Victim); // Both blades and duplicate overlap components share the same stage/window ID.
 FCombatHitSpec Spec; Spec.Damage=S.Damage; Spec.bCanBeBlocked=S.bCanBeBlocked; Spec.bCanBeParried=S.bCanBeParried;
 Spec.ImpactPoint=Point; Spec.ActionSerial=ActionSerial; Spec.WindowId=FName(*FString::Printf(TEXT("Stage_%d"),StageIndex));
 const bool bSiphon=CurrentAction==EBossAction::Siphon; const int64 Serial=ActionSerial;
 const FCombatHitResult Result=H->ApplyCombatHit(Spec,Boss);
 UE_LOG(LogTemp,Verbose,TEXT("Countess Hit Action=%d Serial=%lld Window=%d Outcome=%d ActualDamage=%.2f Phase=%d Poise=%.1f"),static_cast<int32>(CurrentAction),Serial,StageIndex,static_cast<int32>(Result.Outcome),Result.ActualDamage,Phase,Poise);
 if (Serial!=ActionSerial || State==EBossState::Dead) return; // A parry cancels this executor reentrantly.
 if (Result.bBlocked && !Result.bGuardBroken)
 {
  // A held guard pushes the attacking shoulders back, without cancelling the combo.
  LastHitReaction=Now(); HitReactionDirection=1; HitReactionStrength=.13f; HitReactionDuration=.18f;
 }
 if (Result.ActualDamage>0)
 {
  if (!Result.bBlocked)
  {
   UParticleSystem* Impact=!TPCCombatVFX::IsEnabled()?GetDefinition()->ImpactEffect:
       CurrentAction==EBossAction::Siphon?GetDefinition()->SiphonHitEffect:
       CurrentAction==EBossAction::ShadowRush?GetDefinition()->RushEffect:GetDefinition()->ImpactEffect;
   if (auto* FX=UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),Impact,Point,FRotator::ZeroRotator,
       FVector(CurrentAction==EBossAction::Siphon || CurrentAction==EBossAction::ShadowRush?GetDefinition()->SkillAccentScale:1.f))) Effects.Add(FX);
  }
  if (bSiphon) Health->Heal(FMath::Min(20.f,Result.ActualDamage*.5f));
 }
}
void UBossActionComponent::ReleaseWave()
{
 const auto& S=GetDefinition()->FindAction(CurrentAction)->Stages[StageIndex];
 FCombatHitSpec Spec; Spec.Damage=S.Damage; Spec.bCanBeParried=false; Spec.ActionSerial=ActionSerial; Spec.WindowId=TEXT("BloodWave");
 FVector Spawn=Boss->GetActorLocation()+LockedDirection*(Boss->GetCapsuleComponent()->GetScaledCapsuleRadius()+32);
 if (auto* Pool=GetWorld()->GetSubsystem<UProjectilePoolSubsystem>()) Pool->AcquireWithHitSpec(GetDefinition()->WaveClass,FTransform(LockedDirection.Rotation(),Spawn),Boss,Boss,Spec,900.f);
 if (auto* FX=UGameplayStatics::SpawnEmitterAttached(GetDefinition()->WaveCastEffect,Boss->GetMesh(),TEXT("weapon_r"))) Effects.Add(FX);
}
bool UBossActionComponent::ValidateRush(FVector& Destination) const
{
 if (!Target.IsValid() || !Boss->GetCharacterMovement()->IsMovingOnGround()) return false;
 const float Distance=FMath::Clamp(FVector::Dist2D(Boss->GetActorLocation(),Target->GetActorLocation())-GetDefinition()->RushStopDistance,0.f,GetDefinition()->RushMaxDistance);
 if (Distance<30) return false;
 auto* Nav=UNavigationSystemV1::GetCurrent(GetWorld()); if (!Nav) return false;
 const FVector Start=Boss->GetActorLocation(); FVector End=Start+LockedDirection*Distance;
 FNavLocation Projected; if (!Nav->ProjectPointToNavigation(End,Projected,FVector(70,70,150))) return false;
 End=Projected.Location+FVector(0,0,Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
 if (ArenaBoundary && !ArenaBoundary->ContainsLocation(End,-Boss->GetCapsuleComponent()->GetScaledCapsuleRadius())) return false;
 if (FMath::Abs(End.Z-Start.Z)>45) return false;
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BossRush),false,Boss);
 FHitResult Hit;
 const auto Capsule=FCollisionShape::MakeCapsule(Boss->GetCapsuleComponent()->GetScaledCapsuleRadius(),Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-2);
 if (GetWorld()->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Pawn,Capsule,Q))
 { End=FMath::Lerp(Start,End,FMath::Max(0.f,Hit.Time-.04f)); }
 if (FVector::DistSquared2D(Start,End)<900) return false;
 // Ground support is checked along the complete segment, not merely at its endpoint.
 for (int32 I=0;I<=10;++I)
 {
  FVector P=FMath::Lerp(Start,End,I/10.f);
  if (!GetWorld()->LineTraceSingleByChannel(Hit,P,P-FVector(0,0,Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+55),ECC_Visibility,Q) || Hit.ImpactNormal.Z<.65f) return false;
 }
 Destination=End; return true;
}
void UBossActionComponent::StartRush()
{
 FVector Destination;
 if (!ValidateRush(Destination))
 {
  // Invalid ground/path spends the accepted cooldown and retains a punishable recovery.
  ++ActionSerial; MontageInstanceId=INDEX_NONE; bWindowOpen=false; Step=EActionStep::Recovery; StepEnd=Now()+.6;
  if (auto* Anim=Boss->GetMesh()->GetAnimInstance()) Anim->Montage_Stop(.12f,ActiveMontage);
  ClearEffects(); return;
 }
 const auto& S=GetDefinition()->FindAction(CurrentAction)->Stages[StageIndex];
 TSharedPtr<FRootMotionSource_MoveToForce> Move=MakeShared<FRootMotionSource_MoveToForce>();
 Move->InstanceName=FName(*FString::Printf(TEXT("CountessRush_%lld"),ActionSerial)); Move->Priority=700;
 Move->AccumulateMode=ERootMotionAccumulateMode::Override; Move->StartLocation=Boss->GetActorLocation(); Move->TargetLocation=Destination;
 // Recast projects to a slightly raised polygon surface. It must not become a jump impulse.
 Move->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
 const float ClipTime=Boss->GetMesh()->GetAnimInstance()->Montage_GetPosition(ActiveMontage);
 Move->Duration=FMath::Max(.05f,S.MoveEnd-ClipTime); Move->bRestrictSpeedToExpected=true;
 Move->FinishVelocityParams.Mode=ERootMotionFinishVelocityMode::SetVelocity; Move->FinishVelocityParams.SetVelocity=FVector::ZeroVector;
 MoveSourceId=Boss->GetCharacterMovement()->ApplyRootMotionSource(Move);
 if (auto* FX=UGameplayStatics::SpawnEmitterAtLocation(GetWorld(),GetDefinition()->RushBeginEffect,Boss->GetActorLocation()-FVector(0,0,Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()))) Effects.Add(FX);
}
void UBossActionComponent::PlayUtility(UAnimSequence* Sequence,float Rate)
{
 if (!Sequence || !Boss || !Boss->GetMesh()->GetAnimInstance()) return;
 ActiveMontage=Boss->GetMesh()->GetAnimInstance()->PlaySlotAnimationAsDynamicMontage(Sequence,TEXT("DefaultSlot"),.1f,.12f,Rate,1);
}
void UBossActionComponent::OnLanded()
{
 if (State==EBossState::Combat && !IsActionActive())
 { PlayUtility(GetDefinition()->Landing); RecoilUntil=Now()+GetDefinition()->Landing->GetPlayLength(); LockMovement(); }
}
void UBossActionComponent::MoveToGoal(const FVector& Goal,float Speed,bool bStrafe)
{
 if (Now()<NextMoveRequest) return; NextMoveRequest=Now()+.4;
 auto* AI=Cast<AAIController>(Boss->GetController()); if (!AI) return;
 const FVector SafeGoal=ArenaBoundary && State!=EBossState::Resetting ? ArenaBoundary->ClampToInterior(Goal) : Goal;
 if (auto* BB=AI->GetBlackboardComponent()) BB->SetValueAsVector(TEXT("MoveGoal"),SafeGoal);
 auto* M=Boss->GetCharacterMovement(); M->MaxWalkSpeed=Speed; M->bOrientRotationToMovement=!bStrafe;
 bStrafing=bStrafe;
 M->bUseControllerDesiredRotation=false;
 Boss->bUseControllerRotationYaw=false;
 // The action component owns strafe-facing. Controller focus must not compete with it.
 AI->ClearFocus(EAIFocusPriority::Gameplay);
 FAIMoveRequest Request; Request.SetGoalLocation(SafeGoal); Request.SetAcceptanceRadius(35.f); Request.SetUsePathfinding(true);
 Request.SetProjectGoalLocation(true); Request.SetAllowPartialPath(false); Request.SetCanStrafe(bStrafe);
 // Reset completion and navigation use capsule-center tolerances, also for scaled bosses.
 if (State==EBossState::Resetting) { Request.SetAcceptanceRadius(55.f); Request.SetReachTestIncludesAgentRadius(false); }
 if (AI->MoveTo(Request).Code==EPathFollowingRequestResult::Failed)
 { bReversingOrbit=!bReversingOrbit; if (bStandoff && ++StandoffPathFailures>=2) bStandoff=false; }
}
bool UBossActionComponent::TickStandoff(float Distance)
{
 if (!bStandoff) return false;
 if (!CanMove() || bPhasePending || Now()>=StandoffUntil || Distance<180 || Distance>600 || !bHasLOS)
 { bStandoff=false; NextMoveRequest=0; return false; }
 const FVector Away=(Boss->GetActorLocation()-Target->GetActorLocation()).GetSafeNormal2D();
 const FVector Side=FVector::CrossProduct(FVector::UpVector,Away)*(bReversingOrbit?-1.f:1.f);
 const float Radius=FMath::Clamp(Distance,260.f,420.f);
 MoveToGoal(Target->GetActorLocation()+Away*Radius+Side*145,GetDefinition()->OrbitSpeed,true);
 return bStandoff;
}
void UBossActionComponent::DriveDecision()
{
 if (!CanMove() || !Target.IsValid()) return;
 float Distance=FVector::Dist2D(Boss->GetActorLocation(),Target->GetActorLocation());
 if (TickStandoff(Distance)) return;
 if (const EBossAction Action=SelectAction(Distance,bHasLOS); Action!=EBossAction::None)
 {
  if (!bStandoffRolled)
  {
   bStandoffRolled=true;
   if (Distance>=180 && Distance<=600 && Now()>=NextStandoffAt && Random.FRand()<GetDefinition()->StandoffChance)
   {
    bStandoff=true; StandoffPathFailures=0; bReversingOrbit=Random.RandRange(0,1)!=0; NextMoveRequest=0;
    const FVector2D D=GetDefinition()->StandoffDuration;
    StandoffUntil=Now()+Random.FRandRange(FMath::Max(.1,D.X),FMath::Max(FMath::Max(.1,D.X),D.Y));
    NextStandoffAt=Now()+FMath::Max(0.f,GetDefinition()->StandoffCooldown);
    if (TickStandoff(Distance)) return;
   }
  }
  if (TryStartAction(Action)) return;
 }
 if (!bHasLOS || Distance>400)
 { MoveToGoal(LastKnownLocation,Phase==2?GetDefinition()->ChaseSpeedTwo:GetDefinition()->ChaseSpeed,false); return; }
 FVector Away=(Boss->GetActorLocation()-Target->GetActorLocation()).GetSafeNormal2D();
 FVector Side=FVector::CrossProduct(FVector::UpVector,Away)*(bReversingOrbit?-1.f:1.f);
 FVector Goal=Distance<180?Target->GetActorLocation()+Away*240:Target->GetActorLocation()+Away*220+Side*140;
 // Separation is a locomotion-only offset. It cannot steal a skill's root-motion source.
 TArray<FOverlapResult> Nearby; FCollisionQueryParams Q(SCENE_QUERY_STAT(BossSeparation),false,Boss);
 GetWorld()->OverlapMultiByObjectType(Nearby,Boss->GetActorLocation(),FQuat::Identity,FCollisionObjectQueryParams(ECC_Pawn),FCollisionShape::MakeSphere(220),Q);
 TSet<AActor*> Seen;
 for (const auto& R:Nearby) if (auto* E=Cast<AEnemyCharacter>(R.GetActor()); E && !Seen.Contains(E))
 { Seen.Add(E); Goal+=(Boss->GetActorLocation()-E->GetActorLocation()).GetSafeNormal2D()*100; }
 MoveToGoal(Goal,Distance<180?GetDefinition()->RetreatSpeed:GetDefinition()->OrbitSpeed,true);
}
void UBossActionComponent::RequestReset()
{
 if (!bInitialized || State==EBossState::Dead || State==EBossState::Resetting || State==EBossState::Dormant) return;
 CancelAction(); Target.Reset(); bHasLOS=false; bPhasePending=false; Health->SetEncounterInvulnerable(true);
 SetState(EBossState::Resetting); NextMoveRequest=0;
 if (StatusWidget) { StatusWidget->RemoveFromParent(); StatusWidget=nullptr; }
 Boss->SetLockOnIndicatorVisible(false);
}
void UBossActionComponent::TickReset()
{
 // Level placement records a capsule center which may still be above the floor.
 // Nav MoveTo projects to the floor; compare against that same reachable center,
 // not the unreachable spawn Z. Keep a vertical test so another floor is not "home".
 FVector ResetLocation=HomeLocation;
 if (auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
 {
  FNavLocation Ground;
  if (Nav->ProjectPointToNavigation(HomeLocation,Ground,FVector(100,100,500)))
   ResetLocation=Ground.Location+FVector(0,0,Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.f);
 }
 auto AtHome=[&]()
 {
  const FVector Position=Boss->GetActorLocation();
  return FVector::DistSquared2D(Position,ResetLocation)<=FMath::Square(80.f) &&
   FMath::Abs(Position.Z-ResetLocation.Z)<=FMath::Max(30.f,Boss->GetCharacterMovement()->MaxStepHeight);
 };
 if (AtHome()) { CompleteReset(); return; }
 MoveToGoal(ResetLocation,GetDefinition()->ChaseSpeed,false);
 if (GetStateElapsed()<8.f) return;
 APlayerController* PC=UGameplayStatics::GetPlayerController(this,0);
 // Never visibly teleport: require both actor and home outside the player's actual view cone/LOS.
 if (PC)
 {
  FVector View; FRotator Rot; PC->GetPlayerViewPoint(View,Rot);
  auto Visible=[&](const FVector& P)
  {
   if (FVector::DotProduct(Rot.Vector(),(P-View).GetSafeNormal())<.15f) return false;
   FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(BossResetView),false,PC->GetPawn()); Q.AddIgnoredActor(Boss);
   return !GetWorld()->LineTraceSingleByChannel(Hit,View,P,ECC_Visibility,Q);
  };
  if (Visible(Boss->GetActorLocation()) || Visible(ResetLocation)) return;
 }
 FCollisionQueryParams Q(SCENE_QUERY_STAT(BossResetHome),false,Boss);
 if (GetWorld()->OverlapBlockingTestByChannel(ResetLocation,FQuat::Identity,ECC_Pawn,
     FCollisionShape::MakeCapsule(Boss->GetCapsuleComponent()->GetScaledCapsuleRadius(),Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-2),Q)) return;
 FHitResult Floor;
 if (!GetWorld()->LineTraceSingleByChannel(Floor,ResetLocation,ResetLocation-FVector(0,0,150),ECC_Visibility,Q) || Floor.ImpactNormal.Z<.65f) return;
 if (Boss->TeleportTo(ResetLocation,HomeRotation,false,false) && AtHome()) CompleteReset();
}
void UBossActionComponent::CompleteReset()
{
 if (State!=EBossState::Resetting) return;
 if (auto* AI=Cast<AAIController>(Boss->GetController())) AI->StopMovement();
 Phase=1; Poise=GetMaxPoise(); bPhasePending=false; CooldownUntil.Reset(); LastSpecial=EBossAction::None;
 OutsideSince=-1; PoiseImmuneUntil=0; RecoilUntil=0; LastPoiseHit=-100;
 bStandoff=false; bStandoffRolled=false; NextStandoffAt=0; StandoffUntil=0;
 Health->SetCurrentHealth(Health->MaxHealth); Health->SetEncounterInvulnerable(false); RestoreMovement();
 Boss->SetActorRotation(HomeRotation); SetState(EBossState::Dormant);
}
void UBossActionComponent::Die()
{
 if (State==EBossState::Dead) return;
 CancelAction(); LockMovement(); Target.Reset(); bPhasePending=false; Health->SetEncounterInvulnerable(false);
 SetState(EBossState::Dead); // Parent death handler stops AI, collision, rewards once; native graph holds Death's last frame.
}
