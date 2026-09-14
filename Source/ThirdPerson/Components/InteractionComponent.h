
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnInteractionPromptChanged,
	const FText&,
	PromptText);

/** 选择当前可交互对象并更新提示；拾取优先考虑角色前方范围，其他交互遵守距离与目标规则。 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class THIRDPERSON_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractionPromptChanged OnInteractionPromptChanged;
	UPROPERTY(EditDefaultsOnly, Category = "Interaction",meta = (ClampMin = "0.0"))
	float TraceDistance = 1000.f;
	UPROPERTY(EditDefaultsOnly, Category = "Interaction",meta = (ClampMin = "0.0"))
	float MaxInteractionDistance = 200.f;
	/** Pickups inside this player-facing semicircle can be collected without aiming the camera. */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Pickup", meta = (ClampMin = "0.0"))
	float PickupRadius = 220.f;
	/** Prevents a pickup far above or below the character from being selected. */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Pickup", meta = (ClampMin = "0.0"))
	float PickupVerticalTolerance = 140.f;
	virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
protected:
	virtual void BeginPlay() override;

public:
	
	void TryInteract();
private:
	class APickupActor* FindBestPickupInFront(const APawn* OwnerPawn) const;
	UPROPERTY(Transient)
	TObjectPtr<AActor> FocusedActor;
	void UpdateInteractionPrompt();
};
