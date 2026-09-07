#pragma once

#include "CoreMinimal.h"
#include "CombatHitTypes.generated.h"

UENUM(BlueprintType)
enum class ECombatHitOutcome : uint8 { Ignored, Hit, Blocked, Parried, Killed };

/** One attempt, not a second health/damage system. All damage still goes through HealthComponent. */
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
