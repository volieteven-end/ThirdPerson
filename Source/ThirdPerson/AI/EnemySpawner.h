
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class USceneComponent;
class AEnemyCharacter;

/** 普通关卡的单敌人生成与补充点：敌人销毁后延迟重生；随机竞技场波次由独立管理器负责。 */
UCLASS()
class THIRDPERSON_API AEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawner();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditInstanceOnly, Category = "Spawner")
	TSubclassOf<AEnemyCharacter> EnemyClass;

	UPROPERTY(EditAnywhere, Category = "Spawner",
		meta = (ClampMin = "0.0"))
	float RespawnDelay = 3.f;

private:
	void SpawnEnemy();

	UFUNCTION()
	void HandleEnemyDestroyed(AActor* DestroyedActor);

	UPROPERTY()
	TObjectPtr<AEnemyCharacter> CurrentEnemy;

	FTimerHandle RespawnTimerHandle;
};