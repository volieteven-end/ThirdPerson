#include "ProjectilePoolSubsystem.h"
#include "WeaponProjectile.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"

AWeaponProjectile* UProjectilePoolSubsystem::Acquire(TSubclassOf<AWeaponProjectile> Class,
 const FTransform& Transform, AActor* Source, APawn* InstigatorPawn, float Damage, float Speed)
{
 FCombatHitSpec Spec; Spec.Damage = Damage;
 return AcquireWithHitSpec(Class, Transform, Source, InstigatorPawn, Spec, Speed);
}
AWeaponProjectile* UProjectilePoolSubsystem::AcquireWithHitSpec(TSubclassOf<AWeaponProjectile> Class,
 const FTransform& Transform, AActor* Source, APawn* InstigatorPawn, const FCombatHitSpec& Spec, float Speed)
{
 if (!Class || !GetWorld() || !IsValid(Source)) return nullptr;
 Projectiles.RemoveAll([](const auto& P) { return !IsValid(P); });
 AWeaponProjectile* Result = nullptr;
 int32 ClassCount = 0;
 for (AWeaponProjectile* P : Projectiles)
 {
  if (P->GetClass() != Class) continue;
  ++ClassCount;
  if (!P->IsProjectileActive()) Result = P;
 }
 if (!Result)
 {
  if (ClassCount >= FMath::Max(1, Class.GetDefaultObject()->MaxPooledInstances) || Projectiles.Num() >= MaximumTotal)
  {
   UE_LOG(LogTemp, Warning, TEXT("Projectile pool full for %s; shot skipped (no active arrow recycled early)."), *Class->GetName());
   return nullptr;
  }
  // Construction scripts finish before collision is activated and damage is initialized.
  Result = GetWorld()->SpawnActorDeferred<AWeaponProjectile>(Class, Transform, Source, InstigatorPawn,
      ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  if (!Result) return nullptr;
  Result->FinishSpawning(Transform);
  if (!IsValid(Result)) return nullptr;
  Projectiles.Add(Result);
 }
 Result->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
 Result->SetOwner(Source);
 Result->SetInstigator(InstigatorPawn);
 Result->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
 Result->InitializeCombatProjectile(Spec, Speed);
 // Dynamic-world geometry can block arrows, but other arrows must not.
 for (AWeaponProjectile* P : Projectiles)
 {
  if (P != Result && P->IsProjectileActive())
  {
   Result->IgnoreProjectileActor(P);
   P->IgnoreProjectileActor(Result);
  }
 }
 return Result;
}
void UProjectilePoolSubsystem::Release(AWeaponProjectile* Projectile)
{
 if (!IsValid(Projectile)) return;
 Projectile->DeactivateProjectile();
 // Also support manually spawned projectiles: they are not silently retained by the pool.
 if (!Projectiles.Contains(Projectile)) Projectile->Destroy();
}
void UProjectilePoolSubsystem::ReleaseForSource(AActor* Source)
{
 for (AWeaponProjectile* P : Projectiles)
  if (IsValid(P) && P->IsProjectileActive() && (P->GetOwner() == Source || P->GetInstigator() == Source)) Release(P);
}
int32 UProjectilePoolSubsystem::GetActiveCount() const
{
 int32 Count=0; for (const AWeaponProjectile* P : Projectiles) if (IsValid(P) && P->IsProjectileActive()) ++Count;
 return Count;
}
void UProjectilePoolSubsystem::Deinitialize()
{
 for (AWeaponProjectile* P : Projectiles) if (IsValid(P)) P->Destroy();
 Projectiles.Reset();
 Super::Deinitialize();
}
