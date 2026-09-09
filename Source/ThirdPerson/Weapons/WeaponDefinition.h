#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WeaponDefinition.generated.h"

class AWeaponActor;
class AWeaponProjectile;
class UAnimMontage;
class UTexture2D;
class UParticleSystem;
class UActionSet;

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	Melee,
	Ranged
};

/** Data shared by weapon pickups, equipment, combat and UI. */
UCLASS(BlueprintType)
class THIRDPERSON_API UWeaponDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Actions")
	TObjectPtr<UActionSet> ActionSet;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName WeaponId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EWeaponType WeaponType = EWeaponType::Melee;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat", meta = (ClampMin = "0.0"))
	float Damage = 25.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat", meta = (ClampMin = "0.0"))
	float AttackCooldown = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat", meta = (ClampMin = "0.0", EditCondition = "WeaponType == EWeaponType::Melee", EditConditionHides))
	float MeleeRange = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat", meta = (ClampMin = "1.0", EditCondition = "WeaponType == EWeaponType::Melee", EditConditionHides))
	float MeleeRadius = 50.f;

	/** Optional physical ground-impact volume for the Dive Landing notify group.
	 * Zero preserves the normal weapon's blade-only traces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Melee", meta = (ClampMin = "0", ClampMax = "200", Units = "cm"))
	float DiveLandingImpactRadius = 0.f;

	/** Socket near the beginning of the blade. Used for melee weapon traces. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat", meta = (EditCondition = "WeaponType == EWeaponType::Melee", EditConditionHides))
	FName BladeBaseSocketName = TEXT("BladeBase");

	/** Socket near the blade tip. Used for melee weapon traces. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat", meta = (EditCondition = "WeaponType == EWeaponType::Melee", EditConditionHides))
	FName BladeTipSocketName = TEXT("BladeTip");

	/** Optional. Ranged weapons use the native projectile class when this is empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ranged", meta = (EditCondition = "WeaponType == EWeaponType::Ranged", EditConditionHides))
	TSubclassOf<AWeaponProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ranged", meta = (ClampMin = "1.0", EditCondition = "WeaponType == EWeaponType::Ranged", EditConditionHides))
	float ProjectileSpeed = 1800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	TArray<TObjectPtr<UAnimMontage>> AttackMontages;

	/** Cascade effect spawned at the real melee collision impact point. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Effects")
	TObjectPtr<UParticleSystem> MeleeHitEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Equip")
	TSubclassOf<AWeaponActor> WeaponActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Equip")
	FName EquipSocketName = TEXT("weapon_r");

	/** Back mount, relative to the chosen spine socket/bone; independent of the hand grip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Equip")
	FName SheathSocketName = TEXT("spine_03");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Equip")
	FTransform SheathRelativeTransform = FTransform(FRotator(-34.f, -170.f, 62.f), FVector(44.f, -14.f, 31.f));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|UI")
	TObjectPtr<UTexture2D> Icon;
};
