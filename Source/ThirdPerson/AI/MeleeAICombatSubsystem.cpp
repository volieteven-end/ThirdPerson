#include "MeleeAICombatSubsystem.h"

#include "GameFramework/Pawn.h"
#include "Engine/World.h"

void UMeleeAICombatSubsystem::RemoveInvalidEntries()
{
	for (auto It = SlotAssignments.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = AttackTokenHolders.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

FVector UMeleeAICombatSubsystem::RequestCombatSlot(
	APawn* EnemyPawn,
	const AActor* TargetActor,
	float MinRadius,
	float MaxRadius)
{
	if (!EnemyPawn || !TargetActor)
	{
		return FVector::ZeroVector;
	}

	RemoveInvalidEntries();

	int32* ExistingSlot = SlotAssignments.Find(EnemyPawn);
	if (!ExistingSlot)
	{
		TSet<int32> UsedSlots;
		for (const TPair<TWeakObjectPtr<APawn>, int32>& Pair : SlotAssignments)
		{
			UsedSlots.Add(Pair.Value);
		}

		const FVector ToEnemy =
			(EnemyPawn->GetActorLocation() - TargetActor->GetActorLocation()).GetSafeNormal2D();
		const float EnemyAngle = FMath::Atan2(ToEnemy.Y, ToEnemy.X);
		int32 BestSlot = 0;
		float BestAngleDelta = BIG_NUMBER;

		for (int32 SlotIndex = 0; SlotIndex < CombatSlotCount; ++SlotIndex)
		{
			if (UsedSlots.Contains(SlotIndex))
			{
				continue;
			}

			const float SlotAngle = 2.f * PI * SlotIndex / CombatSlotCount;
			const float AngleDelta = FMath::Abs(FMath::FindDeltaAngleRadians(
				EnemyAngle, SlotAngle));
			if (AngleDelta < BestAngleDelta)
			{
				BestAngleDelta = AngleDelta;
				BestSlot = SlotIndex;
			}
		}

		SlotAssignments.Add(EnemyPawn, BestSlot);
		ExistingSlot = SlotAssignments.Find(EnemyPawn);
	}

	const float Radius = FMath::Lerp(MinRadius, MaxRadius,
		static_cast<float>(*ExistingSlot % 2));
	
	const float Angle =
	2.f * PI * *ExistingSlot / CombatSlotCount;
	const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
	return TargetActor->GetActorLocation() + Offset;
}

bool UMeleeAICombatSubsystem::RequestAttackToken(APawn* EnemyPawn)
{
	if (!EnemyPawn || !GetWorld())
	{
		return false;
	}

	RemoveInvalidEntries();
	if (AttackTokenHolders.Contains(EnemyPawn))
	{
		return true;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const int32 AllowedAttackers = SlotAssignments.Num() <= 2
		? 1 : MaxConcurrentAttackers;
	if (AttackTokenHolders.Num() >= AllowedAttackers ||
		CurrentTime < NextAttackGrantTime)
	{
		return false;
	}

	AttackTokenHolders.Add(EnemyPawn);
	NextAttackGrantTime = CurrentTime + GlobalAttackSpacing;
	return true;
}

void UMeleeAICombatSubsystem::ReleaseAttackToken(APawn* EnemyPawn)
{
	if (EnemyPawn)
	{
		AttackTokenHolders.Remove(EnemyPawn);
	}
}

void UMeleeAICombatSubsystem::ReleaseEnemy(APawn* EnemyPawn)
{
	if (!EnemyPawn)
	{
		return;
	}

	AttackTokenHolders.Remove(EnemyPawn);
	SlotAssignments.Remove(EnemyPawn);
}
