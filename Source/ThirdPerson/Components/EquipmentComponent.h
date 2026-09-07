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

	UFUNCTION(BlueprintPure, Category = "Equipment")
	UWeaponDefinition* GetEquippedWeaponDefinition() const { return EquippedWeaponDefinition; }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	AWeaponActor* GetEquippedWeaponActor() const { return EquippedWeaponActor; }

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FOnEquippedWeaponChanged OnEquippedWeaponChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Leave empty on the player to start unarmed. Useful for armed enemies and testing. */
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TObjectPtr<UWeaponDefinition> StartingWeapon;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWeaponDefinition> EquippedWeaponDefinition;

	UPROPERTY(Transient)
	TObjectPtr<AWeaponActor> EquippedWeaponActor;
};
