
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Interfaces/Interactable.h"
#include "PickupActor.generated.h"
class UStaticMeshComponent;
class UItemDefinition;
class URotatingMovementComponent;
/** 世界中的可拾取物品实例；交互成功后交给背包组件接收，不在 UI 中直接增添物品。 */
UCLASS()
class THIRDPERSON_API APickupActor : public AActor,public IInteractable
{
	
	GENERATED_BODY()

public:
	APickupActor();
	virtual FText GetInteractionText() const override;
	virtual void Interact(APawn* InstigatorPawn) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,Category = "Components")
	TObjectPtr<URotatingMovementComponent> RotatingMovement;
protected:
	virtual void BeginPlay() override;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Components")
	TObjectPtr<UStaticMeshComponent> PickupMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
	TObjectPtr<UItemDefinition> ItemDefinition;
	UPROPERTY(EditDefaultsOnly,Blueprintable,Category="Item",meta=(ClampMin="1"))
	int32 ItemCount=1;
	/** Maximum downward distance used to place a spawned pickup on static ground. */
	UPROPERTY(EditDefaultsOnly, Category="Pickup|Grounding", meta=(ClampMin="0.0"))
	float GroundTraceDistance = 1500.f;
	/** Small extra clearance above the hit surface after accounting for mesh bounds. */
	UPROPERTY(EditDefaultsOnly, Category="Pickup|Grounding")
	float GroundClearance = 2.f;

	void SnapToGround();
public:
	
};
