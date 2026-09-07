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

    /** Animation commit changes visibility only; it never respawns the equipped actor. */
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

	/** Leave empty on the player to start unarmed. Useful for armed enemies and testing. */
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TObjectPtr<UWeaponDefinition> StartingWeapon;

private:
    bool bWeaponDrawn = true;
	UPROPERTY(Transient)
	TObjectPtr<UWeaponDefinition> EquippedWeaponDefinition;

	UPROPERTY(Transient)
	TObjectPtr<AWeaponActor> EquippedWeaponActor;
};
