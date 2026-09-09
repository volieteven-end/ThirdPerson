#include "TutorialActors.h"
#include "TutorialDirector.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "../Character/TPCCharacter.h"
#include "../Components/HealthComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Weapons/WeaponDefinition.h"

ATutorialZone::ATutorialZone()
{
    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds")); SetRootComponent(Bounds);
    Bounds->SetBoxExtent(FVector(140,140,180)); Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Bounds->SetCollisionResponseToAllChannels(ECR_Ignore); Bounds->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
    Bounds->OnComponentBeginOverlap.AddDynamic(this,&ThisClass::Enter);
    Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker")); Marker->SetupAttachment(Bounds);
    Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision); Marker->SetCanEverAffectNavigation(false);
    Number = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Number")); Number->SetupAttachment(Bounds);
    Number->SetHorizontalAlignment(EHTA_Center); Number->SetWorldSize(55); Number->SetTextRenderColor(FColor(246,224,155));
    Number->SetRelativeLocation(FVector(0,0,80));
}
bool ATutorialZone::Contains(const FVector& Location) const
{
    const FVector Local = Bounds->GetComponentTransform().InverseTransformPosition(Location);
    const FVector Extent = Bounds->GetUnscaledBoxExtent();
    return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}
void ATutorialZone::SetHighlighted(bool Active)
{
    Marker->SetVisibility(Active); Number->SetVisibility(Active && !bEntry);
}
void ATutorialZone::Enter(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (auto* P = Cast<ATPCCharacter>(Other))
        if (auto* D = ATutorialDirector::Find(this)) D->HandleZone(this,P);
}
ATutorialGate::ATutorialGate()
{
    Bars = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Gate")); SetRootComponent(Bars);
    Bars->SetMobility(EComponentMobility::Movable);
    // Gates never divide the cooked navigation mesh; collision enforces the current lesson.
    Bars->SetCanEverAffectNavigation(false); Bars->SetCollisionProfileName(TEXT("BlockAll"));
}
void ATutorialGate::SetOpen(bool Open)
{
    Bars->SetVisibility(!Open); Bars->SetCollisionEnabled(Open ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}

ATutorialTrainingEnemy::ATutorialTrainingEnemy()
{
    AIControllerClass = ATutorialTrainingController::StaticClass(); AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    static ConstructorHelpers::FClassFinder<AEnemyCharacter> Template(TEXT("/Game/Third/Character/BP_EnemyCharacter"));
    if (const auto* E = Template.Class ? Template.Class->GetDefaultObject<AEnemyCharacter>() : nullptr)
    {
        GetCapsuleComponent()->InitCapsuleSize(E->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), E->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
        GetMesh()->SetSkeletalMeshAsset(E->GetMesh()->GetSkeletalMeshAsset());
        GetMesh()->SetAnimInstanceClass(E->GetMesh()->GetAnimClass());
        GetMesh()->SetRelativeTransform(E->GetMesh()->GetRelativeTransform());
        HitReactMontage = E->HitReactMontage; DeathMontage = E->DeathMontage; ParryStaggerMontage = E->ParryStaggerMontage;
        UppercutHitMontage = E->UppercutHitMontage; UppercutLandMontage = E->UppercutLandMontage;
        UppercutDownIdleMontage = E->UppercutDownIdleMontage; UppercutDownHitMontage = E->UppercutDownHitMontage;
        UppercutGetUpMontage = E->UppercutGetUpMontage; UppercutLaunchVelocity = E->UppercutLaunchVelocity;
        UppercutHorizontalVelocity = E->UppercutHorizontalVelocity;
    }
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    GetCharacterMovement()->MaxWalkSpeed = 230.f;
    DropPickupClass = nullptr; DropChance = 0.f; ExperienceReward = 0;
    MinimumDeathLifeSpan = 2.f;
}
void ATutorialTrainingEnemy::BeginPlay()
{
    HealthComponent->MaxHealth = TrainingMode == ETutorialEnemyMode::Combat ? 100.f : 100000.f;
    Super::BeginPlay();
    HealthComponent->SetCurrentHealth(HealthComponent->MaxHealth); PreviousHealth = HealthComponent->MaxHealth;
    if (TrainingWeapon) EquipmentComponent->EquipWeapon(TrainingWeapon);
    CombatComponent->SetCombatEnabled(TrainingMode != ETutorialEnemyMode::Passive);
    // Nonlethal targets should not display an enormous, meaningless HP count.
    HealthBarWidget->SetVisibility(TrainingMode == ETutorialEnemyMode::Combat);
}
bool ATutorialTrainingEnemy::IsThreatening() const
{
    const auto* C = Cast<ATutorialTrainingController>(Controller);
    return C && (C->bTelegraphing || CombatComponent->IsMeleeAttackInProgress());
}
void ATutorialTrainingEnemy::HandleDeath()
{
    const bool WasAlive = GetLaunchPhase() != EEnemyLaunchPhase::Dead;
    Super::HandleDeath();
    if (WasAlive && Director.IsValid()) Director->TrainingEnemyDefeated(this);
}

ATutorialTrainingController::ATutorialTrainingController() { PrimaryActorTick.bCanEverTick = true; }
void ATutorialTrainingController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* E = Cast<ATutorialTrainingEnemy>(GetPawn());
    auto* P = Cast<ATPCCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!E || !P || E->TrainingMode == ETutorialEnemyMode::Passive) { StopMovement(); bTelegraphing = false; return; }
    auto* H = E->FindComponentByClass<UHealthComponent>(); auto* C = E->FindComponentByClass<UCombatComponent>();
    const double Now = GetWorld()->GetTimeSeconds();
    if (!H || H->GetCurrentHealth() <= 0 || P->HealthComponent->GetCurrentHealth() <= 0 || !C ||
        E->IsHitReacting() || E->IsParryStaggered() || E->GetLaunchPhase() != EEnemyLaunchPhase::None)
    { StopMovement(); bTelegraphing = false; NextAttack = Now + 2.; return; }
    const FVector Difference = P->GetActorLocation() - E->GetActorLocation();
    const float Distance = Difference.Size2D();
    if (!C->IsMeleeAttackInProgress()) E->SetActorRotation(FRotator(0,Difference.Rotation().Yaw,0));
    if (Distance > 175.f)
    {
        if (Now - LastMoveRequest > .35) { MoveToActor(P,135.f,true,true,false); LastMoveRequest = Now; }
        bTelegraphing = false; NextAttack = Now + 1.5; return;
    }
    StopMovement();
    if (NextAttack <= 0.) NextAttack = Now + 2.;
    bTelegraphing = Now >= NextAttack - .8 && !C->IsMeleeAttackInProgress();
    if (Now >= NextAttack && !C->IsMeleeAttackInProgress())
    { bTelegraphing = false; C->TryAttack(); NextAttack = Now + (E->TrainingMode == ETutorialEnemyMode::Guard ? 3.5 : 2.8); }
}
