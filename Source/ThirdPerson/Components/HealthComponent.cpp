// Fill out your copyright notice in the Description page of Project Settings.


#include "HealthComponent.h"
#include "CombatComponent.h"


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
	if (bIsInvulnerable || Damage <= 0.f)
	{
		return;
	}
	if (Damage <= 0.f || CurrentHealth <= 0.f)
	{
		return;
	}

	LastDamageSource = DamageSource;
	float ModifiedDamage = Damage;
	if (UCombatComponent* Combat =
		GetOwner() ? GetOwner()->FindComponentByClass<UCombatComponent>() : nullptr)
	{
		ModifiedDamage = Combat->ModifyIncomingDamage(Damage, DamageSource);
	}
	const float EffectiveDamage = ModifiedDamage * DamageReceivedMultiplier;
	CurrentHealth = FMath::Clamp(
		CurrentHealth - EffectiveDamage,
		0.f,
		MaxHealth);

	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.f)
	{
		OnDeath.Broadcast();
	}
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
