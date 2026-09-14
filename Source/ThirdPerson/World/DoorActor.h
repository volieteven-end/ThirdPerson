
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Interfaces/Interactable.h"
#include "Components/SceneComponent.h"
#include "DoorActor.generated.h"
class UInventoryComponent;
class USceneComponent;
class UStaticMeshComponent;

/** 可交互的门，结合背包条件处理开启并暴露门状态，供所属地图的检查点保存。 */
UCLASS()
class THIRDPERSON_API ADoorActor
	: public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADoorActor();

	virtual void Tick(float DeltaTime) override;

	virtual FText GetInteractionText() const override;
	virtual void Interact(APawn* InstigatorPawn) override;
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> DoorHinge;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Door")
	float OpenYaw = 90.f;

	UPROPERTY(EditDefaultsOnly, Category = "Door")
	float OpenSpeed = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Door|Lock")
	bool bRequiresKey = false;

	UPROPERTY(EditDefaultsOnly, Category = "Door|Lock",meta = (EditCondition = "bRequiresKey"))
	FName RequiredKeyId = TEXT("Key");
	UPROPERTY(EditDefaultsOnly, Category = "Door|Lock",meta = (EditCondition = "bRequiresKey"))
	bool bConsumeKeyOnOpen = false;
	UPROPERTY(EditInstanceOnly, Category = "Save")
	FName SaveId;
	FName GetSaveId() const { return SaveId; }
	bool IsOpen() const { return bIsOpen; }
	void RestoreOpenState(bool bShouldBeOpen);
	
protected:
	virtual void BeginPlay() override;
private:
	bool bIsOpen = false;
	FRotator ClosedRotation;
	FRotator TargetRotation;
};