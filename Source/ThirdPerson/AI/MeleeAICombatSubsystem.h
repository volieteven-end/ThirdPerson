#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MeleeAICombatSubsystem.generated.h"

class APawn;

/**
 * Coordinates melee enemies targeting the same player.
 * It assigns positions around the target and limits simultaneous attackers.
 */
UCLASS()
class THIRDPERSON_API UMeleeAICombatSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	FVector RequestCombatSlot(APawn* EnemyPawn, const AActor* TargetActor,
		float MinRadius = 220.f, float MaxRadius = 300.f);
	bool RequestAttackToken(APawn* EnemyPawn);
	void ReleaseAttackToken(APawn* EnemyPawn);
	void ReleaseEnemy(APawn* EnemyPawn);

	int32 GetActiveAttackerCount() const { return AttackTokenHolders.Num(); }

private:
	void RemoveInvalidEntries();

	static constexpr int32 CombatSlotCount = 8;
	static constexpr int32 MaxConcurrentAttackers = 2;
	static constexpr float GlobalAttackSpacing = 0.45f;

	TMap<TWeakObjectPtr<APawn>, int32> SlotAssignments;
	TSet<TWeakObjectPtr<APawn>> AttackTokenHolders;
	float NextAttackGrantTime = 0.f;
};
