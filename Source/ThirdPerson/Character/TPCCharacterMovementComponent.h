#pragma once
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TPCCharacterMovementComponent.generated.h"

/** Brief physical landing drag without clearing momentum or replacing walking collision. */
UCLASS()
class THIRDPERSON_API UTPCCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()
public:
    virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
};
