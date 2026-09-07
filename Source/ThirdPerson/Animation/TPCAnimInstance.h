#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "../Actions/ActionDefinition.h"
#include "TPCAnimInstance.generated.h"

class ATPCCharacter;

UENUM(BlueprintType)
enum class ETPCTurnInPlaceDirection : uint8
{
	None,
	Left90,
	Right90,
	Left180,
	Right180
};

UCLASS(BlueprintType, Blueprintable)
class THIRDPERSON_API UTPCAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Movement") float GroundSpeed = 0.f;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Movement") float Direction = 0.f;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Movement") float VerticalVelocity = 0.f;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Movement") bool bShouldMove = false;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Movement") bool bIsInAir = false;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Movement") bool bIsLockedOn = false;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Movement") bool bIsCrouched = false;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Combat") bool bIsBlocking = false;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Combat") bool bIsParrying = false;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Combat") ETPCActionState ActionState = ETPCActionState::Free;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Combat") bool bGuardHitActive = false;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Combat") bool bGuardHoldReady = false;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Movement") bool bWantJumpPose = false;
	/** Controller yaw relative to the character while standing still. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Turn In Place") float AimYawDelta = 0.f;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Turn In Place")
	ETPCTurnInPlaceDirection TurnInPlaceDirection = ETPCTurnInPlaceDirection::None;
	/** True when no root-motion turn is active; lets a legacy Turn state exit. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Turn In Place")
	bool bTurnAnimationFinished = true;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Turn In Place")
	bool bIsTurningInPlace = false;

	/** Compatibility hook only. The root-motion montage performs all turn rotation. */
	UFUNCTION(BlueprintCallable, Category = "Turn In Place")
	void CommitTurnInPlace();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "1.0", ClampMax = "179.0"))
	float Turn90Threshold = 55.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "1.0", ClampMax = "179.0"))
	float Turn180Threshold = 135.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Turn In Place", meta = (ClampMin = "0.02", ClampMax = "0.5"))
	float TurnFinishHoldTime = 0.12f;

private:
	friend struct FTPActionTestAccess;
	UPROPERTY(Transient) TObjectPtr<ATPCCharacter> Character;
};
