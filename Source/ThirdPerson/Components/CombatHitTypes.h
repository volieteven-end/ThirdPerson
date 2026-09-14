#pragma once

#include "CoreMinimal.h"
#include "CombatHitTypes.generated.h"

UENUM(BlueprintType)
enum class ECombatHitOutcome : uint8 { Ignored, Hit, Blocked, Parried, Killed };

/** 一次命中尝试的数据：伤害、破韧、格挡规则与命中组；统一交给生命组件结算，不是第二套血量系统。 */
USTRUCT(BlueprintType)
struct THIRDPERSON_API FCombatHitSpec
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) float PoiseDamage = 0.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCanBeBlocked = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCanBeParried = true;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector ImpactPoint = FVector::ZeroVector;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 ActionSerial = 0;
 UPROPERTY(EditAnywhere, BlueprintReadWrite) FName WindowId;
};

/** 命中结算结果，记录实际扣血及格挡、弹反、击杀等状态；反馈与奖励应读取结果而非请求伤害。 */
USTRUCT(BlueprintType)
struct THIRDPERSON_API FCombatHitResult
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) ECombatHitOutcome Outcome = ECombatHitOutcome::Ignored;
 UPROPERTY(BlueprintReadOnly) float ActualDamage = 0.f;
 UPROPERTY(BlueprintReadOnly) bool bBlocked = false;
 UPROPERTY(BlueprintReadOnly) bool bGuardBroken = false;
 UPROPERTY(BlueprintReadOnly) bool bParried = false;
 UPROPERTY(BlueprintReadOnly) bool bKilled = false;
};
