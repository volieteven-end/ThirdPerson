#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MeleeAICombatSubsystem.generated.h"

class APawn;

/** 世界级近战协调器：分配包围位置与攻击令牌；每个世界独立，敌人退出交战时必须释放占用。 */
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
	friend struct FMeleeAITestAccess;
	void RemoveInvalidEntries();

	static constexpr int32 CombatSlotCount = 8;
	static constexpr int32 MaxConcurrentAttackers = 2;
	static constexpr float GlobalAttackSpacing = 0.45f;

	TMap<TWeakObjectPtr<APawn>, int32> SlotAssignments;
	TSet<TWeakObjectPtr<APawn>> AttackTokenHolders;
	float NextAttackGrantTime = 0.f;
};
