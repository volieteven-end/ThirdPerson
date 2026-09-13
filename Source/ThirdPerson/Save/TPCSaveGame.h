// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TPCSaveGame.generated.h"
class UItemDefinition;
class UWeaponDefinition;
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
	/** Progress is portable; checkpoint position/doors belong only to CheckpointMap. */
	UPROPERTY(SaveGame) FString CheckpointMap;
	UPROPERTY(SaveGame) bool bHasCheckpoint = true; // Legacy saves contained a checkpoint.
	UPROPERTY(SaveGame) bool bHasEquipmentState = false;
	UPROPERTY(SaveGame) TSoftObjectPtr<UWeaponDefinition> EquippedWeapon;
	UPROPERTY(SaveGame) bool bWeaponDrawn = true;
	UPROPERTY(SaveGame) bool bDoubleJumpUnlocked = false;
	UPROPERTY(SaveGame) int32 PendingUpgradeSelections = 0;
	UPROPERTY(SaveGame) TArray<uint8> PendingUpgradeChoices;

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
