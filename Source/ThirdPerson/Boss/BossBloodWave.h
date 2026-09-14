#pragma once
#include "CoreMinimal.h"
#include "../Weapons/WeaponProjectile.h"
#include "BossBloodWave.generated.h"
class UParticleSystemComponent;
class UParticleSystem;
/** Countess 血浪投射物：复用投射物碰撞、伤害和对象池，仅补充飞行与撞击表现。 */
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
 float ImpactScale = .85f;
};
