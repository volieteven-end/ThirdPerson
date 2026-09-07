
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Interfaces/Interactable.h"
#include "TreasureChestActor.generated.h"

class UStaticMeshComponent;
class APickupActor;

UCLASS()
class THIRDPERSON_API ATreasureChestActor
	: public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ATreasureChestActor();

	virtual FText GetInteractionText() const override;
	virtual void Interact(APawn* InstigatorPawn) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ChestMesh;

	// 用现有 BP_PickupActor 或它的子类作为奖励
	UPROPERTY(EditDefaultsOnly, Category = "Reward")
	TSubclassOf<APickupActor> RewardPickupClass;

	UPROPERTY(EditDefaultsOnly, Category = "Reward",meta = (ClampMin = "1"))
	int32 RewardPickupCount = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Reward")
	float DropRadius = 80.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chest")
	bool bIsOpened = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "Chest")
	void OnChestOpened();
};