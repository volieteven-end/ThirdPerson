#include "StaminaComponent.h"

UStaminaComponent::UStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStaminaComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentStamina = MaxStamina;
	OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
}

void UStaminaComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetWorld() ||GetWorld()->GetTimeSeconds() < RecoveryResumeTime)
	{
		return;
	}
	const float OldStamina = CurrentStamina;
	CurrentStamina = FMath::Min(CurrentStamina + RecoveryPerSecond * DeltaTime,MaxStamina);
	
	if (!FMath::IsNearlyEqual(OldStamina, CurrentStamina))
	{
		OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
	}
}

bool UStaminaComponent::TryConsume(float Amount)
{
	if (CurrentStamina < Amount)
	{
		return false;
	}

	CurrentStamina -= Amount;

	if (UWorld* World = GetWorld())
	{
		RecoveryResumeTime =World->GetTimeSeconds() + RecoveryDelay;
	}

	OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
	return true;

}
void UStaminaComponent::SetCurrentStamina(float NewStamina)
{
	CurrentStamina = FMath::Clamp(NewStamina,0.f,MaxStamina);

	OnStaminaChanged.Broadcast(CurrentStamina,MaxStamina);
}

void UStaminaComponent::AddMaxStamina(float Amount, bool bRestoreAddedAmount)
{
	if (Amount <= 0.f)
	{
		return;
	}

	MaxStamina += Amount;
	if (bRestoreAddedAmount)
	{
		CurrentStamina = FMath::Min(CurrentStamina + Amount, MaxStamina);
	}
	OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
}
