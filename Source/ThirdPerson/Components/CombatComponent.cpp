// Fill out your copyright notice in the Description page of Project Settings.


#include "CombatComponent.h"
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

namespace
{
	bool IsEnemyFriendlyFire(const AActor* Source, const AActor* Target)
	{
		return Source && Target &&
			Source->IsA<AEnemyCharacter>() && Target->IsA<AEnemyCharacter>();
	}
}
// Sets default values for this component's properties
UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
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
			ComboBuffer.Queue();
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
	if (!StartMeleeMontageAttack() && GetEffectiveAttackMontages().IsEmpty())
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
	if (StartSpecialMontageAttack(UppercutMontage, UppercutDamageMultiplier, EActiveCombatAttackType::Uppercut))
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
	if (!bCombatEnabled || !Character || !GetWorld() || !Character->GetCharacterMovement()->IsFalling() ||
		bMeleeAttackInProgress || bRangedAttackInProgress || !AirDiveMontage ||
		GetWorld()->GetTimeSeconds() - LastAttackTime < GetEffectiveAttackCooldown()) { return; }
	if (AirDiveMontage->GetSectionIndex(AirDiveStartSection) == INDEX_NONE ||
		AirDiveMontage->GetSectionIndex(AirDiveLoopSection) == INDEX_NONE ||
		AirDiveMontage->GetSectionIndex(AirDiveLandSection) == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("Air Dive requires Start / Loop / Land sections: %s"), *GetNameSafe(AirDiveMontage));
		return;
	}
	CancelBlock();
	if (StartSpecialMontageAttack(AirDiveMontage, AirDiveDamageMultiplier, EActiveCombatAttackType::AirDive))
	{
		UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance();
		Anim->Montage_JumpToSection(AirDiveStartSection, AirDiveMontage);
		Anim->Montage_SetNextSection(AirDiveStartSection, AirDiveLoopSection, AirDiveMontage);
		Anim->Montage_SetNextSection(AirDiveLoopSection, AirDiveLoopSection, AirDiveMontage);
		Anim->Montage_SetNextSection(AirDiveLandSection, NAME_None, AirDiveMontage);
		Character->LaunchCharacter(FVector(0.f, 0.f, -AirAttackDownwardVelocity), false, true);
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
		ComboBuffer.Reset(AttackGeneration);
		ACharacter* Character = Cast<ACharacter>(GetOwner());
		UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		if (Anim) { Anim->Montage_JumpToSection(AirDiveLandSection, ActiveAttackMontage.Get()); }
		else { CancelActiveAttack(); }
	}
	else if (ActiveAttackType == EActiveCombatAttackType::Air)
	{
		CancelActiveAttack(); // Do not continue an airborne combo while standing on the floor.
	}
}


void UCombatComponent::StartBlock()
{
	if (!bCombatEnabled || bMeleeAttackInProgress || !GetWorld())
	{
		return;
	}

	ClearDashCombo();
	bBlockInputHeld = true;
	bIsBlocking = true;
	bParryWindowActive = true;
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
	}
}

void UCombatComponent::EndParryWindow()
{
	bParryWindowActive = false;
	if (!bBlockInputHeld)
	{
		bIsBlocking = false;
	}
}

void UCombatComponent::CancelBlock()
{
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
		bParryWindowActive = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ParryWindowTimerHandle);
		}
		if (!bBlockInputHeld)
		{
			bIsBlocking = false;
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
		CancelBlock();
		return IncomingDamage;
	}

	Result.bBlocked = true; Result.Outcome = ECombatHitOutcome::Blocked;
	return IncomingDamage * BlockDamageMultiplier;
}

FCombatHitSpec UCombatComponent::MakeCurrentHitSpec(const FVector& ImpactPoint) const
{
 FCombatHitSpec Spec;
 Spec.Damage = GetEffectiveDamage() * ActiveDamageMultiplier;
 Spec.ImpactPoint = ImpactPoint;
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
	const auto& Montages = GetEffectiveAttackMontages();
	if (!Montages.IsValidIndex(NextAttackIndex)) { return false; }
	if (!StartSpecialMontageAttack(Montages[NextAttackIndex], 1.f, EActiveCombatAttackType::Normal)) { return false; }
	++NextAttackIndex; // The last stage ends the combo; it never wraps to stage 1.
	return true;
}

bool UCombatComponent::StartAirMontageAttack()
{
	UAnimMontage* Montage = AirAttackMontages.IsEmpty()
		? (NextAirAttackIndex == 0 ? AirAttackMontage.Get() : nullptr)
		: (AirAttackMontages.IsValidIndex(NextAirAttackIndex) ? AirAttackMontages[NextAirAttackIndex].Get() : nullptr);
	if (!StartSpecialMontageAttack(Montage, AirAttackDamageMultiplier, EActiveCombatAttackType::Air)) { return false; }
	++NextAirAttackIndex;
	return true;
}


bool UCombatComponent::StartSpecialMontageAttack(UAnimMontage* Montage, float DamageScale, EActiveCombatAttackType AttackType)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!bCombatEnabled || !Anim || !GetWorld() || !Montage) { return false; }
	UAnimMontage* PreviousMontage = ActiveAttackMontage.Get();
	// Advance the generation BEFORE playing; old montage callbacks may fire during replacement.
	++AttackGeneration;
	ComboBuffer.Reset(AttackGeneration);
	ActiveAttackMontage = Montage;
	ActiveAttackInstanceId = INDEX_NONE;
	bMeleeAttackInProgress = true;
	bComboInputWindowOpen = false;
	bAirDiveLanded = false;
	ActiveDamageMultiplier = FMath::Max(0.f, DamageScale);
	ActiveAttackType = AttackType;
	if (Character->PlayAnimMontage(Montage) <= 0.f)
	{
		ResetMeleeAction();
		if (PreviousMontage) { Anim->Montage_Stop(0.1f, PreviousMontage); }
		return false;
	}
	if (FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage))
	{
		ActiveAttackInstanceId = Instance->GetInstanceID();
	}
	FOnMontageEnded End;
	End.BindUObject(this, &ThisClass::HandleAttackMontageEnded, AttackGeneration);
	Anim->Montage_SetEndDelegate(End, Montage);
	FOnMontageBlendingOutStarted Blend;
	Blend.BindUObject(this, &ThisClass::HandleAttackBlendingOut, AttackGeneration);
	Anim->Montage_SetBlendingOutDelegate(Blend, Montage);
	LastAttackTime = GetWorld()->GetTimeSeconds();
	OnMeleeAttackStarted.Broadcast();
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
		ComboBuffer.bChainPointReached = true;
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
	ComboBuffer.bChainPointReached = true;
	TryCommitBufferedCombo();
}

bool UCombatComponent::CanChainAttack() const
{
	if (!bCombatEnabled || !bMeleeAttackInProgress) { return false; }
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
	if (!ComboBuffer.Consume(AttackGeneration, CanChainAttack())) { return false; }
	EndAttackWindow();
	StopAttackEffects();
	const EActiveCombatAttackType PreviousType = ActiveAttackType;
	const bool bStarted = PreviousType == EActiveCombatAttackType::Air
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
	UEquipmentComponent* Equipment = GetOwner() ? GetOwner()->FindComponentByClass<UEquipmentComponent>() : nullptr;
	if (AWeaponActor* Weapon = Equipment ? Equipment->GetEquippedWeaponActor() : nullptr)
	{
		Weapon->SetAttackEffectActive(false);
	}
}

void UCombatComponent::ResetMeleeAction()
{
	ClearDashCombo();
	++AttackGeneration;
	ComboBuffer.Reset(AttackGeneration);
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
			Health->ApplyCombatHit(MakeCurrentHitSpec(Hit.ImpactPoint), OwnerActor);
			SpawnMeleeHitEffect(Hit);
			ApplySpecialHitReaction(HitActor);
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
	float InTraceRadius)
{
	if (!bCombatEnabled || !bMeleeAttackInProgress) { return; }
	ACharacter* OwnerCharacter =Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter ||!OwnerCharacter->GetMesh())
	{
		return;
	}
	USkeletalMeshComponent* Mesh =OwnerCharacter->GetMesh();
	UEquipmentComponent* Equipment =
		OwnerCharacter->FindComponentByClass<UEquipmentComponent>();
	const UWeaponDefinition* WeaponDefinition =
		Equipment ? Equipment->GetEquippedWeaponDefinition() : nullptr;
	AWeaponActor* WeaponActor =
		Equipment ? Equipment->GetEquippedWeaponActor() : nullptr;
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
	}

	if (!bUseWeaponBladeTrace &&
		Mesh->GetBoneIndex(InAttackBoneName) == INDEX_NONE)
	{
		UE_LOG(LogTemp,Warning,TEXT("Attack bone not found: %s"),*InAttackBoneName.ToString());
		return;
	}

	ActiveAttackBoneName = InAttackBoneName;
	ActiveTraceRadius = InTraceRadius;
	HitActors.Reset();
	if (!bUseWeaponBladeTrace)
	{
		PreviousAttackLocation =Mesh->GetBoneLocation(ActiveAttackBoneName);
	}
	bAttackWindowActive = true;
	SetComponentTickEnabled(true);
}
void UCombatComponent::EndAttackWindow()
{
	bAttackWindowActive = false;
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
	auto ApplyHits = [this, OwnerCharacter](const TArray<FHitResult>& Hits)
	{
		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (!HitActor || HitActors.Contains(HitActor) ||
				IsEnemyFriendlyFire(OwnerCharacter, HitActor))
			{
				continue;
			}

			HitActors.Add(HitActor);
			if (UHealthComponent* Health =
				HitActor->FindComponentByClass<UHealthComponent>())
			{
				Health->ApplyCombatHit(MakeCurrentHitSpec(Hit.ImpactPoint), OwnerCharacter);
				SpawnMeleeHitEffect(Hit);
				ApplySpecialHitReaction(HitActor);
			}
		}
	};

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

		constexpr int32 BladeTraceSamples = 5;
		for (int32 SampleIndex = 0; SampleIndex < BladeTraceSamples; ++SampleIndex)
		{
			const float Alpha = static_cast<float>(SampleIndex) /
				static_cast<float>(BladeTraceSamples - 1);
			const FVector PreviousSample = FMath::Lerp(
				PreviousBladeBaseLocation, PreviousBladeTipLocation, Alpha);
			const FVector CurrentSample = FMath::Lerp(
				CurrentBladeBaseLocation, CurrentBladeTipLocation, Alpha);

			TArray<FHitResult> Hits;
			World->SweepMultiByChannel(
				Hits,
				PreviousSample,
				CurrentSample,
				FQuat::Identity,
				ECC_Pawn,
				FCollisionShape::MakeSphere(ActiveTraceRadius),
				QueryParams);
			ApplyHits(Hits);
		}

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
		Hits, PreviousAttackLocation, CurrentAttackLocation, FQuat::Identity,
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
	if (ActiveAttackType == EActiveCombatAttackType::Uppercut)
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
	return Equipment ? Equipment->GetEquippedWeaponDefinition() : nullptr;
}

float UCombatComponent::GetEffectiveDamage() const
{
	const UWeaponDefinition* Weapon = GetEquippedWeaponDefinition();
	const float BaseDamage = Weapon ? Weapon->Damage : Damage;
	return (BaseDamage + LevelDamageBonus) * DamageMultiplier;
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
	return BaseRange * MeleeReachMultiplier;
}

float UCombatComponent::GetEffectiveAttackRadius() const
{
	const UWeaponDefinition* Weapon = GetEquippedWeaponDefinition();
	const float BaseRadius = Weapon && Weapon->WeaponType == EWeaponType::Melee
		? Weapon->MeleeRadius
		: AttackRadius;
	return BaseRadius * MeleeReachMultiplier;
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

