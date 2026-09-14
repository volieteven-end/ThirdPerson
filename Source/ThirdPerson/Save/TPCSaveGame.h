
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TPCSaveGame.generated.h"
class UItemDefinition;
class UWeaponDefinition;
/** 可序列化的背包槽位，只保存恢复物品所需的数据，不保存世界中的拾取 Actor。 */
USTRUCT()
struct FSaveInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TSoftObjectPtr<UItemDefinition> ItemDefinition;

	UPROPERTY(SaveGame)
	int32 Count = 0;
};
/** 属于检查点地图的门状态，跨地图时不能直接应用到其他地图。 */
USTRUCT()
struct FSaveDoorState
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName SaveId = NAME_None;

	UPROPERTY(SaveGame)
	bool bIsOpen = false;
};
/** 正式存档容器：分开保存角色进度和地图检查点数据；保留旧字段默认值以兼容已有存档。 */
UCLASS()
class THIRDPERSON_API UTPCSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Progress is portable; checkpoint position/doors belong only to CheckpointMap. */
	UPROPERTY(SaveGame) FString CheckpointMap;
	UPROPERTY(SaveGame) bool bHasCheckpoint = true; // 旧存档默认包含检查点，保留该默认值用于兼容。
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
