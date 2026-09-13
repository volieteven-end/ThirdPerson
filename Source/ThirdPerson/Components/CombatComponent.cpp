// Fill out your copyright notice in the Description page of Project Settings.


#include "CombatComponent.h"
#include "../Audio/TPCCharacterAudioComponent.h"
#include "ActionComponent.h"
#include "../Components/HealthComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "EquipmentComponent.h"
#include "../Weapons/WeaponActor.h"
#include "../Weapons/WeaponDefinition.h"
#include "../Weapons/WeaponProjectile.h"
#include "../Weapons/ProjectilePoolSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StaminaComponent.h"
#include "../AI/EnemyCharacter.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "../Animation/SwordBladeSampling.h"
#include "../Animation/MeleeTraceGeometry.h"
#include "../Character/TPCCharacter.h"

namespace
{
	bool IsEnemyFriendlyFire(const AActor* Source, const AActor* Target)
	{
		return Source && Target &&
			Source->IsA<AEnemyCharacter>() && Target->IsA<AEnemyCharacter>();
	}
}
// Sets default values for this component's properties
void UCombatComponent::RestoreRespawnAttributes(const UCombatComponent& Source)
{
	Damage = Source.Damage;
	AttackRange = Source.AttackRange;
	AttackRadius = Source.AttackRadius;
	AttackCooldown = Source.AttackCooldown;
	LevelDamageBonus = Source.LevelDamageBonus;
	DamageMultiplier = Source.DamageMultiplier;
	AttackCooldownMultiplier = Source.AttackCooldownMultiplier;
	MeleeReachMultiplier = Source.MeleeReachMultiplier;
}

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetComponentTickEnabled(false);
}

void UCombatComponent::TryAttack()
{
	UWorld* World = GetWorld();
	if (!bCombatEnabled || !GetOwner() || !World || bRangedAttackInProgress)
	{
		return;
	}
	if (bMeleeAttackInProgress)
	{
		if (CanChainAttack() && (bAllowEarlyComboBuffer || bComboInputWindowOpen))
		{
			GetComboBuffer().Queue();
			TryCommitBufferedCombo(); // Late input after ChainPoint can still use recovery.
		}
		return;
	}
	if (World->GetTimeSeconds() - LastAttackTime < GetEffectiveAttackCooldown())
	{
		return;
	}
	CancelBlock();
	const UWeaponDefinition* WeaponDefinition = GetEquippedWeaponDefinition();
	if (WeaponDefinition && WeaponDefinition->WeaponType == EWeaponType::Ranged)
	{
		TryRangedAttackAt(nullptr);
		return;
	}
	ClearDashCombo(); // A fresh attack begins a new dash-continuation budget.
	NextAttackIndex = 0;
	if (!StartMeleeMontageAttack() && !GetActionSet() && GetEffectiveAttackMontages().IsEmpty())
	{
		// Retain the original no-animation unarmed fallback, not broken configured attacks.
		LastAttackTime = World->GetTimeSeconds();
		PerformAttackHit();
	}
}


void UCombatComponent::TryUppercutAttack()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!bCombatEnabled || !Character || !GetWorld() ||
		Character->GetCharacterMovement()->IsFalling() || bMeleeAttackInProgress ||
		bRangedAttackInProgress || GetWorld()->GetTimeSeconds() - LastAttackTime < GetEffectiveAttackCooldown())
	{
		return;
	}
	CancelBlock();
	if (const UActionSet* Set = GetActionSet())
	{
		StartDefinedAttack(Set->Rising, EActiveCombatAttackType::Uppercut);
	}
	else if (StartSpecialMontageAttack(UppercutMontage, UppercutDamageMultiplier, EActiveCombatAttackType::Uppercut))
	{
		Character->LaunchCharacter(FVector(0.f, 0.f, UppercutLaunchVelocity), false, true);
	}
}


void UCombatComponent::TryAirAttack()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!bCombatEnabled || !Character || !GetWorld() || !Character->GetCharacterMovement()->IsFalling())
	{
		return;
	}
	if (bMeleeAttackInProgress)
	{
		if (ActiveAttackType == EActiveCombatAttackType::Air) { TryAttack(); }
		return;
	}
	if (bRangedAttackInProgress || GetWorld()->GetTimeSeconds() - LastAttackTime < GetEffectiveAttackCooldown())
	{
		return;
	}
	CancelBlock();
	// AirLight preserves XY momentum and gravity. No forced Z velocity here.
	StartAirMontageAttack();
}

void UCombatComponent::TryAirDiveAttack()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (const UActionSet* Set = GetActionSet()) { AirDiveMontage = Set->Dive ? Set->Dive->Montage : nullptr; }
	if (!bCombatEnabled || !Character || !GetWorld() || !Character->GetCharacterMovement()->IsFalling() ||
		(bMeleeAttackInProgress && (!GetActions() || !GetActions()->CanRequest(ETPCActionIntent::Dive))) || bRangedAttackInProgress || !AirDiveMontage ||
		(!GetActionSet() && GetWorld()->GetTimeSeconds() - LastAttackTime < GetEffectiveAttackCooldown())) { return; }
	if (AirDiveMontage->GetSectionIndex(AirDiveStartSection) == INDEX_NONE ||
		AirDiveMontage->GetSectionIndex(AirDiveLoopSection) == INDEX_NONE ||
		AirDiveMontage->GetSectionIndex(AirDiveLandSection) == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("Air Dive requires Start / Loop / Land sections: %s"), *GetNameSafe(AirDiveMontage));
		return;
	}
    const auto* Set = GetActionSet();
    const auto* Stamina = Character->FindComponentByClass<UStaminaComponent>();
    if (Set && Set->Dive && Set->Dive->StaminaCost > 0.f && (!Stamina || Stamina->GetCurrentStamina() < Set->Dive->StaminaCost)) return;
    if (bMeleeAttackInProgress) CancelActiveAttack(.08f);
	CancelBlock();
	if (StartSpecialMontageAttack(AirDiveMontage, AirDiveDamageMultiplier, EActiveCombatAttackType::AirDive))
	{
		UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance();
		Anim->Montage_JumpToSection(AirDiveStartSection, AirDiveMontage);
		Anim->Montage_SetNextSection(AirDiveStartSection, AirDiveLoopSection, AirDiveMontage);
		Anim->Montage_SetNextSection(AirDiveLoopSection, AirDiveLoopSection, AirDiveMontage);
		Anim->Montage_SetNextSection(TEXT("Descent"), TEXT("ContactWait"), AirDiveMontage);
		Anim->Montage_SetNextSection(TEXT("ContactWait"), TEXT("ContactWait"), AirDiveMontage);
		Anim->Montage_SetNextSection(AirDiveLandSection, NAME_None, AirDiveMontage);
		if (!GetActiveDefinition()) { CommitSpecialMovement(); }
	}
}

void UCombatComponent::HandleOwnerLanded()
{
	NextAirAttackIndex = 0;
	if (!bMeleeAttackInProgress) { return; }
	if (ActiveAttackType == EActiveCombatAttackType::AirDive && !bAirDiveLanded)
	{
		bAirDiveLanded = true;
		EndAttackWindow();
		StopAttackEffects();
		GetComboBuffer().Reset(GetComboGeneration());
		ACharacter* Character = Cast<ACharacter>(GetOwner());
		UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		if (Anim) { UpdateDiveApproach(); }
		else { CancelActiveAttack(); }
	}
	else if (ActiveAttackType == EActiveCombatAttackType::Air)
	{
		CancelActiveAttack(); // Do not continue an airborne combo while standing on the floor.
	}
}


void UCombatComponent::StartBlock()
{
	if (UActionComponent* Actions = GetActions())
	{
		if (!Actions->CanRequest(ETPCActionIntent::Guard)) return;
		if (bMeleeAttackInProgress) CancelActiveAttack(0.08f);
	}
	if (!bCombatEnabled || bMeleeAttackInProgress || !GetWorld())
	{
		return;
	}
	if (bIsBlocking) { bBlockInputHeld = true; return; } // Held/repeated Started never refreshes the parry window.

	ClearDashCombo();
	bBlockInputHeld = true;
	bIsBlocking = true;
	bParryWindowActive = true;
	if (UActionComponent* Actions = GetActions()) GuardActionId = Actions->BeginAction(ETPCActionState::Guard);
	GetWorld()->GetTimerManager().SetTimer(
		ParryWindowTimerHandle,
		this,
		&ThisClass::EndParryWindow,
		PerfectParryWindow,
		false);
}

void UCombatComponent::StopBlock()
{
	bBlockInputHeld = false;
	if (!bParryWindowActive)
	{
		bIsBlocking = false;
		if (UActionComponent* Actions = GetActions()) Actions->EndAction(GuardActionId);
	}
}

void UCombatComponent::EndParryWindow()
{
	bParryWindowActive = false;
	if (!bBlockInputHeld)
	{
		bIsBlocking = false;
		if (UActionComponent* Actions = GetActions()) Actions->EndAction(GuardActionId);
	}
}

void UCombatComponent::CancelBlock()
{
	if (UActionComponent* Actions = GetActions()) Actions->EndAction(GuardActionId);
	GuardActionId = 0;
	bBlockInputHeld = false;
	bParryWindowActive = false;
	bIsBlocking = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ParryWindowTimerHandle);
	}
}

void UCombatComponent::SetCombatEnabled(bool bEnabled)
{
	bCombatEnabled = bEnabled;
	if (!bCombatEnabled)
	{
		ParryCounterExpiresAt = -1.f;
        ClearSwordBuff();
		NextAirAttackIndex = 0;
		if (UActionComponent* Actions = GetActions()) Actions->ClearInputBuffers();
		CancelBlock();
		CancelActiveAttack();
	}
}


float UCombatComponent::ModifyIncomingDamage(float IncomingDamage, const AActor* DamageSource)
{
 FCombatHitSpec Spec; Spec.Damage = IncomingDamage;
 FCombatHitResult Result;
 return ResolveIncomingHit(Spec, DamageSource, Result);
}
float UCombatComponent::ResolveIncomingHit(const FCombatHitSpec& Spec, const AActor* DamageSource, FCombatHitResult& Result)
{
	const float IncomingDamage = Spec.Damage;
 AActor* OwnerActor = GetOwner();
	if (!Spec.bCanBeBlocked || !bIsBlocking || !OwnerActor || !DamageSource || IncomingDamage <= 0.f)
	{
		return IncomingDamage;
	}

	const FVector ToSource =
		(DamageSource->GetActorLocation() - OwnerActor->GetActorLocation())
		.GetSafeNormal2D();
	const float HalfArcRadians = FMath::DegreesToRadians(BlockArcDegrees * 0.5f);
	if (FVector::DotProduct(OwnerActor->GetActorForwardVector(), ToSource) <
		FMath::Cos(HalfArcRadians))
	{
		return IncomingDamage;
	}

	if (Spec.bCanBeParried && bParryWindowActive)
	{
		ParryCounterExpiresAt = GetWorld() ? GetWorld()->GetTimeSeconds() + 1.f : -1.f;
		bParryWindowActive = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ParryWindowTimerHandle);
		}
		if (!bBlockInputHeld)
		{
			bIsBlocking = false;
			if (UActionComponent* Actions = GetActions()) Actions->EndAction(GuardActionId);
		}
		if (AEnemyCharacter* Enemy =
			const_cast<AEnemyCharacter*>(Cast<AEnemyCharacter>(DamageSource)))
		{
			Enemy->ApplyParryStagger(OwnerActor);
		}
		Result.bParried = true; Result.Outcome = ECombatHitOutcome::Parried;
		return 0.f;
	}

	UStaminaComponent* Stamina =
		OwnerActor->FindComponentByClass<UStaminaComponent>();
	if (Stamina && !Stamina->TryConsume(BlockStaminaCostPerHit))
	{
		Stamina->SetCurrentStamina(0.f);
		Result.bGuardBroken = true;
		CancelBlock();
		return IncomingDamage;
	}
	if (Stamina && Stamina->GetCurrentStamina() <= KINDA_SMALL_NUMBER)
	{
		Result.bGuardBroken = true;
		CancelBlock();
	}

	Result.bBlocked = true; Result.Outcome = ECombatHitOutcome::Blocked;
	return IncomingDamage * BlockDamageMultiplier;
}

FCombatHitSpec UCombatComponent::MakeCurrentHitSpec(const FVector& ImpactPoint) const
{
 FCombatHitSpec Spec;
 Spec.Damage = GetEffectiveDamage() * ActiveDamageMultiplier;
 Spec.ImpactPoint = ImpactPoint;
 Spec.WindowId = ActiveHitGroup;
 Spec.ActionSerial = GetActions() ? GetActions()->GetActionInstanceId() : AttackGeneration;
 if (const UActionDefinition* Def = GetActiveDefinition())
 {
     Spec.PoiseDamage = Def->PoiseDamage;
     return Spec;
 }
 switch (ActiveAttackType)
 {
 case EActiveCombatAttackType::Uppercut: Spec.PoiseDamage = 30.f; break;
 case EActiveCombatAttackType::Air: Spec.PoiseDamage = 12.f; break;
 case EActiveCombatAttackType::AirDive: Spec.PoiseDamage = 25.f; break;
 default: Spec.PoiseDamage = 10.f; break;
 }
 return Spec;
}

bool UCombatComponent::StartMeleeMontageAttack()
{
	if (const UActionSet* Set = GetActionSet())
	{
		if (!Set->GroundCombo.IsValidIndex(NextAttackIndex) ||
			!StartDefinedAttack(Set->GroundCombo[NextAttackIndex], EActiveCombatAttackType::Normal)) return false;
		++NextAttackIndex;
		return true;
	}
	const auto& Montages = GetEffectiveAttackMontages();
	if (!Montages.IsValidIndex(NextAttackIndex)) { return false; }
	if (!StartSpecialMontageAttack(Montages[NextAttackIndex], 1.f, EActiveCombatAttackType::Normal)) { return false; }
	++NextAttackIndex; // The last stage ends the combo; it never wraps to stage 1.
	return true;
}

bool UCombatComponent::StartAirMontageAttack()
{
	if (const UActionSet* Set = GetActionSet())
	{
		if (!Set->AirCombo.IsValidIndex(NextAirAttackIndex) ||
			!StartDefinedAttack(Set->AirCombo[NextAirAttackIndex], EActiveCombatAttackType::Air)) return false;
		++NextAirAttackIndex;
		return true;
	}
	UAnimMontage* Montage = AirAttackMontages.IsEmpty()
		? (NextAirAttackIndex == 0 ? AirAttackMontage.Get() : nullptr)
		: (AirAttackMontages.IsValidIndex(NextAirAttackIndex) ? AirAttackMontages[NextAirAttackIndex].Get() : nullptr);
	if (!StartSpecialMontageAttack(Montage, AirAttackDamageMultiplier, EActiveCombatAttackType::Air)) { return false; }
	++NextAirAttackIndex;
	return true;
}


bool UCombatComponent::StartSpecialMontageAttack(UAnimMontage* Montage, float DamageScale, EActiveCombatAttackType AttackType, const UActionDefinition* AuthoredDefinition)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!bCombatEnabled || !Anim || !GetWorld() || !Montage) { return false; }
	const UActionSet* Set = GetActionSet();
	const UActionDefinition* Def = AuthoredDefinition ? AuthoredDefinition : Set ? Set->FindByMontage(Montage) : nullptr;
	UStaminaComponent* Stamina = Character->FindComponentByClass<UStaminaComponent>();
	if (Def && Def->StaminaCost > 0.f && (!Stamina || Stamina->GetCurrentStamina() < Def->StaminaCost)) return false;
    if (const UActionComponent* Actions = GetActions())
        if (!Actions->CanRequest(ETPCActionIntent::PrimaryAttack)) return false;
	UAnimMontage* PreviousMontage = ActiveAttackMontage.Get();
	// Advance the generation BEFORE playing; old montage callbacks may fire during replacement.
	++AttackGeneration;
	HitGroups.Reset();
    if (UActionComponent* Actions = GetActions())
    {
        OwnedActionId = Actions->BeginAction(Def ? Def->State : ETPCActionState::Attack, Def);
        if (!OwnedActionId) return false;
    }
    // Gameplay traces need posed sockets even when this combatant is outside the camera.
    if (!bPosePolicySaved)
    {
        PreviousPosePolicy = static_cast<uint8>(Character->GetMesh()->VisibilityBasedAnimTickOption);
        bPosePolicySaved = true;
    }
    Character->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    AddTickPrerequisiteComponent(Character->GetMesh());
	GetComboBuffer().Reset(GetComboGeneration());
	ActiveAttackMontage = Montage;
	ActiveAttackInstanceId = INDEX_NONE;
	bMeleeAttackInProgress = true;
	bComboInputWindowOpen = false;
	bAirDiveLanded = false;
	ActiveDamageMultiplier = FMath::Max(0.f, DamageScale);
	ActiveAttackType = AttackType;
	bSpecialMovementCommitted = false;
	bDiveApproachStarted = false;
	if (Character->PlayAnimMontage(Montage, Def ? Def->PlayRate : 1.f, Def ? Def->EntrySection : NAME_None) <= 0.f)
	{
		ResetMeleeAction();
		if (PreviousMontage) { Anim->Montage_Stop(0.1f, PreviousMontage); }
		return false;
	}
	if (FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage))
	{
		ActiveAttackInstanceId = Instance->GetInstanceID();
	}
	if (UActionComponent* Actions = GetActions()) Actions->BindMontage(OwnedActionId, Montage, ActiveAttackInstanceId);
	if (Def && Stamina && Def->StaminaCost > 0.f) Stamina->TryConsume(Def->StaminaCost);
	FOnMontageEnded End;
	End.BindUObject(this, &ThisClass::HandleAttackMontageEnded, AttackGeneration);
	Anim->Montage_SetEndDelegate(End, Montage);
	FOnMontageBlendingOutStarted Blend;
	Blend.BindUObject(this, &ThisClass::HandleAttackBlendingOut, AttackGeneration);
	Anim->Montage_SetBlendingOutDelegate(Blend, Montage);
	LastAttackTime = GetWorld()->GetTimeSeconds();
	if (!Def || Def->State == ETPCActionState::Attack) OnMeleeAttackStarted.Broadcast();
	return true;
}


void UCombatComponent::OpenComboInputWindow()
{
	if (bMeleeAttackInProgress)
	{
		bComboInputWindowOpen = true;
	}
}

void UCombatComponent::CloseComboInputWindow()
{
	bComboInputWindowOpen = false;
	if (bChainOnLegacyComboWindowEnd && bMeleeAttackInProgress)
	{
		GetComboBuffer().bChainPointReached = true;
		TryCommitBufferedCombo();
	}
}

bool UCombatComponent::IsCurrentAttackNotify(const UAnimSequenceBase* Animation, int32 MontageInstanceId) const
{
	if (!bCombatEnabled || !bMeleeAttackInProgress || !ActiveAttackMontage.IsValid()) { return false; }
	if (MontageInstanceId != INDEX_NONE) { return MontageInstanceId == ActiveAttackInstanceId; }
	// A context-less notify may only identify the active montage itself, not an arbitrary sequence.
	return Animation == ActiveAttackMontage.Get();
}

void UCombatComponent::ReachComboChainPoint(UAnimSequenceBase* Animation, int32 MontageInstanceId)
{
	if (!IsCurrentAttackNotify(Animation, MontageInstanceId)) { return; }
	GetComboBuffer().bChainPointReached = true;
	TryCommitBufferedCombo();
}

void UCombatComponent::FinishAuthoredDamageWindow(UAnimSequenceBase* Animation, int32 MontageInstanceId)
{
    if (!IsCurrentAttackNotify(Animation, MontageInstanceId)) return;
    EndAttackWindow();
    // The mannequin's original punches have damage notifies but no sword chain-point
    // notifies. Their real hit-window end is a safe, animation-authored combo boundary.
    if (IsUnarmedPlayer()) ReachComboChainPoint(Animation, MontageInstanceId);
}

bool UCombatComponent::CanChainAttack() const
{
	if (!bCombatEnabled || !bMeleeAttackInProgress) { return false; }
	if (const UActionSet* Set = GetActionSet())
	{
		const UActionDefinition* Def = GetActiveDefinition();
		const bool bAirFollowUp = ActiveAttackType == EActiveCombatAttackType::Air || ActiveAttackType == EActiveCombatAttackType::Uppercut;
        const auto& Moves = bAirFollowUp ? Set->AirCombo : Set->GroundCombo;
		const int32 Index = bAirFollowUp ? NextAirAttackIndex : NextAttackIndex;
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		return (ActiveAttackType == EActiveCombatAttackType::Normal || bAirFollowUp) &&
			(ActiveAttackType != EActiveCombatAttackType::Air || (Character && Character->GetCharacterMovement()->IsFalling())) &&
			Def && Moves.IsValidIndex(Index) && Moves[Index] && Def->AllowedNextActions.Contains(Moves[Index]->ActionId);
	}
	if (ActiveAttackType == EActiveCombatAttackType::Normal)
	{
		return GetEffectiveAttackMontages().IsValidIndex(NextAttackIndex);
	}
	if (ActiveAttackType == EActiveCombatAttackType::Air)
	{
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		return Character && Character->GetCharacterMovement()->IsFalling() &&
			AirAttackMontages.IsValidIndex(NextAirAttackIndex);
	}
	return false;
}


bool UCombatComponent::TryCommitBufferedCombo()
{
    const auto* Character = Cast<ACharacter>(GetOwner());
    if (ActiveAttackType == EActiveCombatAttackType::Uppercut && (!Character || !Character->GetCharacterMovement()->IsFalling())) return false;
	if (!GetComboBuffer().Consume(GetComboGeneration(), CanChainAttack())) { return false; }
	EndAttackWindow();
	StopAttackEffects();
	const EActiveCombatAttackType PreviousType = ActiveAttackType;
	const bool bStarted = (PreviousType == EActiveCombatAttackType::Air || PreviousType == EActiveCombatAttackType::Uppercut)
		? StartAirMontageAttack() : StartMeleeMontageAttack();
	if (!bStarted) { CancelActiveAttack(); }
	return bStarted;
}


void UCombatComponent::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint64 Generation)
{
	if (Generation != AttackGeneration || Montage != ActiveAttackMontage.Get()) { return; }
	ResetMeleeAction(); // End/interrupt drops the buffer. Only ChainPoint starts the next stage.
}

void UCombatComponent::HandleAttackBlendingOut(UAnimMontage* Montage, bool bInterrupted, uint64 Generation)
{
	if (bInterrupted && Generation == AttackGeneration && Montage == ActiveAttackMontage.Get())
	{
		ResetMeleeAction();
	}
}

void UCombatComponent::StopAttackEffects()
{
    if (const auto* Character = Cast<ACharacter>(GetOwner()))
    {
        TArray<USceneComponent*> Children;
        Character->GetMesh()->GetChildrenComponents(true, Children);
        for (auto* Child : Children)
            if (auto* Trail = Cast<UParticleSystemComponent>(Child)) Trail->EndTrails();
    }
	UEquipmentComponent* Equipment = GetOwner() ? GetOwner()->FindComponentByClass<UEquipmentComponent>() : nullptr;
	if (AWeaponActor* Weapon = Equipment ? Equipment->GetEquippedWeaponActor() : nullptr)
	{
		Weapon->SetAttackEffectActive(false);
	}
}

void UCombatComponent::ResetMeleeAction()
{
    if (bPosePolicySaved)
    {
        if (auto* Character = Cast<ACharacter>(GetOwner()))
            Character->GetMesh()->VisibilityBasedAnimTickOption = static_cast<EVisibilityBasedAnimTickOption>(PreviousPosePolicy);
        bPosePolicySaved = false;
    }
	if (UActionComponent* Actions = GetActions()) Actions->EndAction(OwnedActionId);
	OwnedActionId = 0;
	HitGroups.Reset();
	ClearDashCombo();
	++AttackGeneration;
	GetComboBuffer().Reset(GetComboGeneration());
	EndAttackWindow();
	StopAttackEffects();
	ActiveAttackMontage.Reset();
	ActiveAttackInstanceId = INDEX_NONE;
	bMeleeAttackInProgress = false;
	bComboInputWindowOpen = false;
	bAirDiveLanded = false;
	NextAttackIndex = 0;
	ActiveDamageMultiplier = 1.f;
	ActiveAttackType = EActiveCombatAttackType::Normal;
}

void UCombatComponent::ClearDashCombo()
{
    DashComboWindow.Reset();
    DashComboWeapon.Reset();
    DashComboWeaponActor.Reset();
    DashComboMontages.Reset();
    bDashUsedInCombo = false;
}

bool UCombatComponent::IsDashComboContextValid() const
{
    const UEquipmentComponent* Equipment = GetOwner() ? GetOwner()->FindComponentByClass<UEquipmentComponent>() : nullptr;
    if (DashComboWeapon.Get() != GetEquippedWeaponDefinition() ||
        DashComboWeaponActor.Get() != (Equipment ? Equipment->GetEquippedWeaponActor() : nullptr)) { return false; }
    const auto& Montages = GetEffectiveAttackMontages();
    if (Montages.Num() != DashComboMontages.Num()) { return false; }
    for (int32 Index = 0; Index < Montages.Num(); ++Index)
    {
        if (!Montages[Index] || Montages[Index] != DashComboMontages[Index].Get()) { return false; }
    }
    return true;
}

void UCombatComponent::CancelAttackForDash(float BlendOutTime)
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    const bool bPreserve = !bDashUsedInCombo && ActiveAttackType == EActiveCombatAttackType::Normal &&
        Character && Character->GetCharacterMovement()->IsMovingOnGround() && CanChainAttack();
    const int32 SavedNextIndex = NextAttackIndex;
    TArray<TWeakObjectPtr<UAnimMontage>> SavedMontages;
    if (bPreserve)
    {
        for (UAnimMontage* Montage : GetEffectiveAttackMontages()) { SavedMontages.Add(Montage); }
    }
    const UWeaponDefinition* SavedWeapon = GetEquippedWeaponDefinition();
    UEquipmentComponent* Equipment = GetOwner() ? GetOwner()->FindComponentByClass<UEquipmentComponent>() : nullptr;
    AWeaponActor* SavedWeaponActor = Equipment ? Equipment->GetEquippedWeaponActor() : nullptr;
    // Advance attack generation and close damage/effects BEFORE stopping its montage.
    CancelActiveAttack(BlendOutTime);
    if (bPreserve)
    {
        bDashUsedInCombo = true;
        DashComboWindow.Arm(SavedNextIndex);
        DashComboWeapon = SavedWeapon;
        DashComboWeaponActor = SavedWeaponActor;
        DashComboMontages = MoveTemp(SavedMontages);
    }
}

bool UCombatComponent::QueueAttackDuringDash()
{
    if (!bCombatEnabled || !GetWorld() || !IsDashComboContextValid()) { ClearDashCombo(); return false; }
    return DashComboWindow.Queue(GetWorld()->GetTimeSeconds());
}

bool UCombatComponent::FinishDashForCombo(bool bInterrupted, float ContinueWindow)
{
    if (bInterrupted || !bCombatEnabled || !GetWorld() || !IsDashComboContextValid())
    {
        ClearDashCombo();
        return false;
    }
    DashComboWindow.Finish(GetWorld()->GetTimeSeconds(), FMath::Max(0.f, ContinueWindow));
    return DashComboWindow.bQueued && DashComboWindow.IsOpen(GetWorld()->GetTimeSeconds());
}

bool UCombatComponent::TryResumeComboAfterDash()
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!bCombatEnabled || bMeleeAttackInProgress || bRangedAttackInProgress || bIsBlocking || !GetWorld() ||
        !Character || !Character->GetCharacterMovement()->IsMovingOnGround() || !IsDashComboContextValid())
    {
        ClearDashCombo();
        return false;
    }
    const int32 ResumeIndex = DashComboWindow.Consume(GetWorld()->GetTimeSeconds());
    if (ResumeIndex == INDEX_NONE) { ClearDashCombo(); return false; }
    NextAttackIndex = ResumeIndex;
    DashComboWeapon.Reset();
    DashComboWeaponActor.Reset();
    DashComboMontages.Reset();
    // This is a continuation, not a new cooldown-gated first attack. Keep the used-dash budget.
    const bool bStarted = StartMeleeMontageAttack();
    if (!bStarted) { ClearDashCombo(); }
    return bStarted;
}

void UCombatComponent::CancelActiveAttack(float BlendOutTime)
{
	UAnimMontage* Melee = ActiveAttackMontage.Get();
	UAnimMontage* Ranged = ActiveRangedMontage.Get();
	ResetMeleeAction();
	// Preserve the per-flight air-attack budget until Landed, even when interrupted.
	bRangedAttackInProgress = false;
	bRangedProjectileReleased = false;
	PendingRangedTarget.Reset();
	PendingRangedWeapon.Reset();
	ActiveRangedMontage.Reset();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (Anim)
	{
		if (Melee) { Anim->Montage_Stop(FMath::Max(0.f, BlendOutTime), Melee); }
		if (Ranged) { Anim->Montage_Stop(FMath::Max(0.f, BlendOutTime), Ranged); }
	}
}


void UCombatComponent::PerformAttackHit()
{
	if (!bCombatEnabled) { return; }
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();

	if (!OwnerActor || !World)
	{
		return;
	}

	const FVector Forward =
		OwnerActor->GetActorForwardVector();

	const FVector Start =
		OwnerActor->GetActorLocation() + Forward * 50.f;

	const float EffectiveRange = GetEffectiveAttackRange();
	const float EffectiveRadius = GetEffectiveAttackRadius();
	const FVector End = Start + Forward * EffectiveRange;

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(MeleeAttack),
		false,
		OwnerActor);

	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(
		Hits,
		Start,
		End,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(EffectiveRadius),
		QueryParams);

	bool bHit = false;
	for (const FHitResult& Hit : Hits)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor || IsEnemyFriendlyFire(OwnerActor, HitActor))
		{
			continue;
		}
		if (UHealthComponent* Health =
			HitActor->FindComponentByClass<UHealthComponent>())
		{
            const FCombatHitResult Result = Health->ApplyCombatHit(MakeCurrentHitSpec(Hit.ImpactPoint), OwnerActor);
            if (Result.ActualDamage > 0.f) SpawnMeleeHitEffect(Hit);
            if (Result.ActualDamage > 0.f && !Result.bBlocked && !Result.bParried && !Result.bKilled) ApplySpecialHitReaction(HitActor);
			bHit = true;
			break;
		}
	}

	const FColor DebugColor =
		bHit ? FColor::Green : FColor::Red;

	DrawDebugLine(
		World, Start, End, DebugColor,
		false, 1.f, 0, 1.5f);

	DrawDebugSphere(
		World, End, EffectiveRadius, 16,
		DebugColor, false, 1.f);

}
void UCombatComponent::StartAttackWindow(
	FName InAttackBoneName,
	float InTraceRadius, FName HitGroup)
{
	if (!bCombatEnabled || !bMeleeAttackInProgress) { return; }
    if (ActiveAttackType == EActiveCombatAttackType::AirDive)
    {
        // Physical contact alone never deals damage. A grounded early-start window must
        // not hit once before the authored landing window hits the same target again.
        if (HitGroup == TEXT("Landing") ? !bAirDiveLanded : bAirDiveLanded) return;
    }
	ACharacter* OwnerCharacter =Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter ||!OwnerCharacter->GetMesh())
	{
		return;
	}
	USkeletalMeshComponent* Mesh =OwnerCharacter->GetMesh();
	UEquipmentComponent* Equipment =
		OwnerCharacter->FindComponentByClass<UEquipmentComponent>();
	const UWeaponDefinition* WeaponDefinition = GetEquippedWeaponDefinition();
	AWeaponActor* WeaponActor =
		WeaponDefinition && Equipment ? Equipment->GetEquippedWeaponActor() : nullptr;
	USkeletalMeshComponent* WeaponTraceMesh =
		WeaponActor ? WeaponActor->GetSkeletalWeaponMesh() : nullptr;

	bUseWeaponBladeTrace =
		WeaponDefinition &&
		WeaponDefinition->WeaponType == EWeaponType::Melee &&
		WeaponTraceMesh &&
		WeaponTraceMesh->DoesSocketExist(WeaponDefinition->BladeBaseSocketName) &&
		WeaponTraceMesh->DoesSocketExist(WeaponDefinition->BladeTipSocketName);

	if (bUseWeaponBladeTrace)
	{
		ActiveWeaponTraceMesh = WeaponTraceMesh;
		ActiveBladeBaseSocketName = WeaponDefinition->BladeBaseSocketName;
		ActiveBladeTipSocketName = WeaponDefinition->BladeTipSocketName;
		PreviousBladeBaseLocation =
			WeaponTraceMesh->GetSocketLocation(ActiveBladeBaseSocketName);
		PreviousBladeTipLocation =
			WeaponTraceMesh->GetSocketLocation(ActiveBladeTipSocketName);
        PreviousTraceMeshTransform = Mesh->GetComponentTransform();
        PreviousTraceMontagePosition = Mesh->GetAnimInstance()->Montage_GetPosition(ActiveAttackMontage.Get());
	}

	if (!bUseWeaponBladeTrace &&
		Mesh->GetBoneIndex(InAttackBoneName) == INDEX_NONE)
	{
		UE_LOG(LogTemp,Warning,TEXT("Attack bone not found: %s"),*InAttackBoneName.ToString());
		return;
	}

	ActiveAttackBoneName = InAttackBoneName;
	ActiveTraceRadius = InTraceRadius * GetMeleeTraceScale();
	ActiveHitGroup = HitGroup;
	HitActors = HitGroups.FindOrAdd(HitGroup);
	if (!bUseWeaponBladeTrace)
	{
		PreviousAttackLocation =Mesh->GetBoneLocation(ActiveAttackBoneName);
	}
	bAttackWindowActive = true;
	if (WeaponDefinition && WeaponDefinition->WeaponType == EWeaponType::Melee)
	{
		if (auto* Audio = OwnerCharacter->FindComponentByClass<UTPCCharacterAudioComponent>())
		{
			const auto* Definition = GetActiveDefinition();
			const bool bHeavy = ActiveAttackType == EActiveCombatAttackType::Uppercut ||
				ActiveAttackType == EActiveCombatAttackType::AirDive || ActiveAttackType == EActiveCombatAttackType::Special ||
				(Definition && Definition->DamageMultiplier >= 1.4f);
			const float Rate = Mesh->GetAnimInstance() ? Mesh->GetAnimInstance()->Montage_GetPlayRate(ActiveAttackMontage.Get()) : 1.f;
			Audio->PlaySwordSwing(AttackGeneration, HitGroup, bHeavy, Rate);
		}
	}
	if (UActionComponent* Actions = GetActions()) Actions->SetDamageWindowActive(true);
	SetComponentTickEnabled(true);
}
void UCombatComponent::EndAttackWindow()
{
	bAttackWindowActive = false;
	if (UActionComponent* Actions = GetActions()) Actions->SetDamageWindowActive(false);
	HitActors.Reset();
	SetComponentTickEnabled(false);
	ActiveAttackBoneName = NAME_None;
	bUseWeaponBladeTrace = false;
	ActiveBladeBaseSocketName = NAME_None;
	ActiveBladeTipSocketName = NAME_None;
	ActiveWeaponTraceMesh.Reset();
}
void UCombatComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime,TickType,ThisTickFunction);

	if (!bAttackWindowActive)
	{
		return;
	}

	ACharacter* OwnerCharacter =Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!OwnerCharacter ||!OwnerCharacter->GetMesh() ||!World)
	{
		EndAttackWindow();
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HandAttack),false,OwnerCharacter);
    const uint64 TraceGeneration = AttackGeneration;
	auto ApplyHits = [this, OwnerCharacter, TraceGeneration](const TArray<FHitResult>& Hits)
	{
		for (const FHitResult& Hit : Hits)
		{
            // Damage delegates can interrupt, kill or replace this action synchronously.
            if (!bCombatEnabled || !bAttackWindowActive || AttackGeneration != TraceGeneration) break;
			AActor* HitActor = Hit.GetActor();
			if (!HitActor || HitActors.Contains(HitActor) ||
				IsEnemyFriendlyFire(OwnerCharacter, HitActor))
			{
				continue;
			}

			HitActors.Add(HitActor);
			HitGroups.FindOrAdd(ActiveHitGroup).Add(HitActor);
			if (UHealthComponent* Health =
				HitActor->FindComponentByClass<UHealthComponent>())
			{
				const FCombatHitResult Result = Health->ApplyCombatHit(MakeCurrentHitSpec(Hit.ImpactPoint), OwnerCharacter);
				if (Result.ActualDamage > 0.f) SpawnMeleeHitEffect(Hit);
				if (Result.ActualDamage > 0.f && !Result.bBlocked && !Result.bParried && !Result.bKilled) ApplySpecialHitReaction(HitActor);
			}
		}
	};

	const UWeaponDefinition* ImpactWeapon = GetEquippedWeaponDefinition();
	if (ActiveAttackType == EActiveCombatAttackType::AirDive && bAirDiveLanded && ActiveHitGroup == TEXT("Landing") &&
		ImpactWeapon && ImpactWeapon->DiveLandingImpactRadius > 0.f)
	{
		// Opt-in weapons can use the physical landing impact instead of the recovery
		// pose's sideways blade. The actual Landed + authored damage window gate this;
		// the existing per-action hit group and health pipeline still own all results.
		const FVector Origin = OwnerCharacter->GetCharacterMovement()->GetActorFeetLocation() + FVector(0,0,60);
		TArray<FHitResult> ImpactHits;
		World->SweepMultiByObjectType(ImpactHits, Origin, Origin + FVector(0,0,.01f), FQuat::Identity,
			FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(FMath::Clamp(ImpactWeapon->DiveLandingImpactRadius,0.f,200.f)), QueryParams);
		ImpactHits.RemoveAll([&](const FHitResult& Hit)
		{
			if (!Hit.GetActor()) return true;
			FCollisionQueryParams Occlusion = QueryParams; Occlusion.AddIgnoredActor(Hit.GetActor());
			FHitResult Wall;
			return World->LineTraceSingleByChannel(Wall,Origin,Hit.GetActor()->GetActorLocation(),ECC_Visibility,Occlusion);
		});
		ApplyHits(ImpactHits);
		return;
	}

	if (bUseWeaponBladeTrace)
	{
		USkeletalMeshComponent* WeaponTraceMesh = ActiveWeaponTraceMesh.Get();
		if (!WeaponTraceMesh)
		{
			EndAttackWindow();
			return;
		}

		const FVector CurrentBladeBaseLocation =
			WeaponTraceMesh->GetSocketLocation(ActiveBladeBaseSocketName);
		const FVector CurrentBladeTipLocation =
			WeaponTraceMesh->GetSocketLocation(ActiveBladeTipSocketName);

        auto SweepBlade = [&](const FVector& BeforeBase, const FVector& BeforeTip, const FVector& AfterBase, const FVector& AfterTip)
        {
            const FVector Origin = OwnerCharacter->GetActorLocation();
            const float Reach = GetMeleeTraceScale();
            const FVector TraceBeforeBase = TPCMeleeTrace::Extend(BeforeBase,Origin,Reach);
            const FVector TraceBeforeTip = TPCMeleeTrace::Extend(BeforeTip,Origin,Reach);
            const FVector TraceAfterBase = TPCMeleeTrace::Extend(AfterBase,Origin,Reach);
            const FVector TraceAfterTip = TPCMeleeTrace::Extend(AfterTip,Origin,Reach);
            // Overlapping spheres cover the blade's full length, not just its tip.
            const float Length = FMath::Max(FVector::Distance(BeforeBase, BeforeTip), FVector::Distance(AfterBase, AfterTip));
            const int32 Samples = FMath::Clamp(FMath::CeilToInt(Length / FMath::Max(1.f, ActiveTraceRadius * 1.5f)) + 1, 5, 32);
            for (int32 I = 0; I < Samples; ++I)
            {
                const float Alpha = static_cast<float>(I) / (Samples - 1);
                TArray<FHitResult> Hits;
                World->SweepMultiByChannel(Hits, FMath::Lerp(TraceBeforeBase, TraceBeforeTip, Alpha),
                    FMath::Lerp(TraceAfterBase, TraceAfterTip, Alpha), FQuat::Identity, ECC_Pawn,
                    FCollisionShape::MakeSphere(ActiveTraceRadius), QueryParams);
                ApplyHits(Hits);
                if (!bAttackWindowActive) break; // A parry can interrupt the attacker inside ApplyCombatHit.
            }
        };
        const UWeaponDefinition* Weapon = GetEquippedWeaponDefinition();
        auto* CharacterMesh = OwnerCharacter->GetMesh();
        const float Position = CharacterMesh->GetAnimInstance()->Montage_GetPosition(ActiveAttackMontage.Get());
        const float Span = Position - PreviousTraceMontagePosition;
        const FTransform MeshWorld = CharacterMesh->GetComponentTransform();
        FTransform CurrentAuthoredPose;
        // A rapidly rotating blade follows an arc, NOT the straight chord between rendered poses.
        // Sample real compressed bone tracks at <= 1/120 s, bounded to 16 substeps for hitches.
        const bool bSampleArc = GetActionSet() && Weapon && Span > 1.f / 120.f + .0001f && Span < .15f &&
            TPCBladeSampling::SampleEquipmentPose(ActiveAttackMontage.Get(), Position, CharacterMesh, Weapon->EquipSocketName, CurrentAuthoredPose);
        const int32 Steps = bSampleArc ? FMath::Clamp(FMath::CeilToInt(Span * 120.f), 1, 16) : 1;
        const FTransform ActualEquip = Weapon ? CharacterMesh->GetSocketTransform(Weapon->EquipSocketName, RTS_Component) : FTransform::Identity;
        const FTransform EquipWorld = ActualEquip * MeshWorld;
        const FTransform BladeBaseOffset = WeaponTraceMesh->GetSocketTransform(ActiveBladeBaseSocketName).GetRelativeTransform(EquipWorld);
        const FTransform BladeTipOffset = WeaponTraceMesh->GetSocketTransform(ActiveBladeTipSocketName).GetRelativeTransform(EquipWorld);
        FVector BeforeBase = PreviousBladeBaseLocation, BeforeTip = PreviousBladeTipLocation;
        for (int32 Step = 1; Step <= Steps && bAttackWindowActive; ++Step)
        {
            FVector NextBase = CurrentBladeBaseLocation, NextTip = CurrentBladeTipLocation;
            if (Step < Steps)
            {
                const float Alpha = static_cast<float>(Step) / Steps;
                FTransform AuthoredPose;
                if (TPCBladeSampling::SampleEquipmentPose(ActiveAttackMontage.Get(), FMath::Lerp(PreviousTraceMontagePosition, Position, Alpha),
                    CharacterMesh, Weapon->EquipSocketName, AuthoredPose))
                {
                    FTransform InterpolatedWorld; InterpolatedWorld.Blend(PreviousTraceMeshTransform, MeshWorld, Alpha);
                    // Anchor at the evaluated current pose so retargeting / montage blend offsets are retained.
                    const FTransform EquipAtTime = AuthoredPose.GetRelativeTransform(CurrentAuthoredPose) * ActualEquip * InterpolatedWorld;
                    NextBase = (BladeBaseOffset * EquipAtTime).GetTranslation();
                    NextTip = (BladeTipOffset * EquipAtTime).GetTranslation();
                }
            }
            SweepBlade(BeforeBase, BeforeTip, NextBase, NextTip);
            BeforeBase = NextBase; BeforeTip = NextTip;
        }
        PreviousTraceMontagePosition = Position;
        PreviousTraceMeshTransform = MeshWorld;

		if (bDrawHandTraceDebug)
		{
			DrawDebugLine(World, CurrentBladeBaseLocation, CurrentBladeTipLocation,
				FColor::Cyan, false, 0.f, 0, 2.f);
			DrawDebugSphere(World, CurrentBladeTipLocation, ActiveTraceRadius,
				12, FColor::Cyan, false, 0.f, 0, 1.f);
		}

		PreviousBladeBaseLocation = CurrentBladeBaseLocation;
		PreviousBladeTipLocation = CurrentBladeTipLocation;
		return;
	}

	const FVector CurrentAttackLocation =
		OwnerCharacter->GetMesh()->GetBoneLocation(ActiveAttackBoneName);
	TArray<FHitResult> Hits;
	World->SweepMultiByChannel(
		Hits, TPCMeleeTrace::Extend(PreviousAttackLocation,OwnerCharacter->GetActorLocation(),GetMeleeTraceScale()),
        TPCMeleeTrace::Extend(CurrentAttackLocation,OwnerCharacter->GetActorLocation(),GetMeleeTraceScale()), FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(ActiveTraceRadius), QueryParams);
	ApplyHits(Hits);
	if (bDrawHandTraceDebug)
	{
		DrawDebugLine(World, PreviousAttackLocation, CurrentAttackLocation,
			FColor::Cyan, false, 0.f, 0, 2.f);
		DrawDebugSphere(World, CurrentAttackLocation, ActiveTraceRadius,
			12, FColor::Cyan, false, 0.f, 0, 1.f);
	}
	PreviousAttackLocation = CurrentAttackLocation;
}

void UCombatComponent::SpawnMeleeHitEffect(const FHitResult& Hit) const
{
	const UWeaponDefinition* WeaponDefinition = GetEquippedWeaponDefinition();
	UParticleSystem* HitEffect = WeaponDefinition && WeaponDefinition->MeleeHitEffect
		? WeaponDefinition->MeleeHitEffect.Get()
		: DefaultMeleeHitEffect.Get();
	UWorld* World = GetWorld();
	if (!World || !HitEffect)
	{
		return;
	}

	const FVector ImpactLocation = Hit.ImpactPoint.IsNearlyZero()
		? Hit.Location
		: Hit.ImpactPoint;
	const FRotator ImpactRotation = Hit.ImpactNormal.IsNearlyZero()
		? FRotator::ZeroRotator
		: Hit.ImpactNormal.Rotation();
	UGameplayStatics::SpawnEmitterAtLocation(
		World,
		HitEffect,
		ImpactLocation,
		ImpactRotation,
		true);
}

void UCombatComponent::ApplySpecialHitReaction(AActor* HitActor) const
{
	const UActionDefinition* Def = GetActiveDefinition();
	if ((Def && Def->HitReactionProfile == ETPCHitReactionProfile::Launch) ||
		(!Def && ActiveAttackType == EActiveCombatAttackType::Uppercut))
	{
		if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(HitActor))
		{
			Enemy->ApplyUppercutHit(GetOwner());
		}
	}
}

const UWeaponDefinition* UCombatComponent::GetEquippedWeaponDefinition() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	const UEquipmentComponent* Equipment =
		OwnerActor->FindComponentByClass<UEquipmentComponent>();
	return Equipment && Equipment->IsWeaponDrawn() ? Equipment->GetEquippedWeaponDefinition() : nullptr;
}

bool UCombatComponent::IsUnarmedPlayer() const
{
    return GetOwner() && GetOwner()->IsA<ATPCCharacter>() && !GetEquippedWeaponDefinition();
}

float UCombatComponent::GetEffectiveDamage() const
{
	const UWeaponDefinition* Weapon = GetEquippedWeaponDefinition();
	const float BaseDamage = Weapon ? Weapon->Damage : Damage;
	return (BaseDamage + LevelDamageBonus) * DamageMultiplier * GetSwordBuffMultiplier();
}

float UCombatComponent::GetEffectiveAttackCooldown() const
{
	const UWeaponDefinition* Weapon = GetEquippedWeaponDefinition();
	const float BaseCooldown = Weapon ? Weapon->AttackCooldown : AttackCooldown;
	return FMath::Max(0.1f, BaseCooldown * AttackCooldownMultiplier);
}

float UCombatComponent::GetEffectiveAttackRange() const
{
	const UWeaponDefinition* Weapon = GetEquippedWeaponDefinition();
	const float BaseRange = Weapon && Weapon->WeaponType == EWeaponType::Melee
		? Weapon->MeleeRange
		: AttackRange;
	return BaseRange * GetMeleeTraceScale();
}

float UCombatComponent::GetEffectiveAttackRadius() const
{
	const UWeaponDefinition* Weapon = GetEquippedWeaponDefinition();
	const float BaseRadius = Weapon && Weapon->WeaponType == EWeaponType::Melee
		? Weapon->MeleeRadius
		: AttackRadius;
	return BaseRadius * GetMeleeTraceScale();
}

void UCombatComponent::AddDamageBonus(float Amount)
{
	LevelDamageBonus = FMath::Max(0.f, LevelDamageBonus + Amount);
}

void UCombatComponent::MultiplyDamage(float Multiplier)
{
	if (Multiplier > 0.f)
	{
		DamageMultiplier *= Multiplier;
	}
}

void UCombatComponent::MultiplyAttackCooldown(float Multiplier)
{
	if (Multiplier > 0.f)
	{
		AttackCooldownMultiplier = FMath::Max(
			0.5f, AttackCooldownMultiplier * Multiplier);
	}
}

void UCombatComponent::MultiplyMeleeReach(float Multiplier)
{
	if (Multiplier > 0.f)
	{
		MeleeReachMultiplier *= Multiplier;
	}
}

const TArray<TObjectPtr<UAnimMontage>>& UCombatComponent::GetEffectiveAttackMontages() const
{
	if (const UActionSet* Set = GetActionSet())
	{
		ResolvedGroundMontages.Reset();
		for (const UActionDefinition* Def : Set->GroundCombo) ResolvedGroundMontages.Add(Def ? Def->Montage : nullptr);
		return ResolvedGroundMontages;
	}
	const UWeaponDefinition* Weapon = GetEquippedWeaponDefinition();
	return Weapon && !Weapon->AttackMontages.IsEmpty()
		? Weapon->AttackMontages
		: AttackMontages;
}

bool UCombatComponent::TryRangedAttackAt(AActor* TargetActor)
{
	UWorld* World = GetWorld();
	const UWeaponDefinition* WeaponDefinition = GetEquippedWeaponDefinition();
	if (!bCombatEnabled || !World || bRangedAttackInProgress ||
		!WeaponDefinition || WeaponDefinition->WeaponType != EWeaponType::Ranged ||
		World->GetTimeSeconds() - LastAttackTime < GetEffectiveAttackCooldown())
	{
		return false;
	}

	CancelBlock();
	LastAttackTime = World->GetTimeSeconds();
	return BeginRangedAttack(*WeaponDefinition, TargetActor);
}

bool UCombatComponent::BeginRangedAttack(
	const UWeaponDefinition& WeaponDefinition,
	AActor* TargetActor)
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	bRangedAttackInProgress = true;
	bRangedProjectileReleased = false;
	PendingRangedTarget = TargetActor;
	PendingRangedWeapon = const_cast<UWeaponDefinition*>(&WeaponDefinition);

	UAnimMontage* RangedMontage = WeaponDefinition.AttackMontages.IsEmpty()
		? nullptr
		: WeaponDefinition.AttackMontages[0].Get();
	if (OwnerCharacter && OwnerCharacter->GetMesh() && RangedMontage &&
		OwnerCharacter->PlayAnimMontage(RangedMontage) > 0.f)
	{
		ActiveRangedMontage = RangedMontage;
		if (UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance())
		{
			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(this, &ThisClass::HandleRangedMontageEnded);
			AnimInstance->Montage_SetEndDelegate(EndDelegate, RangedMontage);
		}
		return true;
	}

	// An unanimated ranged weapon remains functional during early prototyping.
	ReleaseRangedProjectile();
	bRangedAttackInProgress = false;
	PendingRangedTarget.Reset();
	PendingRangedWeapon.Reset();
	return true;
}

void UCombatComponent::ReleaseRangedProjectile()
{
	if (!bRangedAttackInProgress || bRangedProjectileReleased ||
		!PendingRangedWeapon.IsValid())
	{
		return;
	}

	bRangedProjectileReleased = true;
	PerformRangedAttack(*PendingRangedWeapon.Get(), PendingRangedTarget.Get());
}

void UCombatComponent::HandleRangedMontageEnded(
	UAnimMontage* Montage,
	bool bInterrupted)
{
	if (Montage != ActiveRangedMontage.Get())
	{
		return;
	}

	// Missing release notifies should not make a configured enemy silently fail.
	if (!bInterrupted && !bRangedProjectileReleased)
	{
		ReleaseRangedProjectile();
	}

	bRangedAttackInProgress = false;
	bRangedProjectileReleased = false;
	PendingRangedTarget.Reset();
	PendingRangedWeapon.Reset();
	ActiveRangedMontage.Reset();
}

void UCombatComponent::PerformRangedAttack(
	const UWeaponDefinition& WeaponDefinition,
	AActor* TargetActor)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!bCombatEnabled || !OwnerActor || !World)
	{
		return;
	}

	CancelBlock();

	FVector AimDirection = OwnerActor->GetActorForwardVector();
	if (IsValid(TargetActor))
	{
		const FVector TargetPoint = TargetActor->GetActorLocation() +
			FVector(0.f, 0.f, 50.f);
		AimDirection = (TargetPoint -
			(OwnerActor->GetActorLocation() + FVector(0.f, 0.f, 55.f)))
			.GetSafeNormal();
	}
	else if (const APawn* OwnerPawn = Cast<APawn>(OwnerActor))
	{
		if (const APlayerController* PlayerController =
			Cast<APlayerController>(OwnerPawn->GetController()))
		{
			FVector ViewLocation;
			FRotator ViewRotation;
			PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
			AimDirection = ViewRotation.Vector();
		}
	}

	const FVector SpawnLocation =
		OwnerActor->GetActorLocation() + AimDirection * 100.f + FVector(0.f, 0.f, 50.f);
	const FRotator SpawnRotation = AimDirection.Rotation();

	const TSubclassOf<AWeaponProjectile> ProjectileClass = WeaponDefinition.ProjectileClass
        ? WeaponDefinition.ProjectileClass.Get() : AWeaponProjectile::StaticClass();
    if (UProjectilePoolSubsystem* Pool = GetWorld()->GetSubsystem<UProjectilePoolSubsystem>())
    {
        Pool->Acquire(ProjectileClass, FTransform(SpawnRotation, SpawnLocation), OwnerActor,
            Cast<APawn>(OwnerActor), GetEffectiveDamage(), WeaponDefinition.ProjectileSpeed);
    }
}


UActionComponent* UCombatComponent::GetActions() const
{
    return GetOwner() ? GetOwner()->FindComponentByClass<UActionComponent>() : nullptr;
}

const UActionSet* UCombatComponent::GetActionSet() const
{
    const UActionComponent* Actions = GetActions();
    return Actions ? Actions->GetActionSet() : nullptr;
}

const UActionDefinition* UCombatComponent::GetActiveDefinition() const
{
    const UActionComponent* Actions = GetActions();
    return Actions && Actions->GetActionInstanceId() == OwnedActionId ? Actions->GetActiveDefinition() : nullptr;
}

TPCActionRules::FComboBuffer& UCombatComponent::GetComboBuffer()
{
    UActionComponent* Actions = GetActions();
    return Actions && Actions->GetActionState() == ETPCActionState::Attack
        ? Actions->GetComboBuffer() : ComboBuffer;
}

uint64 UCombatComponent::GetComboGeneration() const
{
    const UActionComponent* Actions = GetActions();
    return Actions && Actions->GetActionState() == ETPCActionState::Attack
        ? Actions->GetActionInstanceId() : AttackGeneration;
}

bool UCombatComponent::HasBufferedComboInput() const
{
    return const_cast<UCombatComponent*>(this)->GetComboBuffer().bQueued;
}

bool UCombatComponent::StartDefinedAttack(const UActionDefinition* Def, EActiveCombatAttackType Type)
{
    return Def && StartSpecialMontageAttack(Def->Montage, Def->DamageMultiplier, Type, Def);
}

bool UCombatComponent::TrySprintAttack()
{
    const UActionSet* Set = GetActionSet();
    if (!bCombatEnabled || bMeleeAttackInProgress || bRangedAttackInProgress || !Set || !Set->SprintAttack) return false;
    CancelBlock();
    return StartDefinedAttack(Set->SprintAttack, EActiveCombatAttackType::Special);
}

bool UCombatComponent::TryParryCounter()
{
    const UActionSet* Set = GetActionSet();
    if (!bCombatEnabled || !GetWorld() || GetWorld()->GetTimeSeconds() > ParryCounterExpiresAt ||
        bMeleeAttackInProgress || bRangedAttackInProgress || !Set || !Set->ParryCounter) return false;
    CancelBlock();
    if (!StartDefinedAttack(Set->ParryCounter, EActiveCombatAttackType::Special)) return false;
    ParryCounterExpiresAt = -1.f;
    return true;
}

void UCombatComponent::CommitSpecialMovement()
{
    if (!bCombatEnabled || !bMeleeAttackInProgress || bSpecialMovementCommitted) return;
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character) return;
    bSpecialMovementCommitted = true;
    if (const auto* Set = GetActionSet())
    {
        if (ActiveAttackType == EActiveCombatAttackType::Buff)
        {
            SwordBuffExpiresAt = GetWorld()->GetTimeSeconds() + Set->BuffDuration;
            SwordBuffMultiplier = Set->BuffDamageMultiplier; // Refresh duration, never multiply repeatedly.
        }
    }
    if (ActiveAttackType == EActiveCombatAttackType::DrawWeapon || ActiveAttackType == EActiveCombatAttackType::SheatheWeapon)
        if (auto* Equipment = Character->FindComponentByClass<UEquipmentComponent>())
            Equipment->SetWeaponDrawn(ActiveAttackType == EActiveCombatAttackType::DrawWeapon);
    if (ActiveAttackType == EActiveCombatAttackType::Uppercut)
        Character->LaunchCharacter(FVector(0.f, 0.f, UppercutLaunchVelocity), false, true);
    else if (ActiveAttackType == EActiveCombatAttackType::AirDive && !bAirDiveLanded && Character->GetCharacterMovement()->IsFalling())
        Character->LaunchCharacter(FVector(0.f, 0.f, -AirAttackDownwardVelocity), false, true);
}

void UCombatComponent::UpdateDiveApproach()
{
    if (!bCombatEnabled || !bMeleeAttackInProgress || ActiveAttackType != EActiveCombatAttackType::AirDive || !GetWorld()) return;
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
    UAnimMontage* M = ActiveAttackMontage.Get();
    if (!Character || !Anim || !M) return;
    const bool bHasDescent = M->GetSectionIndex(TEXT("Descent")) != INDEX_NONE;
    if (!bHasDescent)
    {
        // Compatibility for non-sword legacy Start/Loop/Land montages.
        if (bAirDiveLanded)
        {
            Anim->Montage_SetNextSection(AirDiveStartSection, AirDiveLandSection, M);
            Anim->Montage_Resume(M);
            if (Anim->Montage_GetCurrentSection(M) == AirDiveLoopSection) Anim->Montage_JumpToSection(AirDiveLandSection, M);
        }
        return;
    }
    const FName Section = Anim->Montage_GetCurrentSection(M);
    const FName Wait = M->GetSectionIndex(TEXT("ContactWait")) != INDEX_NONE ? FName(TEXT("ContactWait")) : AirDiveLandSection;
    if (bAirDiveLanded)
    {
        bDiveApproachStarted = true;
        Anim->Montage_SetNextSection(AirDiveStartSection, TEXT("Descent"), M);
        Anim->Montage_SetNextSection(AirDiveLoopSection, TEXT("Descent"), M);
        Anim->Montage_SetNextSection(TEXT("Descent"), Wait, M);
        if (Wait != AirDiveLandSection) Anim->Montage_SetNextSection(Wait, AirDiveLandSection, M);
        Anim->Montage_Resume(M);
        // Only the holding loop can be skipped. Start and the flip-out always play through.
        if (Section == AirDiveLoopSection) Anim->Montage_JumpToSection(TEXT("Descent"), M);
        return;
    }
    if (!bSpecialMovementCommitted || !Character->GetCharacterMovement()->IsFalling()) return;
    if (bDiveApproachStarted)
    {
        if (Section == TEXT("Descent") || Section == Wait)
        {
            const int32 LandIndex = M->GetSectionIndex(AirDiveLandSection);
            const float Contact = LandIndex != INDEX_NONE ? M->CompositeSections[LandIndex].GetTime() : BIG_NUMBER;
            const float Position = Anim->Montage_GetPosition(M);
            if (Position + GetWorld()->GetDeltaSeconds() * FMath::Max(1.f, Anim->Montage_GetPlayRate(M)) * 1.1f >= Contact)
            {
                Anim->Montage_SetPosition(M, Contact - 1.f / 60.f);
                Anim->Montage_Pause(M); // Physical contact opens the barrier, not a timer.
            }
        }
        return;
    }
    FHitResult Hit;
    const FVector Feet = Character->GetCharacterMovement()->GetActorFeetLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SwordDiveFloor), false, Character);
    const float LeadDistance = FMath::Clamp(-Character->GetVelocity().Z * (17.f / 60.f), 80.f, 240.f);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Feet + FVector(0,0,5), Feet - FVector(0,0,LeadDistance), ECC_Visibility, Query) &&
        Character->GetCharacterMovement()->IsWalkable(Hit))
    {
        bDiveApproachStarted = true;
        Anim->Montage_SetNextSection(AirDiveStartSection, TEXT("Descent"), M);
        Anim->Montage_SetNextSection(AirDiveLoopSection, TEXT("Descent"), M);
        Anim->Montage_SetNextSection(TEXT("Descent"), Wait, M);
        if (Wait != AirDiveLandSection) Anim->Montage_SetNextSection(Wait, Wait, M);
        if (Section == AirDiveLoopSection) Anim->Montage_JumpToSection(TEXT("Descent"), M);
    }
}

float UCombatComponent::GetSwordBuffMultiplier() const
{
    return !IsUnarmedPlayer() && GetWorld() && GetWorld()->GetTimeSeconds() < SwordBuffExpiresAt ? SwordBuffMultiplier : 1.f;
}

bool UCombatComponent::TryBuff()
{
    const auto* Set = GetActionSet(); const auto* Actions = GetActions();
    const auto* Character = Cast<ACharacter>(GetOwner());
    const auto* Equipment = GetOwner() ? GetOwner()->FindComponentByClass<UEquipmentComponent>() : nullptr;
    if (!Set || !Set->Buff || !Actions || !Actions->CanRequest(ETPCActionIntent::Buff) ||
        !Character || Character->GetCharacterMovement()->IsFalling() || !Equipment || !Equipment->IsWeaponDrawn()) return false;
    return StartDefinedAttack(Set->Buff, EActiveCombatAttackType::Buff);
}

bool UCombatComponent::TryToggleWeapon()
{
    const auto* Actions = GetActions();
    const auto* Character = Cast<ACharacter>(GetOwner());
    const auto* Equipment = GetOwner() ? GetOwner()->FindComponentByClass<UEquipmentComponent>() : nullptr;
    const auto* Weapon = Equipment ? Equipment->GetEquippedWeaponDefinition() : nullptr;
    const auto* Set = Weapon ? Weapon->ActionSet.Get() : nullptr;
    if (!Set || !Actions || !Actions->CanRequest(ETPCActionIntent::ToggleWeapon) || !Equipment ||
        !Character || Character->GetCharacterMovement()->IsFalling()) return false;
    return StartDefinedAttack(Equipment->IsWeaponDrawn() ? Set->SheatheWeapon : Set->DrawWeapon,
        Equipment->IsWeaponDrawn() ? EActiveCombatAttackType::SheatheWeapon : EActiveCombatAttackType::DrawWeapon);
}

void UCombatComponent::NotifyActionCommit(UAnimSequenceBase* Animation, int32 MontageInstanceId)
{
    if (IsCurrentAttackNotify(Animation, MontageInstanceId)) CommitSpecialMovement();
}
