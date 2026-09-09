// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractionComponent.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"
class UItemDefinition;
USTRUCT( BlueprintType )
struct FInventorySlot
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly)
	FName ItemId=NAME_None;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly)
	int32 Count=0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UItemDefinition> ItemDefinition;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnConsumableUsed, UItemDefinition*, float, int32);
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THIRDPERSON_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UInventoryComponent();
	bool AddItem(
	UItemDefinition* InItemDefinition,
	int32 Count);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory",
	meta = (ClampMin = "1"))
	int32 MaxSlots = 20;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Inventory")
	TArray<FInventorySlot> Slots;
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;
	bool UseItemAtSlot(int32 SlotIndex);
	/** Successful healing only; quantity is the remaining stack total after consumption. */
	FOnConsumableUsed OnConsumableUsed;
	/** The same definition as the world's health pickup; also used by the quick slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Health Potion")
	TObjectPtr<UItemDefinition> HealthPotionDefinition;
	UFUNCTION(BlueprintPure, Category = "Inventory|Health Potion")
	int32 GetHealthPotionCount() const;
	bool RefillHealthPotionsAfterDeath();
	void RestoreRespawnInventory(const UInventoryComponent& Source);
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
	void TryUseFirstConsumable();
	bool UseFirstConsumable();
	bool HasItem(const FName& ItemId) const;
	bool RemoveItem(const FName& ItemId, int32 Count);
};
