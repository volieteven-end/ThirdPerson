#include "CountessBossCharacter.h"
#include "BossActionComponent.h"
#include "CountessBossAIController.h"
#include "CountessBossAnimInstance.h"
#include "../Components/HealthComponent.h"
#include "../Components/CombatComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
ACountessBossCharacter::ACountessBossCharacter()
{
 BossActions=CreateDefaultSubobject<UBossActionComponent>(TEXT("BossActions"));
 AIControllerClass=ACountessBossAIController::StaticClass(); AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;
 GetCapsuleComponent()->InitCapsuleSize(42.f,96.f);
 ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(TEXT("/Game/ParagonCountess/Characters/Heroes/Countess/Meshes/SM_Countess"));
 GetMesh()->SetSkeletalMesh(MeshAsset.Object);
 GetMesh()->SetRelativeLocation(FVector(0,0,-96)); GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
 GetMesh()->SetAnimInstanceClass(UCountessBossAnimInstance::StaticClass());
 GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 MinimumDeathLifeSpan=5.f; DeathMontage=nullptr; HitReactMontage=nullptr;
 DropPickupClass=nullptr; DropChance=0.f; ExperienceReward=300;
 HealthComponent->MaxHealth=1500; HealthComponent->CurrentHealth=1500;
 GetCharacterMovement()->RotationRate=FRotator(0,240,0);
 GetCharacterMovement()->MaxAcceleration=1000.f;
 GetCharacterMovement()->BrakingDecelerationWalking=1200.f;
}
void ACountessBossCharacter::BeginPlay()
{
 // BossActionComponent is initialized before the actor's BeginPlay; share one health/reward configuration.
 const auto* D=BossActions->GetDefinition();
 ExperienceReward=D->Experience;
 DeathMontage=nullptr; HitReactMontage=nullptr; MinimumDeathLifeSpan=5.f;
 Super::BeginPlay();
 HealthBarWidget->SetVisibility(false);
 CombatComponent->SetCombatEnabled(false); // Boss has its own executor, never the small-enemy attack loop.
}
void ACountessBossCharacter::HandleHealthChanged(float Current,float Max)
{ PreviousHealth=Current; if (BossActions) BossActions->ConsiderPhase(Current,Max); }
void ACountessBossCharacter::ApplyParryStagger(AActor* Player) { BossActions->OnParried(Player); }
void ACountessBossCharacter::ApplyUppercutHit(AActor* Player)
{ /* The typed incoming hit already applied exactly 30 poise. Boss cannot be launched. */ }
void ACountessBossCharacter::HandleDeath()
{ BossActions->Die(); Super::HandleDeath(); }
void ACountessBossCharacter::Landed(const FHitResult& Hit)
{ Super::Landed(Hit); BossActions->OnLanded(); }
bool ACountessBossCharacter::StartEncounter(AActor* Player) { return BossActions->BeginEncounter(Player); }
void ACountessBossCharacter::RequestEncounterReset() { BossActions->RequestReset(); }
int32 ACountessBossCharacter::GetBossPhase() const { return BossActions->Phase; }
float ACountessBossCharacter::GetPoisePercent() const { return BossActions->Poise/FMath::Max(1.f,BossActions->GetMaxPoise()); }
