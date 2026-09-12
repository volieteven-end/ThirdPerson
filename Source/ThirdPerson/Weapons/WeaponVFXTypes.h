#pragma once

#include "CoreMinimal.h"
#include "WeaponVFXTypes.generated.h"

/** Cosmetic only. Automatic resolves from the weapon action set, never changes hit rules. */
UENUM(BlueprintType)
enum class EWeaponVFXStyle : uint8 { Automatic, Basic, Ice, Electric };
