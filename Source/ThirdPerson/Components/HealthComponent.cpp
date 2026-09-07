// Fill out your copyright notice in the Description page of Project Settings.


#include "HealthComponent.h"
#include "CombatComponent.h"
#include "ActionComponent.h"
#include "../AI/EnemyCharacter.h"


UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UHealthComponent::ApplyDamage(float Damage)
{
	ApplyDamageFrom(Damage, nullptr);
}

void UHealthComponent::ApplyDamageFrom(float Damage, AActor* DamageSource)
{
 FCombatHitSpec Spec; Spec.Damage = Damage;
 ApplyCombatHit(Spec, DamageSource);
}

FCombatHitResult UHealthComponent::ApplyCombatHit(const FCombatHitSpec& Spec, AActor* DamageSource)
{
 FCombatHitResult Result;
 if (const UActionComponent* Actions = GetOwner() ? GetOwner()->FindComponentByClass<UActionComponent>() : nullptr)
     if (Actions->IsInvulnerable()) return Result;
 if (bIsInvulnerable || bEncounterInvulnerable || !FMath::IsFinite(Spec.Damage) ||
     Spec.Damage <= 0.f || CurrentHealth <= 0.f || DamageSource == GetOwner()) return Result;
 if (IsValid(DamageSource) && DamageSource->IsA<AEnemyCharacter>() &&
     GetOwner() && GetOwner()->IsA<AEnemyCharacter>()) return Result;
 LastDamageSource = DamageSource;
 Result.Outcome = ECombatHitOutcome::Hit;
 float ModifiedDamage = Spec.Damage;
 if (UCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UCombatComponent>() : nullptr)
     ModifiedDamage = Combat->ResolveIncomingHit(Spec, DamageSource, Result);
 const float Before = CurrentHealth;
 CurrentHealth = FMath::Clamp(CurrentHealth - FMath::Max(0.f, ModifiedDamage * DamageReceivedMultiplier), 0.f, MaxHealth);
 Result.ActualDamage = Before - CurrentHealth;
 Result.bKilled = CurrentHealth <= 0.f;
 if (Result.bKilled) Result.Outcome = ECombatHitOutcome::Killed;
 LastHitResult = Result; // Reactions read the resolved outcome, not the held input state.
 OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
 if (Result.bKilled) OnDeath.Broadcast();
 OnCombatHitResolved.Broadcast(Spec, Result, DamageSource);
 return Result;
}

void UHealthComponent::Heal(float Amount)
{
	if (Amount <= 0.f || CurrentHealth <= 0.f)
	{
		return;
	}

	CurrentHealth = FMath::Clamp(
		CurrentHealth + Amount,
		0.f,
		MaxHealth);

	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}
void UHealthComponent::SetInvulnerableFor(float Duration)
{
	if (Duration <= 0.f || !GetWorld())
	{
		return;
	}
	bIsInvulnerable = true;
	GetWorld()->GetTimerManager().SetTimer(InvulnerabilityTimerHandle,this,&ThisClass::ClearInvulnerability,Duration,false);
}

void UHealthComponent::ClearInvulnerability()
{
	bIsInvulnerable = false;
}

void UHealthComponent::SetCurrentHealth(float NewHealth)
{
	CurrentHealth = FMath::Clamp(NewHealth,0.f,MaxHealth);

	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UHealthComponent::AddMaxHealth(float Amount, bool bHealAddedAmount)
{
	if (Amount <= 0.f)
	{
		return;
	}

	MaxHealth += Amount;
	if (bHealAddedAmount)
	{
		CurrentHealth = FMath::Min(CurrentHealth + Amount, MaxHealth);
	}
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UHealthComponent::MultiplyDamageReceived(float Multiplier)
{
	if (Multiplier > 0.f)
	{
		DamageReceivedMultiplier = FMath::Max(
			0.25f, DamageReceivedMultiplier * Multiplier);
	}
}
