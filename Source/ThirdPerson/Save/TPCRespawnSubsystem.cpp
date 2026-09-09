#include "TPCRespawnSubsystem.h"
#include "../Character/TPCCharacter.h"

ATPCCharacter* UTPCRespawnSubsystem::TakeSourceFor(const ATPCCharacter* NewPlayer)
{
	ATPCCharacter* Source = PendingSource.Get();
	if (!Source || Source == NewPlayer || Source->GetClass() != NewPlayer->GetClass()) return nullptr;
	PendingSource.Reset();
	return Source;
}
