

#include "EnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "../Components/HealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "../UI/EnemyHealthWidget.h"
#include "../Components/CombatComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Items/PickupActor.h"
#include "Engine/World.h"
#include "../GameMode/TPCGameMode.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "../Components/LevelComponent.h"
#include "../UI/LockOnMarkerWidget.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "TimerManager.h"
#include "BrainComponent.h"
#include "MeleeAICombatSubsystem.h"
#include "../Audio/TPCCharacterAudioComponent.h"
#include "../Weapons/WeaponActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "UObject/ConstructorHelpers.h"

namespace EnemyPresentation
{
void IgnoreCamera(AActor* Actor)
{
	if (!IsValid(Actor)) return;
	TInlineComponentArray<UPrimitiveComponent*> Components;
	Actor->GetComponents(Components, true);
	for (UPrimitiveComponent* Component : Components)
	{
		// Leave Pawn, Visibility, weapon traces and world collision untouched.
		Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
}
}

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
	// Local steering complements the behavior-tree separation branch while
	// multiple enemies are travelling through the same narrow path.
	GetCharacterMovement()->bUseRVOAvoidance = true;
	GetCharacterMovement()->AvoidanceConsiderationRadius = 260.f;
	GetCharacterMovement()->AvoidanceWeight = 0.45f;

	// 让 AI 路径移动产生加速度，供 ABP_Unarmed 判断 ShouldMove。
	GetCharacterMovement()->bRequestedMoveUseAcceleration = true;
	HealthComponent =CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	CreateDefaultSubobject<UTPCCharacterAudioComponent>(TEXT("CharacterAudioComponent"));
	HealthBarWidget =CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));

	HealthBarWidget->SetupAttachment(RootComponent);
	HealthBarWidget->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	HealthBarWidget->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarWidget->SetDrawSize(FVector2D(160.f, 20.f));
	static ConstructorHelpers::FClassFinder<UEnemyHealthWidget> HealthWidgetClass(TEXT("/Game/Third/Widget/WBP_EnemyHealth"));
	HealthBarWidget->SetWidgetClass(HealthWidgetClass.Class);
	HealthBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBarWidget->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	HealthBarWidget->SetGenerateOverlapEvents(false);
	HealthBarWidget->SetWindowFocusable(false);
	CombatComponent =CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	EquipmentComponent = CreateDefaultSubobject<UEquipmentComponent>(TEXT("EquipmentComponent"));

	LockOnIndicatorWidget = CreateDefaultSubobject<UWidgetComponent>(
		TEXT("LockOnIndicatorWidget"));
	LockOnIndicatorWidget->SetupAttachment(RootComponent);
	LockOnIndicatorWidget->SetWidgetSpace(EWidgetSpace::Screen);
	LockOnIndicatorWidget->SetWidgetClass(ULockOnMarkerWidget::StaticClass());
	LockOnIndicatorWidget->SetDrawSize(FVector2D(LockOnIndicatorSize));
	LockOnIndicatorWidget->SetPivot(FVector2D(0.5f, 0.5f));
	LockOnIndicatorWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LockOnIndicatorWidget->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	LockOnIndicatorWidget->SetGenerateOverlapEvents(false);
	LockOnIndicatorWidget->SetWindowFocusable(false);
	LockOnIndicatorWidget->SetHiddenInGame(true);
	LockOnIndicatorWidget->SetVisibility(false);
}

void AEnemyCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	IgnoreCameraCollision();
}

void AEnemyCharacter::IgnoreCameraCollision()
{
	EnemyPresentation::IgnoreCamera(this);
	TArray<AActor*> Attachments;
	GetAttachedActors(Attachments, true, true);
	for (AActor* Attachment : Attachments) EnemyPresentation::IgnoreCamera(Attachment);
}

void AEnemyCharacter::HandleEquippedWeaponChanged(UWeaponDefinition*, AWeaponActor* Weapon)
{
	// Covers equipment spawned after BeginPlay as well as the initial loadout.
	EnemyPresentation::IgnoreCamera(Weapon);
	IgnoreCameraCollision();
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	// Blueprint defaults, placed-instance overrides and construction scripts have run.
	// Both EnemySpawner and arena waves enter this same path; no per-frame scan.
	IgnoreCameraCollision();
	if (EquipmentComponent)
	{
		EquipmentComponent->OnEquippedWeaponChanged.AddDynamic(this, &ThisClass::HandleEquippedWeaponChanged);
		HandleEquippedWeaponChanged(EquipmentComponent->GetEquippedWeaponDefinition(), EquipmentComponent->GetEquippedWeaponActor());
	}

	if (HealthComponent)
	{
		PreviousHealth = HealthComponent->GetCurrentHealth();
		HealthComponent->OnDeath.AddDynamic(this,&ThisClass::HandleDeath);
		HealthComponent->OnHealthChanged.AddDynamic(this,&ThisClass::HandleHealthChanged);
	}
	if (HealthBarWidget)
	{
		// Older blueprints can serialize an empty override of the native widget class.
		if (!HealthBarWidget->GetWidgetClass())
			HealthBarWidget->SetWidgetClass(GetDefault<AEnemyCharacter>()->HealthBarWidget->GetWidgetClass());
		HealthBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HealthBarWidget->SetGenerateOverlapEvents(false);
		HealthBarWidget->InitWidget();
		if (UEnemyHealthWidget* HealthWidget =Cast<UEnemyHealthWidget>(HealthBarWidget->GetUserWidgetObject()))
		{
			HealthWidget->SetHealthComponent(HealthComponent);
		}
	}
	PositionLockOnIndicator();
}

void AEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!LockOnIndicatorWidget || !GetWorld() || !LockOnIndicatorWidget->IsVisible())
	{
		return;
	}

	const float Pulse = FMath::Sin(
		GetWorld()->GetTimeSeconds() * LockOnIndicatorPulseSpeed) *
		LockOnIndicatorPulseAmount;
	const float CurrentSize = FMath::Max(8.f, LockOnIndicatorSize + Pulse);
	LockOnIndicatorWidget->SetDrawSize(FVector2D(CurrentSize));
}

void AEnemyCharacter::SetLockOnIndicatorVisible(bool bVisible)
{
	if (!LockOnIndicatorWidget)
	{
		return;
	}

	// A stale lock request must not resurrect the marker after this enemy has died.
	bVisible = bVisible && !bDead;
	if (bVisible)
	{
		PositionLockOnIndicator();
		LockOnIndicatorWidget->InitWidget();
	}
	LockOnIndicatorWidget->SetHiddenInGame(!bVisible);
	LockOnIndicatorWidget->SetVisibility(bVisible);
	SetActorTickEnabled(bVisible);
}

FVector AEnemyCharacter::GetLockOnAimPoint() const
{
	// The camera must not inherit chest-bone animation, orb offsets or its pulse.
	const bool bUsesCameraSocket = !LockOnCameraReferenceSocketName.IsNone() &&
		GetMesh() && GetMesh()->DoesSocketExist(LockOnCameraReferenceSocketName);
	const FVector Reference = (bUsesCameraSocket || !bHasLockOnCameraReference)
		? GetLockOnCameraReferencePoint()
		: GetActorTransform().TransformPosition(CachedLockOnCameraReferenceLocal);
	return Reference - FVector(0.f, 0.f, LockOnCameraAimBelowIndicator);
}

FVector AEnemyCharacter::GetLockOnCameraReferencePoint() const
{
	if (!LockOnCameraReferenceSocketName.IsNone() && GetMesh() &&
		GetMesh()->DoesSocketExist(LockOnCameraReferenceSocketName))
	{
		return GetMesh()->GetSocketTransform(LockOnCameraReferenceSocketName)
			.TransformPosition(LockOnCameraReferenceOffset);
	}
	FVector BoundsOrigin;
	FVector BoundsExtent;
	GetActorBounds(true, BoundsOrigin, BoundsExtent);
	return BoundsOrigin + FVector(0.f, 0.f, BoundsExtent.Z) + LockOnCameraReferenceOffset;
}

void AEnemyCharacter::PositionLockOnIndicator()
{
	if (!LockOnIndicatorWidget)
	{
		return;
	}
	// Cache the bounds-based aim point at lock time, just like the old root-attached
	// indicator, so animation bounds changes do not introduce camera bobbing.
	CachedLockOnCameraReferenceLocal = GetActorTransform().InverseTransformPosition(
		GetLockOnCameraReferencePoint());
	bHasLockOnCameraReference = true;

	if (!LockOnIndicatorSocketName.IsNone() &&
		GetMesh() && GetMesh()->DoesSocketExist(LockOnIndicatorSocketName))
	{
		LockOnIndicatorWidget->AttachToComponent(
			GetMesh(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			LockOnIndicatorSocketName);
		LockOnIndicatorWidget->SetRelativeLocation(LockOnIndicatorOffset);
	}
	else
	{
		LockOnIndicatorWidget->AttachToComponent(
			RootComponent,
			FAttachmentTransformRules::KeepRelativeTransform);
		const float ChestHeight = GetCapsuleComponent()
			? GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * 0.28f : 24.f;
		LockOnIndicatorWidget->SetRelativeLocation(
			FVector(0.f, 0.f, ChestHeight) + LockOnIndicatorOffset);
	}
	LockOnIndicatorWidget->SetDrawSize(FVector2D(LockOnIndicatorSize));
}

void AEnemyCharacter::HandleDeath()
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	ClearHitReaction();
	if (UMeleeAICombatSubsystem* Coordinator = GetWorld()->GetSubsystem<UMeleeAICombatSubsystem>())
	{
		Coordinator->ReleaseEnemy(this);
	}
	UE_LOG(LogTemp, Warning, TEXT("Enemy died"));
	SetLockOnIndicatorVisible(false);
	if (HealthBarWidget)
	{
		HealthBarWidget->SetHiddenInGame(true);
		HealthBarWidget->SetVisibility(false);
	}
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (ULevelComponent* Level =
			PlayerPawn->FindComponentByClass<ULevelComponent>())
		{
			Level->AddExperience(ExperienceReward);
		}
	}
	if (ATPCGameMode* GameMode =Cast<ATPCGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GameMode->RegisterEnemyDefeated();
	}
	if (Controller)
	{
		Controller->StopMovement();
	}
	if (AAIController* AIController = Cast<AAIController>(Controller))
	{
		if (UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent())
		{
			Blackboard->SetValueAsBool(TEXT("bIsDead"), true);
		}
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Enemy died"));
		}
	}
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	if (CombatComponent)
	{
		CombatComponent->SetCombatEnabled(false);
	}
	GetWorldTimerManager().ClearTimer(ParryStaggerTimerHandle);
	GetWorldTimerManager().ClearTimer(UppercutRecoveryTimerHandle);
	bParryStaggered = false;
	bUppercutStunned = false;
	LaunchPhase = EEnemyLaunchPhase::Dead;
	LaunchesThisFlight = 0;
	if (DropPickupClass &&FMath::FRand() <= DropChance)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride =ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		GetWorld()->SpawnActor<APickupActor>(DropPickupClass,GetActorLocation() + FVector(0.f, 0.f, 50.f),GetActorRotation(),SpawnParams);
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StopAnimMontage();
	float LifeSpan = MinimumDeathLifeSpan;
	if (DeathMontage)
	{
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		const float MontageLength = AnimInstance
			? AnimInstance->Montage_Play(DeathMontage, 1.f, EMontagePlayReturnType::Duration) : 0.f;
		if (MontageLength > 0.f)
		{
			// Set the playing instance, not the shared asset: dead enemies must not
			// blend back into walking/aiming while waiting for corpse cleanup.
			if (FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(DeathMontage))
				Instance->bEnableAutoBlendOut = false;
			LifeSpan = FMath::Max(
				MinimumDeathLifeSpan,
				MontageLength + DeathDestroyDelay);
		}
	}
	SetLifeSpan(LifeSpan);
}
AActor* AEnemyCharacter::GetCurrentPatrolPoint() const
{
	if (!PatrolPoints.IsValidIndex(CurrentPatrolIndex))
	{
		return nullptr;
	}

	return PatrolPoints[CurrentPatrolIndex];
}

void AEnemyCharacter::AdvancePatrolPoint()
{
	if (PatrolPoints.IsEmpty())
	{
		return;
	}

	CurrentPatrolIndex =
		(CurrentPatrolIndex + 1) % PatrolPoints.Num();
}
void AEnemyCharacter::HandleHealthChanged(
	float CurrentHealth,
	float MaxHealth)
{
	const bool bTookDamage =CurrentHealth < PreviousHealth;

	if (!bDead && bTookDamage && CurrentHealth > 0.f && bUppercutStunned)
	{
		if (LaunchPhase == EEnemyLaunchPhase::DownIdle || LaunchPhase == EEnemyLaunchPhase::DownHit)
		{
			LaunchPhase = EEnemyLaunchPhase::DownHit;
			const float Length = UppercutDownHitMontage ? PlayAnimMontage(UppercutDownHitMontage) : .3f;
			GetWorldTimerManager().SetTimer(UppercutRecoveryTimerHandle, this, &ThisClass::StartUppercutDownIdle, FMath::Max(.01f, Length), false);
		}
		// Air/landing/get-up reactions retain their paired pose and physical recovery gate.
	}
	else if (!bDead && bTookDamage && CurrentHealth > 0.f && !bParryStaggered)
	{
		BeginHitReaction();
	}
	PreviousHealth = CurrentHealth;
}

void AEnemyCharacter::BeginHitReaction()
{
	// Repeated hits extend one recovery gate; they must not overwrite the saved
	// enabled state with the disabled state owned by the previous hit.
	if (!bHitReacting)
	{
		bCombatEnabledBeforeHit = CombatComponent && CombatComponent->IsCombatEnabled();
		MovementModeBeforeHit = GetCharacterMovement()->MovementMode;
		bHitReactionDisabledMovement = GetCharacterMovement()->IsMovingOnGround();
	}
	bHitReacting = true;
	SetAIStunned(true);
	if (Controller) Controller->StopMovement();
	if (CombatComponent) CombatComponent->SetCombatEnabled(false);
	const float FallingSpeed = GetCharacterMovement()->IsFalling() ? GetCharacterMovement()->Velocity.Z : 0.f;
	GetCharacterMovement()->StopMovementImmediately();
	if (GetCharacterMovement()->IsFalling()) GetCharacterMovement()->Velocity.Z = FallingSpeed;
	if (bHitReactionDisabledMovement) GetCharacterMovement()->DisableMovement();

	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	const float MontageDuration = HitReactMontage && Anim
		? Anim->Montage_Play(HitReactMontage, 1.f, EMontagePlayReturnType::Duration) : 0.f;
	GetWorldTimerManager().SetTimer(HitReactionTimerHandle, this,
		&ThisClass::FinishHitReaction,
		FMath::Max(0.05f, MontageDuration > 0.f ? MontageDuration : HitReactFallbackDuration), false);
}

void AEnemyCharacter::FinishHitReaction()
{
	if (!bHitReacting) return;
	bHitReacting = false;
	if (bDead || bParryStaggered || bUppercutStunned) return;
	if (bHitReactionDisabledMovement && GetCharacterMovement()->MovementMode == MOVE_None)
	{
		GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(MovementModeBeforeHit));
	}
	bHitReactionDisabledMovement = false;
	if (CombatComponent && bCombatEnabledBeforeHit) CombatComponent->SetCombatEnabled(true);
	SetAIStunned(false);
}

void AEnemyCharacter::ClearHitReaction()
{
	// Stronger reactions take ownership of movement/combat before restoring them.
	GetWorldTimerManager().ClearTimer(HitReactionTimerHandle);
	bHitReacting = false;
	bHitReactionDisabledMovement = false;
}

void AEnemyCharacter::ApplyParryStagger(AActor* ParryingActor)
{
	if (!HealthComponent || HealthComponent->GetCurrentHealth() <= 0.f)
	{
		return;
	}

	ClearHitReaction();
	bParryStaggered = true;
	if (Controller)
	{
		Controller->StopMovement();
	}
	SetAIStunned(true);
	if (CombatComponent)
	{
		CombatComponent->SetCombatEnabled(false);
	}
	if (!GetCharacterMovement()->IsFalling()) GetCharacterMovement()->DisableMovement();
	StopAnimMontage();

    float StaggerDuration = FMath::Max(ParryStaggerFallbackDuration, ParryStaggerMinimumDuration);
    if (ParryStaggerMontage && GetMesh()->GetAnimInstance())
    {
        const float AuthoredDuration = ParryStaggerMontage->GetPlayLength() / FMath::Max(.01f, ParryStaggerMontage->RateScale);
        const float PlayRate = FMath::Min(1.f, AuthoredDuration / FMath::Max(.1f, ParryStaggerMinimumDuration));
        const float PlayedDuration = GetMesh()->GetAnimInstance()->Montage_Play(ParryStaggerMontage, PlayRate, EMontagePlayReturnType::Duration);
        if (PlayedDuration > 0.f) StaggerDuration = FMath::Max(ParryStaggerMinimumDuration, PlayedDuration);
    }
	GetWorldTimerManager().SetTimer(
		ParryStaggerTimerHandle,
		this,
		&ThisClass::EndParryStagger,
		StaggerDuration,
		false);
}

void AEnemyCharacter::ApplyUppercutHit(AActor* AttackingActor)
{
	if (bDead || !HealthComponent || HealthComponent->GetCurrentHealth() <= 0.f)
	{
		return;
	}
	if (LaunchPhase == EEnemyLaunchPhase::Airborne && LaunchesThisFlight >= MaxLaunchesPerFlight) return;
	ClearHitReaction();
	++LaunchesThisFlight;
	LaunchPhase = EEnemyLaunchPhase::Airborne;
	bReceivedUppercutLanding = false;
	GetWorldTimerManager().ClearTimer(UppercutRecoveryTimerHandle);
	GetWorldTimerManager().ClearTimer(ParryStaggerTimerHandle);
	bParryStaggered = false;

	bUppercutStunned = true;
	bUppercutLandingRecovery = false;
	if (Controller)
	{
		Controller->StopMovement();
	}
	SetAIStunned(true);
	if (CombatComponent)
	{
		CombatComponent->SetCombatEnabled(false);
	}
	StopAnimMontage();
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);

	FVector AwayDirection = GetActorForwardVector();
	if (AttackingActor)
	{
		AwayDirection =
			(GetActorLocation() - AttackingActor->GetActorLocation()).GetSafeNormal2D();
	}
	LaunchCharacter(
		AwayDirection * UppercutHorizontalVelocity +
		FVector(0.f, 0.f, UppercutLaunchVelocity),
		true,
		true);

	float FirstGroundCheckDelay = 0.1f;
	if (UppercutHitMontage)
	{
		const float MontageDuration = PlayAnimMontage(UppercutHitMontage);
		if (MontageDuration > 0.f)
		{
			FirstGroundCheckDelay = FMath::Min(0.1f, MontageDuration);
		}
	}
	GetWorldTimerManager().SetTimer(
		UppercutRecoveryTimerHandle,
		this,
		&ThisClass::TryEndUppercutStun,
		FirstGroundCheckDelay,
		false);
}

void AEnemyCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (bUppercutStunned)
	{
		bReceivedUppercutLanding = true;
		LaunchesThisFlight = 0;
		TryEndUppercutStun();
	}
}

void AEnemyCharacter::TryEndUppercutStun()
{
	if (bDead || !bUppercutStunned || bUppercutLandingRecovery)
	{
		return;
	}
	if (!bReceivedUppercutLanding || (GetCharacterMovement() && GetCharacterMovement()->IsFalling()))
	{
		GetWorldTimerManager().SetTimer(
			UppercutRecoveryTimerHandle,
			this,
			&ThisClass::TryEndUppercutStun,
			0.1f,
			false);
		return;
	}

	// The character has reached the ground. End the airborne pose, then keep
	// AI/combat locked through the optional landing recovery animation.
	bUppercutLandingRecovery = true;
	LaunchPhase = EEnemyLaunchPhase::LandImpact;
	if (UppercutHitMontage)
	{
		StopAnimMontage(UppercutHitMontage);
	}
	float LandingDuration = UppercutLandingRecoveryDuration;
	if (UppercutLandMontage)
	{
		const float MontageDuration = PlayAnimMontage(UppercutLandMontage);
		if (MontageDuration > 0.f)
		{
			LandingDuration = MontageDuration;
		}
	}
	GetWorldTimerManager().SetTimer(
		UppercutRecoveryTimerHandle,
		this,
		&ThisClass::StartUppercutDownIdle,
		FMath::Max(0.01f, LandingDuration),
		false);
}

void AEnemyCharacter::StartUppercutGetUp()
{
	if (bDead || !bUppercutStunned)
	{
		return;
	}
	if (!bReceivedUppercutLanding || GetCharacterMovement()->IsFalling()) return;
	LaunchPhase = EEnemyLaunchPhase::GetUp;

	float GetUpDuration = UppercutGetUpFallbackDuration;
	if (UppercutGetUpMontage)
	{
		const float MontageDuration = PlayAnimMontage(UppercutGetUpMontage);
		if (MontageDuration > 0.f)
		{
			GetUpDuration = MontageDuration;
		}
	}
	GetWorldTimerManager().SetTimer(
		UppercutRecoveryTimerHandle,
		this,
		&ThisClass::FinishUppercutStun,
		FMath::Max(0.01f, GetUpDuration),
		false);
}

void AEnemyCharacter::StartUppercutDownIdle()
{
    if (bDead || !bUppercutStunned || !bReceivedUppercutLanding || GetCharacterMovement()->IsFalling()) return;
    LaunchPhase = EEnemyLaunchPhase::DownIdle;
    if (UppercutDownIdleMontage) PlayAnimMontage(UppercutDownIdleMontage);
    GetWorldTimerManager().SetTimer(UppercutRecoveryTimerHandle, this, &ThisClass::StartUppercutGetUp,
        FMath::Max(.01f, UppercutDownDuration), false);
}

void AEnemyCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);
    if (bDead || !bUppercutStunned || !GetCharacterMovement()->IsFalling() || LaunchPhase == EEnemyLaunchPhase::Airborne) return;
    // A platform can disappear during landing/down/get-up. Keep the physical gate closed.
    LaunchPhase = EEnemyLaunchPhase::Airborne;
    bUppercutLandingRecovery = false;
    bReceivedUppercutLanding = false;
    if (UppercutHitMontage) PlayAnimMontage(UppercutHitMontage);
    GetWorldTimerManager().SetTimer(UppercutRecoveryTimerHandle, this, &ThisClass::TryEndUppercutStun, .1f, false);
}

void AEnemyCharacter::FinishUppercutStun()
{
	if (bDead || !bUppercutStunned)
	{
		return;
	}
	if (!bReceivedUppercutLanding || GetCharacterMovement()->IsFalling()) return;
	LaunchPhase = EEnemyLaunchPhase::None;
	bUppercutStunned = false;
	bUppercutLandingRecovery = false;
	SetAIStunned(false);
	if (CombatComponent)
	{
		CombatComponent->SetCombatEnabled(true);
	}
}

void AEnemyCharacter::SetAIStunned(bool bStunned)
{
	if (bStunned && GetWorld())
	{
		if (UMeleeAICombatSubsystem* Coordinator = GetWorld()->GetSubsystem<UMeleeAICombatSubsystem>())
		{
			Coordinator->ReleaseAttackToken(this);
		}
	}
	if (AAIController* AIController = Cast<AAIController>(Controller))
	{
		if (UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent())
		{
			Blackboard->SetValueAsBool(TEXT("bIsStunned"), bStunned);
			if (bStunned) Blackboard->SetValueAsBool(TEXT("bHasAttackToken"), false);
		}
	}
}

void AEnemyCharacter::EndParryStagger()
{
	if (bUppercutStunned || bDead) return;
	bParryStaggered = false;
	if (!HealthComponent || HealthComponent->GetCurrentHealth() <= 0.f)
	{
		return;
	}
	if (GetCharacterMovement()->MovementMode == MOVE_None) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (CombatComponent)
	{
		CombatComponent->SetCombatEnabled(true);
	}
	if (AAIController* AIController = Cast<AAIController>(Controller))
	{
		if (UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent())
		{
			Blackboard->SetValueAsBool(TEXT("bIsStunned"), false);
		}
	}
}
