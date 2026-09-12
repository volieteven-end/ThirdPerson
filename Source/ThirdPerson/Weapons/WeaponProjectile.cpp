#include "WeaponProjectile.h"
#include "ProjectilePoolSubsystem.h"
#include "../Components/HealthComponent.h"
#include "../AI/EnemyCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

AWeaponProjectile::AWeaponProjectile()
{
 PrimaryActorTick.bCanEverTick = true;
 PrimaryActorTick.bStartWithTickEnabled = false;
 CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
 SetRootComponent(CollisionSphere);
 CollisionSphere->InitSphereRadius(8.f);
 CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 CollisionSphere->OnComponentHit.AddDynamic(this, &ThisClass::HandleProjectileHit);
 ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
 ProjectileMovement->UpdatedComponent = CollisionSphere;
 ProjectileMovement->InitialSpeed = ProjectileMovement->MaxSpeed = 1800.f;
 ProjectileMovement->bRotationFollowsVelocity = true;
 ProjectileMovement->ProjectileGravityScale = .15f;
 ProjectileMovement->bAutoActivate = false;
 InitialLifeSpan = 0.f;
}
void AWeaponProjectile::BeginPlay()
{
 Super::BeginPlay();
 // BP defaults previously disabled the sphere while enabling BlockAllDynamic on ArrowMesh.
 // Repair the runtime invariant after Blueprint construction, including existing child assets.
 ConfigureCollision();
 DeactivateProjectile();
}
void AWeaponProjectile::ConfigureCollision()
{
 TInlineComponentArray<UPrimitiveComponent*> Parts(this);
 for (UPrimitiveComponent* Part : Parts)
 {
  Part->SetSimulatePhysics(false);
  Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Part->SetGenerateOverlapEvents(false);
 }
 CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
 CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
 CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
 CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
 CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
 CollisionSphere->SetCanEverAffectNavigation(false);
}
void AWeaponProjectile::InitializeProjectile(float InDamage, float InSpeed)
{
 FCombatHitSpec Spec; Spec.Damage = InDamage;
 InitializeCombatProjectile(Spec, InSpeed);
}
void AWeaponProjectile::InitializeCombatProjectile(const FCombatHitSpec& Spec, float InSpeed)
{
 GetWorldTimerManager().ClearTimer(RecycleTimer);
 SetLifeSpan(0.f);
 DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
 ConfigureCollision();
 CollisionSphere->ClearMoveIgnoreActors();
 HitSpec = Spec;
 Damage = FMath::Max(0.f, Spec.Damage);
 HitSpec.Damage = Damage;
 bActive = true;
 bImpacted = false;
 const AActor* Source = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();
 bEnemyShot = IsValid(Source) && Source->IsA<AEnemyCharacter>();
 auto IgnoreSource = [this](AActor* Source)
 {
  if (!IsValid(Source)) return;
  CollisionSphere->IgnoreActorWhenMoving(Source, true);
  TArray<AActor*> Attached; Source->GetAttachedActors(Attached, true, true);
  for (AActor* Part : Attached) CollisionSphere->IgnoreActorWhenMoving(Part, true);
 };
 IgnoreSource(GetOwner()); IgnoreSource(GetInstigator());
 const float SafeSpeed = FMath::Max(1.f, InSpeed);
 // StopSimulating clears UpdatedComponent: explicitly restore it on every acquisition.
 ProjectileMovement->SetUpdatedComponent(CollisionSphere);
 ProjectileMovement->InitialSpeed = ProjectileMovement->MaxSpeed = SafeSpeed;
 ProjectileMovement->Velocity = GetActorForwardVector() * SafeSpeed;
 ProjectileMovement->UpdateComponentVelocity();
 ProjectileMovement->Activate(true);
 ProjectileMovement->SetComponentTickEnabled(true);
 SetActorHiddenInGame(false);
 SetActorEnableCollision(true);
 CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 SetActorTickEnabled(bDrawDebugProjectile);
 GetWorldTimerManager().SetTimer(RecycleTimer, this, &ThisClass::ReturnToPool, FMath::Max(.1f, FlightLifetime), false);
}
void AWeaponProjectile::IgnoreProjectileActor(AActor* Actor)
{
 if (Actor) CollisionSphere->IgnoreActorWhenMoving(Actor, true);
}
void AWeaponProjectile::DeactivateProjectile()
{
 GetWorldTimerManager().ClearTimer(RecycleTimer);
 SetLifeSpan(0.f);
 bActive = false; bImpacted = false; bEnemyShot = false; Damage = 0.f;
 HitSpec = FCombatHitSpec();
 SetActorEnableCollision(false);
 CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 CollisionSphere->ClearMoveIgnoreActors();
 ProjectileMovement->StopMovementImmediately();
 ProjectileMovement->Deactivate();
 ProjectileMovement->SetComponentTickEnabled(false);
 DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
 SetActorHiddenInGame(true);
 SetActorTickEnabled(false);
 SetOwner(nullptr); SetInstigator(nullptr);
}
void AWeaponProjectile::ReturnToPool()
{
 if (UProjectilePoolSubsystem* Pool = GetWorld()->GetSubsystem<UProjectilePoolSubsystem>()) Pool->Release(this);
 else Destroy();
}
void AWeaponProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
 GetWorldTimerManager().ClearTimer(RecycleTimer);
 Super::EndPlay(Reason);
}
void AWeaponProjectile::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 if (bActive && bDrawDebugProjectile) DrawDebugSphere(GetWorld(), GetActorLocation(), 8.f, 8, FColor::Yellow, false, 0.f, 0, 1.f);
}
void AWeaponProjectile::HandleProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
 UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
 if (!bActive || bImpacted || OtherActor == GetOwner() || OtherActor == GetInstigator()) return;
 bImpacted = true; // Set before damage delegates can cause reentrant callbacks / death.
 ProjectileMovement->StopMovementImmediately();
 ProjectileMovement->Deactivate();
 ProjectileMovement->SetComponentTickEnabled(false);
 SetActorEnableCollision(false);
 CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 SetActorTickEnabled(false);
 AActor* Source = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();
 const bool bFriendly = bEnemyShot && IsValid(OtherActor) && OtherActor->IsA<AEnemyCharacter>();
 if (IsValid(OtherActor) && !bFriendly)
 {
  if (UHealthComponent* Health = OtherActor->FindComponentByClass<UHealthComponent>())
  {
   HitSpec.ImpactPoint = Hit.ImpactPoint;
   Health->ApplyCombatHit(HitSpec, IsValid(Source) ? Source : nullptr);
  }
 }
 if (!IsValid(this) || !bActive) return;
 PlayImpactEffect(Hit);
 if (bReturnImmediatelyOnImpact) { ReturnToPool(); return; }
 // Attach to an animated bone rather than the capsule so embedded arrows follow the body.
 if (ACharacter* Character = Cast<ACharacter>(OtherActor); IsValid(Character) && Character->GetMesh())
 {
  USkeletalMeshComponent* Mesh = Character->GetMesh();
  FName Bone = Hit.BoneName;
  if (Bone.IsNone() || Mesh->GetBoneIndex(Bone) == INDEX_NONE) Bone = Mesh->FindClosestBone(Hit.ImpactPoint);
  AttachToComponent(Mesh, FAttachmentTransformRules::KeepWorldTransform, Bone);
 }
 else if (IsValid(OtherComponent)) AttachToComponent(OtherComponent, FAttachmentTransformRules::KeepWorldTransform);
 // This replaces the flight timer: five seconds are measured from impact, not spawn.
 GetWorldTimerManager().SetTimer(RecycleTimer, this, &ThisClass::ReturnToPool, FMath::Max(.1f, ImpactLifetime), false);
}
