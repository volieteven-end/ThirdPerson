#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Components/CombatHitTypes.h"
#include "WeaponProjectile.generated.h"
class USphereComponent;
class UProjectileMovementComponent;
class UPrimitiveComponent;

/** Swept sphere owns hit detection; attached meshes are cosmetic and never push characters. */
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
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<USphereComponent> CollisionSphere;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components") TObjectPtr<UProjectileMovementComponent> ProjectileMovement;
 UPROPERTY(EditDefaultsOnly, Category="Debug") bool bDrawDebugProjectile = false;
 UFUNCTION() void HandleProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
     UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
private:
 friend struct FProjectileTestAccess;
 void ConfigureCollision();
 void ReturnToPool();
 float Damage = 0.f;
 FCombatHitSpec HitSpec;
 bool bActive = false;
 bool bImpacted = false;
 bool bEnemyShot = false;
 FTimerHandle RecycleTimer;
};
