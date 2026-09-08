// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TPCCharacter.generated.h"
class UTPCSaveGame;
class UAnimMontage;
class UAnimSequenceBase;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UInteractionComponent;
class UInventoryComponent;
class UHealthComponent;
class UCombatComponent;	
class UActionComponent;
class UStaminaComponent;
class UEquipmentComponent;
class ULevelComponent;
class UAIPerceptionStimuliSourceComponent;
class UMotionWarpingComponent;
class AEnemyCharacter;
struct FInputActionValue;

enum class ETPCMotionAction : uint8 { None, Dodge, Turn };

UCLASS()
class THIRDPERSON_API ATPCCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ATPCCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    UFUNCTION(BlueprintCallable, Category="Death") void RestartAfterDeath();
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void Landed(const FHitResult& Hit) override;
    virtual void OnJumped_Implementation() override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	UFUNCTION()
	void HandleDeath();

	void RespawnPlayer();
    void FinishDeathPresentation();
    bool bDeathPresentationReady = false;

	FTimerHandle RespawnTimerHandle;
public:	

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FollowCamera;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Components")
	TObjectPtr<UInteractionComponent> InteractionComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInventoryComponent> InventoryComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHealthComponent> HealthComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCombatComponent> CombatComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UActionComponent> ActionComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaminaComponent> StaminaComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UEquipmentComponent> EquipmentComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULevelComponent> LevelComponent;
	/** Applies montage-authored root-motion warp windows to attack targets. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;
	/** Explicitly registers the player as a sight stimulus for enemy AI. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAIPerceptionStimuliSourceComponent> AIStimuliSource;
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;
	/** Optional separate dive action. Choose the physical key in IMC_Default. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> AirDiveAction;
    UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> BuffAction;
    UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> ToggleWeaponAction;
    UFUNCTION(BlueprintCallable, Category="Combat") void HandleBuff();
    UFUNCTION(BlueprintCallable, Category="Equipment") void HandleToggleWeapon();
    UFUNCTION(BlueprintCallable, Category="Abilities") void SetDoubleJumpUnlocked(bool bUnlocked);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Abilities") bool bDoubleJumpUnlocked = false;
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;
	
	UPROPERTY(EditDefaultsOnly,Category = "Input")
	TObjectPtr<UInputAction> InteractAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> DashAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> UseItemAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InventoryAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> PauseAction;

	/** Middle mouse toggles the current enemy lock. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LockOnAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;
	/** Hold to crouch. Assign IA_Crouch in BP_TPCCharacter. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CrouchAction;
	/** Hold to block. Assign IA_Block in BP_TPCCharacter. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> BlockAction;

	/** Enemy followed by character facing / attack assist. The camera always remains free. */
	UPROPERTY(BlueprintReadOnly, Category = "Lock On")
	TObjectPtr<AEnemyCharacter> LockedTarget;

	UPROPERTY(EditDefaultsOnly, Category = "Lock On", meta = (ClampMin = "100.0"))
	float LockOnRadius = 2200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Lock On", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float InitialLockOnMaxAngle = 70.f;

	/** Character-facing interpolation speed; never rotates the player's camera. */
	UPROPERTY(EditDefaultsOnly, Category = "Lock On", meta = (ClampMin = "0.1"))
	float LockOnRotationSpeed = 10.f;

	/** Hold Alt and move the mouse horizontally to deliberately switch one target. */
	UPROPERTY(EditDefaultsOnly, Category = "Lock On", meta = (ClampMin = "0.1"))
	float TargetSwitchMouseThreshold = 2.5f;

	/** Fallback duration for attacks whose montage has no Motion Warping window. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Attack Assist", meta = (ClampMin = "0.0"))
	float AttackFacingDuration = 0.35f;

	/** Must match the Warp Target Name authored in every melee montage. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Attack Assist")
	FName AttackWarpTargetName = TEXT("AttackTarget");

	/** Locked targets farther than this are not pulled toward. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Attack Assist", meta = (ClampMin = "0.0"))
	float AttackMagnetismRange = 600.f;

	/** Capsule-to-target distance retained by attack magnetism. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Attack Assist", meta = (ClampMin = "0.0"))
	float AttackMagnetismStopDistance = 120.f;
	/** Approach budget per attack startup; separate from movement authored after the warp window. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Attack Assist", meta = (ClampMin = "0.0", Units = "cm"))
	float MaxLockOnAttackPullDistance = 60.f;
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Attack Assist", meta = (ClampMin = "0.0"))
	float MaxAttackWarpTranslation = 250.f;

	/** Root-motion destination used for camera-facing attacks without a nearby lock target. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Attack Assist", meta = (ClampMin = "0.0"))
	float FreeAttackWarpDistance = 100.f;

	/** Maximum yaw speed used only when a montage has no Motion Warping window. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Attack Assist", meta = (ClampMin = "0.0"))
	float AttackFallbackTurnRate = 540.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float DashStrength = 900.f;
	/** Sprint speed; StopSprint restores the pre-sprint CharacterMovement speed. */
	UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float SprintMaxWalkSpeed = 650.f;
    UPROPERTY(EditDefaultsOnly, Category="Movement|Input", meta=(ClampMin="0.1", ClampMax="0.35", Units="s"))
    float SprintHoldThreshold = .2f;
    /** Brief foot contact only; attacks, dodge and re-jump can interrupt it. */
    UPROPERTY(EditDefaultsOnly, Category="Movement|Transitions", meta=(ClampMin="0.0", ClampMax="0.2", Units="s"))
    float LandingContactTime = .12f;
    /** Exponential contact drag (per second); momentum decays rather than snapping to zero. */
    UPROPERTY(EditDefaultsOnly, Category="Movement|Transitions", meta=(ClampMin="0.0", ClampMax="8.0"))
    float LandingInertiaDrag = 4.f;
    bool IsLandingInertiaActive() const;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float DashCooldown = 0.8f;
    /** Ground combo may resume its next stage after its first dodge. Timed from dodge end. */
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Dash Cancel", meta = (ClampMin = "0.0", ClampMax = "2.0", Units = "s"))
    float DashComboContinueWindow = 0.65f;
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Dash Cancel", meta = (ClampMin = "0.0", ClampMax = "0.25", Units = "s"))
    float AttackDashCancelBlendOut = 0.08f;
	UPROPERTY(EditDefaultsOnly, Category = "Movement",meta = (ClampMin = "0.0"))
	float DashCost = 30.f;
	UPROPERTY(EditDefaultsOnly, Category = "Movement",meta = (ClampMin = "0.0"))
	float SprintStaminaCostPerSecond = 18.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Movement",meta = (ClampMin = "0.0"))
	float DashInvulnerabilityDuration = 0.25f;
	
	/** Legacy/fallback dash montage. Directional root-motion montages take priority. */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Dash")
	TObjectPtr<UAnimMontage> DashMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Animation|Dash")
	TObjectPtr<UAnimMontage> DashForwardMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Animation|Dash")
	TObjectPtr<UAnimMontage> DashBackwardMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Animation|Dash")
	TObjectPtr<UAnimMontage> DashLeftMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Animation|Dash")
	TObjectPtr<UAnimMontage> DashRightMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Animation|Root Motion Turn")
	bool bEnableRootMotionTurn = true;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Root Motion Turn")
	TObjectPtr<UAnimMontage> TurnLeft90Montage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Root Motion Turn")
	TObjectPtr<UAnimMontage> TurnRight90Montage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Root Motion Turn")
	TObjectPtr<UAnimMontage> TurnLeft180Montage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Root Motion Turn")
	TObjectPtr<UAnimMontage> TurnRight180Montage;
	
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> DeathMontage;
	
	UPROPERTY(EditDefaultsOnly, Category = "Animation",meta = (ClampMin = "0.0"))
	float RespawnDelayAfterDeath = 0.5f;
	UFUNCTION()
	void HandleHealthChanged(float CurrentHealth,float MaxHealth);
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Hit Reactions")
	TObjectPtr<UAnimSequenceBase> HitReactFront;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Hit Reactions")
	TObjectPtr<UAnimSequenceBase> HitReactBack;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Hit Reactions")
	TObjectPtr<UAnimSequenceBase> HitReactLeft;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Hit Reactions")
	TObjectPtr<UAnimSequenceBase> HitReactRight;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Hit Reactions")
	TObjectPtr<UAnimSequenceBase> HitReactAir;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Hit Reactions")
	TObjectPtr<UAnimSequenceBase> BlockHitReact;
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Hit Reactions")
	TObjectPtr<UAnimSequenceBase> BlockBreachReact;
	/** Minimum time movement input is ignored after taking a hit. */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Hit Reactions", meta = (ClampMin = "0.0"))
	float HitMovementLockDuration = 0.35f;

	/** Intentional camera lowering after the capsule jump has been cancelled. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Crouch", meta = (ClampMin = "0.0"))
	float CrouchCameraDrop = 15.f;
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Crouch", meta = (ClampMin = "0.1"))
	float CrouchCameraBlendSpeed = 10.f;
	float PreviousHealth = 0.f;
	float LastDashTime = -BIG_NUMBER;
	bool bIsSprinting = false;
	void Move(const FInputActionValue& Value);
	void ClearMoveInput();
	void Look(const FInputActionValue& Value);
    /** Shared Shift: release a short press to dodge; hold to sprint, never both. */
    void StartSprintOrDodgeInput();
    void FinishSprintOrDodgeInput();
    void CancelSprintOrDodgeInput();
	void StartSprint();
	void StopSprint();
	void Dash();
	void StartJump();
	void EndJump();
	void StartCrouch();
	void StopCrouch();
	void StartBlock();
	void StopBlock();
	void HandlePrimaryAttack();
	void HandleAirDiveAttack();
	virtual void Tick(float DeltaSeconds) override;
	void ToggleInventory();
	void TogglePauseMenu();
	void ToggleLockOn();
	void FinishLookInput();

	UFUNCTION(BlueprintPure, Category = "Lock On")
	AEnemyCharacter* GetLockedTarget() const { return LockedTarget; }
	UFUNCTION(BlueprintPure, Category = "Combat|Action")
	bool IsMovementInputLocked() const;
	UFUNCTION(BlueprintPure, Category = "Combat|Action")
	bool IsDashing() const { return MotionAction == ETPCMotionAction::Dodge; }
	UFUNCTION(BlueprintPure, Category = "Combat|Action")
	bool IsTurningInPlace() const { return MotionAction == ETPCMotionAction::Turn; }
	bool IsGuardHitReactionActive() const;
    bool HasMovementIntent() const { return !LastMoveInputAxis.IsNearlyZero(); }
    bool CanBlendOutOfLanding() const;
    bool IsLocomotionInputPaused() const;
private:
	friend struct FTPActionTestAccess;
	friend struct FTPCSwordPIETestAccess;
	bool StartMotionMontage(UAnimMontage* Montage, ETPCMotionAction Action);
	void CancelMotionAction(float BlendOutTime = 0.1f, bool bInterrupted = true);
	void HandleMotionMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint64 Generation);
	void HandleMotionBlendingOut(UAnimMontage* Montage, bool bInterrupted, uint64 Generation);
	void UpdateMotionAction();
	bool HasActionRotationOwner() const;
	void ApplyActionRotationPolicy();
	void StopGroundInputMomentum();
	void ClearAttackRootMotionForDodge();
	ETPCMotionAction MotionAction = ETPCMotionAction::None;
	TWeakObjectPtr<UAnimMontage> ActiveMotionMontage;
	int32 ActiveMotionInstanceId = INDEX_NONE;
	uint64 MotionGeneration = 0;
	uint64 MotionActionId = 0;
	uint64 HitActionId = 0;
	float NextTurnAllowedTime = 0.f;
	float PreSprintMaxWalkSpeed = 450.f;
    bool bSprintOrDodgeHeld = false;
    double SprintOrDodgePressedAt = 0.;
    double LocomotionInputResumeAt = 0.;
    double LandingBlendReadyAt = 0.;
    double LandingInertiaEndsAt = 0.;
    void UpdateHeldSprint();
	bool bActionDead = false;
	void LockMovementForHit(float AnimationDuration);
	void ClearHitMovementLock();
	FTimerHandle HitMovementLockTimerHandle;
	bool bMovementLockedByHit = false;
	void RestoreDoorStates(const UTPCSaveGame* SaveGame);
	void UpdateLockOn(float DeltaSeconds);
	void FaceAttackDirection();
	void UpdateAttackWarpTarget();
	void BeginAttackTargetAssist();
	void UpdateAttackTargetAssist(float DeltaSeconds);
	void SwitchLockTarget(int32 Direction);
	void SetLockedTarget(AEnemyCharacter* NewTarget);
	AEnemyCharacter* FindInitialLockTarget() const;
	bool IsValidLockTarget(const AEnemyCharacter* Candidate, bool bRequireLineOfSight) const;
	bool bCrouchInputHeld = false;
	FVector StandingCameraTargetOffset = FVector::ZeroVector;
	float ActiveCrouchHeightAdjustment = 0.f;
	float AccumulatedSwitchInput = 0.f;
	bool bTargetSwitchLatched = false;
	bool bAttackTargetAssistActive = false;
	float AttackTargetAssistEndTime = 0.f;
	float AttackFacingYaw = 0.f;
	FVector AttackAssistStartLocation = FVector::ZeroVector;
	/** Last currently-held WASD axis; cleared when the Move action completes. */
	FVector2D LastMoveInputAxis = FVector2D::ZeroVector;
};
