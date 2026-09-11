#pragma once
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TPCCharacterMovementComponent.generated.h"

/** Landing inertia and a swept, dive-only escape from non-standable enemy capsules. */
UCLASS()
class THIRDPERSON_API UTPCCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()
public:
    virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
    virtual bool IsWalkable(const FHitResult& Hit) const override;
    virtual void PhysFalling(float DeltaTime, int32 Iterations) override;
    virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;
    virtual void ProcessLanded(const FHitResult& Hit, float RemainingTime, int32 Iterations) override;
    virtual void JumpOff(AActor* MovementBaseActor) override;

private:
    bool IsDiveObstacle(const AActor* Actor) const;
    void MoveOffDiveObstacle(float DeltaTime);
    TWeakObjectPtr<AActor> DiveObstacle;
    FVector DiveEscapeDirection = FVector::ZeroVector;
};
