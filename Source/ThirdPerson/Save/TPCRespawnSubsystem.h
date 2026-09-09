#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TPCRespawnSubsystem.generated.h"

class ATPCCharacter;

/** Scoped to one synchronous RestartPlayer call, never shared across PIE worlds.
 * Pawn BeginPlay runs before possession, so the controller cannot carry this handoff. */
UCLASS()
class THIRDPERSON_API UTPCRespawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<ATPCCharacter> PendingSource;
	ATPCCharacter* TakeSourceFor(const ATPCCharacter* NewPlayer);
};
