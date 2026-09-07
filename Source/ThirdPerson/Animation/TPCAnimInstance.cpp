#include "TPCAnimInstance.h"

#include "../Character/TPCCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "../Components/CombatComponent.h"
#include "../Components/ActionComponent.h"

void UTPCAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	Character = Cast<ATPCCharacter>(TryGetPawnOwner());
}

void UTPCAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (!Character)
	{
		Character = Cast<ATPCCharacter>(TryGetPawnOwner());
	}
	if (!Character)
	{
		return;
	}

	const FVector Velocity = Character->GetVelocity();
	GroundSpeed = Velocity.Size2D();
	VerticalVelocity = Velocity.Z;
	bShouldMove = GroundSpeed > 3.f;
	bIsLockedOn = Character->GetLockedTarget() != nullptr;
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	bIsInAir = Movement && Movement->IsFalling();
	bIsCrouched = Character->bIsCrouched;
	if (const UCombatComponent* Combat =
		Character->FindComponentByClass<UCombatComponent>())
	{
		bIsBlocking = Combat->IsBlocking();
		bIsParrying = Combat->IsParryWindowActive();
	}
	ActionState = Character->ActionComponent ? Character->ActionComponent->GetActionState() : ETPCActionState::Free;
	bGuardHitActive = Character->IsGuardHitReactionActive();
	bGuardHoldReady = bIsBlocking && !bGuardHitActive;
	bWantJumpPose = bIsInAir && !bIsBlocking;
	const FVector LocalVelocity =
		Character->GetActorTransform().InverseTransformVectorNoScale(Velocity);
	Direction = GroundSpeed > KINDA_SMALL_NUMBER
		? FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X))
		: 0.f;

	AimYawDelta = FMath::FindDeltaAngleDegrees(
		Character->GetActorRotation().Yaw,
		Character->GetControlRotation().Yaw);
	// Root-motion montages own turns. Keep the legacy enum idle so an old
	// Sequence state does not run a second turn underneath the Slot.
	bIsTurningInPlace = Character->IsTurningInPlace();
	bTurnAnimationFinished = !bIsTurningInPlace;
	TurnInPlaceDirection = ETPCTurnInPlaceDirection::None;
}

void UTPCAnimInstance::CommitTurnInPlace()
{
	// Compatibility for existing skeleton notifies only. Never rotate the Actor here.
	TurnInPlaceDirection = ETPCTurnInPlaceDirection::None;
	bTurnAnimationFinished = !Character || !Character->IsTurningInPlace();
}

