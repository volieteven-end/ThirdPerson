// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class USceneComponent;
class AEnemyCharacter;

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