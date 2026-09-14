#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Components/CombatHitTypes.h"
#include "WeaponProjectile.generated.h"
class USphereComponent;
class UProjectileMovementComponent;
class UPrimitiveComponent;

/** 通用投射物：处理飞行、碰撞、单次命中和嵌入／回收；对象池复用前必须清除上次发射状态。 */
UCLASS()
class THIRDPERSON_API AWeaponProjectile : public AActor
{
 GENERATED_BODY()
public:
 AWeaponProjectile();
 virtual void Tick(float DeltaSeconds) override;
 void InitializeProjectile(float InDamage, float InSpeed);
 virtual void InitializeCombatProjectile(const FCombatHitSpec& Spec, float InSpeed);
 virtual void DeactivateProjectile();
 void IgnoreProjectileActor(AActor* Actor);
 UFUNCTION(BlueprintPure, Category="Projectile") bool IsProjectileActive() const { return bActive; }
 UFUNCTION(BlueprintPure, Category="Projectile") bool HasImpacted() const { return bImpacted; }
 UPROPERTY(EditDefaultsOnly, Category="Projectile|Lifetime", meta=(ClampMin="0.1")) float ImpactLifetime = 5.f;
 UPROPERTY(EditDefaultsOnly, Category="Projectile|Lifetime", meta=(ClampMin="0.1")) float FlightLifetime = 10.f;
 UPROPERTY(EditDefaultsOnly, Category="Projectile|Pool", meta=(ClampMin="1", ClampMax="512")) int32 MaxPooledInstances = 128;
 UPROPERTY(EditDefaultsOnly, Category="Projectile") bool bReturnImmediatelyOnImpact = false;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 /** Exactly once on a validated impact, before this projectile returns to its pool. */
 virtual void PlayImpactEffect(const FHitResult& Hit) {}
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<USphereComponent> CollisionSphere;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UProjectileMovementComponent> ProjectileMovement;
 UPROPERTY(EditDefaultsOnly, Category="Debug") bool bDrawDebugProjectile = false;
 UFUNCTION() void HandleProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
     UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
private:
 friend struct FProjectileTestAccess;
 friend struct FCombatVFXTestAccess;
 void ConfigureCollision();
 void ReturnToPool();
 float Damage = 0.f;
 FCombatHitSpec HitSpec;
 bool bActive = false;
 bool bImpacted = false;
 bool bEnemyShot = false;
 FTimerHandle RecycleTimer;
};
