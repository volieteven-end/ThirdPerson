// Fill out your copyright notice in the Description page of Project Settings.


#include "TPCCharacter.h"
#include "../Animation/TPCAnimInstance.h"
#include "../Animation/CombatActionRules.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "../Components/InteractionComponent.h"
#include "../Components/InventoryComponent.h"
#include "../Components/HealthComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/ActionComponent.h"
#include "GameFramework/GameModeBase.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "../GameMode/TPCGameMode.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "../Components/StaminaComponent.h"
#include "../Components/EquipmentComponent.h"
#include "../Components/LevelComponent.h"
#include "../Save/TPCSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "../Items/ItemDefinition.h"
#include "TPCPlayerController.h"
#include "../World/DoorActor.h"
#include "EngineUtils.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "../AI/EnemyCharacter.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier.h"

namespace
{
	FVector GetLockTargetPoint(const AActor* Actor)
	{
		if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Actor))
		{
			return Enemy->GetLockOnAimPoint();
		}
		FVector Origin;
		FVector Extent;
		Actor->GetActorBounds(true, Origin, Extent);
		return Origin;
	}
}
// Sets default values
ATPCCharacter::ATPCCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw=false;
	// 角色朝移动方向转身，而不是直接跟随鼠标旋转
	GetCharacterMovement()->bOrientRotationToMovement=true;
	GetCharacterMovement()->RotationRate=FRotator(0.0f,500.f,0.f);
	GetCharacterMovement()->MaxWalkSpeed=450.f;
	GetCharacterMovement()->JumpZVelocity=650.f;
	GetCharacterMovement()->AirControl=0.35f;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 100.f;
	// 弹簧臂：负责第三人称镜头距离、旋转和防穿墙
	CameraBoom=CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength=400.f;
	CameraBoom->bUsePawnControlRotation=true;
	// 相机挂在弹簧臂末端
	FollowCamera=CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom,USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation=false;
	//创建交互组件
	InteractionComponent=CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	InventoryComponent=CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	HealthComponent=CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	CombatComponent=CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	ActionComponent=CreateDefaultSubobject<UActionComponent>(TEXT("ActionComponent"));
	StaminaComponent = CreateDefaultSubobject<UStaminaComponent>(TEXT("StaminaComponent"));
	EquipmentComponent = CreateDefaultSubobject<UEquipmentComponent>(TEXT("EquipmentComponent"));
	LevelComponent = CreateDefaultSubobject<ULevelComponent>(TEXT("LevelComponent"));
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(
		TEXT("MotionWarpingComponent"));
	AIStimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(
		TEXT("AIStimuliSource"));
	AIStimuliSource->bAutoRegister = true;
	AIStimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
} 	

// Called when the game starts or when spawned
void ATPCCharacter::BeginPlay()
{
	Super::BeginPlay();
	StandingCameraTargetOffset = CameraBoom
		? CameraBoom->TargetOffset
		: FVector::ZeroVector;

	if (HealthComponent)
	{
		PreviousHealth = HealthComponent->GetCurrentHealth();

		HealthComponent->OnDeath.AddDynamic(
			this,
			&ThisClass::HandleDeath);

		HealthComponent->OnHealthChanged.AddDynamic(
			this,
			&ThisClass::HandleHealthChanged);
	}
	if (CombatComponent)
	{
		CombatComponent->OnMeleeAttackStarted.AddUObject(
			this,
			&ThisClass::BeginAttackTargetAssist);
	}
	const FString SaveSlotName = TEXT("PlayerSave");

	if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		if (UTPCSaveGame* SaveGame =Cast<UTPCSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName,0)))
		{
			SetActorTransform(SaveGame->PlayerTransform,false,nullptr,ETeleportType::TeleportPhysics);

			if (LevelComponent)
			{
				TArray<ELevelUpgradeType> SavedUpgrades;
				for (const uint8 SavedUpgrade : SaveGame->PlayerUpgrades)
				{
					if (SavedUpgrade <= static_cast<uint8>(ELevelUpgradeType::IronSkin))
					{
						SavedUpgrades.Add(static_cast<ELevelUpgradeType>(SavedUpgrade));
					}
				}
				LevelComponent->RestoreProgress(
					SaveGame->PlayerLevel,
					SaveGame->PlayerExperience,
					SavedUpgrades);
			}

			if (HealthComponent)
			{
				HealthComponent->SetCurrentHealth(SaveGame->PlayerHealth);
			}
			if (StaminaComponent)
			{
				StaminaComponent->SetCurrentStamina(SaveGame->PlayerStamina);
			}
			if (InventoryComponent)
			{
				InventoryComponent->Slots.Reset();

				for (const FSaveInventorySlot& SavedSlot :SaveGame->InventorySlots)
				{
					if (SavedSlot.Count <= 0)
					{
						continue;
					}
					UItemDefinition* ItemDefinition =SavedSlot.ItemDefinition.LoadSynchronous();
					if (ItemDefinition)
					{
						InventoryComponent->AddItem(ItemDefinition,SavedSlot.Count);
					}
				}
			}
			if (ATPCGameMode* GameMode =Cast<ATPCGameMode>(GetWorld()->GetAuthGameMode()))
			{
				GameMode->RestoreEnemiesDefeated(SaveGame->EnemiesDefeated);
			}
			TWeakObjectPtr<ATPCCharacter> WeakCharacter(this);
			TWeakObjectPtr<UTPCSaveGame> WeakSaveGame(SaveGame);
			GetWorldTimerManager().SetTimerForNextTick([WeakCharacter, WeakSaveGame]()
				{
					if (WeakCharacter.IsValid() &&WeakSaveGame.IsValid())
					{
						WeakCharacter->RestoreDoorStates(WeakSaveGame.Get());
					}
				});
			UE_LOG(LogTemp, Warning, TEXT("Checkpoint loaded"));
		}
	}
	if (!DefaultMappingContext)
	{
		return;
	}

	APlayerController* PlayerController =
		Cast<APlayerController>(Controller);

	if (!PlayerController)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* SubSystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
			PlayerController->GetLocalPlayer());

	if (SubSystem)
	{
		SubSystem->AddMappingContext(DefaultMappingContext, 0);
	}

}



// Called to bind functionality to input
void ATPCCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* Input=CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);
	Input->BindAction(MoveAction, ETriggerEvent::Completed, this, &ThisClass::ClearMoveInput);
	Input->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ThisClass::ClearMoveInput);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
	Input->BindAction(LookAction, ETriggerEvent::Completed, this, &ThisClass::FinishLookInput);
	Input->BindAction(SprintAction, ETriggerEvent::Started, this, &ThisClass::StartSprintOrDodgeInput);
	Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &ThisClass::FinishSprintOrDodgeInput);
	Input->BindAction(SprintAction, ETriggerEvent::Canceled, this, &ThisClass::CancelSprintOrDodgeInput);
	Input->BindAction(DashAction,ETriggerEvent::Started,this,&ThisClass::Dash);
	Input->BindAction(InventoryAction,ETriggerEvent::Started,this,&ThisClass::ToggleInventory);
	Input->BindAction(PauseAction,ETriggerEvent::Started,this,&ThisClass::TogglePauseMenu);
	if (JumpAction)
	{
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ThisClass::StartJump);
		Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ThisClass::EndJump);
	}
	if (LockOnAction)
	{
		Input->BindAction(LockOnAction, ETriggerEvent::Started, this, &ThisClass::ToggleLockOn);
	}
	if (InteractionComponent)
	{
		Input->BindAction(InteractAction,ETriggerEvent::Started,InteractionComponent.Get(),&UInteractionComponent::TryInteract);
	}
    if (BuffAction) Input->BindAction(BuffAction, ETriggerEvent::Started, this, &ThisClass::HandleBuff);
    if (ToggleWeaponAction) Input->BindAction(ToggleWeaponAction, ETriggerEvent::Started, this, &ThisClass::HandleToggleWeapon);
	if (AirDiveAction)
	{
		Input->BindAction(AirDiveAction, ETriggerEvent::Started, this, &ThisClass::HandleAirDiveAttack);
	}
	if (CombatComponent && AttackAction)
	{
		Input->BindAction(AttackAction,ETriggerEvent::Started,this,&ThisClass::HandlePrimaryAttack);
	}
	if (CrouchAction)
	{
		Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &ThisClass::StartCrouch);
		Input->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ThisClass::StopCrouch);
		Input->BindAction(CrouchAction, ETriggerEvent::Canceled, this, &ThisClass::StopCrouch);
	}
	if (BlockAction)
	{
		Input->BindAction(BlockAction, ETriggerEvent::Started, this, &ThisClass::StartBlock);
		Input->BindAction(BlockAction, ETriggerEvent::Completed, this, &ThisClass::StopBlock);
		Input->BindAction(BlockAction, ETriggerEvent::Canceled, this, &ThisClass::StopBlock);
	}
	if (InventoryComponent && UseItemAction)
	{
		Input->BindAction(UseItemAction,ETriggerEvent::Started,InventoryComponent.Get(),&UInventoryComponent::TryUseFirstConsumable);
	}
}

void ATPCCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
    const bool bStartingToMove = LastMoveInputAxis.IsNearlyZero() && !Axis.IsNearlyZero();
	LastMoveInputAxis = Axis.GetSafeNormal();
    // Give a stationary entry/out pose a few frames to blend to actual footwork.
    // Moving crouch bypasses the stationary entry altogether in the AnimGraph.
    if (bStartingToMove && GetWorld() && GetMesh() && GetMesh()->GetAnimInstance())
    {
        auto* Anim = GetMesh()->GetAnimInstance();
        const FName State = Anim->GetCurrentStateName(Anim->GetStateMachineIndex(TEXT("Locomotion")));
        if (State == TEXT("CrouchIn") || State == TEXT("CrouchOut") || State == TEXT("JumpEnd"))
            LocomotionInputResumeAt = FMath::Max(LocomotionInputResumeAt, GetWorld()->GetTimeSeconds() + .06);
    }
	if (!LastMoveInputAxis.IsNearlyZero() && IsTurningInPlace()) { CancelMotionAction(); }
	if (!Controller || IsMovementInputLocked() || IsLocomotionInputPaused())
	{
		return;
	}

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.f, ControlRotation.Yaw, 0.f);

	const FVector Forward =FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

	const FVector Right =FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.X);
	AddMovementInput(Right, Axis.Y);
}

void ATPCCharacter::ClearMoveInput()
{
	LastMoveInputAxis = FVector2D::ZeroVector;
}

void ATPCCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (LockedTarget)
	{
		if (!bTargetSwitchLatched)
		{
			AccumulatedSwitchInput += Axis.X;
			if (FMath::Abs(AccumulatedSwitchInput) >= TargetSwitchMouseThreshold)
			{
				SwitchLockTarget(AccumulatedSwitchInput < 0.f ? -1 : 1);
				AccumulatedSwitchInput = 0.f;
				bTargetSwitchLatched = true;
			}
		}
		return;
	}

	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void ATPCCharacter::FinishLookInput()
{
	AccumulatedSwitchInput = 0.f;
	bTargetSwitchLatched = false;
}
void ATPCCharacter::StartSprintOrDodgeInput()
{
    if (bActionDead || !GetWorld()) return;
    bSprintOrDodgeHeld = true;
    SprintOrDodgePressedAt = GetWorld()->GetTimeSeconds();
}

void ATPCCharacter::FinishSprintOrDodgeInput()
{
    const bool bTap = bSprintOrDodgeHeld && GetWorld() &&
        GetWorld()->GetTimeSeconds() - SprintOrDodgePressedAt < SprintHoldThreshold;
    CancelSprintOrDodgeInput();
    if (bTap) Dash();
}

void ATPCCharacter::CancelSprintOrDodgeInput()
{
    bSprintOrDodgeHeld = false;
    StopSprint();
}

void ATPCCharacter::UpdateHeldSprint()
{
    if (!bSprintOrDodgeHeld || !GetWorld()) return;
    if (bActionDead) { CancelSprintOrDodgeInput(); return; }
    if (GetWorld()->GetTimeSeconds() - SprintOrDodgePressedAt < SprintHoldThreshold) return;
    if (!HasMovementIntent() || bIsCrouched || bCrouchInputHeld || GetCharacterMovement()->IsFalling() ||
        IsMovementInputLocked() || IsLocomotionInputPaused()) { StopSprint(); return; }
    StartSprint();
}

bool ATPCCharacter::IsLocomotionInputPaused() const
{
    return GetWorld() && GetWorld()->GetTimeSeconds() < LocomotionInputResumeAt &&
        GetCharacterMovement()->IsMovingOnGround() && !IsMovementInputLocked();
}

bool ATPCCharacter::CanBlendOutOfLanding() const
{
    return GetWorld() && GetWorld()->GetTimeSeconds() >= LandingBlendReadyAt;
}

void ATPCCharacter::StartSprint()
{
	if (IsMovementInputLocked() || !StaminaComponent || StaminaComponent->GetCurrentStamina() <= 0.f) { StopSprint(); return; }
    if (LockedTarget && Controller)
    {
        const FRotationMatrix ViewYaw(FRotator(0.f,Controller->GetControlRotation().Yaw,0.f));
        const FVector Direction=(ViewYaw.GetUnitAxis(EAxis::X)*LastMoveInputAxis.X + ViewYaw.GetUnitAxis(EAxis::Y)*LastMoveInputAxis.Y).GetSafeNormal2D();
        if (FVector::DotProduct(Direction,GetActorForwardVector())<.7f) { StopSprint(); return; }
    }
	if (IsTurningInPlace()) { CancelMotionAction(); }
	if (!bIsSprinting) { PreSprintMaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeed; }
	bIsSprinting = true;
	GetCharacterMovement()->MaxWalkSpeed = SprintMaxWalkSpeed;
}


void ATPCCharacter::StopSprint()
{
	if (bIsSprinting) { GetCharacterMovement()->MaxWalkSpeed = PreSprintMaxWalkSpeed; }
	bIsSprinting = false;
}


void ATPCCharacter::HandleDeath()
{
	if (bActionDead) { return; }
	bActionDead = true;
	if (ActionComponent) ActionComponent->EnterDead();
	bAttackTargetAssistActive = false;
	GetWorldTimerManager().ClearTimer(HitMovementLockTimerHandle);
	CancelMotionAction(0.05f);
	if (CombatComponent) { CombatComponent->SetCombatEnabled(false); }
    CancelSprintOrDodgeInput();
	ConsumeMovementInputVector();
	ApplyActionRotationPolicy();
	UE_LOG(LogTemp, Warning, TEXT("Player died"));
	DisableInput(Cast<APlayerController>(Controller));
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	float RespawnDelay = 2.f;
	if (DeathMontage)
	{
		const float MontageLength =PlayAnimMontage(DeathMontage);
		if (MontageLength > 0.f)
		{
			RespawnDelay =MontageLength + RespawnDelayAfterDeath;
		}
	}
	GetWorldTimerManager().SetTimer(RespawnTimerHandle,this,&ThisClass::FinishDeathPresentation,RespawnDelay,false);
}
void ATPCCharacter::FinishDeathPresentation()
{
    if (!bActionDead) return;
    bDeathPresentationReady = true;
    if (auto* PC = Cast<ATPCPlayerController>(Controller)) PC->ShowDeathScreen();
}

void ATPCCharacter::RestartAfterDeath()
{
    if (bActionDead && bDeathPresentationReady) RespawnPlayer();
}

void ATPCCharacter::RespawnPlayer()
{
	AController* RespawnController = Controller;

	if (!RespawnController)
	{
		return;
	}

	if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
	{
		RespawnController->UnPossess(); // 不再控制死亡的旧角色
		Destroy();                      // 删除旧角色，避免它继续跌落

		GameMode->RestartPlayer(RespawnController);

		if (APawn* NewPawn = RespawnController->GetPawn())
		{
            if (auto* P = Cast<ATPCCharacter>(NewPawn))
            {
                P->HealthComponent->SetCurrentHealth(P->HealthComponent->GetMaxHealth());
                P->StaminaComponent->SetCurrentStamina(P->StaminaComponent->GetMaxStamina());
                P->ActionComponent->ClearInputBuffers();
                P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
                P->GetCharacterMovement()->StopMovementImmediately();
                if (AActor* Start = GameMode->FindPlayerStart(RespawnController))
                    P->SetActorLocationAndRotation(Start->GetActorLocation(), Start->GetActorRotation(), false, nullptr, ETeleportType::TeleportPhysics);
                if (auto* PC = Cast<ATPCPlayerController>(RespawnController))
                { PC->SetViewTarget(P); PC->RestoreGameplayInput(); }
            }
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("New pawn: %s, Location: %s"),
				*NewPawn->GetName(),
				*NewPawn->GetActorLocation().ToString());
		}
	}
}
void ATPCCharacter::HandleHealthChanged(float CurrentHealth,float MaxHealth)
{
	const bool bTookDamage =CurrentHealth < PreviousHealth;

	if (bTookDamage && CurrentHealth > 0.f)
	{
		UAnimSequenceBase* HitAnimation = HitReactFront;
		if (HealthComponent && HealthComponent->GetLastCombatHitResult().bGuardBroken && BlockBreachReact)
        {
            HitAnimation = BlockBreachReact;
        }
        else if (HealthComponent && HealthComponent->GetLastCombatHitResult().bBlocked && BlockHitReact)
		{
			HitAnimation = BlockHitReact;
		}
		else if (GetCharacterMovement() &&
			GetCharacterMovement()->IsFalling() && HitReactAir)
		{
			HitAnimation = HitReactAir;
		}
		else if (HealthComponent)
		{
			if (const AActor* DamageSource = HealthComponent->GetLastDamageSource())
			{
				const FVector ToSource =
					(DamageSource->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
				const float ForwardDot = FVector::DotProduct(GetActorForwardVector(), ToSource);
				const float RightDot = FVector::DotProduct(GetActorRightVector(), ToSource);
				if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
				{
					HitAnimation = ForwardDot >= 0.f ? HitReactFront : HitReactBack;
				}
				else
				{
					HitAnimation = RightDot >= 0.f ? HitReactRight : HitReactLeft;
				}
			}
		}
		float ReactionDuration = HitMovementLockDuration;
		if (HitAnimation)
		{
			ReactionDuration = FMath::Max(ReactionDuration, HitAnimation->GetPlayLength());
		}
		LockMovementForHit(ReactionDuration);
		if (HitAnimation && GetMesh() && GetMesh()->GetAnimInstance())
		{
			GetMesh()->GetAnimInstance()->PlaySlotAnimationAsDynamicMontage(
				HitAnimation, TEXT("DefaultSlot"), 0.08f, 0.15f);
		}
		else if (HitReactMontage)
		{
			PlayAnimMontage(HitReactMontage);
		}
	}

	PreviousHealth = CurrentHealth;
}
void ATPCCharacter::Dash()
{
	if (ActionComponent && !ActionComponent->AuthorizeOrBuffer(ETPCActionIntent::Dodge)) return;
    // Attacks still lock WASD, but no longer lock the dedicated dash action.
    if (TPCActionRules::BlocksDodge(bActionDead, bMovementLockedByHit,
        CombatComponent && CombatComponent->IsBlocking(), IsDashing()))
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float CurrentTime =World->GetTimeSeconds();
	if (CurrentTime - LastDashTime < DashCooldown)
	{
		if (ActionComponent) ActionComponent->BufferIntent(ETPCActionIntent::Dodge);
		return;
	}
	FVector DashDirection = FVector::ZeroVector;
	if (Controller && !LastMoveInputAxis.IsNearlyZero())
	{
		const FRotator ControlRotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, ControlRotation.Yaw, 0.f);
		const FVector Forward =
			FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector Right =
			FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		DashDirection =
			(Forward * LastMoveInputAxis.X + Right * LastMoveInputAxis.Y)
			.GetSafeNormal2D();
	}

	if (DashDirection.IsNearlyZero())
	{
		DashDirection =GetActorForwardVector().GetSafeNormal2D();
	}
	

	// Root-motion dodges move relative to the character, so classify the desired
	// world-space direction in the character's local forward/right frame. This
	// also remains correct when lock-on keeps the character facing the target.
	const float LocalForward = FVector::DotProduct(
		DashDirection, GetActorForwardVector().GetSafeNormal2D());
	const float LocalRight = FVector::DotProduct(
		DashDirection, GetActorRightVector().GetSafeNormal2D());

	UAnimMontage* SelectedDashMontage = nullptr;
	if (FMath::Abs(LocalForward) >= FMath::Abs(LocalRight))
	{
		SelectedDashMontage = LocalForward >= 0.f
			? DashForwardMontage.Get()
			: DashBackwardMontage.Get();
	}
	else
	{
		SelectedDashMontage = LocalRight >= 0.f
			? DashRightMontage.Get()
			: DashLeftMontage.Get();
	}

    const UActionSet* SwordSet = ActionComponent ? ActionComponent->GetActionSet() : nullptr;
    const int32 DirectionIndex = FMath::Abs(LocalForward) >= FMath::Abs(LocalRight) ? (LocalForward >= 0.f ? 0 : 1) : (LocalRight >= 0.f ? 3 : 2);
    const UActionDefinition* DodgeDefinition = SwordSet && SwordSet->Dodges.IsValidIndex(DirectionIndex) ? SwordSet->Dodges[DirectionIndex].Get() : nullptr;
    if (DodgeDefinition) SelectedDashMontage = DodgeDefinition->Montage;
    const float Cost = DodgeDefinition ? DodgeDefinition->StaminaCost : DashCost;
    if (!StaminaComponent || StaminaComponent->GetCurrentStamina() < Cost) return;
	if (!SelectedDashMontage)
	{
		SelectedDashMontage = DashMontage.Get();
	}

	if (!SelectedDashMontage || !SelectedDashMontage->HasRootMotion())
	{
		UE_LOG(LogTemp, Warning, TEXT("Dash needs a root-motion montage in BP_TPCCharacter."));
		return;
	}
    // A rejected dash (cooldown, stamina, missing montage/AnimInstance) must not cancel a valid attack.
    if (!GetMesh() || !GetMesh()->GetAnimInstance()) { return; }
    if (IsTurningInPlace()) { CancelMotionAction(); }
    if (CombatComponent) { CombatComponent->CancelAttackForDash(AttackDashCancelBlendOut); }
    ClearAttackRootMotionForDodge();
	if (StartMotionMontage(SelectedDashMontage, ETPCMotionAction::Dodge))
	{
		if (!StaminaComponent->TryConsume(Cost)) { CancelMotionAction(); return; }
		LastDashTime = CurrentTime;
		StopGroundInputMomentum();
		if (ActionComponent && !ActionComponent->GetActiveDefinition())
            ActionComponent->SetFallbackInvulnerability(DashInvulnerabilityDuration);
	}
}
void ATPCCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    UpdateHeldSprint();
    if (IsLocomotionInputPaused())
    {
        ConsumeMovementInputVector();
        GetCharacterMovement()->StopMovementImmediately();
    }
	UpdateMotionAction();
	ApplyActionRotationPolicy();
	if (IsMovementInputLocked()) { StopSprint(); ConsumeMovementInputVector(); }
	UpdateLockOn(DeltaTime);
	UpdateAttackTargetAssist(DeltaTime);
	if (CameraBoom)
	{
		FVector DesiredTargetOffset = StandingCameraTargetOffset;
		if (bIsCrouched)
		{
			DesiredTargetOffset.Z +=
				ActiveCrouchHeightAdjustment - CrouchCameraDrop;
		}
		CameraBoom->TargetOffset = FMath::VInterpTo(
			CameraBoom->TargetOffset,
			DesiredTargetOffset,
			DeltaTime,
			CrouchCameraBlendSpeed);
	}

    if (bIsSprinting && LockedTarget && GetVelocity().SizeSquared2D() > 1.f &&
        FVector::DotProduct(GetVelocity().GetSafeNormal2D(), GetActorForwardVector()) < .7f) StopSprint();
	if (!bIsSprinting || !StaminaComponent)
	{
		return;
	}

	const bool bIsActuallyMoving =GetVelocity().SizeSquared2D() > 1.f;
	if (!bIsActuallyMoving)
	{
		return;
	}
	const float CostThisFrame =SprintStaminaCostPerSecond * DeltaTime;
	if (!StaminaComponent->TryConsume(CostThisFrame))
	{
		StopSprint();
	}
}
void ATPCCharacter::ToggleInventory()
{
	if (ATPCPlayerController* PlayerController =Cast<ATPCPlayerController>(Controller))
	{
		PlayerController->ToggleInventory();
	}
}
void ATPCCharacter::TogglePauseMenu()
{
	if (ATPCPlayerController* PlayerController =Cast<ATPCPlayerController>(Controller))
	{
		PlayerController->TogglePauseMenu();
	}
}

void ATPCCharacter::SetDoubleJumpUnlocked(bool bUnlocked)
{
    bDoubleJumpUnlocked = bUnlocked;
    JumpMaxCount = bUnlocked ? 2 : 1;
}

void ATPCCharacter::OnJumped_Implementation()
{
    Super::OnJumped_Implementation();
    LocomotionInputResumeAt = LandingBlendReadyAt = 0.;
    if (JumpCurrentCount < 2 || !ActionComponent) return;
    const auto* Set = ActionComponent->GetActionSet();
    if (Set && Set->DoubleJumpMontage) PlayAnimMontage(Set->DoubleJumpMontage);
}

void ATPCCharacter::StartJump()
{
    const auto* Set = ActionComponent ? ActionComponent->GetActionSet() : nullptr;
    JumpMaxCount = bDoubleJumpUnlocked || (Set && Set->bAllowDoubleJump) ? 2 : 1;
	if (ActionComponent && !ActionComponent->AuthorizeOrBuffer(ETPCActionIntent::Jump)) return;
	if (IsMovementInputLocked())
	{
		return;
	}
	if (IsTurningInPlace()) { CancelMotionAction(); }
	if (!CanJump()) { if (ActionComponent) ActionComponent->BufferIntent(ETPCActionIntent::Jump, true); return; }
	Jump();
}

void ATPCCharacter::EndJump()
{
	StopJumping();
}

void ATPCCharacter::StartCrouch()
{
	if (IsMovementInputLocked()) { return; }
	if (IsTurningInPlace()) { CancelMotionAction(); }
	bCrouchInputHeld = true;
	if (GetCharacterMovement() && !GetCharacterMovement()->IsFalling())
	{
		Crouch();
	}
}

void ATPCCharacter::OnStartCrouch(
	float HalfHeightAdjust,
	float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	ActiveCrouchHeightAdjustment = ScaledHalfHeightAdjust;
    StopSprint();
    if (GetWorld() && GetVelocity().SizeSquared2D() < 9.f)
        LocomotionInputResumeAt = GetWorld()->GetTimeSeconds() + .08;
	if (CameraBoom)
	{
		// Crouch moves the capsule down immediately. Apply the inverse offset in
		// the same callback, then Tick smoothly lowers only CrouchCameraDrop.
		CameraBoom->TargetOffset.Z += ActiveCrouchHeightAdjustment;
	}
}

void ATPCCharacter::OnEndCrouch(
	float HalfHeightAdjust,
	float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	if (CameraBoom)
	{
		// Standing raises the capsule immediately, so cancel that jump as well.
		CameraBoom->TargetOffset.Z -= ActiveCrouchHeightAdjustment;
	}
	ActiveCrouchHeightAdjustment = 0.f;
}

void ATPCCharacter::StopCrouch()
{
	bCrouchInputHeld = false;
	UnCrouch();
}

void ATPCCharacter::StartBlock()
{
    if (EquipmentComponent && !EquipmentComponent->IsWeaponDrawn()) return;
	if (ActionComponent && !ActionComponent->CanRequest(ETPCActionIntent::Guard)) return;
	if (!CombatComponent || bActionDead || bMovementLockedByHit || IsDashing() ||
		CombatComponent->IsRangedAttackInProgress()) { return; }
	if (IsTurningInPlace()) { CancelMotionAction(); }
	CombatComponent->StartBlock();
	if (CombatComponent->IsBlocking()) { StopGroundInputMomentum(); }
	ApplyActionRotationPolicy();
}


void ATPCCharacter::StopBlock()
{
	if (CombatComponent)
	{
		CombatComponent->StopBlock();
	}
}

void ATPCCharacter::HandlePrimaryAttack()
{
	if (ActionComponent && !ActionComponent->AuthorizeOrBuffer(ETPCActionIntent::PrimaryAttack)) return;
	if (!CombatComponent || !GetCharacterMovement() || bActionDead || bMovementLockedByHit) { return; }
    if (IsDashing())
    {
        CombatComponent->QueueAttackDuringDash();
        return;
    }
	if (IsTurningInPlace()) { CancelMotionAction(); }
    if (EquipmentComponent && EquipmentComponent->GetEquippedWeaponDefinition() && !EquipmentComponent->IsWeaponDrawn())
    { HandleToggleWeapon(); return; } // One press draws; it does not also queue an unseen attack.
	if (CombatComponent->IsMeleeAttackInProgress())
	{
		CombatComponent->TryAttack();
		return;
	}
	if (CombatComponent->TryParryCounter()) { ApplyActionRotationPolicy(); return; }
    if (!GetCharacterMovement()->IsFalling() && !bCrouchInputHeld && !bIsCrouched && CombatComponent->TryResumeComboAfterDash())
    {
        ApplyActionRotationPolicy();
        return;
    }
    CombatComponent->ClearDashCombo();
	if (GetCharacterMovement()->IsFalling()) { CombatComponent->TryAirAttack(); }
	else if (bCrouchInputHeld || bIsCrouched)
	{
		UnCrouch();
		CombatComponent->TryUppercutAttack();
	}
	else if (!bIsSprinting || !CombatComponent->TrySprintAttack()) { CombatComponent->TryAttack(); }
    if (!CombatComponent->IsMeleeAttackInProgress() && GetCharacterMovement()->IsFalling() && ActionComponent)
        ActionComponent->BufferIntent(ETPCActionIntent::PrimaryAttack, true);
	if (CombatComponent->IsRangedAttackInProgress()) { StopGroundInputMomentum(); }
	ApplyActionRotationPolicy();
}

void ATPCCharacter::HandleBuff()
{
    if (!CombatComponent || !ActionComponent || !ActionComponent->CanRequest(ETPCActionIntent::Buff)) return;
    if (IsTurningInPlace()) CancelMotionAction();
    if (CombatComponent->TryBuff()) { StopGroundInputMomentum(); ApplyActionRotationPolicy(); }
}

void ATPCCharacter::HandleToggleWeapon()
{
    if (!CombatComponent || !ActionComponent || !ActionComponent->CanRequest(ETPCActionIntent::ToggleWeapon)) return;
    if (IsTurningInPlace()) CancelMotionAction();
    if (CombatComponent->TryToggleWeapon()) { StopGroundInputMomentum(); ApplyActionRotationPolicy(); }
}

void ATPCCharacter::HandleAirDiveAttack()
{
    if (EquipmentComponent && !EquipmentComponent->IsWeaponDrawn()) return;
	if (ActionComponent && !ActionComponent->CanRequest(ETPCActionIntent::Dive)) return;
    if (!CombatComponent || bActionDead || bMovementLockedByHit || !GetCharacterMovement()->IsFalling()) return;
    if (IsTurningInPlace()) CancelMotionAction();
    CombatComponent->TryAirDiveAttack();
    ApplyActionRotationPolicy();
}

void ATPCCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (CombatComponent) { CombatComponent->HandleOwnerLanded(); }
    if (GetWorld() && !IsMovementInputLocked())
    {
        const double Time = GetWorld()->GetTimeSeconds();
        LocomotionInputResumeAt = Time + LandingContactTime;
        LandingBlendReadyAt = Time + FMath::Max(0.f, LandingContactTime - .06f);
        ConsumeMovementInputVector();
        GetCharacterMovement()->StopMovementImmediately();
    }
}


void ATPCCharacter::FaceAttackDirection()
{
	FVector DesiredDirection = FVector::ZeroVector;
	if (LockedTarget && IsValidLockTarget(LockedTarget, false))
	{
		DesiredDirection = LockedTarget->GetActorLocation() - GetActorLocation();
	}
	else if (Controller)
	{
		DesiredDirection = FRotationMatrix(
			FRotator(0.f, Controller->GetControlRotation().Yaw, 0.f))
			.GetUnitAxis(EAxis::X);
	}

	DesiredDirection.Z = 0.f;
	if (DesiredDirection.Normalize())
	{
		AttackFacingYaw = DesiredDirection.Rotation().Yaw;
	}
}

void ATPCCharacter::UpdateAttackWarpTarget()
{
	if (!MotionWarpingComponent)
	{
		return;
	}

	FaceAttackDirection();
	const FVector AttackForward = FRotationMatrix(
		FRotator(0.f, AttackFacingYaw, 0.f)).GetUnitAxis(EAxis::X);
	FVector WarpLocation = GetActorLocation() + AttackForward * FreeAttackWarpDistance;
	if (LockedTarget && IsValidLockTarget(LockedTarget, false))
	{
		FVector TargetToPlayer = GetActorLocation() - LockedTarget->GetActorLocation();
		TargetToPlayer.Z = 0.f;
		const float Distance = TargetToPlayer.Size();
		if (Distance <= AttackMagnetismRange && TargetToPlayer.Normalize())
		{
			const float CapsuleGap = GetCapsuleComponent()->GetScaledCapsuleRadius() +
				LockedTarget->GetCapsuleComponent()->GetScaledCapsuleRadius() + 8.f;
			WarpLocation = LockedTarget->GetActorLocation() +
				TargetToPlayer * FMath::Max(CapsuleGap, AttackMagnetismStopDistance);
			WarpLocation.Z = GetActorLocation().Z;
		}
	}

	// Capsule-centre convention, shared with all sword warp modifiers. Clamp and sweep
	// before publishing a target so even large enemies and intervening walls keep a gap.
	WarpLocation = GetActorLocation() + (WarpLocation - GetActorLocation()).GetClampedToMaxSize(MaxAttackWarpTranslation);
	if (GetWorld() && GetCapsuleComponent())
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(SwordWarpDestination), false, this);
		const FVector Lift(0.f, 0.f, 2.f);
		if (GetWorld()->SweepSingleByChannel(Hit, GetActorLocation() + Lift, WarpLocation + Lift, FQuat::Identity,
			ECC_Pawn, GetCapsuleComponent()->GetCollisionShape(), Query))
		{
			WarpLocation = Hit.bStartPenetrating ? GetActorLocation() :
				FMath::Lerp(GetActorLocation(), WarpLocation, FMath::Max(0.f, Hit.Time - 0.02f));
		}
	}
	MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
		AttackWarpTargetName,
		WarpLocation,
		FRotator(0.f, AttackFacingYaw, 0.f));
}

void ATPCCharacter::BeginAttackTargetAssist()
{
	if (IsTurningInPlace()) { CancelMotionAction(); }
	StopGroundInputMomentum();
	ApplyActionRotationPolicy();
	UpdateAttackWarpTarget();
	if (!GetWorld() || AttackFacingDuration <= 0.f)
	{
		bAttackTargetAssistActive = false;
		return;
	}

	bAttackTargetAssistActive = true;
	AttackTargetAssistEndTime = GetWorld()->GetTimeSeconds() + AttackFacingDuration;
}

void ATPCCharacter::UpdateAttackTargetAssist(float DeltaSeconds)
{
	if (!bAttackTargetAssistActive || !GetWorld() || !CombatComponent ||
		GetWorld()->GetTimeSeconds() >= AttackTargetAssistEndTime ||
		!CombatComponent->IsMeleeAttackInProgress())
	{
		bAttackTargetAssistActive = false;
		return;
	}

	if (ActionComponent && !ActionComponent->IsFacingWindowOpen()) { bAttackTargetAssistActive = false; return; }
	UpdateAttackWarpTarget();

	// A valid notify window owns rotation and translation through root motion.
	// Smoothly rotate only as a compatibility fallback for unconfigured montages.
	const bool bHasMotionWarpingWindow = MotionWarpingComponent &&
		!MotionWarpingComponent->GetModifiers().IsEmpty();
	if (!bHasMotionWarpingWindow)
	{
		const FRotator DesiredRotation(0.f, AttackFacingYaw, 0.f);
		SetActorRotation(FMath::RInterpConstantTo(
			GetActorRotation(),
			DesiredRotation,
			DeltaSeconds,
			AttackFallbackTurnRate));
	}
}

bool ATPCCharacter::IsMovementInputLocked() const
{
	return (ActionComponent && ActionComponent->IsMovementLocked()) || TPCActionRules::LocksMovement(bActionDead, bMovementLockedByHit,
		CombatComponent && CombatComponent->IsBlocking(),
		CombatComponent && (CombatComponent->IsMeleeAttackInProgress() || CombatComponent->IsRangedAttackInProgress()),
		IsDashing());
}


void ATPCCharacter::LockMovementForHit(float AnimationDuration)
{
	bMovementLockedByHit = true;
	bAttackTargetAssistActive = false;
	CancelMotionAction(0.05f);
	if (CombatComponent) { CombatComponent->CancelActiveAttack(0.05f); }
    const bool bGuardHit = HealthComponent && HealthComponent->GetLastCombatHitResult().bBlocked &&
        !HealthComponent->GetLastCombatHitResult().bGuardBroken && CombatComponent && CombatComponent->IsBlocking();
    if (ActionComponent && !bGuardHit) HitActionId = ActionComponent->BeginAction(ETPCActionState::HitReact);
    if (CombatComponent && !bGuardHit) CombatComponent->CancelGuard();
    if (ActionComponent) ActionComponent->ClearInputBuffers();
	StopGroundInputMomentum();
	ApplyActionRotationPolicy();
	GetWorldTimerManager().SetTimer(
		HitMovementLockTimerHandle,
		this,
		&ThisClass::ClearHitMovementLock,
		FMath::Max(0.01f, AnimationDuration),
		false);
}

void ATPCCharacter::ClearHitMovementLock()
{
    bMovementLockedByHit = false;
    if (ActionComponent) ActionComponent->EndAction(HitActionId);
    HitActionId = 0;
	ApplyActionRotationPolicy();
}

bool ATPCCharacter::IsGuardHitReactionActive() const
{
    return bMovementLockedByHit && HealthComponent && HealthComponent->GetLastCombatHitResult().bBlocked &&
        !HealthComponent->GetLastCombatHitResult().bGuardBroken && CombatComponent && CombatComponent->IsBlocking();
}

void ATPCCharacter::ToggleLockOn()
{
	if (LockedTarget)
	{
		SetLockedTarget(nullptr);
		FinishLookInput();
		UE_LOG(LogTemp, Log, TEXT("Lock-on released"));
		return;
	}

	SetLockedTarget(FindInitialLockTarget());
	FinishLookInput();
	if (LockedTarget)
	{
		UE_LOG(LogTemp, Log, TEXT("Locked on: %s"), *LockedTarget->GetName());
	}
}

void ATPCCharacter::UpdateLockOn(float DeltaSeconds)
{
	if (!LockedTarget)
	{
		return;
	}

	if (!IsValidLockTarget(LockedTarget, false))
	{
		SetLockedTarget(nullptr);
		FinishLookInput();
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (!PlayerController || !FollowCamera)
	{
		return;
	}

	FRotator DesiredRotation = (GetLockTargetPoint(LockedTarget) -
		FollowCamera->GetComponentLocation()).Rotation();
	DesiredRotation.Roll = 0.f;
	DesiredRotation.Pitch = FMath::ClampAngle(DesiredRotation.Pitch, -55.f, 35.f);

	const FRotator NewRotation = FMath::RInterpTo(
		PlayerController->GetControlRotation(),
		DesiredRotation,
		DeltaSeconds,
		LockOnRotationSpeed);
	PlayerController->SetControlRotation(NewRotation);

}

AEnemyCharacter* ATPCCharacter::FindInitialLockTarget() const
{
	if (!GetWorld() || !FollowCamera)
	{
		return nullptr;
	}

	AEnemyCharacter* BestTarget = nullptr;
	float BestScore = BIG_NUMBER;
	const FVector CameraLocation = FollowCamera->GetComponentLocation();
	const FVector CameraForward = FollowCamera->GetForwardVector();
	const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(InitialLockOnMaxAngle));

	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AEnemyCharacter* Candidate = *It;
		if (!IsValidLockTarget(Candidate, true))
		{
			continue;
		}

		const FVector ToTarget = GetLockTargetPoint(Candidate) - CameraLocation;
		const float Distance = ToTarget.Size();
		const float ViewDot = FVector::DotProduct(CameraForward, ToTarget.GetSafeNormal());
		if (ViewDot < MinimumDot)
		{
			continue;
		}

		// Prefer the enemy closest to screen centre; distance resolves close ties.
		const float Score = (1.f - ViewDot) * 5000.f + Distance * 0.1f;
		if (Score < BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

void ATPCCharacter::SwitchLockTarget(int32 Direction)
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (!PlayerController || !LockedTarget || Direction == 0 || !GetWorld())
	{
		return;
	}

	FVector2D CurrentScreenPosition;
	if (!PlayerController->ProjectWorldLocationToScreen(
		GetLockTargetPoint(LockedTarget), CurrentScreenPosition, true))
	{
		return;
	}

	AEnemyCharacter* BestTarget = nullptr;
	float BestScore = BIG_NUMBER;
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AEnemyCharacter* Candidate = *It;
		if (Candidate == LockedTarget || !IsValidLockTarget(Candidate, true))
		{
			continue;
		}

		FVector2D CandidateScreenPosition;
		if (!PlayerController->ProjectWorldLocationToScreen(
			GetLockTargetPoint(Candidate), CandidateScreenPosition, true))
		{
			continue;
		}

		const FVector2D ScreenDelta = CandidateScreenPosition - CurrentScreenPosition;
		if ((Direction < 0 && ScreenDelta.X >= -1.f) ||
			(Direction > 0 && ScreenDelta.X <= 1.f))
		{
			continue;
		}

		// Horizontal neighbour is primary; vertical separation and distance break ties.
		const float WorldDistance = FVector::Dist(GetActorLocation(), Candidate->GetActorLocation());
		const float Score = FMath::Abs(ScreenDelta.X) +
			FMath::Abs(ScreenDelta.Y) * 0.35f + WorldDistance * 0.01f;
		if (Score < BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	// No enemy on that side: intentionally keep the current target.
	if (BestTarget)
	{
		SetLockedTarget(BestTarget);
		UE_LOG(LogTemp, Log, TEXT("Switched lock-on to: %s"), *LockedTarget->GetName());
	}
}

void ATPCCharacter::SetLockedTarget(AEnemyCharacter* NewTarget)
{
	if (LockedTarget == NewTarget)
	{
		return;
	}

	if (LockedTarget)
	{
		LockedTarget->SetLockOnIndicatorVisible(false);
	}

	LockedTarget = NewTarget;
	if (IsTurningInPlace()) { CancelMotionAction(); }
	ApplyActionRotationPolicy();
	if (LockedTarget)
	{
		LockedTarget->SetLockOnIndicatorVisible(true);
	}
}

bool ATPCCharacter::IsValidLockTarget(
	const AEnemyCharacter* Candidate,
	bool bRequireLineOfSight) const
{
	if (!Candidate || !IsValid(Candidate) || Candidate->IsActorBeingDestroyed() ||
		FVector::DistSquared(GetActorLocation(), Candidate->GetActorLocation()) >
		FMath::Square(LockOnRadius))
	{
		return false;
	}

	const UHealthComponent* CandidateHealth =
		Candidate->FindComponentByClass<UHealthComponent>();
	if (!CandidateHealth || CandidateHealth->GetCurrentHealth() <= 0.f)
	{
		return false;
	}

	if (!bRequireLineOfSight || !GetWorld() || !FollowCamera)
	{
		return true;
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LockOnVisibility), false, this);
	QueryParams.AddIgnoredActor(this);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		FollowCamera->GetComponentLocation(),
		GetLockTargetPoint(Candidate),
		ECC_Visibility,
		QueryParams);
	return !bHit || Hit.GetActor() == Candidate;
}

void ATPCCharacter::RestoreDoorStates(
	const UTPCSaveGame* SaveGame)
{
	if (!SaveGame || !GetWorld())
	{
		return;
	}

	for (TActorIterator<ADoorActor> It(GetWorld());It;++It)
	{
		ADoorActor* Door = *It;
		if (!Door || Door->GetSaveId().IsNone())
		{
			continue;
		}
		const FSaveDoorState* SavedState =SaveGame->DoorStates.FindByPredicate([Door](const FSaveDoorState& State)
				{
					return State.SaveId == Door->GetSaveId();
				});

		if (SavedState)
		{
			Door->RestoreOpenState(SavedState->bIsOpen);
		}
	}
}


void ATPCCharacter::StopGroundInputMomentum()
{
	StopSprint();
	ConsumeMovementInputVector();
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		// Blocking / attacking in the air stops input, not physical inertia or gravity.
		if (!Movement->IsFalling()) { Movement->StopMovementImmediately(); }
	}
}

void ATPCCharacter::ApplyActionRotationPolicy()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement) { return; }
	const bool bActionOwnsRotation = (ActionComponent && ActionComponent->OwnsRotation()) || bActionDead || bMovementLockedByHit ||
		MotionAction != ETPCMotionAction::None ||
		(CombatComponent && (CombatComponent->IsMeleeAttackInProgress() || CombatComponent->IsRangedAttackInProgress()));
	// One rotation owner: root motion / attack assist during actions, CMC outside actions.
	// DesiredRotation uses RotationRate, avoiding an instant controller-yaw snap on release.
	bUseControllerRotationYaw = false;
	Movement->bUseControllerDesiredRotation = !bActionOwnsRotation && LockedTarget != nullptr;
	Movement->bOrientRotationToMovement = !bActionOwnsRotation && LockedTarget == nullptr;
}

void ATPCCharacter::ClearAttackRootMotionForDodge()
{
    // Accepted full-body dodge takes over from the canceled attack. Removing only the
    // target leaves an already-active warp modifier / this frame's extracted delta alive.
    // Run after cancel callbacks and BEFORE the new montage extracts any root motion.
    bAttackTargetAssistActive = false;
    if (MotionWarpingComponent)
    {
        MotionWarpingComponent->DisableAllRootMotionModifiers();
        MotionWarpingComponent->RemoveWarpTarget(AttackWarpTargetName);
    }
    if (GetMesh()) { GetMesh()->ConsumeRootMotion(); }
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->RootMotionParams.Clear();
    }
}

bool ATPCCharacter::StartMotionMontage(UAnimMontage* Montage, ETPCMotionAction Action)
{
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim || !Montage || !Montage->HasRootMotion() || bActionDead) { return false; }
    ++MotionGeneration;
    if (ActionComponent)
    {
        const UActionSet* Set = ActionComponent->GetActionSet();
        MotionActionId = ActionComponent->BeginAction(Action == ETPCMotionAction::Dodge ? ETPCActionState::Dodge : ETPCActionState::Turn,
            Set ? Set->FindByMontage(Montage) : nullptr);
    }
    MotionAction = Action;
	ActiveMotionMontage = Montage;
	ActiveMotionInstanceId = INDEX_NONE;
	ApplyActionRotationPolicy();
	const auto* Def = ActionComponent ? ActionComponent->GetActiveDefinition() : nullptr;
    if (PlayAnimMontage(Montage, Def ? Def->PlayRate : 1.f, Def ? Def->EntrySection : NAME_None) <= 0.f)
	{
		CancelMotionAction();
		return false;
	}
	if (FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage))
	{
		ActiveMotionInstanceId = Instance->GetInstanceID();
    }
    if (ActionComponent) ActionComponent->BindMontage(MotionActionId, Montage, ActiveMotionInstanceId);
	FOnMontageEnded End;
	End.BindUObject(this, &ThisClass::HandleMotionMontageEnded, MotionGeneration);
	Anim->Montage_SetEndDelegate(End, Montage);
	FOnMontageBlendingOutStarted Blend;
	Blend.BindUObject(this, &ThisClass::HandleMotionBlendingOut, MotionGeneration);
	Anim->Montage_SetBlendingOutDelegate(Blend, Montage);
	return true;
}

void ATPCCharacter::CancelMotionAction(float BlendOutTime, bool bInterrupted)
{
    if (ActionComponent) ActionComponent->EndAction(MotionActionId);
    MotionActionId = 0;
	UAnimMontage* Montage = ActiveMotionMontage.Get();
	const bool bWasTurning = IsTurningInPlace();
	const bool bWasDashing = IsDashing();
	++MotionGeneration;
	MotionAction = ETPCMotionAction::None;
	ActiveMotionMontage.Reset();
	ActiveMotionInstanceId = INDEX_NONE;
	if (bWasTurning && GetWorld())
	{
		const UTPCAnimInstance* Anim = GetMesh() ? Cast<UTPCAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr;
		NextTurnAllowedTime = GetWorld()->GetTimeSeconds() + (Anim ? Anim->TurnFinishHoldTime : 0.12f);
	}
	if (Montage && GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Stop(FMath::Max(0.f, BlendOutTime), Montage);
	}
	ApplyActionRotationPolicy();
    if (bWasDashing && CombatComponent && CombatComponent->FinishDashForCombo(bInterrupted, DashComboContinueWindow))
    {
        // The montage/generation has been cleared above: one buffered click now starts one next stage.
        HandlePrimaryAttack();
    }
}

void ATPCCharacter::HandleMotionMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint64 Generation)
{
	if (Generation != MotionGeneration || Montage != ActiveMotionMontage.Get()) { return; }
	// Rotation is already extracted from the montage. Do not add 90/180 or snap to view yaw.
	CancelMotionAction(0.f, bInterrupted);
}

void ATPCCharacter::HandleMotionBlendingOut(UAnimMontage* Montage, bool bInterrupted, uint64 Generation)
{
	if (bInterrupted && Generation == MotionGeneration && Montage == ActiveMotionMontage.Get())
	{
		CancelMotionAction(0.f);
	}
}

void ATPCCharacter::UpdateMotionAction()
{
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (MotionAction != ETPCMotionAction::None)
	{
		// Covers mesh / AnimInstance replacement even when the original delegate is lost.
		if (!Anim || !Anim->GetMontageInstanceForID(ActiveMotionInstanceId)) { CancelMotionAction(); }
		return;
	}
	if (!bEnableRootMotionTurn || IsMovementInputLocked() || LockedTarget || !Controller || !GetWorld() ||
		GetWorld()->GetTimeSeconds() < NextTurnAllowedTime || bIsCrouched || !GetCharacterMovement()->IsMovingOnGround() ||
		GetVelocity().SizeSquared2D() > 9.f || !LastMoveInputAxis.IsNearlyZero() || !GetPendingMovementInputVector().IsNearlyZero()) { return; }
	const UTPCAnimInstance* TPCAnim = Cast<UTPCAnimInstance>(Anim);
	const float Threshold90 = TPCAnim ? TPCAnim->Turn90Threshold : 55.f;
	const float Threshold180 = TPCAnim ? TPCAnim->Turn180Threshold : 135.f;
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, GetControlRotation().Yaw);
	UAnimMontage* Montage = nullptr;
	if (FMath::Abs(DeltaYaw) >= Threshold180)
	{
		Montage = DeltaYaw < 0.f ? TurnLeft180Montage.Get() : TurnRight180Montage.Get();
	}
	else if (FMath::Abs(DeltaYaw) >= Threshold90)
	{
		Montage = DeltaYaw < 0.f ? TurnLeft90Montage.Get() : TurnRight90Montage.Get();
	}
    if (Montage && ActionComponent)
    {
        const auto* Set = ActionComponent->GetActionSet();
        const int32 TurnIndex = FMath::Abs(DeltaYaw) >= Threshold180 ? (DeltaYaw < 0.f ? 2 : 3) : (DeltaYaw < 0.f ? 0 : 1);
        if (Set && Set->Turns.IsValidIndex(TurnIndex) && Set->Turns[TurnIndex]) Montage = Set->Turns[TurnIndex]->Montage;
    }
	if (Montage && !StartMotionMontage(Montage, ETPCMotionAction::Turn))
	{
		NextTurnAllowedTime = GetWorld()->GetTimeSeconds() + 0.5f;
	}
}
