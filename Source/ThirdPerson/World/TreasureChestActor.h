
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Interfaces/Interactable.h"
#include "TreasureChestActor.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class APickupActor;

/** 宝箱交互和一次性奖励入口，根据开启状态选择模型；缺少开启模型时保留显示，不重复发奖。 */
UCLASS()
class THIRDPERSON_API ATreasureChestActor
	: public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ATreasureChestActor();
	virtual void OnConstruction(const FTransform& Transform) override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest|Appearance") TObjectPtr<UStaticMesh> ClosedMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chest|Appearance") TObjectPtr<UStaticMesh> OpenedMesh;
	UFUNCTION(BlueprintCallable, Category="Chest") void RefreshChestAppearance();

	virtual FText GetInteractionText() const override;
	virtual void Interact(APawn* InstigatorPawn) override;

protected:
	friend struct FCombatUIAccess;
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ChestMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> OriginalMesh;

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
