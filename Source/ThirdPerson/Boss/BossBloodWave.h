#pragma once
#include "CoreMinimal.h"
#include "../Weapons/WeaponProjectile.h"
#include "BossBloodWave.generated.h"
class UParticleSystemComponent;
class UParticleSystem;
/** Uses the existing bounded world pool; unlike arrows, blood waves never embed in a victim. */
UCLASS()
class THIRDPERSON_API ABossBloodWave : public AWeaponProjectile
{
 GENERATED_BODY()
public:
 ABossBloodWave();
 virtual void InitializeCombatProjectile(const FCombatHitSpec& Spec, float Speed) override;
 virtual void DeactivateProjectile() override;
protected:
 virtual void PlayImpactEffect(const FHitResult& Hit) override;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UParticleSystemComponent> FlightEffect;
 UPROPERTY(Transient) TObjectPtr<UParticleSystem> ImpactEffect;
 float EffectScale = .55f;
};
