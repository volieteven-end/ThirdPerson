#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Interfaces/Interactable.h"
#include "StarterWeaponNPC.generated.h"

class APawn;
class UBoxComponent;
class UStarterWeaponWidget;
class UStaticMeshComponent;
class UWeaponDefinition;

UENUM(BlueprintType)
enum class EStarterWeaponChoice : uint8
{
	Melee,
	Ranged
};

/** Interaction-only starter NPC. Its mesh, dialogue art and animations are optional. */
UCLASS()
class THIRDPERSON_API AStarterWeaponNPC : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AStarterWeaponNPC();

	virtual FText GetInteractionText() const override;
	virtual void Interact(APawn* InstigatorPawn) override;

	bool CompleteWeaponChoice(EStarterWeaponChoice Choice);
	void CancelWeaponSelection();

	FText GetMeleeWeaponName() const;
	FText GetRangedWeaponName() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> NPCMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starter Weapons")
	TObjectPtr<UWeaponDefinition> MeleeWeaponDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Starter Weapons")
	TObjectPtr<UWeaponDefinition> RangedWeaponDefinition;

	UPROPERTY(EditDefaultsOnly, Category = "Starter Weapons|UI")
	TSubclassOf<UStarterWeaponWidget> SelectionWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Starter Weapons")
	bool bOnlyGrantOnce = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Starter Weapons")
	bool bWeaponClaimed = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "Starter Weapons")
	void OnWeaponGranted(UWeaponDefinition* GrantedWeapon);

private:
	UPROPERTY(Transient)
	TObjectPtr<APawn> PendingPawn;

	UPROPERTY(Transient)
	TObjectPtr<UStarterWeaponWidget> SelectionWidget;
};
