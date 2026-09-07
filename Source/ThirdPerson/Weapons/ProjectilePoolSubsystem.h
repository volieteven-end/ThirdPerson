#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "../Components/CombatHitTypes.h"
#include "ProjectilePoolSubsystem.generated.h"
class AWeaponProjectile;
/** World-local, bounded pool. Active / embedded arrows are never stolen for another shot. */
UCLASS()
class THIRDPERSON_API UProjectilePoolSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable, Category="Projectile Pool")
 AWeaponProjectile* Acquire(TSubclassOf<AWeaponProjectile> ProjectileClass, const FTransform& Transform,
     AActor* Source, APawn* InstigatorPawn, float Damage, float Speed);
 AWeaponProjectile* AcquireWithHitSpec(TSubclassOf<AWeaponProjectile> ProjectileClass, const FTransform& Transform,
     AActor* Source, APawn* InstigatorPawn, const FCombatHitSpec& Spec, float Speed);
 void ReleaseForSource(AActor* Source);
 void Release(AWeaponProjectile* Projectile);
 UFUNCTION(BlueprintPure, Category="Projectile Pool") int32 GetPoolSize() const { return Projectiles.Num(); }
 UFUNCTION(BlueprintPure, Category="Projectile Pool") int32 GetActiveCount() const;
 virtual void Deinitialize() override;
private:
 UPROPERTY(Transient) TArray<TObjectPtr<AWeaponProjectile>> Projectiles;
 static constexpr int32 MaximumTotal = 512;
};
