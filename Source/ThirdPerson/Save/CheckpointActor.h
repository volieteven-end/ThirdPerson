
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheckpointActor.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;
/** 普通关卡检查点：玩家进入后保存所属地图、重生位置和角色进度，并通知关卡表现。 */
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