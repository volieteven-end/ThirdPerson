// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Interfaces/Interactable.h"
#include "LevelExitActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS()
class THIRDPERSON_API ALevelExitActor : public AActor,public IInteractable
{
	GENERATED_BODY()

public:
	ALevelExitActor();
	virtual FText GetInteractionText() const override;
	virtual void Interact(APawn* InstigatorPawn) override;
protected:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Components")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Components")
	TObjectPtr<UStaticMeshComponent> ExitMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Exit")
	bool bRequireKillTarget = true;
	
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleEnemyDefeated(int32 NewDefeatedCount);

	UFUNCTION(BlueprintImplementableEvent, Category = "Exit")
	void SetExitUnlockedVisual(bool bUnlocked);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Exit")
	void OnPlayerExit();
private:
	bool bIsUnlocked = false;
};