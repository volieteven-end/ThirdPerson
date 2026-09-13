#pragma once
#include "CoreMinimal.h"
class ATPCCharacter;
class UTPCSaveGame;

/** Common checkpoint / portal serialization. Never captures world-owned actor pointers. */
namespace TPCPlayerProgress
{
    void Capture(const ATPCCharacter& Player, UTPCSaveGame& Save, bool bRestoreVitals = false);
    void Apply(ATPCCharacter& Player, const UTPCSaveGame& Save);
    bool OwnsCheckpoint(const UTPCSaveGame& Save, const UObject* WorldContext);
}
