// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../World/DoorActor.h"
#include "EngineUtils.h"
#include "CheckpointActor.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;
UCLASS()
class THIRDPERSON_API ACheckpointActor : public AActor
{
	GENERATED_BODY()

public:
	ACheckpointActor();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> RespawnPoint;
	
	UPROPERTY(EditDefaultsOnly, Category = "Save")
	FString SaveSlotName = TEXT("PlayerSave");

	UFUNCTION()
	void HandlePlayerEnter(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
	bool bIsActivated = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "Save")
	void OnCheckpointActivated();
	
};