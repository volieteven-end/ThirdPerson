#include "EnemyHealthWidget.h"
#include "../Components/HealthComponent.h"

void UEnemyHealthWidget::SetHealthComponent(
	UHealthComponent* InHealthComponent)
{
	if (HealthComponent == InHealthComponent)
	{
		return;
	}

	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(
			this,
			&ThisClass::HandleHealthChanged);
	}

	HealthComponent = InHealthComponent;

	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.AddDynamic(
			this,
			&ThisClass::HandleHealthChanged);

		HandleHealthChanged(
			HealthComponent->GetCurrentHealth(),
			HealthComponent->GetMaxHealth());
	}
}

void UEnemyHealthWidget::HandleHealthChanged(
	float CurrentHealth,
	float MaxHealth)
{
	const float Percent = MaxHealth > 0.f
		? CurrentHealth / MaxHealth
		: 0.f;

	RefreshHealthPercent(Percent);
}

void UEnemyHealthWidget::NativeDestruct()
{
	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(
			this,
			&ThisClass::HandleHealthChanged);
	}

	Super::NativeDestruct();
}