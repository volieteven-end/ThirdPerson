#include "TPCCharacterMovementComponent.h"
#include "TPCCharacter.h"
#include "../AI/EnemyCharacter.h"
#include "../Components/CombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

bool UTPCCharacterMovementComponent::IsDiveObstacle(const AActor* Actor) const
{
    const auto* Player = Cast<ATPCCharacter>(CharacterOwner);
    return Actor && Actor->IsA<AEnemyCharacter>() && Player && Player->CombatComponent &&
        Player->CombatComponent->IsAirDiveDescending();
}

bool UTPCCharacterMovementComponent::IsWalkable(const FHitResult& Hit) const
{
    // Also used by the combat component's approach trace: a capsule top is not ground.
    return !IsDiveObstacle(Hit.GetActor()) && Super::IsWalkable(Hit);
}

void UTPCCharacterMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
    if (IsFalling() && IsDiveObstacle(Hit.GetActor()) &&
        FVector::DotProduct(GetActorFeetLocation() - Hit.GetActor()->GetActorLocation(), -GetGravityDirection()) >= 0.f)
    {
        if (DiveObstacle != Hit.GetActor()) DiveEscapeDirection = FVector::ZeroVector;
        DiveObstacle = Hit.GetActor();
    }
    Super::HandleImpact(Hit, TimeSlice, MoveDelta);
}

void UTPCCharacterMovementComponent::ProcessLanded(const FHitResult& Hit, float RemainingTime, int32 Iterations)
{
    if (IsDiveObstacle(Hit.GetActor()))
    {
        // PhysFalling has a stuck/ditch fallback that can call this even when
        // IsValidLandingSpot is false. Do not emit a false Landed or reset the air budget.
        if (DiveObstacle != Hit.GetActor()) DiveEscapeDirection = FVector::ZeroVector;
        DiveObstacle = Hit.GetActor();
        return;
    }
    DiveObstacle.Reset(); DiveEscapeDirection = FVector::ZeroVector;
    Super::ProcessLanded(Hit, RemainingTime, Iterations);
}

void UTPCCharacterMovementComponent::JumpOff(AActor* MovementBaseActor)
{
    if (IsDiveObstacle(MovementBaseActor))
    {
        DiveObstacle = MovementBaseActor;
        const FVector Up = -GetGravityDirection();
        Velocity -= Up * FMath::Max(0.f, FVector::DotProduct(Velocity, Up));
        SetMovementMode(MOVE_Falling);
        return; // No random XY kick or upward launch while descending into a dive.
    }
    Super::JumpOff(MovementBaseActor);
}

void UTPCCharacterMovementComponent::PhysFalling(float DeltaTime, int32 Iterations)
{
    if (IsDiveObstacle(DiveObstacle.Get())) MoveOffDiveObstacle(DeltaTime);
    else { DiveObstacle.Reset(); DiveEscapeDirection = FVector::ZeroVector; }
    Super::PhysFalling(DeltaTime, Iterations);
}

void UTPCCharacterMovementComponent::MoveOffDiveObstacle(float DeltaTime)
{
    const auto* Enemy = Cast<AEnemyCharacter>(DiveObstacle.Get());
    if (!HasValidData() || !Enemy || DeltaTime <= 0.f) return;
    const FVector Up = -GetGravityDirection();
    const FVector Start = UpdatedComponent->GetComponentLocation();
    const FVector Away = FVector::VectorPlaneProject(Start - Enemy->GetActorLocation(), Up);
    const float Clearance = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() +
        Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius() + 4.f;
    if (Away.SizeSquared() >= FMath::Square(Clearance))
    {
        DiveObstacle.Reset(); DiveEscapeDirection = FVector::ZeroVector; return;
    }
    FVector Preferred = DiveEscapeDirection;
    if (Preferred.IsNearlyZero()) Preferred = Away.SizeSquared() > 1.f ? Away.GetSafeNormal() :
        FVector::VectorPlaneProject(-CharacterOwner->GetActorForwardVector(), Up).GetSafeNormal();

    FCollisionQueryParams Query(SCENE_QUERY_STAT(DiveHeadEscape), false, CharacterOwner);
    FCollisionResponseParams Response;
    InitCollisionParams(Query, Response);
    // Ignore the contacted capsule only in planning. The actual movement still
    // sweeps against it, other pawns, and the world; no collision state is changed.
    Query.AddIgnoredActor(Enemy);
    const auto Shape = GetPawnCapsuleCollisionShape(SHRINK_None);
    const auto Channel = UpdatedPrimitive->GetCollisionObjectType();
    for (const float Angle : {0.f, 45.f, -45.f, 90.f, -90.f, 135.f, -135.f, 180.f})
    {
        const FVector Direction = Preferred.RotateAngleAxis(Angle, Up);
        const float Along = FVector::DotProduct(Away, Direction);
        if (Along < -.01f) continue; // Never try to climb back up the capsule dome.
        const float Distance = -Along + FMath::Sqrt(FMath::Max(0.f, Along * Along + Clearance * Clearance - Away.SizeSquared()));
        const FVector Delta = Direction * Distance;
        const FVector End = Start + Delta;
        FHitResult Route, Floor;
        if (GetWorld()->SweepSingleByChannel(Route, Start, End, UpdatedComponent->GetComponentQuat(), Channel, Shape, Query, Response)) continue;
        const FVector EndFeet = GetActorFeetLocation() + Delta;
        // Do not steer off a ledge or onto another enemy. This is at most one
        // capsule-width adjustment toward nearby supporting world geometry.
        const float FloorSearch = Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.f + MaxStepHeight + 20.f;
        if (!GetWorld()->LineTraceSingleByChannel(Floor, EndFeet + Up * 5.f, EndFeet - Up * FloorSearch, Channel, Query, Response) ||
            !IsWalkable(Floor)) continue;
        FHitResult MoveHit;
        const FVector Step = Delta.GetClampedToMaxSize(300.f * DeltaTime);
        SafeMoveUpdatedComponent(Step, UpdatedComponent->GetComponentQuat(), true, MoveHit);
        if (FVector::DistSquared(Start, UpdatedComponent->GetComponentLocation()) > .01f)
        {
            DiveEscapeDirection = Direction;
            return;
        }
    }
}

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
