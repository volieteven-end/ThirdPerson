#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EquipmentComponent.generated.h"

class AWeaponActor;
class UWeaponDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnEquippedWeaponChanged,
	UWeaponDefinition*, WeaponDefinition,
	AWeaponActor*, WeaponActor);

/** 管理装备定义与实际武器 Actor，通知换装并切换手持／背挂；不把装备基础数值直接当作最终伤害。 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THIRDPERSON_API UEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEquipmentComponent();

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool EquipWeapon(UWeaponDefinition* NewWeaponDefinition);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void UnequipWeapon();

    /** 动画提交时将同一个武器 Actor 切换到手部或背部挂点。 */
    UFUNCTION(BlueprintCallable, Category = "Equipment") void SetWeaponDrawn(bool bDrawn);
    UFUNCTION(BlueprintPure, Category = "Equipment") bool IsWeaponDrawn() const { return bWeaponDrawn; }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	UWeaponDefinition* GetEquippedWeaponDefinition() const { return EquippedWeaponDefinition; }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	AWeaponActor* GetEquippedWeaponActor() const { return EquippedWeaponActor; }

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FOnEquippedWeaponChanged OnEquippedWeaponChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 主角留空则以徒手开始；预装备主要用于敌人和测试。 */
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TObjectPtr<UWeaponDefinition> StartingWeapon;

private:
    bool bWeaponDrawn = true;
	UPROPERTY(Transient)
	TObjectPtr<UWeaponDefinition> EquippedWeaponDefinition;

	UPROPERTY(Transient)
	TObjectPtr<AWeaponActor> EquippedWeaponActor;
};
