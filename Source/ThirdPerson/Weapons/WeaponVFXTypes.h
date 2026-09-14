// 可选自动武器特效的风格枚举；动画自带特效不依赖此枚举重新生成。
#pragma once

#include "CoreMinimal.h"
#include "WeaponVFXTypes.generated.h"

/** Cosmetic only. Automatic resolves from the weapon action set, never changes hit rules. */
UENUM(BlueprintType)
enum class EWeaponVFXStyle : uint8 { Automatic, Basic, Ice, Electric };
