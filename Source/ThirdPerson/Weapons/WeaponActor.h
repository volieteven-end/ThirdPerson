#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponActor.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class UWeaponDefinition;

/** Visual world representation of an equipped weapon. */
UCLASS()
class THIRDPERSON_API AWeaponActor : public AActor
{
	GENERATED_BODY()

public:
	AWeaponActor();

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
