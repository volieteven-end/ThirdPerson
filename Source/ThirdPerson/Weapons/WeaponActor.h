#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponActor.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class UWeaponDefinition;
class UWeaponVFXComponent;

/** 已装备武器的场景实例，负责模型挂载和表现入口；基础数值来自武器定义，伤害由战斗组件计算。 */
UCLASS()
class THIRDPERSON_API AWeaponActor : public AActor
{
	GENERATED_BODY()

public:
	AWeaponActor();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon|Effects")
	TObjectPtr<UWeaponVFXComponent> WeaponVFX;

	void InitializeWeapon(UWeaponDefinition* InWeaponDefinition);

	UWeaponDefinition* GetWeaponDefinition() const { return WeaponDefinition; }
	UStaticMeshComponent* GetWeaponMesh() const { return WeaponMesh; }
	USkeletalMeshComponent* GetSkeletalWeaponMesh() const { return SkeletalWeaponMesh; }
	/** Blueprint weapon owns the actual Niagara/Cascade component and decides how to show it. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Weapon|Effects")
	void SetAttackEffectActive(bool bActive);
	virtual void SetAttackEffectActive_Implementation(bool bActive);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	/** Use this component for animated weapons such as bows or skeletal swords. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> SkeletalWeaponMesh;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UWeaponDefinition> WeaponDefinition;
};
