// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TPCSaveGame.generated.h"
class UItemDefinition;
USTRUCT()
struct FSaveInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TSoftObjectPtr<UItemDefinition> ItemDefinition;

	UPROPERTY(SaveGame)
	int32 Count = 0;
};
USTRUCT()
struct FSaveDoorState
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName SaveId = NAME_None;

	UPROPERTY(SaveGame)
	bool bIsOpen = false;
};
UCLASS()
class THIRDPERSON_API UTPCSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	FTransform PlayerTransform;

	UPROPERTY(SaveGame)
	float PlayerHealth = 100.f;
	
	UPROPERTY(SaveGame)
	float PlayerStamina = 100.f;
	
	UPROPERTY(SaveGame)
	int32 EnemiesDefeated = 0;

	UPROPERTY(SaveGame)
	int32 PlayerLevel = 1;

	UPROPERTY(SaveGame)
	int32 PlayerExperience = 0;

	UPROPERTY(SaveGame)
	TArray<uint8> PlayerUpgrades;
	
	UPROPERTY(SaveGame)
	TArray<FSaveInventorySlot> InventorySlots;
	UPROPERTY(SaveGame)
	int32 InventoryCapacity = 20;
	UPROPERTY(SaveGame)
	TArray<FSaveDoorState> DoorStates;
};
