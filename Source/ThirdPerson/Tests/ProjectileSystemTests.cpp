#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "../Weapons/WeaponProjectile.h"
#include "../Weapons/ProjectilePoolSubsystem.h"
#include "../AI/EnemyCharacter.h"
#include "../Character/TPCCharacter.h"
#include "../Components/HealthComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
struct FProjectileTestAccess
{
 static void Hit(AWeaponProjectile* Arrow,AActor* Victim)
 {FHitResult H;H.ImpactPoint=Victim->GetActorLocation();Arrow->HandleProjectileHit(nullptr,Victim,nullptr,FVector::ZeroVector,H);}
 static float Remaining(AWeaponProjectile* A){return A->GetWorldTimerManager().GetTimerRemaining(A->RecycleTimer);}
};
namespace
{
 struct FProjectileWorld
 {
  UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);
  AEnemyCharacter* Enemy=nullptr;ATPCCharacter* Player=nullptr;UProjectilePoolSubsystem* Pool=nullptr;
  FProjectileWorld(){GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);Enemy=W->SpawnActor<AEnemyCharacter>();Player=W->SpawnActor<ATPCCharacter>();Player->HealthComponent->CurrentHealth=100;Pool=W->GetSubsystem<UProjectilePoolSubsystem>();}
  ~FProjectileWorld(){GEngine->DestroyWorldContext(W);W->DestroyWorld(false);if(W->IsRooted())W->RemoveFromRoot();}
  AWeaponProjectile* Fire(AActor* Source=nullptr){return Pool->Acquire(AWeaponProjectile::StaticClass(),FTransform(FRotator::ZeroRotator,FVector(-300,0,100)),Source?Source:Enemy,Cast<APawn>(Source?Source:Enemy),10.f,1500.f);}
 };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectileDamageTest,"ThirdPerson.Projectiles.DamageOnceAndFriendlyFire",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProjectileDamageTest::RunTest(const FString& Parameters)
{
 FProjectileWorld F;auto* A=F.Fire();TestNotNull(TEXT("Pool acquisition"),A);if(!A)return false;
 FProjectileTestAccess::Hit(A,F.Enemy);TestFalse(TEXT("Owner cannot spend arrow"),A->HasImpacted());
 FProjectileTestAccess::Hit(A,F.Player);TestEqual(TEXT("One hit removes ten HP"),F.Player->HealthComponent->CurrentHealth,90.f);
 FProjectileTestAccess::Hit(A,F.Player);TestEqual(TEXT("Duplicate callbacks do not damage twice"),F.Player->HealthComponent->CurrentHealth,90.f);
 TestTrue(TEXT("Impact has five seconds remaining"),FMath::IsNearlyEqual(FProjectileTestAccess::Remaining(A),5.f,.01f));
 auto* Ally=F.W->SpawnActor<AEnemyCharacter>();auto* H=Ally->FindComponentByClass<UHealthComponent>();H->CurrentHealth=100;
 auto* B=F.Fire();FProjectileTestAccess::Hit(B,Ally);TestEqual(TEXT("Enemy friendly fire stays disabled"),H->CurrentHealth,100.f);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectileReuseTest,"ThirdPerson.Projectiles.ReuseAndReset",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProjectileReuseTest::RunTest(const FString& Parameters)
{
 FProjectileWorld F;auto* A=F.Fire();FProjectileTestAccess::Hit(A,F.Player);F.Pool->Release(A);
 TestTrue(TEXT("Returned arrow hidden"),A->IsHidden());TestFalse(TEXT("Returned arrow inactive"),A->IsProjectileActive());TestNull(TEXT("Old owner cleared"),A->GetOwner());TestNull(TEXT("Old attachment cleared"),A->GetAttachParentActor());
 auto* B=F.Fire(F.Player);TestTrue(TEXT("Same actor reused"),A==B);TestEqual(TEXT("No extra allocation"),F.Pool->GetPoolSize(),1);
 TestTrue(TEXT("New owner installed"),B->GetOwner()==F.Player);TestFalse(TEXT("Impact flag reset"),B->HasImpacted());
 auto* M=B->FindComponentByClass<UProjectileMovementComponent>();TestNotNull(TEXT("Updated component restored after impact"),M->UpdatedComponent.Get());TestTrue(TEXT("Movement reactivated"),M->IsActive());TestTrue(TEXT("Fresh velocity"),M->Velocity.Equals(FVector(1500,0,0)));
 TestTrue(TEXT("New flight timer replaces impact timer"),FMath::IsNearlyEqual(FProjectileTestAccess::Remaining(B),10.f,.01f));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectileCollisionTest,"ThirdPerson.Projectiles.CollisionInvariant",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProjectileCollisionTest::RunTest(const FString& Parameters)
{
 FProjectileWorld F;auto* A=F.Fire();auto* S=A->FindComponentByClass<USphereComponent>();
 TestTrue(TEXT("Only swept query collision, no physical pushes"),S->GetCollisionEnabled()==ECollisionEnabled::QueryOnly);
 TestTrue(TEXT("Pawn hit detection restored"),S->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block);
 FProjectileTestAccess::Hit(A,F.Player);TestFalse(TEXT("Embedded arrow has no collision"),A->GetActorEnableCollision());
 TestEqual(TEXT("Embedded arrow velocity zero"),A->GetVelocity(),FVector::ZeroVector);return true;
}
#endif
