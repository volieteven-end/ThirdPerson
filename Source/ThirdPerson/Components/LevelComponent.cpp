#include "LevelComponent.h"

#include "CombatComponent.h"
#include "HealthComponent.h"
#include "StaminaComponent.h"

ULevelComponent::ULevelComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULevelComponent::BeginPlay()
{
	Super::BeginPlay();
	BroadcastProgress();
}

int32 ULevelComponent::GetExperienceToNextLevel() const
{
	return FMath::Max(
		1,
		FMath::RoundToInt(
			static_cast<float>(BaseExperienceRequirement) *
			FMath::Pow(ExperienceGrowth, static_cast<float>(Level - 1))));
}

void ULevelComponent::AddExperience(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	CurrentExperience += Amount;
	while (CurrentExperience >= GetExperienceToNextLevel())
	{
		CurrentExperience -= GetExperienceToNextLevel();
		LevelUpOnce();
	}

	BroadcastProgress();
}

void ULevelComponent::LevelUpOnce()
{
	++Level;

	if (UHealthComponent* Health =
		GetOwner()->FindComponentByClass<UHealthComponent>())
	{
		Health->AddMaxHealth(MaxHealthPerLevel, true);
	}

	if (UCombatComponent* Combat =
		GetOwner()->FindComponentByClass<UCombatComponent>())
	{
		Combat->AddDamageBonus(DamagePerLevel);
	}

	OnLevelUp.Broadcast(Level);

	if (Level % 5 == 0)
	{
		++PendingUpgradeSelections;
		if (PendingUpgradeSelections == 1)
		{
			GenerateUpgradeChoices();
		}
	}
}

void ULevelComponent::GenerateUpgradeChoices()
{
	TArray<ELevelUpgradeType> Candidates = {
		ELevelUpgradeType::Vitality,
		ELevelUpgradeType::Power,
		ELevelUpgradeType::Frenzy,
		ELevelUpgradeType::Endurance,
		ELevelUpgradeType::LongReach,
		ELevelUpgradeType::IronSkin
	};

	CurrentChoices.Reset();
	for (int32 ChoiceIndex = 0; ChoiceIndex < 3; ++ChoiceIndex)
	{
		const int32 CandidateIndex = FMath::RandRange(0, Candidates.Num() - 1);
		CurrentChoices.Add(MakeUpgradeChoice(Candidates[CandidateIndex]));
		Candidates.RemoveAtSwap(CandidateIndex);
	}

	OnUpgradeChoicesReady.Broadcast(
		CurrentChoices[0], CurrentChoices[1], CurrentChoices[2]);
}

FLevelUpgradeChoice ULevelComponent::MakeUpgradeChoice(ELevelUpgradeType Type) const
{
	FLevelUpgradeChoice Choice;
	Choice.Type = Type;

	switch (Type)
	{
	case ELevelUpgradeType::Vitality:
		Choice.DisplayName = FText::FromString(TEXT("Vitality"));
		Choice.Description = FText::FromString(TEXT("Max health +25 and recover 25 health"));
		break;
	case ELevelUpgradeType::Power:
		Choice.DisplayName = FText::FromString(TEXT("Power"));
		Choice.Description = FText::FromString(TEXT("All damage +15%"));
		break;
	case ELevelUpgradeType::Frenzy:
		Choice.DisplayName = FText::FromString(TEXT("Frenzy"));
		Choice.Description = FText::FromString(TEXT("Attack cooldown -10%"));
		break;
	case ELevelUpgradeType::Endurance:
		Choice.DisplayName = FText::FromString(TEXT("Endurance"));
		Choice.Description = FText::FromString(TEXT("Max stamina +20 and recover 20 stamina"));
		break;
	case ELevelUpgradeType::LongReach:
		Choice.DisplayName = FText::FromString(TEXT("Long Reach"));
		Choice.Description = FText::FromString(TEXT("Melee range and hit radius +15%"));
		break;
	case ELevelUpgradeType::IronSkin:
		Choice.DisplayName = FText::FromString(TEXT("Iron Skin"));
		Choice.Description = FText::FromString(TEXT("Damage received -10%"));
		break;
	default:
		break;
	}

	return Choice;
}

bool ULevelComponent::SelectUpgrade(int32 ChoiceIndex)
{
	if (PendingUpgradeSelections <= 0 ||
		!CurrentChoices.IsValidIndex(ChoiceIndex))
	{
		return false;
	}

	const ELevelUpgradeType SelectedType = CurrentChoices[ChoiceIndex].Type;
	ApplyUpgrade(SelectedType);
	SelectedUpgrades.Add(SelectedType);
	CurrentChoices.Reset();
	--PendingUpgradeSelections;

	if (PendingUpgradeSelections > 0)
	{
		GenerateUpgradeChoices();
	}

	return true;
}

void ULevelComponent::ApplyUpgrade(ELevelUpgradeType Type)
{
	UHealthComponent* Health = GetOwner()->FindComponentByClass<UHealthComponent>();
	UCombatComponent* Combat = GetOwner()->FindComponentByClass<UCombatComponent>();
	UStaminaComponent* Stamina = GetOwner()->FindComponentByClass<UStaminaComponent>();

	switch (Type)
	{
	case ELevelUpgradeType::Vitality:
		if (Health) Health->AddMaxHealth(25.f, true);
		break;
	case ELevelUpgradeType::Power:
		if (Combat) Combat->MultiplyDamage(1.15f);
		break;
	case ELevelUpgradeType::Frenzy:
		if (Combat) Combat->MultiplyAttackCooldown(0.9f);
		break;
	case ELevelUpgradeType::Endurance:
		if (Stamina) Stamina->AddMaxStamina(20.f, true);
		break;
	case ELevelUpgradeType::LongReach:
		if (Combat) Combat->MultiplyMeleeReach(1.15f);
		break;
	case ELevelUpgradeType::IronSkin:
		if (Health) Health->MultiplyDamageReceived(0.9f);
		break;
	default:
		break;
	}
}

void ULevelComponent::RestoreProgress(
	int32 SavedLevel,
	int32 SavedExperience,
	const TArray<ELevelUpgradeType>& SavedUpgrades)
{
	if (bProgressRestored)
	{
		return;
	}
	bProgressRestored = true;

	Level = FMath::Max(1, SavedLevel);
	CurrentExperience = FMath::Max(0, SavedExperience);

	if (UHealthComponent* Health =
		GetOwner()->FindComponentByClass<UHealthComponent>())
	{
		Health->AddMaxHealth(MaxHealthPerLevel * static_cast<float>(Level - 1), false);
	}
	if (UCombatComponent* Combat =
		GetOwner()->FindComponentByClass<UCombatComponent>())
	{
		Combat->AddDamageBonus(DamagePerLevel * static_cast<float>(Level - 1));
	}

	SelectedUpgrades.Reset();
	for (const ELevelUpgradeType Upgrade : SavedUpgrades)
	{
		ApplyUpgrade(Upgrade);
		SelectedUpgrades.Add(Upgrade);
	}

	BroadcastProgress();
}

void ULevelComponent::BroadcastProgress()
{
	OnLevelProgressChanged.Broadcast(
		Level, CurrentExperience, GetExperienceToNextLevel());
}
