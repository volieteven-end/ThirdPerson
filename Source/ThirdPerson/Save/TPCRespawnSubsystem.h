#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TPCRespawnSubsystem.generated.h"

class ATPCCharacter;

/** 单次同步重生的世界内交接器：Pawn 的 BeginPlay 早于占有，故在此暂存旧角色来源，不跨世界共享。 */
UCLASS()
class THIRDPERSON_API UTPCRespawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<ATPCCharacter> PendingSource;
	ATPCCharacter* TakeSourceFor(const ATPCCharacter* NewPlayer);
};
