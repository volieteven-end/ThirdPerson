
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemDefinition.generated.h"
class APickupActor;
class UTexture2D;
/**
 * 
 */
UENUM(BlueprintType)
enum class EItemType : uint8
{
	Material,
	Consumable
};
/** 物品的共享静态配置，描述名称、类型和使用效果；持有数量由背包槽位保存。 */
UCLASS(BlueprintType)
class THIRDPERSON_API UItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item",
		meta = (ClampMin = "1"))
	int32 MaxStack = 99;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EItemType ItemType = EItemType::Material;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item",meta = (ClampMin = "0.0"))
	float HealAmount = 0.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,Category = "Item|World")
	TSubclassOf<APickupActor> WorldPickupClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,Category = "Item|UI")
	TObjectPtr<UTexture2D> Icon;
};
