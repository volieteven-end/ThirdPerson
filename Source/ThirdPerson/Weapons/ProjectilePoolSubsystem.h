#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ProjectilePoolSubsystem.generated.h"
class AWeaponProjectile;
struct FCombatHitSpec;
/** 世界内有上限的投射物池；按具体类复用，正在飞行或嵌入的投射物不会被其他发射请求抢占。 */
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
