#include "TPCCharacterMovementComponent.h"
#include "TPCCharacter.h"

void UTPCCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
    const auto* Player = Cast<ATPCCharacter>(CharacterOwner);
    if (HasValidData() && Player && Player->IsLandingInertiaActive() && !HasAnimRootMotion())
    {
        // Short, frame-rate independent contact drag. Default walking braking would stop
        // a 450 cm/s run almost entirely during this 0.12 s input blend; never zero it here.
        const float Retention = FMath::Exp(-FMath::Max(0.f, Player->LandingInertiaDrag) * FMath::Max(0.f, DeltaTime));
        Velocity += (Retention - 1.f) * ProjectToGravityFloor(Velocity);
        return; // PhysWalking still sweeps, follows slopes, and resolves obstacles normally.
    }
    Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}
