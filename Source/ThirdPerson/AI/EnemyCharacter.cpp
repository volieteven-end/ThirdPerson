// Fill out your copyright notice in the Description page of Project Settings.


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
AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
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
	HealthBarWidget =CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidget"));

	HealthBarWidget->SetupAttachment(RootComponent);
	HealthBarWidget->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	HealthBarWidget->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarWidget->SetDrawSize(FVector2D(160.f, 20.f));
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
	LockOnIndicatorWidget->SetGenerateOverlapEvents(false);
	LockOnIndicatorWidget->SetWindowFocusable(false);
	LockOnIndicatorWidget->SetHiddenInGame(true);
	LockOnIndicatorWidget->SetVisibility(false);
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HealthComponent)
	{
		PreviousHealth = HealthComponent->GetCurrentHealth();
		HealthComponent->OnDeath.AddDynamic(this,&ThisClass::HandleDeath);
		HealthComponent->OnHealthChanged.AddDynamic(this,&ThisClass::HandleHealthChanged);
	}
	if (HealthBarWidget)
	{
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
		const float MontageLength =PlayAnimMontage(DeathMontage);
		if (MontageLength > 0.f)
		{
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
	else if (!bDead && bTookDamage &&CurrentHealth > 0.f &&HitReactMontage)
	{
		PlayAnimMontage(HitReactMontage);
	}
	PreviousHealth = CurrentHealth;
}

void AEnemyCharacter::ApplyParryStagger(AActor* ParryingActor)
{
	if (!HealthComponent || HealthComponent->GetCurrentHealth() <= 0.f)
	{
		return;
	}

	bParryStaggered = true;
	if (Controller)
	{
		Controller->StopMovement();
	}
	if (AAIController* AIController = Cast<AAIController>(Controller))
	{
		if (UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent())
		{
			Blackboard->SetValueAsBool(TEXT("bIsStunned"), true);
		}
	}
	if (CombatComponent)
	{
		CombatComponent->SetCombatEnabled(false);
	}
	if (!GetCharacterMovement()->IsFalling()) GetCharacterMovement()->DisableMovement();
	StopAnimMontage();

	float StaggerDuration = ParryStaggerFallbackDuration;
	if (ParryStaggerMontage)
	{
		const float MontageDuration = PlayAnimMontage(ParryStaggerMontage);
		if (MontageDuration > 0.f)
		{
			StaggerDuration = MontageDuration;
		}
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
	if (AAIController* AIController = Cast<AAIController>(Controller))
	{
		if (UBlackboardComponent* Blackboard = AIController->GetBlackboardComponent())
		{
			Blackboard->SetValueAsBool(TEXT("bIsStunned"), bStunned);
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
