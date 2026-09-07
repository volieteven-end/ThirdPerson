#pragma once
#include "CoreMinimal.h"
#include "../Weapons/WeaponProjectile.h"
#include "BossBloodWave.generated.h"
class UStaticMeshComponent;
/** Uses the existing bounded world pool; unlike arrows, blood waves never embed in a victim. */
UCLASS()
class THIRDPERSON_API ABossBloodWave : public AWeaponProjectile
{
 GENERATED_BODY()
public:
 ABossBloodWave();
 virtual void InitializeCombatProjectile(const FCombatHitSpec& Spec, float Speed) override;
protected:
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
};
