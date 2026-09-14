#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponVFXTypes.h"
#include "WeaponVFXComponent.generated.h"

class AWeaponActor;
class UWeaponVFXProfile;
class UNiagaraComponent;
class UCombatComponent;
class UEquipmentComponent;
class UActionComponent;

/** 可选的自动武器特效管理器，复用 Niagara 组件并管理启停；主角默认关闭此通道，保留动画自带特效。 */
UCLASS(ClassGroup=(Effects), meta=(BlueprintSpawnableComponent))
class THIRDPERSON_API UWeaponVFXComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UWeaponVFXComponent();
    /** 自动主角特效需要显式开启；默认由动画内的通知负责刀光，不能同时开启造成叠加。 */
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
