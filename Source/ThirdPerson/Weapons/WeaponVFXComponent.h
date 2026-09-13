#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponVFXTypes.h"
#include "WeaponVFXComponent.generated.h"

class AWeaponActor;
class UWeaponVFXProfile;
class UNiagaraComponent;
class UNiagaraSystem;
class UCombatComponent;
class UEquipmentComponent;
class UActionComponent;

/** Two reusable Niagara components; polls buff state at 20 Hz, ticks sockets only while visible. */
UCLASS(ClassGroup=(Effects), meta=(BlueprintSpawnableComponent))
class THIRDPERSON_API UWeaponVFXComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UWeaponVFXComponent();
    /** Legacy automatic player effects are opt-in. Authored animation notifies own sword FX by default. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effects") bool bEnableAutomaticEffects = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Effects") TObjectPtr<UWeaponVFXProfile> Profile;
    void SetAttackActive(bool bActive);
    void SetStyle(EWeaponVFXStyle InStyle) { RequestedStyle = InStyle; }
    bool IsTrailActive() const { return bTrailActive; }
    bool IsBuffActive() const { return bBuffActive; }
    EWeaponVFXStyle GetResolvedStyle() const;
    void RefreshState();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function) override;
private:
    friend struct FCombatVFXTestAccess;
    void SuppressLegacyTrail();
    bool ReadBlade(FVector& Base, FVector& Tip) const;
    bool CanShow() const;
    void UpdateParameters(UNiagaraComponent* FX, bool bSword, const FVector& Base, const FVector& Tip);
    void StartTrail();
    void StopAll();
    UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Trail;
    UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Sword;
    TWeakObjectPtr<AWeaponActor> Weapon;
    TWeakObjectPtr<UCombatComponent> Combat;
    TWeakObjectPtr<UEquipmentComponent> Equipment;
    TWeakObjectPtr<UActionComponent> Actions;
    FTimerHandle StateTimer;
    EWeaponVFXStyle RequestedStyle = EWeaponVFXStyle::Automatic;
    EWeaponVFXStyle PlayingStyle = EWeaponVFXStyle::Automatic;
    bool bTrailActive = false;
    bool bBuffActive = false;
    bool bHavePreviousOwnerLocation = false;
    FVector PreviousOwnerLocation = FVector::ZeroVector;
};
